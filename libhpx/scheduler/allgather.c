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

/// Given a set of elements distributed across all processes, Allgather will
/// gather all of the elements to all the processes. It gathers and broadcasts
/// to all.
///
///           sendbuff
///           ########
///           #      #
///         0 #  A0  #
///           #      #
///           ########
///      T    #      #
///         1 #  B0  #
///      a    #      #
///           ########
///      s    #      #
///         2 #  C0  #                                   BEFORE
///      k    #      #
///           ########
///      s    #      #
///         3 #  D0  #
///           #      #
///           ########
///           #      #
///         4 #  E0  #
///           #      #
///           ########
///
///             <---------- recvbuff ---------->
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  A0  #  B0  #  C0  #  D0  #  E0  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  A0  #  B0  #  C0  #  D0  #  E0  #       AFTER
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################

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

/// Local allgather interface.
/// @{
typedef struct {
  lco_t           lco;
  cvar_t         wait;
  size_t participants;
  size_t        count;
  volatile int  phase;
  void         *value;
} _allgather_t;

static const int GATHERING = 0;
static const int READING = 1;

typedef struct {
  int offset;
  char buffer[];
} _allgather_set_offset_t;

static size_t _allgather_size(lco_t *lco) {
  _allgather_t *allgather = (_allgather_t *)lco;
  return sizeof(*allgather);
}

/// Deletes a gathering.
static void _allgather_fini(lco_t *lco) {
  if (!lco)
    return;

  lco_lock(lco);
  _allgather_t *g = (_allgather_t *)lco;
  if (g->value) {
    free(g->value);
  }
  lco_fini(lco);
}

/// Handle an error condition.
static void _allgather_error(lco_t *lco, hpx_status_t code) {
  _allgather_t *g = (_allgather_t *)lco;
  lco_lock(&g->lco);
  scheduler_signal_error(&g->wait, code);
  lco_unlock(&g->lco);
}

static void _allgather_reset(lco_t *lco) {
  _allgather_t *g = (_allgather_t *)lco;
  lco_lock(&g->lco);
  dbg_assert_str(cvar_empty(&g->wait),
                 "Reset on allgather LCO that has waiting threads.\n");
  cvar_reset(&g->wait);
  lco_unlock(&g->lco);
}

static hpx_status_t _allgather_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _allgather_t *g = (_allgather_t *)lco;

  // Pick attach to mean "set" for allgather. We have to wait for gathering to
  // complete before sending the parcel.
  if (g->phase != GATHERING) {
    status = cvar_attach(&g->wait, p);
    goto unlock;
  }

  // If the allgather has an error, then return that error without sending the
  // parcel.
  status = cvar_get_error(&g->wait);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // Go ahead and send this parcel eagerly.
  hpx_parcel_send(p, HPX_NULL);

  unlock:
  lco_unlock(lco);
  return status;
}

/// Get the value of the gathering, will wait if the phase is gathering.
static hpx_status_t _allgather_get(lco_t *lco, int size, void *out, int reset) {
  _allgather_t *g = (_allgather_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  // wait until we're reading, and watch for errors
  while ((g->phase != READING) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->wait);
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // we're in a reading phase, if the user wants the data, copy it out
  if (size && out) {
    memcpy(out, g->value, size);
  }

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to gathering---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier READING epochs
  if (++g->count == g->participants) {
    g->phase = GATHERING;
    scheduler_signal_all(&g->wait);
    goto unlock;
  }

  while ((g->phase == READING) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->wait);
  }

 unlock:
  lco_unlock(lco);
  return status;
}

// Wait for the gathering, loses the value of the gathering for this round.
static hpx_status_t _allgather_wait(lco_t *lco, int reset) {
  return _allgather_get(lco, 0, NULL, reset);
}

// We universally clone the buffer here, because the all* family of LCOs will
// reset themselves so we can't retain a pointer to their buffer.
static hpx_status_t
_allgather_getref(lco_t *lco, int size, void **out, int *unpin) {
  *out = registered_malloc(size);
  *unpin = 1;
  return _allgather_get(lco, size, *out, 0);
}

// We know that allreduce buffers were always copies, so we can just free them
// here.
static int
_allgather_release(lco_t *lco, void *out) {
  registered_free(out);
  return 0;
}

// Local set id function.
static hpx_status_t _allgather_setid(_allgather_t *g, unsigned offset, int size,
                                     const void* buffer) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're gathering
  while ((g->phase != GATHERING) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->wait);
  }

  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // copy in our chunk of the data
  assert(size && buffer);
  memcpy((char*)g->value + (offset * size), buffer, size);

  // if we're the last one to arrive, switch the phase and signal readers
  if (--g->count == 0) {
    g->phase = READING;
    scheduler_signal_all(&g->wait);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

static HPX_ACTION_DECL(_allgather_setid_proxy);

/// Set the ID for allgather. This is global setid for the user to use.
///
/// @param   allgather  Global address of the altogether LCO
/// @param   id         ID to be set
/// @param   size       The size of the data being gathered
/// @param   value      Address of the value to be set
/// @param   lsync      An LCO to signal on local completion HPX_NULL if we
///                     don't care. Local completion indicates that the
///                     @p value may be freed or reused.
/// @param   rsync      An LCO to signal remote completion HPX_NULL if we
///                     don't care.
/// @returns HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_allgather_setid(hpx_addr_t allgather, unsigned id,
                                     int size, const void *value,
                                     hpx_addr_t lsync, hpx_addr_t rsync)
{
  hpx_status_t status = HPX_SUCCESS;
  _allgather_t *local;

  if (!hpx_gas_try_pin(allgather, (void**)&local)) {
    size_t args_size = sizeof(_allgather_set_offset_t) + size;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, args_size);
    assert(p);
    hpx_parcel_set_target(p, allgather);
    hpx_parcel_set_action(p, _allgather_setid_proxy);
    hpx_parcel_set_cont_target(p, rsync);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    _allgather_set_offset_t *args = hpx_parcel_get_data(p);
    args->offset = id;
    memcpy(&args->buffer, value, size);
    hpx_parcel_send(p, lsync);
  }
  else {
    status = _allgather_setid(local, id, size, value);
    hpx_gas_unpin(allgather);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return status;
}


static int _allgather_setid_proxy_handler(_allgather_t *g, void *args, size_t n) {
  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local setid routine
  _allgather_set_offset_t *a = args;
  size_t size = n - sizeof(_allgather_set_offset_t);
  return _allgather_setid(g, a->offset, size, &a->buffer);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _allgather_setid_proxy,
                     _allgather_setid_proxy_handler, HPX_POINTER,
                     HPX_POINTER, HPX_SIZE_T);

/// Update the gathering, will wait if the phase is reading.
static int _allgather_set(lco_t *lco, int size, const void *from) {
  // can't call set on an allgather
  hpx_abort();
  return 0;
}

static const lco_class_t _allgather_vtable = {
  .on_fini     = _allgather_fini,
  .on_error    = _allgather_error,
  .on_set      = _allgather_set,
  .on_attach   = _allgather_attach,
  .on_get      = _allgather_get,
  .on_getref   = _allgather_getref,
  .on_release  = _allgather_release,
  .on_wait     = _allgather_wait,
  .on_reset    = _allgather_reset,
  .on_size     = _allgather_size
};

static int _allgather_init_handler(_allgather_t *g, size_t participants, size_t size) {
  lco_init(&g->lco, &_allgather_vtable);
  cvar_reset(&g->wait);
  g->participants = participants;
  g->count = participants;
  g->phase = GATHERING;
  g->value = NULL;

  if (size) {
    // Ultimately, g->value points to start of the array containing the
    // gathered data.
    g->value = malloc(size * participants);
    assert(g->value);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _allgather_init_async,
                     _allgather_init_handler, HPX_POINTER, HPX_SIZE_T, HPX_SIZE_T);

/// Allocate a new gather LCO. It gathers elements from each process in order
/// of their rank and sends the result to all the processes
///
/// The gathering is allocated in gathering-mode, i.e., it expects @p
/// participants to call the hpx_lco_allgather_setid() operation as the first
/// phase of operation.
///
/// @param inputs The static number of participants in the gathering.
/// @param size   The size of the data being gathered.
hpx_addr_t hpx_lco_allgather_new(size_t inputs, size_t size) {
  _allgather_t *g = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*g), 0);
  dbg_assert_str(gva, "Could not malloc global memory\n");
  if (!hpx_gas_try_pin(gva, (void**)&g)) {
    int e = hpx_call_sync(gva, _allgather_init_async, NULL, 0, &inputs, &size);
    dbg_check(e, "couldn't initialize allgather at %"PRIu64"\n", gva);
  }
  else {
    _allgather_init_handler(g, inputs, size);
    hpx_gas_unpin(gva);
  }
  return gva;
}

/// Initialize a block of array of lco.
static int _block_local_init_handler(void *lco, uint32_t n, uint32_t inputs,
                                     uint32_t size) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_allgather_t) + size));
    _allgather_init_handler(addr, inputs, size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler, HPX_POINTER, HPX_UINT32,
                     HPX_UINT32, HPX_UINT32);

/// Allocate an array of allgather LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param     inputs Number of inputs to allgather LCO
/// @param       size The size of the value for allgather LCO
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_allgather_local_array_new(int n, size_t inputs, size_t size) {
  uint32_t lco_bytes = sizeof(_allgather_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &inputs, &size);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}


