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

/// Given a set of buffers distributed across processes, gather will
/// collect all of the elements to.
///

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Local gather interface.
/// @{
typedef struct {
  lco_t           lco;
  cvar_t         cvar;
  int         writers;
  int         readers;
  volatile int wcount;
  volatile int rcount;
  void         *value;
} _gather_t;

static size_t _gather_size(lco_t *lco) {
  _gather_t *gather = (_gather_t *)lco;
  return sizeof(*gather);
}

/// Deletes a gather LCO.
static void _gather_fini(lco_t *lco) {
  if (!lco)
    return;

  lco_lock(lco);
  _gather_t *g = (_gather_t *)lco;
  if (g->value) {
    free(g->value);
  }
  lco_fini(lco);
}

/// Handle an error condition.
static void _gather_error(lco_t *lco, hpx_status_t code) {
  _gather_t *g = (_gather_t*)lco;
  lco_lock(&g->lco);
  scheduler_signal_error(&g->cvar, code);
  lco_unlock(&g->lco);
}

static void _gather_reset(lco_t *lco) {
  _gather_t *g = (_gather_t*)lco;
  lco_lock(&g->lco);
  dbg_assert_str(cvar_empty(&g->cvar),
                 "Reset on gather LCO that has waiting threads.\n");
  cvar_reset(&g->cvar);
  lco_unlock(&g->lco);
}

static hpx_status_t _gather_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _gather_t *g = (_gather_t*)lco;

  // Pick attach to mean "set" for gather. We have to wait for gathering to
  // complete before sending the parcel.
  if (g->wcount == g->writers) {
    status = cvar_attach(&g->cvar, p);
    goto unlock;
  }

  // If the gather has an error, then return that error without sending the
  // parcel.
  status = cvar_get_error(&g->cvar);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // Go ahead and send this parcel eagerly.
  hpx_parcel_send(p, HPX_NULL);

  unlock:
  lco_unlock(lco);
  return status;
}

/// Get the value of the gather LCO. This operation will wait if the
/// writers have not finished gathering.
static hpx_status_t _gather_get(lco_t *lco, int size, void *out, int reset) {
  _gather_t *g = (_gather_t*)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  // wait until we're reading, and watch for errors
  while ((g->wcount < g->writers) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->cvar);
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // we're in a reading phase, and if the user wants the data, copy it out
  if (size && out) {
    memcpy(out, g->value, size);
  }

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to gathering---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier READING epochs
  if (--g->rcount == 0) {
    g->wcount = 0;
    scheduler_signal_all(&g->cvar);
    goto unlock;
  }

 unlock:
  lco_unlock(lco);
  return status;
}

// Wait for the gather LCO. This operation loses the value of the gathering for this round.
static hpx_status_t _gather_wait(lco_t *lco, int reset) {
  return _gather_get(lco, 0, NULL, reset);
}

// We clone the buffer here because the gather LCO will reset itself
// so we can't retain a pointer to its buffer.
static hpx_status_t
_gather_getref(lco_t *lco, int size, void **out, int *unpin) {
  *out = registered_malloc(size);
  *unpin = 1;
  return _gather_get(lco, size, *out, 0);
}

// We know that gather buffer was a copy, so we can just free it here.
static int
_gather_release(lco_t *lco, void *out) {
  registered_free(out);
  return 0;
}

// Local set id function.
static hpx_status_t _gather_setid(_gather_t *g, unsigned offset, int size,
                                  const void* buffer) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're gathering
  while ((g->wcount == g->writers) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->cvar);
  }

  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // copy in our chunk of the data
  assert(size && buffer);
  memcpy((char*)g->value + (offset * size), buffer, size);

  // if we're the last one to arrive, switch the phase and signal readers
  if (++g->wcount == g->writers) {
    g->rcount = g->readers;
    scheduler_signal_all(&g->cvar);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

typedef struct {
  int offset;
  char buffer[];
} _gather_set_offset_t;

static HPX_ACTION_DECL(_gather_setid_proxy);
static int _gather_setid_proxy_handler(_gather_t *g, void *args, size_t n) {
  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local setid routine
  _gather_set_offset_t *a = args;
  size_t size = n - sizeof(_gather_set_offset_t);
  return _gather_setid(g, a->offset, size, &a->buffer);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _gather_setid_proxy,
                     _gather_setid_proxy_handler, HPX_POINTER,
                     HPX_POINTER, HPX_SIZE_T);

/// Set the ID for gather. This is global setid for the user to use.
///
/// @param   gather  Global address of the altogether LCO
/// @param   id         ID to be set
/// @param   size       The size of the data being gathered
/// @param   value      Address of the value to be set
/// @param   lsync      An LCO to signal on local completion HPX_NULL if we
///                     don't care. Local completion indicates that the
///                     @p value may be freed or reused.
/// @param   rsync      An LCO to signal remote completion HPX_NULL if we
///                     don't care.
/// @returns HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_gather_setid(hpx_addr_t gather, unsigned id,
                                     int size, const void *value,
                                     hpx_addr_t lsync, hpx_addr_t rsync) {
  hpx_status_t status = HPX_SUCCESS;
  _gather_t *local;

  if (!hpx_gas_try_pin(gather, (void**)&local)) {
    size_t args_size = sizeof(_gather_set_offset_t) + size;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, args_size);
    assert(p);
    hpx_parcel_set_target(p, gather);
    hpx_parcel_set_action(p, _gather_setid_proxy);
    hpx_parcel_set_cont_target(p, rsync);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    _gather_set_offset_t *args = hpx_parcel_get_data(p);
    args->offset = id;
    memcpy(&args->buffer, value, size);
    hpx_parcel_send(p, lsync);
  }
  else {
    status = _gather_setid(local, id, size, value);
    hpx_gas_unpin(gather);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return status;
}

/// Update the gathering, will wait if the phase is reading.
static int _gather_set(lco_t *lco, int size, const void *from) {
  // can't call set on an gather
  hpx_abort();
  return 0;
}

static const lco_class_t _gather_vtable = {
  .type        = LCO_GATHER,
  .on_fini     = _gather_fini,
  .on_error    = _gather_error,
  .on_set      = _gather_set,
  .on_attach   = _gather_attach,
  .on_get      = _gather_get,
  .on_getref   = _gather_getref,
  .on_release  = _gather_release,
  .on_wait     = _gather_wait,
  .on_reset    = _gather_reset,
  .on_size     = _gather_size
};

static void HPX_CONSTRUCTOR _register_vtable(void) {
  lco_vtables[LCO_GATHER] = &_gather_vtable;
}

static int _gather_init_handler(_gather_t *g, int writers, int readers,
                                size_t size) {
  lco_init(&g->lco, &_gather_vtable);
  cvar_reset(&g->cvar);
  g->writers = writers;
  g->readers = readers;
  g->wcount = 0;
  g->rcount = readers;
  g->value = NULL;

  if (size) {
    // Ultimately, g->value points to start of the array containing the
    // gathered data.
    g->value = malloc(size * writers);
    assert(g->value);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _gather_init_async,
                     _gather_init_handler, HPX_POINTER, HPX_INT, HPX_INT, HPX_SIZE_T);

/// Allocate a new gather LCO.
///
/// The gathering is allocated in gathering-mode, i.e., it expects @p
/// participants to call the hpx_lco_gather_setid() operation as the first
/// phase of operation.
///
/// @param  inputs The static number of writers in the gathering.
/// @param outputs The static number of readers in the gathering.
/// @param    size The size of the data being gathered.
hpx_addr_t hpx_lco_gather_new(size_t inputs, size_t outputs, size_t size) {
  _gather_t *g = NULL;
  hpx_addr_t gva = lco_alloc_local(1, sizeof(*g), 0);
  dbg_assert_str(gva, "Could not malloc global memory\n");
  if (!hpx_gas_try_pin(gva, (void**)&g)) {
    int e = hpx_call_sync(gva, _gather_init_async, NULL, 0, &inputs, &outputs, &size);
    dbg_check(e, "couldn't initialize gather at %"PRIu64"\n", gva);
  }
  else {
    _gather_init_handler(g, inputs, outputs, size);
    hpx_gas_unpin(gva);
  }
  return gva;
}

/// Initialize a block of array of lco.
static int _block_init_handler(void *lco, uint32_t n, uint32_t inputs,
                               uint32_t outputs, uint32_t size) {
  for (int i = 0; i < n; i++) {
    void *addr = (void*)((uintptr_t)lco + i * (sizeof(_gather_t) + size));
    _gather_init_handler(addr, inputs, outputs, size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_init,
                     _block_init_handler, HPX_POINTER, HPX_UINT32,
                     HPX_UINT32, HPX_UINT32, HPX_UINT32);

/// Allocate an array of gather LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param     inputs Number of inputs to gather LCO
/// @param    outputs Number of outputs to gather LCO
/// @param       size The size of the value for gather LCO
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_gather_local_array_new(int n, size_t inputs, size_t outputs,
                                          size_t size) {
  uint32_t lco_bytes = sizeof(_gather_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = lco_alloc_local(n, lco_bytes, 0);

  int e = hpx_call_sync(base, _block_init, NULL, 0, &n, &inputs, &outputs,
                        &size);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}
