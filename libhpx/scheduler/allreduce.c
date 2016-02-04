// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/scheduler/allreduce.c
/// @brief Defines the all-reduction LCO.

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Local reduce interface.
/// @{
typedef struct {
  lco_t          lco;
  cvar_t        wait;
  size_t     readers;
  size_t     writers;
  hpx_action_t    id;
  hpx_action_t    op;
  size_t       count;
  volatile int phase;
  void        *value;  // out-of-place for alignment reasons
} _allreduce_t;

static const int REDUCING = 0;
static const int READING = 1;

static void _op(_allreduce_t *r, size_t size, const void *from) {
  dbg_assert(!size || r->op);
  dbg_assert(!size || from);
  if (size) {
    handler_t f = actions[r->op].handler;
    hpx_monoid_op_t op = (hpx_monoid_op_t)f;
    op(r->value, from, size);
  }
}

static void _id(_allreduce_t *r, size_t size) {
  dbg_assert(!size || r->id);
  if (r->id) {
    handler_t f = actions[r->id].handler;
    hpx_monoid_id_t id = (hpx_monoid_id_t)f;
    id(r->value, size);
  }
}

static int _set(_allreduce_t *allreduce, size_t size, const void *value) {
  // wait until we're reducing (rather than reading) and then perform the
  // operation
  while (allreduce->phase != REDUCING) {
    if (HPX_SUCCESS != scheduler_wait(&allreduce->lco.lock, &allreduce->wait)) {
      return 0;
    }
  }

  _op(allreduce, size, value);

  // if we were the last one to arrive then its our responsibility to switch the
  // phase
  if (0 == --allreduce->count) {
    allreduce->phase = READING;
    scheduler_signal_all(&allreduce->wait);
    return 1;
  }

  return 0;
}

static hpx_status_t _get(_allreduce_t *r, int size, void *out) {
  dbg_assert(!size || out);
  hpx_status_t rc = HPX_SUCCESS;

  // wait until we're reading
  while ((r->phase != READING) && (rc == HPX_SUCCESS)) {
    rc = scheduler_wait(&r->lco.lock, &r->wait);
  }

  if (rc != HPX_SUCCESS) {
    return rc;
  }

  // copy out the value if the caller wants it
  if (size) {
    memcpy(out, r->value, size);
  }

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to reducing---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier READING epochs because sets don't block
  if (r->readers == ++r->count) {
    r->count = r->writers;
    r->phase = REDUCING;
    _id(r, size);
    scheduler_signal_all(&r->wait);
    return rc;
  }

  while ((r->phase == READING) && (rc == HPX_SUCCESS)) {
    rc = scheduler_wait(&r->lco.lock, &r->wait);
  }
  return rc;
}

static size_t _allreduce_size(lco_t *lco) {
  _allreduce_t *allreduce = (_allreduce_t *)lco;
  return sizeof(*allreduce);
}

/// Deletes a reduction.
static void _allreduce_fini(lco_t *lco) {
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  if (r->value) {
    free(r->value);
  }
  lco_fini(lco);
}

/// Handle an error condition.
static void _allreduce_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  scheduler_signal_error(&r->wait, code);
  lco_unlock(lco);
}

static void _allreduce_reset(lco_t *lco) {
  _allreduce_t *r = (_allreduce_t *)lco;
  lco_lock(&r->lco);
  dbg_assert_str(cvar_empty(&r->wait),
                 "Reset on allreduce LCO that has waiting threads.\n");
  cvar_reset(&r->wait);
  lco_unlock(&r->lco);
}

/// Update the reduction, will wait if the phase is reading.
static int _allreduce_set(lco_t *lco, int size, const void *from) {
  int set = 0;
  lco_lock(lco);
  set = _set((_allreduce_t *)lco, size, from);
  lco_unlock(lco);
  return set;
}

static hpx_status_t _allreduce_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;

  // Pick attach to mean "set" for allreduce. We have to wait for reducing to
  // complete before sending the parcel.
  if (r->phase != REDUCING) {
    status = cvar_attach(&r->wait, p);
    goto unlock;
  }

  // If the allreduce has an error, then return that error without sending the
  // parcel.
  status = cvar_get_error(&r->wait);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // Go ahead and send this parcel eagerly.
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

/// Get the value of the reduction, will wait if the phase is reducing.
static hpx_status_t _allreduce_get(lco_t *lco, int size, void *out, int reset) {
  hpx_status_t rc = HPX_SUCCESS;
  lco_lock(lco);
  rc = _get((_allreduce_t *)lco, size, out);
  lco_unlock(lco);
  return rc;
}

// Wait for the reduction, loses the value of the reduction for this round.
static hpx_status_t _allreduce_wait(lco_t *lco, int reset) {
  return _allreduce_get(lco, 0, NULL, reset);
}

// We universally clone the buffer here, because the all* family of LCOs will
// reset themselves so we can't retain a pointer to their buffer.
static hpx_status_t
_allreduce_getref(lco_t *lco, int size, void **out, int *unpin) {
  *out = registered_malloc(size);
  *unpin = 1;
  return _allreduce_get(lco, size, *out, 0);
}

// We know that allreduce buffers were always copies, so we can just free them
// here.
static int _allreduce_release(lco_t *lco, void *out) {
  registered_free(out);
  return 0;
}

// vtable
static const lco_class_t _allreduce_vtable = {
  .on_fini     = _allreduce_fini,
  .on_error    = _allreduce_error,
  .on_set      = _allreduce_set,
  .on_attach   = _allreduce_attach,
  .on_get      = _allreduce_get,
  .on_getref   = _allreduce_getref,
  .on_release  = _allreduce_release,
  .on_wait     = _allreduce_wait,
  .on_reset    = _allreduce_reset,
  .on_size     = _allreduce_size
};

static int
_allreduce_init_handler(_allreduce_t *r, size_t writers, size_t readers,
                        size_t size, hpx_action_t id, hpx_action_t op) {
  if (size) {
    assert(id);
    assert(op);
  }

  lco_init(&r->lco, &_allreduce_vtable);
  cvar_reset(&r->wait);
  r->readers = readers;
  r->op = op;
  r->id = id;
  r->count = writers;
  r->writers = writers;
  r->phase = REDUCING;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
    _id(r, size);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _allreduce_init_async,
                     _allreduce_init_handler, HPX_POINTER, HPX_SIZE_T,
                     HPX_SIZE_T, HPX_SIZE_T, HPX_ACTION_T, HPX_ACTION_T);
/// @}

hpx_addr_t hpx_lco_allreduce_new(size_t inputs, size_t outputs, size_t size,
                                 hpx_action_t id, hpx_action_t op) {
  _allreduce_t *r = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*r), 0);

  if (!hpx_gas_try_pin(gva, (void**)&r)) {
    int e = hpx_call_sync(gva, _allreduce_init_async, NULL, 0, &inputs,
                          &outputs, &size, &id, &op);
    dbg_check(e, "could not initialize an allreduce at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, r);
    _allreduce_init_handler(r, inputs, outputs, size, id, op);
    hpx_gas_unpin(gva);
  }

  return gva;
}

/// Initialize a block allreduce LCOs.
static int
_block_local_init_handler(void *lco, int n, size_t participants, size_t readers,
                          size_t size, hpx_action_t id, hpx_action_t op) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_allreduce_t) + size));
    _allreduce_init_handler(addr, participants, readers, size, id, op);
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler, HPX_POINTER, HPX_INT,
                     HPX_SIZE_T, HPX_SIZE_T, HPX_POINTER, HPX_SIZE_T,
                     HPX_POINTER);

hpx_addr_t
hpx_lco_allreduce_local_array_new(int n, size_t participants, size_t readers,
                                  size_t size, hpx_action_t id, hpx_action_t op)
{
  uint32_t lco_bytes = sizeof(_allreduce_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  dbg_check( hpx_call_sync(base, _block_local_init, NULL, 0, &n, &participants,
                           &readers, &size, &id, &op) );

  // return the base address of the allocation
  return base;
}

static int
_join_sync(_allreduce_t *allreduce, size_t size, const void *value, void *out) {
  int rc = HPX_SUCCESS;
  lco_lock(&allreduce->lco);
  _set(allreduce, size, value);
  rc = _get(allreduce, size, out);
  lco_unlock(&allreduce->lco);
  return rc;
}

/// Generic allreduce join functionaliy
/// @{

/// This address just continues the reduced data after the join.
static int _join_handler(_allreduce_t *lco, const void *data, size_t n) {
  hpx_parcel_t *p = self->current;

  // Allocate a parcel that targeting our continuation with enough space for the
  // reduced value, and use its data buffer to join---this prevents a copy or
  // two. This "steals" the current continuation.
  hpx_parcel_t *c = parcel_new(p->c_target, p->c_action, 0, 0, p->pid, NULL, n);
  dbg_assert(c);
  p->c_target = 0;
  p->c_action = 0;
  int rc = _join_sync(lco, n, data, hpx_parcel_get_data(c));
  parcel_launch_error(c, rc);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_PINNED, _join,
                     _join_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

hpx_status_t
hpx_lco_allreduce_join(hpx_addr_t lco, int id, size_t n, const void *value,
                       hpx_action_t cont, hpx_addr_t at) {
  dbg_assert(cont && at);

  // This interface is *asynchronous* so we can just blast out a
  // call-with-continuation using the arguments, even if the allreduce is
  // local.
  return hpx_call_with_continuation(lco, _join, at, cont, value, n);
}
/// @}

/// Synchronous allreduce join interface.
hpx_status_t
hpx_lco_allreduce_join_sync(hpx_addr_t lco, int id, size_t n,
                            const void *value, void *out) {
  _allreduce_t *allreduce = NULL;
  if (!hpx_gas_try_pin(lco, (void**)&allreduce)) {
    return hpx_call_sync(lco, _join, out, n, value, n);
  }

  int rc = _join_sync(allreduce, n, value, out);
  hpx_gas_unpin(lco);
  return rc;
}

/// @{
/// Async allreduce join functionality. This is slightly complicated because of
/// the interface. It's set up like a memget-with-completion interface where the
/// user is supplying both a local target address and an LCO to signal when the
/// get is complete. The LCO isn't necessarily local to the caller though, so we
/// need to pass it through a couple of levels. We use an explicit request-reply
/// mechanism here, rather than relying on pwc, because of that.

/// The request-reply structure is a header and a data buffer.
typedef struct {
  void *out;
  hpx_addr_t done;
  char data[];
} _join_async_args_t;

/// The request handler uses the _join_sync operation to do the join, and then
/// continues the reduced value along with the header information.
static int
_join_async_request_handler(_allreduce_t *lco, _join_async_args_t *args,
                            size_t n) {
  size_t bytes = n - sizeof(*args);
  int rc = _join_sync(lco, bytes, &args->data, &args->data);
  return (rc != HPX_SUCCESS) ? rc : hpx_thread_continue(args, n);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_PINNED,
                     _join_async_request, _join_async_request_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// The join reply handler copies the data as specified in the arguments, and
/// then signals the done LCO explicitly.
static int _join_async_reply_handler(_join_async_args_t *args, size_t n) {
  size_t bytes = n - sizeof(*args);
  if (bytes) {
    memcpy(args->out, args->data, bytes);
  }
  hpx_lco_error(args->done, HPX_SUCCESS, HPX_NULL);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _join_async_reply,
                     _join_async_reply_handler, HPX_POINTER, HPX_SIZE_T);

hpx_status_t
hpx_lco_allreduce_join_async(hpx_addr_t lco, int id, size_t n,
                             const void *value, void *out, hpx_addr_t done) {
  _allreduce_t *allreduce = NULL;
  if (!hpx_gas_try_pin(lco, (void**)&allreduce)) {
    // avoid extra copy by allocating and sending a parcel directly
    size_t bytes = sizeof(_join_async_args_t) + n;
    uint64_t pid = self->current->pid;
    hpx_parcel_t *p = parcel_new(lco, _join_async_request, HPX_HERE,
                                 _join_async_reply, pid, NULL, bytes);
    _join_async_args_t *args = hpx_parcel_get_data(p);
    args->out = out;
    args->done = done;
    memcpy(args->data, value, n);
    parcel_launch(p);
    return HPX_SUCCESS;
  }

  int rc = _join_sync(allreduce, n, value, out);
  hpx_lco_error(done, rc, HPX_NULL);
  hpx_gas_unpin(lco);
  return rc;
}
/// @}

