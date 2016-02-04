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

/// @file libhpx/scheduler/reduce.c
/// @brief Defines the reduction LCO.

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Local reduce interface.
/// @{
typedef struct {
  lco_t              lco;
  cvar_t         barrier;
  hpx_action_t        id;
  hpx_action_t        op;
  size_t            size;
  int             inputs;
  volatile int remaining;
  void            *value;
} _reduce_t;

static void _reset(_reduce_t *r) {
  dbg_assert_str(cvar_empty(&r->barrier),
                 "Reset on allreduce LCO that has waiting threads.\n");
  cvar_reset(&r->barrier);
  r->remaining = r->inputs;

  handler_t f = actions[r->id].handler;
  hpx_monoid_id_t id = (hpx_monoid_id_t)f;
  id(r->value, r->size);
}

static size_t _reduce_size(lco_t *lco) {
  _reduce_t *reduce = (_reduce_t *)lco;
  return sizeof(*reduce);
}

/// Deletes a reduction.
static void _reduce_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  if (r->value) {
    free(r->value);
  }
  lco_fini(lco);
}

static hpx_status_t _reduce_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;

  // if the reduce is still waiting
  if (r->remaining) {
    status = cvar_attach(&r->barrier, p);
    goto unlock;
  }

  // if the reduce has an error, then return that error without sending the
  // parcel
  //
  // NB: should we actually send some sort of error condition?
  status = cvar_get_error(&r->barrier);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

/// Handle an error condition.
static void _reduce_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  scheduler_signal_error(&r->barrier, code);
  lco_unlock(lco);
}

static void _reduce_reset(lco_t *lco) {
  _reduce_t *r = (_reduce_t *)lco;
  lco_lock(lco);
  _reset(r);
  lco_unlock(lco);
}

/// Update the reduction.
static int _reduce_set(lco_t *lco, int size, const void *from) {
  int set = 0;
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;

  // perform the op()
  assert(size && from);
  handler_t f = actions[r->op].handler;
  hpx_monoid_op_t op = (hpx_monoid_op_t)f;
  op(r->value, from, size);

  if (--r->remaining == 0) {
    scheduler_signal_all(&r->barrier);
    set = 1;
  }
  else {
    log_lco("reduce: received input %d\n", r->remaining);
    dbg_assert_str(r->remaining > 0,
                   "reduction: too many threads joined (%d).\n", r->remaining);
  }

  lco_unlock(lco);
  return set;
}

/// Get the value of the reduction.
static hpx_status_t _reduce_get(lco_t *lco, int size, void *out, int reset) {
  _reduce_t *r = (_reduce_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  int remaining = r->remaining;
  while (remaining > 0 && status == HPX_SUCCESS) {
    status = scheduler_wait(&lco->lock, &r->barrier);
    remaining = r->remaining;
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    lco_unlock(lco);
    return status;
  }

  // copy out the value if the caller wants it
  if (size && out) {
    memcpy(out, r->value, size);
  }
  else {
    dbg_assert(!size && !out);
  }

  if (reset) {
    _reset(r);
  }

  lco_unlock(lco);
  return status;
}

static hpx_status_t _reduce_wait(lco_t *lco, int reset) {
  return _reduce_get(lco, 0, NULL, reset);
}

static hpx_status_t _reduce_getref(lco_t *lco, int size, void **out, int *unpin)
{
  dbg_assert(size && out);

  hpx_status_t status = _reduce_wait(lco, 0);
  if (status != HPX_SUCCESS) {
    return status;
  }

  // no need for a lock here, synchronization happened in _wait(), and the LCO
  // is pinned externally
  _reduce_t *r = (_reduce_t *)lco;
  *out = r->value;
  *unpin = 0;
  return HPX_SUCCESS;
}

// Release a reference to the buffer. There is no such thing as a "release
// remote reference", the caller knows that if the LCO is not local then it has
// a temporary buffer---that code path doesn't make it here. Just return '1' to
// indicate that the caller should unpin the LCO.
static int _reduce_release(lco_t *lco, void *out) {
  dbg_assert(lco && out && out == ((_reduce_t *)lco)->value);
  return 1;
}

// vtable
static const lco_class_t _reduce_vtable = {
  .on_fini     = _reduce_fini,
  .on_error    = _reduce_error,
  .on_set      = _reduce_set,
  .on_attach   = _reduce_attach,
  .on_get      = _reduce_get,
  .on_getref   = _reduce_getref,
  .on_release  = _reduce_release,
  .on_wait     = _reduce_wait,
  .on_reset    = _reduce_reset,
  .on_size     = _reduce_size
};

static int _reduce_init_handler(_reduce_t *r, int inputs, size_t size,
                                hpx_action_t id, hpx_action_t op) {
  assert(id);
  assert(op);

  lco_init(&r->lco, &_reduce_vtable);
  cvar_reset(&r->barrier);
  r->op = op;
  r->id = id;
  r->size = size;
  r->inputs = inputs;
  r->remaining = inputs;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
  }

  handler_t f = actions[r->id].handler;
  hpx_monoid_id_t lid = (hpx_monoid_id_t)f;
  lid(r->value, size);

  log_lco("initialized with %d inputs lco %p\n", r->inputs, (void*)r);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _reduce_init_async,
                     _reduce_init_handler,
                     HPX_POINTER, HPX_INT, HPX_SIZE_T, HPX_ACTION_T,
                     HPX_ACTION_T);
/// @}

hpx_addr_t hpx_lco_reduce_new(int inputs, size_t size, hpx_action_t id,
                              hpx_action_t op) {
  _reduce_t *r = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*r), 0);

  if (!hpx_gas_try_pin(gva, (void**)&r)) {
    int e = hpx_call_sync(gva, _reduce_init_async, NULL, 0, &inputs, &size, &id,
                          &op);
    dbg_check(e, "could not initialize an allreduce at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, r);
    _reduce_init_handler(r, inputs, size, id, op);
    hpx_gas_unpin(gva);
  }

  return gva;
}

/// Initialize a block of array of lco.
static int _block_local_init_handler(void *lco, int n, int inputs, size_t size,
                                     hpx_action_t id, hpx_action_t op) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_reduce_t) + size));
    _reduce_init_handler(addr, inputs, size, id, op);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler,
                     HPX_POINTER, HPX_INT, HPX_INT, HPX_SIZE_T,
                     HPX_POINTER, HPX_POINTER);

/// Allocate an array of reduce LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param     inputs The static number of inputs to the reduction.
/// @param       size The size of the data being reduced.
/// @param         id An initialization function for the data, this is
///                   used to initialize the data in every epoch.
/// @param         op The commutative-associative operation we're
///                   performing.
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_reduce_local_array_new(int n, int inputs, size_t size,
                                          hpx_action_t id,
                                          hpx_action_t op) {
  uint32_t lco_bytes = sizeof(_reduce_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &inputs, &size, &id, &op);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}
