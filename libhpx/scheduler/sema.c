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

/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.
#include <assert.h>
#include <inttypes.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Local sema interface.
/// @{
typedef struct {
  lco_t       lco;
  cvar_t    avail;
  volatile uintptr_t count;
  uintptr_t init;
} _sema_t;

static void _sema_fini(lco_t *lco);
static void _sema_error(lco_t *lco, hpx_status_t code);
static void _sema_reset(lco_t *lco);
static int _sema_set(lco_t *lco, int size, const void *from);
static hpx_status_t _sema_wait(lco_t *lco, int reset);
static hpx_status_t _sema_get(lco_t *lco, int size, void *out, int reset);

static void _reset(_sema_t *sema, int reset) {
  if (reset) {
    dbg_assert_str(cvar_empty(&sema->avail),
                   "Reset on a sema that has waiting threads.\n");
    cvar_reset(&sema->avail);
    sync_store(&sema->count, sema->init, SYNC_RELEASE);
  }
}

static size_t _sema_size(lco_t *lco) {
  _sema_t *sema = (_sema_t *)lco;
  return sizeof(*sema);
}

// the semaphore vtable
static const lco_class_t _sema_vtable = {
  .on_fini     = _sema_fini,
  .on_error    = _sema_error,
  .on_set      = _sema_set,
  .on_get      = _sema_get,
  .on_getref   = NULL,
  .on_release  = NULL,
  .on_wait     = _sema_wait,
  .on_attach   = NULL,
  .on_reset    = _sema_reset,
  .on_size     = _sema_size
};

static int _sema_init_handler(_sema_t *sema, unsigned count) {
  lco_init(&sema->lco, &_sema_vtable);
  cvar_reset(&sema->avail);
  sema->count = count;
  sema->init = count;
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _sema_init_async,
                     _sema_init_handler, HPX_POINTER, HPX_UINT);

/// Allocate a semaphore LCO.
hpx_addr_t hpx_lco_sema_new(unsigned count) {
  _sema_t *sema = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*sema), 0);

  if (!hpx_gas_try_pin(gva, (void**)&sema)) {
    int e = hpx_call_sync(gva, _sema_init_async, NULL, 0, &count);
    dbg_check(e, "could not initialize a future at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, sema);
    _sema_init_handler(sema, count);
    hpx_gas_unpin(gva);
  }

  return gva;
}

/// Decrement a semaphore.
///
/// Just forward to the equivalent lco_wait() operation.
hpx_status_t hpx_lco_sema_p(hpx_addr_t sema) {
  return hpx_lco_wait(sema);
}

/// Increment a semaphore.
///
/// If the semaphore is local, then we can use the _sema_set operation directly,
/// otherwise we perform the operation as an asynchronous remote call using the
/// _sema_v action.
void hpx_lco_sema_v(hpx_addr_t sema, hpx_addr_t rsync) {
  hpx_lco_set(sema, 0, NULL, HPX_NULL, rsync);
}

/// Increment a semaphore synchronously.
///
/// Just forwards on.
void hpx_lco_sema_v_sync(hpx_addr_t sema) {
  hpx_lco_set_rsync(sema, 0, NULL);
}

void _sema_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
}

void _sema_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _sema_t *sema = (_sema_t *)lco;
  scheduler_signal_error(&sema->avail, code);
  lco_unlock(lco);
}

void _sema_reset(lco_t *lco) {
  _sema_t *sema = (_sema_t *)lco;
  lco_lock(&sema->lco);
  _reset(sema, 1);
  lco_unlock(&sema->lco);
}

/// Set is equivalent to returning a resource to the semaphore.
int _sema_set(lco_t *lco, int size, const void *from) {
  lco_lock(lco);
  _sema_t *sema = (_sema_t *)lco;
  if (sema->count++ == 0) {
    // only signal one sleeping thread since we're only returning one resource,
    // waking everyone up is inefficient
    scheduler_signal(&sema->avail);
  }

  lco_unlock(lco);
  return 1;
}

hpx_status_t _sema_wait(lco_t *lco, int reset) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _sema_t *sema = (_sema_t *)lco;

  // wait until the count is non-zero, use while here and re-read count because
  // our condition variables have MESA semantics
  unsigned count = sema->count;
  while (count == 0 && status == HPX_SUCCESS) {
    status = scheduler_wait(&sema->lco.lock, &sema->avail);
    count = sema->count;
  }

  // reduce the count, unless there was an error
  if (status == HPX_SUCCESS) {
    sema->count = count - 1;
    _reset(sema, reset);
  }

  lco_unlock(lco);
  return status;
}

hpx_status_t _sema_get(lco_t *lco, int size, void *out, int reset) {
  assert(size == 0);
  return _sema_wait(lco, reset);
}

/// @}
