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
# include "config.h"
#endif

/// @file libhpx/scheduler/and.c
/// Defines the AND LCO.

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/gpa.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// And LCO class interface.
/// @{
typedef struct {
  lco_t      lco;
  cvar_t barrier;
  intptr_t count;
  intptr_t value;                               // the threshold
} _and_t;

static void _reset(_and_t *and) {
  dbg_assert_str(cvar_empty(&and->barrier), "Reset on AND LCO that has waiting threads.\n");
  log_lco("%p resetting lco %p\n", (void*)self->current, (void*)and);
  sync_store(&and->count, and->value, SYNC_RELEASE);
  cvar_reset(&and->barrier);
  lco_reset_triggered(&and->lco);
  if (!and->value) {
    lco_set_triggered(&and->lco);
  }
}

static hpx_status_t _wait(_and_t *and, int reset) {
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS) {
    return status;
  }

  // wait for the lco if its not triggered
  if (!lco_get_triggered(&and->lco)) {
    log_lco("%p waiting for lco %p (reset=%d)\n", (void*)self->current, (void*)and, reset);
    status = scheduler_wait(&and->lco.lock, &and->barrier);
    log_lco("%p resuming in lco %p (reset=%d)\n", (void*)self->current, (void*)and, reset);
  }

  if (reset && status == HPX_SUCCESS) {
    _reset(and);
  }

  return status;
}

static hpx_status_t _attach(_and_t *and, hpx_parcel_t *p) {
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS) {
    return status;
  }

  if (lco_get_triggered(&and->lco)) {
    return hpx_parcel_send(p, HPX_NULL);
  }

  return cvar_attach(&and->barrier, p);
}

static void _and_fini(lco_t *lco) {
  if (lco) {
    lco_lock(lco);
    lco_fini(lco);
  }
}

static void _and_error(lco_t *lco, hpx_status_t code) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  scheduler_signal_error(&and->barrier, code);
  lco_unlock(&and->lco);
}

static void _and_reset(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  _reset(and);
  lco_unlock(&and->lco);
}

/// Fast set decrements the count, and sets triggered and signals when it gets
/// to 0.
static int _and_set(lco_t *lco, int size, const void *from) {
  dbg_assert(lco);
  dbg_assert(!size || from);

  _and_t *and = (_and_t *)lco;

  int num = (from) ? *(const int*)from : 1;
  intptr_t count = sync_fadd(&and->count, (0 - num), SYNC_ACQ_REL);
  log_lco("%p reduced count to %" PRIdPTR " lco %p\n", (void*)self->current, count - num, (void*)lco);

  if (count > num) {
    return 0;
  }

  if (count == num) {
    lco_lock(lco);
    log_lco("%p triggering lco %p\n", (void*)self->current, (void*)lco);
    dbg_assert(!lco_get_triggered(lco));
    lco_set_triggered(lco);
    scheduler_signal_all(&and->barrier);
    lco_unlock(lco);
    return 1;
  }

  dbg_assert_str(count > num,
                 "too many threads joined (%"PRIdPTR").\n", count - num);
  return 0;
}

static size_t _and_size(lco_t *lco) {
  return sizeof(_and_t);
}

static hpx_status_t _and_wait(lco_t *lco, int reset) {
  lco_lock(lco);
  hpx_status_t status = _wait((void*)lco, reset);
  lco_unlock(lco);
  return status;
}

static hpx_status_t _and_attach(lco_t *lco, hpx_parcel_t *p) {
  lco_lock(lco);
  hpx_status_t status = _attach((void*)lco, p);
  lco_unlock(lco);
  return status;
}

static hpx_status_t _and_get(lco_t *lco, int size, void *out, int reset) {
  lco_lock(lco);
  hpx_status_t status = _wait((void*)lco, reset);
  lco_unlock(lco);
  return status;
}

// getref is a bit nonsensical for and, but we need it to support LCO superclass
static hpx_status_t _and_getref(lco_t *lco, int size, void **out, int *unpin) {
  dbg_assert(size && out);

  hpx_status_t status = _and_wait(lco, 0);
  if (status != HPX_SUCCESS) {
    return status;
  }

  _and_t *and = (void*)lco;
  *out = &and->count;
  *unpin = 0;
  return HPX_SUCCESS;
}

// Release a reference to the buffer. There is no such thing as a "release
// remote reference", the caller knows that if the LCO is not local then it has
// a temporary buffer---that code path doesn't make it here. Just return '1' to
// indicate that the caller should unpin the LCO.
static int _and_release(lco_t *lco, void *out) {
  dbg_assert(lco && out && out == &((_and_t *)lco)->count);
  return 1;
}

static const lco_class_t _and_vtable = {
  .on_fini     = _and_fini,
  .on_error    = _and_error,
  .on_set      = _and_set,
  .on_get      = _and_get,
  .on_getref   = _and_getref,
  .on_release  = _and_release,
  .on_wait     = _and_wait,
  .on_attach   = _and_attach,
  .on_reset    = _and_reset,
  .on_size     = _and_size
};

static int _and_init_handler(_and_t *and, int64_t count) {
  dbg_assert(count >= 0);
  lco_init(&and->lco, &_and_vtable);
  cvar_reset(&and->barrier);
  sync_store(&and->count, count, SYNC_RELEASE);
  and->value = count;
  log_lco("initialized with %" PRId64 " inputs lco %p\n", (int64_t)and->count,
          (void*)and);

  if (!count) {
    lco_set_triggered(&and->lco);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _and_init, _and_init_handler,
                     HPX_POINTER, HPX_SINT64);

/// @}

/// Allocate an and LCO. This is synchronous.
hpx_addr_t hpx_lco_and_new(int64_t limit) {
  _and_t *and = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*and), 0);

  if (!hpx_gas_try_pin(gva, (void**)&and)) {
    int e = hpx_call_sync(gva, _and_init, NULL, 0, &limit);
    dbg_check(e, "could not initialize an and gate at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, and);
    _and_init_handler(and, limit);
    hpx_gas_unpin(gva);
  }
  return gva;
}

/// Join the and.
void hpx_lco_and_set(hpx_addr_t and, hpx_addr_t rsync) {
  hpx_lco_set(and, 0, NULL, HPX_NULL, rsync);
}

/// Set an and "num" times.
void hpx_lco_and_set_num(hpx_addr_t and, int sum, hpx_addr_t rsync) {
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_lco_set(and, sizeof(sum), &sum, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
}

static int _block_local_init_handler(_and_t *ands, uint32_t n, int64_t limit) {
  for (int i = 0; i < n; ++i) {
    _and_init_handler(&ands[i], limit);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler,
                     HPX_POINTER, HPX_UINT32, HPX_SINT64);

hpx_addr_t hpx_lco_and_local_array_new(int n, int arg) {
  uint32_t lco_bytes = sizeof(_and_t);
  dbg_assert(lco_bytes < (UINT64_C(1) << GPA_MAX_LG_BSIZE) / n);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &arg);
  dbg_check(e, "call of _block_init_action failed\n");
  return base;
}

