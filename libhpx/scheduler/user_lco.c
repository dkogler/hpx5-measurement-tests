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

/// @file libhpx/scheduler/user_lco.c
/// @brief A user-defined LCO.
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

typedef void (*_hpx_user_lco_id_t)(void *i, size_t size,
                                   void *init, size_t init_size);

/// Generic LCO interface.
/// @{
typedef struct {
  lco_t              lco;
  cvar_t            cvar;
  size_t            size;
  hpx_action_t        id;
  hpx_action_t        op;
  hpx_action_t predicate;
  void             *init;
  void           *buffer;
  char           data[0];
} _user_lco_t;

// Forward declaration---used during reset as well.
static int _user_lco_init(_user_lco_t *u, size_t size, hpx_action_t id,
                          hpx_action_t op, hpx_action_t predicate,
                          void *init, size_t init_size);

static void _reset(_user_lco_t *u) {
  cvar_clear_error(&u->cvar);
  dbg_assert_str(cvar_empty(&u->cvar),
                 "Reset on an LCO that has waiting threads.\n");
  lco_reset_triggered(&u->lco);
  size_t init_size = ((char*)u->buffer - u->data);
  _user_lco_init(u, u->size, u->id, u->op, u->predicate, u->init, init_size);
}


static bool _trigger(_user_lco_t *u) {
  if (lco_get_triggered(&u->lco)) {
    return false;
  }
  lco_set_triggered(&u->lco);
  return true;
}

static size_t _user_lco_size(lco_t *lco) {
  _user_lco_t *u = (_user_lco_t *)lco;
  size_t init_size = ((char*)u->buffer - u->data);
  return (sizeof(*u) + u->size + init_size);
}

/// Deletes a user-defined LCO.
static void _user_lco_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
}

/// Handle an error condition.
static void _user_lco_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;
  _trigger(u);
  scheduler_signal_error(&u->cvar, code);
  lco_unlock(lco);
}

static void _user_lco_reset(lco_t *lco) {
  _user_lco_t *u = (_user_lco_t *)lco;
  lco_lock(lco);
  _reset(u);
  lco_unlock(lco);
}

/// Invoke an operation on the user-defined LCO's buffer.
static int _user_lco_set(lco_t *lco, int size, const void *from) {
  int set = 0;
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;

  if (lco_get_triggered(&u->lco)) {
    dbg_error("cannot set an already set user_lco.\n");
    goto unlock;
  }

  // perform the op()
  handler_t f = actions[u->op].handler;
  hpx_monoid_op_t op = (hpx_monoid_op_t)f;
  op(u->buffer, from, size);

  f = actions[u->predicate].handler;
  hpx_predicate_t predicate = (hpx_predicate_t)f;
  if (predicate(u->buffer, u->size)) {
    lco_set_triggered(&u->lco);
    scheduler_signal_all(&u->cvar);
    set = 1;
  }

  unlock:
   lco_unlock(lco);
   return set;
}

static hpx_status_t _user_lco_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;

  if (!lco_get_triggered(lco)) {
    status = cvar_attach(&u->cvar, p);
    goto unlock;
  }

  // If there was an error, then return that error without sending the parcel
  status = cvar_get_error(&u->cvar);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

static hpx_status_t _wait(_user_lco_t *u) {
  if (!lco_get_triggered(&u->lco))
    return scheduler_wait(&u->lco.lock, &u->cvar);

  return cvar_get_error(&u->cvar);
}

/// Get the user-defined LCO's buffer.
static hpx_status_t _user_lco_get(lco_t *lco, int size, void *out, int reset) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  _user_lco_t *u = (_user_lco_t *)lco;

  status = _wait(u);
  if (status != HPX_SUCCESS) {
    lco_unlock(lco);
    return status;
  }

  // copy out the value if the caller wants it
  if (out) {
    memcpy(out, u->buffer, size);
  }

  if (reset) {
    _reset(u);
  }

  lco_unlock(lco);
  return status;
}

// Wait for the reduction.
static hpx_status_t _user_lco_wait(lco_t *lco, int reset) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;
  status = _wait(u);

  if (reset && status == HPX_SUCCESS) {
    _reset(u);
  }

  lco_unlock(lco);
  return status;
}

// Get the reference to the reduction.
static hpx_status_t _user_lco_getref(lco_t *lco, int size, void **out, int *unpin) {
  dbg_assert(size && out);

  hpx_status_t status = _user_lco_wait(lco, 0);
  if (status != HPX_SUCCESS) {
    return status;
  }

  // No need for a lock here, synchronization happened in _wait(), and the LCO
  // is pinned externally.
  _user_lco_t *u = (_user_lco_t *)lco;
  *out  = u->buffer;
  *unpin = 0;
  return HPX_SUCCESS;
}

// Release the reference to the buffer.
static int _user_lco_release(lco_t *lco, void *out) {
  dbg_assert(lco && out && out == ((_user_lco_t *)lco)->buffer);
  return 1;
}

// vtable
static const lco_class_t _user_lco_vtable = {
  .on_fini     = _user_lco_fini,
  .on_error    = _user_lco_error,
  .on_set      = _user_lco_set,
  .on_attach   = _user_lco_attach,
  .on_get      = _user_lco_get,
  .on_getref   = _user_lco_getref,
  .on_release  = _user_lco_release,
  .on_wait     = _user_lco_wait,
  .on_reset    = _user_lco_reset,
  .on_size     = _user_lco_size
};

static int
_user_lco_init(_user_lco_t *u, size_t size, hpx_action_t id,
               hpx_action_t op, hpx_action_t predicate, void *init,
               size_t init_size) {
  dbg_assert(id);
  dbg_assert(op);
  dbg_assert(predicate);

  lco_init(&u->lco, &_user_lco_vtable);
  cvar_reset(&u->cvar);
  u->size = size;
  u->id = id;
  u->op = op;
  u->predicate = predicate;
  u->init = u->data;
  u->buffer = (char*)u->data + init_size;

  handler_t f = actions[u->id].handler;
  _hpx_user_lco_id_t init_fn = (_hpx_user_lco_id_t)f;
  init_fn(u->buffer, u->size, init, init_size);
  return HPX_SUCCESS;
}
/// @}

typedef struct {
  int                  n;
  size_t            size;
  hpx_action_t        id;
  hpx_action_t        op;
  hpx_action_t predicate;
  size_t       init_size;
  char           data[0];
} _user_lco_init_args_t;

static int
_user_lco_init_handler(_user_lco_t *u, _user_lco_init_args_t *args) {
  size_t size = args->size;
  hpx_action_t id = args->id;
  hpx_action_t op = args->op;
  hpx_action_t predicate = args->predicate;
  size_t init_size = args->init_size;
  void *init = args->data;

  return _user_lco_init(u, size, id, op, predicate, init, init_size);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED,
                     _user_lco_init_action, _user_lco_init_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

hpx_addr_t hpx_lco_user_new(size_t size, hpx_action_t id, hpx_action_t op,
                            hpx_action_t predicate, void *init,
                            size_t init_size) {
  _user_lco_t *u = NULL;
  hpx_addr_t gva = hpx_gas_calloc_local(1, sizeof(*u) + size + init_size, 0);

  if (!hpx_gas_try_pin(gva, (void**)&u)) {
    size_t args_size = sizeof(_user_lco_t) + init_size;
    _user_lco_init_args_t *args = calloc(1, args_size);
    args->size = size;
    args->id = id;
    args->op = op;
    args->predicate = predicate;
    args->init_size = init_size;
    memcpy(args->data, init, init_size);

    int e = hpx_call_sync(gva, _user_lco_init_action, NULL, 0, args, args_size);
    dbg_check(e, "could not initialize an allreduce at %"PRIu64"\n", gva);
    free(args);
  } else {
    LCO_LOG_NEW(gva, u);
    memcpy(u->data, init, init_size);
    _user_lco_init(u, size, id, op, predicate, init, init_size);
    hpx_gas_unpin(gva);
  }

  return gva;
}

/// Initialize a block of array of lco.
static int
_block_local_init_handler(_user_lco_t *lco, _user_lco_init_args_t *args) {
  int n = args->n;
  int lco_bytes = sizeof(_user_lco_t) + args->size + args->init_size;
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + (i * lco_bytes));
    int e = _user_lco_init_handler(addr, args);
    dbg_check(e, "_block_local_init_handler failed\n");
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED,
                     _block_local_init, _block_local_init_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// Allocate an array of user LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param       size The size of the LCO Buffer
/// @param         id An initialization function for the data, this is
///                   used to initialize the data in every epoch.
/// @param         op The commutative-associative operation we're
///                   performing.
/// @param  predicate Predicate to guard the LCO.
/// @param       init The initialization data address.
/// @param  init_size The size of the initialization data.
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_user_local_array_new(int n, size_t size, hpx_action_t id,
                                        hpx_action_t op, hpx_action_t predicate,
                                        void *init, size_t init_size) {
  uint32_t lco_bytes = sizeof(_user_lco_t) + size + init_size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  size_t args_size = sizeof(_user_lco_t) + init_size;
  _user_lco_init_args_t *args = calloc(1, args_size);
  args->n = n;
  args->size = size;
  args->id = id;
  args->op = op;
  args->predicate = predicate;
  args->init_size = init_size;
  memcpy(args->data, init, init_size);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, args, args_size);
  dbg_check(e, "call of _block_init_action failed\n");

  free(args);
  // return the base address of the allocation
  return base;
}

/// Get the user-defined LCO's user data. This allows to access the buffer
/// portion of the user-defined LCO regardless the LCO has been set or not.
void *hpx_lco_user_get_user_data(void *lco) {
  _user_lco_t *u = lco;
  return u->buffer;
}
