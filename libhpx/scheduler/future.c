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

/// @file libhpx/scheduler/future.c
/// Defines the future structure.

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "hpx/builtins.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// Local future interface.
/// @{
typedef struct {
  lco_t     lco;
  cvar_t   full;
  char  value[];
} _future_t;

static void _reset(_future_t *f) {
  dbg_assert_str(cvar_empty(&f->full),
                 "Reset on a future that has waiting threads.\n");
  log_lco("resetting future %p\n", (void*)f);
  lco_reset_triggered(&f->lco);
  cvar_reset(&f->full);
}

static size_t _future_size(lco_t *lco) {
  _future_t *future = (_future_t *)lco;
  return sizeof(*future);
}

static hpx_status_t _wait(_future_t *f) {
  lco_t *lco = &f->lco;
  if (lco_get_triggered(lco)) {
    return cvar_get_error(&f->full);
  }
  else {
    return scheduler_wait(&lco->lock, &f->full);
  }
}

static bool _trigger(_future_t *f) {
  if (lco_get_triggered(&f->lco))
    return false;
  lco_set_triggered(&f->lco);
  return true;
}

// Nothing extra allocated in the future
static void _future_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
}

/// Copies @p from into the appropriate location.
static int _future_set(lco_t *lco, int size, const void *from) {
  DEBUG_IF (size && !lco_get_user(lco)) {
    dbg_error("setting 0-sized future with %d bytes\n", size);
  }

  int set = 0;
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;
  log_lco("setting future %p\n", (void*)f);
  // futures are write-once
  if (!_trigger(f)) {
    dbg_error("cannot set an already set future\n");
    goto unlock;
  }

  if (from && size) {
    memcpy(&f->value, from, size);
  }

  scheduler_signal_all(&f->full);
  set = 1;
 unlock:
  lco_unlock(lco);
  return set;
}

static void _future_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;
  _trigger(f);
  scheduler_signal_error(&f->full, code);
  lco_unlock(lco);
}

static void _future_reset(lco_t *lco) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  _reset(f);
  lco_unlock(&f->lco);
}

static hpx_status_t _future_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;

  // if the future isn't triggered, then attach this parcel to the full
  // condition
  if (!lco_get_triggered(lco)) {
    status = cvar_attach(&f->full, p);
    goto unlock;
  }

  // if the future has an error, then return that error without sending the
  // parcel
  //
  // NB: should we actually send some sort of error condition?
  status = cvar_get_error(&f->full);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t _future_get(lco_t *lco, int size, void *out, int reset) {
  DEBUG_IF (size && !lco_get_user(lco)) {
    dbg_error("getting %d bytes from a 0-sized future\n", size);
  }

  lco_lock(lco);

  _future_t *f = (_future_t *)lco;
  hpx_status_t status = _wait(f);
  if (status != HPX_SUCCESS) {
    lco_unlock(lco);
    return status;
  }

  if (size && out) {
    memcpy(out, &f->value, size);
  }
  else {
    dbg_assert(!size && !out);
  }

  if (reset) {
    _reset(f);
  }

  lco_unlock(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _future_wait(lco_t *lco, int reset) {
  return _future_get(lco, 0, NULL, reset);
}

/// Returns the reference to the future's value in @p out, waiting if the lco
/// isn't set yet.
static hpx_status_t _future_getref(lco_t *lco, int size, void **out, int *unpin)
{
  dbg_assert(size && out);

  hpx_status_t status = _future_wait(lco, 0);
  if (status != HPX_SUCCESS) {
    return status;
  }

  // no need for a lock here, synchronization happened in _wait(), and the LCO
  // is pinned externally
  _future_t *f = (_future_t *)lco;
  *out = f->value;
  *unpin = 0;
  return HPX_SUCCESS;
}

// Release a reference to the buffer. There is no such thing as a "release
// remote reference", the caller knows that if the LCO is not local then it has
// a temporary buffer---that code path doesn't make it here. Just return '1' to
// indicate that the caller should unpin the LCO.
static int _future_release(lco_t *lco, void *out) {
  _future_t *f = (_future_t *)lco;
  dbg_assert(lco && out && out == f->value);
  return 1;
  (void)f;
}

// the future vtable
static const lco_class_t _future_vtable = {
  .on_fini     = _future_fini,
  .on_error    = _future_error,
  .on_set      = _future_set,
  .on_get      = _future_get,
  .on_getref   = _future_getref,
  .on_release  = _future_release,
  .on_wait     = _future_wait,
  .on_attach   = _future_attach,
  .on_reset    = _future_reset,
  .on_size     = _future_size
};

/// initialize the future
static int _future_init_handler(_future_t *f, int size) {
  log_lco("initializing future %p\n", (void*)f);
  lco_init(&f->lco, &_future_vtable);
  cvar_reset(&f->full);
  if (size) {
    if (DEBUG) {
      lco_set_user(&f->lco);
    }
    memset(&f->value, 0, size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _future_init_async,
                     _future_init_handler, HPX_POINTER, HPX_INT);

/// Initialize a block of futures.
static int _block_init_handler(char *base, const uint32_t size,
                               const uint32_t nfutures) {
  // sequentially initialize each future
  for (uint32_t i = 0; i < nfutures; ++i) {
    _future_init_handler((_future_t*)(base + i * size), size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_init,
                     _block_init_handler, HPX_POINTER, HPX_UINT32,
                     HPX_UINT32);

hpx_addr_t hpx_lco_future_new(int size) {
  _future_t *future = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*future) + size, 0);

  if (!hpx_gas_try_pin(gva, (void**)&future)) {
    int e = hpx_call_sync(gva, _future_init_async, NULL, 0, &size);
    dbg_check(e, "could not initialize a future at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, future);
    _future_init_handler(future, size);
    hpx_gas_unpin(gva);
  }
  return gva;
}

// Allocate a global array of futures.
hpx_addr_t hpx_lco_future_array_new(int n, int size, int futures_per_block) {
  // perform the global allocation
  uint32_t       blocks = ceil_div_32(n, futures_per_block);
  uint32_t future_bytes = sizeof(_future_t) + size;
  uint32_t  block_bytes = futures_per_block * future_bytes;
  hpx_addr_t       base = hpx_gas_alloc_cyclic(blocks, block_bytes, 0);

  // for each block, initialize the future
  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (int i = 0; i < blocks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes, block_bytes);
    int e = hpx_call(there, _block_init, and, &size, &futures_per_block);
    dbg_check(e, "call of _block_init_action failed\n");
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // return the base address of the allocation
  return base;
}


// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_addr_t hpx_lco_future_array_at(hpx_addr_t array, int i, int size, int bsize)
{
  uint32_t future_bytes = sizeof(_future_t) + size;
  uint32_t  block_bytes = bsize * future_bytes;
  return hpx_addr_add(array, i * (sizeof(_future_t) + size), block_bytes);
}

/// Initialize a block of array of lco.
static int _block_local_init_handler(void *lco, uint32_t n, uint32_t size) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_future_t) + size));
    _future_init_handler(addr, size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler, HPX_POINTER, HPX_UINT32, HPX_UINT32);

/// Allocate an array of future local to the calling locality.
/// @param          n The (total) number of futures to allocate
/// @param       size The size of each future's value
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_future_local_array_new(int n, int size) {
  uint32_t lco_bytes = sizeof(_future_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  // for each block, initialize the future.
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &size);
  dbg_check(e, "call of _block_init_action failed\n");

  return base;
}
