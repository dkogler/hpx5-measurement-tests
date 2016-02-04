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

/// AlltoAll is an extention of allgather to the case where each process sends
/// distinct data to each of the receivers. The jth block sent from process i
/// is received by process j and is placed in the ith block of recvbuf.
/// (Complete exchange).
///
///           sendbuff
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  A1  #  A2  #  A3  #  A4  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  B0  #  B1  #  B2  #  B3  #  B4  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  C0  #  C1  #  C2  #  C3  #  C4  #       BEFORE
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  D0  #  D1  #  D2  #  D3  #  D4  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  E0  #  E1  #  E2  #  E3  #  E4  #
///           #      #      #      #      #      #
///           ####################################
///
///             <---------- recvbuff ---------->
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  A1  #  B1  #  C1  #  D1  #  E1  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  A2  #  B2  #  C2  #  D2  #  E2  #       AFTER
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  A3  #  B3  #  C3  #  D3  #  E3  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  A4  #  B4  #  C4  #  D4  #  E4  #
///           #      #      #      #      #      #
///           ####################################

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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

/// Local alltoall interface.
/// @{
typedef struct {
  lco_t           lco;
  cvar_t         wait;
  size_t participants;
  size_t        count;
  volatile int  phase;
  void         *value;
} _alltoall_t;

static const int GATHERING = 0;
static const int READING = 1;

typedef struct {
  int offset;
  char buffer[];
} _alltoall_set_offset_t;

typedef struct {
  int size;
  int offset;
} _alltoall_get_offset_t;

static HPX_ACTION_DECL(_alltoall_setid_proxy);
static HPX_ACTION_DECL(_alltoall_getid_proxy);

static size_t _alltoall_size(lco_t *lco) {
  _alltoall_t *alltoall = (_alltoall_t *)lco;
  return sizeof(*alltoall);
}

/// Deletes a gathering.
static void _alltoall_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  _alltoall_t *g = (_alltoall_t *)lco;
  if (g->value) {
    free(g->value);
  }
  lco_fini(lco);
}

/// Handle an error condition.
static void _alltoall_error(lco_t *lco, hpx_status_t code) {
  _alltoall_t *g = (_alltoall_t *)lco;
  lco_lock(&g->lco);
  scheduler_signal_error(&g->wait, code);
  lco_unlock(&g->lco);
}

static void _alltoall_reset(lco_t *lco) {
  _alltoall_t *g = (_alltoall_t *)lco;
  lco_lock(&g->lco);
  dbg_assert_str(cvar_empty(&g->wait),
                 "Reset on alltoall LCO that has waiting threads.\n");
  cvar_reset(&g->wait);
  lco_unlock(&g->lco);
}

static hpx_status_t _alltoall_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _alltoall_t *g = (_alltoall_t *)lco;

  // We have to wait for gathering to complete before sending the parcel.
  if (g->phase != GATHERING) {
    status = cvar_attach(&g->wait, p);
    goto unlock;
  }

  // If the alltoall has an error, then return that error without sending the
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
static hpx_status_t _alltoall_getid(_alltoall_t *g, unsigned offset, int size,
                                    void *out) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're reading, and watch for errors
  while ((g->phase != READING) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->wait);
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // We're in the reading phase, if the user wants data copy it out
  if (size && out) {
    memcpy(out, (char *)g->value + (offset * size), size);
  }

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to gathering---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier READING epochs
  if (++g->count == g->participants) {
    g->phase = GATHERING;
    scheduler_signal_all(&g->wait);
  }
  else {
    while ((g->phase == READING) && (status == HPX_SUCCESS)) {
      status = scheduler_wait(&g->lco.lock, &g->wait);
    }
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

/// Get the ID for alltoall. This is global getid for the user to use.
/// Since the LCO is local, we use the local get functionality
///
/// @param   alltoall    Global address of the alltoall LCO
/// @param   id          The ID of our rank
/// @param   size        The size of the data being gathered
/// @param   value       Address of the value buffer
hpx_status_t hpx_lco_alltoall_getid(hpx_addr_t alltoall, unsigned id, int size,
                                    void *value) {
  hpx_status_t status = HPX_SUCCESS;
  _alltoall_t *local;

  if (!hpx_gas_try_pin(alltoall, (void**)&local)) {
    _alltoall_get_offset_t args = {.size = size, .offset = id};
    hpx_action_t act = _alltoall_getid_proxy;
    return hpx_call_sync(alltoall, act, value, size, &args, sizeof(args));
  }

  status = _alltoall_getid(local, id, size, value);
  hpx_gas_unpin(alltoall);
  return status;
}

static int _alltoall_getid_proxy_handler(_alltoall_get_offset_t *args, size_t n) {
  // try and pin the alltoall LCO, if we fail, we need to resend the underlying
  // parcel to "catch up" to the moving LCO
  hpx_addr_t target = hpx_thread_current_target();
  _alltoall_t *g;
  if (!hpx_gas_try_pin(target, (void **)&g)) {
     return HPX_RESEND;
  }

  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local getid routine
  char buffer[args->size];
  hpx_status_t status = _alltoall_getid(g, args->offset, args->size, buffer);
  hpx_gas_unpin(target);

  // if success, finish the current thread's execution, sending buffer value to
  // the thread's continuation address else finish the current thread's execution.
  if (status == HPX_SUCCESS) {
    return hpx_thread_continue(buffer, args->size);
  }

  return status;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _alltoall_getid_proxy,
                     _alltoall_getid_proxy_handler, HPX_POINTER, HPX_SIZE_T);


// Wait for the gathering, loses the value of the gathering for this round.
static hpx_status_t _alltoall_wait(lco_t *lco, int reset) {
  _alltoall_t *g = (_alltoall_t *)lco;
  return _alltoall_getid(g, 0, 0, NULL);
}

// Local set id function.
static hpx_status_t _alltoall_setid(_alltoall_t *g, unsigned offset, int size,
                                    const void* buffer) {
  int nDoms;
  int elementSize;
  int columnOffset;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're gathering
  while ((g->phase != GATHERING) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&g->lco.lock, &g->wait);
  }

  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  nDoms = g->participants;
  // copy in our chunk of the data
  assert(size && buffer);
  elementSize = size / nDoms;
  columnOffset = offset * elementSize;

  for (int i = 0; i < nDoms; i++) {
    int rowOffset = i * size;
    int tempOffset = rowOffset + columnOffset;
    int sourceOffset = i * elementSize;
    memcpy((char*)g->value + tempOffset, (char *)buffer + sourceOffset, elementSize);
  }

  // if we're the last one to arrive, switch the phase and signal readers
  if (--g->count == 0) {
    g->phase = READING;
    scheduler_signal_all(&g->wait);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

/// Set the ID for alltoall. This is global setid for the user to use.
///
/// @param   alltoall   Global address of the alltoall LCO
/// @param   id         ID to be set
/// @param   size       The size of the data being gathered
/// @param   value      Address of the value to be set
/// @param   lsync      An LCO to signal on local completion HPX_NULL if we
///                     don't care. Local completion indicates that the
///                     @p value may be freed or reused.
/// @param   rsync      An LCO to signal remote completion HPX_NULL if we
///                     don't care.
/// @returns HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_alltoall_setid(hpx_addr_t alltoall, unsigned id, int size,
                                    const void *value, hpx_addr_t lsync,
                                    hpx_addr_t rsync) {
  hpx_status_t status = HPX_SUCCESS;
  _alltoall_t *local;

  if (!hpx_gas_try_pin(alltoall, (void**)&local)) {
    size_t args_size = sizeof(_alltoall_set_offset_t) + size;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, args_size);
    assert(p);
    hpx_parcel_set_target(p, alltoall);
    hpx_parcel_set_action(p, _alltoall_setid_proxy);
    hpx_parcel_set_cont_target(p, rsync);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    _alltoall_set_offset_t *args = hpx_parcel_get_data(p);
    args->offset = id;
    memcpy(&args->buffer, value, size);
    hpx_parcel_send(p, lsync);
  }
  else {
    status = _alltoall_setid(local, id, size, value);
    hpx_gas_unpin(alltoall);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return status;
}


static int _alltoall_setid_proxy_handler(_alltoall_t *g, void *args, size_t n) {
  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local setid routine
  _alltoall_set_offset_t *a = args;
  size_t size = n - sizeof(_alltoall_set_offset_t);
  return _alltoall_setid(g, a->offset, size, &a->buffer);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _alltoall_setid_proxy,
                     _alltoall_setid_proxy_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);


static int _alltoall_set(lco_t *lco, int size, const void *from) {
  dbg_assert_str(false, "can't call set on an alltoall LCO.\n");
}

static hpx_status_t _alltoall_get(lco_t *lco, int size, void *out, int release) {
  dbg_assert_str(false, "can't call get on an alltoall LCO.\n");
  return HPX_SUCCESS;
}

// We universally clone the buffer here, because the all* family of LCOs will
// reset themselves so we can't retain a pointer to their buffer.
static hpx_status_t
_alltoall_getref(lco_t *lco, int size, void **out, int *unpin) {
  *out = registered_malloc(size);
  *unpin = 1;
  return _alltoall_get(lco, size, *out, 0);
}

// We know that allreduce buffers were always copies, so we can just free them
// here.
static int _alltoall_release(lco_t *lco, void *out) {
  registered_free(out);
  return 0;
}

static const lco_class_t _alltoall_vtable = {
  .on_fini     = _alltoall_fini,
  .on_error    = _alltoall_error,
  .on_set      = _alltoall_set,
  .on_get      = _alltoall_get,
  .on_getref   = _alltoall_getref,
  .on_release  = _alltoall_release,
  .on_wait     = _alltoall_wait,
  .on_attach   = _alltoall_attach,
  .on_reset    = _alltoall_reset,
  .on_size     = _alltoall_size
};

static int _alltoall_init_handler(_alltoall_t *g, size_t participants, size_t size) {
  lco_init(&g->lco, &_alltoall_vtable);
  cvar_reset(&g->wait);
  g->participants = participants;
  g->count = participants;
  g->phase = GATHERING;
  g->value = NULL;

  if (size) {
    // Ultimately, g->value points to start of the array containing the
    // scattered data.
    g->value = malloc(size * participants);
    assert(g->value);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _alltoall_init_async,
                     _alltoall_init_handler, HPX_POINTER, HPX_SIZE_T, HPX_SIZE_T);

/// Allocate a new alltoall LCO. It scatters elements from each process in order
/// of their rank and sends the result to all the processes
///
/// The gathering is allocated in gathering-mode, i.e., it expects @p
/// participants to call the hpx_lco_alltoall_setid() operation as the first
/// phase of operation.
///
/// @param inputs The static number of participants in the gathering.
/// @param size   The size of the data being gathered.
hpx_addr_t hpx_lco_alltoall_new(size_t inputs, size_t size) {
  _alltoall_t *g = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(1, sizeof(*g), 0);

  if (!hpx_gas_try_pin(gva, (void**)&g)) {
    int e = hpx_call_sync(gva, _alltoall_init_async, NULL, 0, &inputs, &size);
    dbg_check(e, "could not initialize an allreduce at %"PRIu64"\n", gva);
  }
  else {
    LCO_LOG_NEW(gva, g);
    _alltoall_init_handler(g, inputs, size);
    hpx_gas_unpin(gva);
  }
  return gva;
}

/// Initialize a block of array of lco.
static int _block_local_init_handler(void *lco, uint32_t n, uint32_t inputs,
                                     uint32_t size) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_alltoall_t) + size));
    _alltoall_init_handler(addr, inputs, size);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                     _block_local_init_handler,
                     HPX_POINTER, HPX_UINT32, HPX_UINT32, HPX_UINT32);

/// Allocate an array of alltoall LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param     inputs Number of inputs to alltoall LCO
/// @param       size The size of the value for alltoall LCO
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_alltoall_local_array_new(int n, size_t inputs, size_t size) {
  uint32_t lco_bytes = sizeof(_alltoall_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  hpx_addr_t base = hpx_gas_alloc_local(n, lco_bytes, 0);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &inputs, &size);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}

