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

/// @brief The parcel layer.
///
/// Parcels are the foundation of HPX. The parcel structure serves as both the
/// actual, "on-the-wire," network data structure, as well as the
/// "thread-control-block" descriptor for the threading subsystem.
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include <libhpx/parcel_block.h>
#include <libhpx/scheduler.h>
#include <libhpx/topology.h>

// this will only be used during instrumentation
__thread uint64_t parcel_count = 0;

static int _delete_launch_through_parcel_handler(hpx_parcel_t *p) {
  hpx_addr_t lsync = hpx_thread_current_target();
  hpx_lco_wait(lsync);
  parcel_delete(p);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _delete_launch_through_parcel,
                     _delete_launch_through_parcel_handler, HPX_POINTER);

/// Serialize and bless a parcel before sending or copying it.
void parcel_prepare(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  if (!parcel_serialized(state) && p->size) {
    void *buffer = hpx_parcel_get_data(p);
    memcpy(&p->buffer, buffer, p->size);
    state |= PARCEL_SERIALIZED;
    parcel_set_state(p, state);
  }

  if (p->pid && !p->credit) {
    hpx_parcel_t *parent = scheduler_current_parcel();
    dbg_assert(parent->pid == p->pid);
    p->credit = ++parent->credit;
  }
}

void parcel_set_state(hpx_parcel_t *p, parcel_state_t state) {
  sync_store(&p->state, state, SYNC_RELEASE);
}

parcel_state_t parcel_get_state(const hpx_parcel_t *p) {
  return sync_load(&p->state, SYNC_ACQUIRE);
}

parcel_state_t parcel_exchange_state(hpx_parcel_t *p, parcel_state_t state) {
  return sync_swap(&p->state, state, SYNC_ACQ_REL);
}

void parcel_pin(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  dbg_assert_str(parcel_serialized(state), "cannot pin out-of-place parcels\n");
  dbg_assert_str(!parcel_nested(state), "cannot pin nested parcels\n");
  dbg_assert_str(!parcel_retained(state), "cannot pin retained parcels\n");
  parcel_set_state(p, state | PARCEL_PINNED);
}

void parcel_nest(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  dbg_assert_str(parcel_serialized(state), "cannot nest out-of-place parcels\n");
  dbg_assert_str(!parcel_pinned(state), "cannot nest pinned parcels\n");
  dbg_assert_str(!parcel_retained(state), "cannot nest retained parcels\n");
  parcel_set_state(p, state | PARCEL_NESTED);
}

void parcel_retain(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  dbg_assert_str(parcel_serialized(state), "cannot retain out-of-place parcels\n");
  dbg_assert_str(!parcel_pinned(state), "cannot retain pinned parcels\n");
  dbg_assert_str(!parcel_nested(state), "cannot retain nested parcels\n");
  parcel_set_state(p, state | PARCEL_RETAINED);
}

void parcel_release(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  dbg_assert_str(parcel_retained(state), "can only release retained parcels\n");
  parcel_set_state(p, state & ~PARCEL_RETAINED);
}

void parcel_launch(hpx_parcel_t *p) {
  dbg_assert(p->action);

  parcel_prepare(p);

  log_parcel("PID:%"PRIu64" CREDIT:%"PRIu64" %s(%p,%u)@(%"PRIu64") => %s@(%"PRIu64")\n",
             p->pid,
             p->credit,
             actions[p->action].key,
             hpx_parcel_get_data(p),
             p->size,
             p->target,
             actions[p->c_action].key,
             p->c_target);

  EVENT_PARCEL_SEND(p);

  // do a local send through loopback, bypassing the network, otherwise dump the
  // parcel out to the network
  if (hpx_gas_try_pin(p->target, NULL)) {
    EVENT_PARCEL_RECV(p); // instrument local "receives"
    scheduler_spawn(p);
  }
  else {
    int e = network_send(here->network, p);
    dbg_check(e, "failed to perform a network send\n");
  }
}

void parcel_launch_error(hpx_parcel_t *p, int error) {
  if (error != HPX_SUCCESS) {
    dbg_error("Launching en error is not yet implemented");
  }
  parcel_launch(p);
}

void parcel_launch_through(hpx_parcel_t *p, hpx_addr_t gate) {
  dbg_assert(gate);
  parcel_prepare(p);
  hpx_pid_t pid = self->current->pid;
  p = parcel_new(gate, lco_attach, 0, 0, pid, p, parcel_size(p));
  parcel_launch(p);
}

void parcel_init(hpx_addr_t target, hpx_action_t action, hpx_addr_t c_target,
                 hpx_action_t c_action, hpx_pid_t pid, const void *data,
                 size_t len, hpx_parcel_t *p)
{
  p->ustack   = NULL;
  p->next     = NULL;
  p->src      = here->rank;
  p->size     = len;
  p->offset   = 0;
  p->action   = action;
  p->c_action = c_action;
  p->target   = target;
  p->c_target = c_target;
  p->pid      = pid;
  p->credit   = 0;

#ifdef ENABLE_INSTRUMENTATION
  if (inst_trace_class(HPX_INST_CLASS_PARCEL)) {
    parcel_count++;
    int rank = hpx_get_my_rank();
    int thread = hpx_get_my_thread_id();
    p->id = topo_offset_to_value(rank, thread, parcel_count);
  }
#endif

  // If there's a user-defined buffer, then remember it---we'll serialize it
  // later, during the send operation.
  if (data && len) {
    // use memcpy to preserve strict aliasing
    memcpy(&p->buffer, &data, sizeof(data));
    parcel_set_state(p, 0);
  }
  else {
    parcel_set_state(p, PARCEL_SERIALIZED);
  }
}

hpx_parcel_t *parcel_alloc(size_t payload) {
  size_t size = sizeof(hpx_parcel_t);
  if (payload != 0) {
    size += max_size_t(sizeof(void*), payload);
    size += _BYTES(8, size);
  }

  hpx_parcel_t *p = as_memalign(AS_REGISTERED, HPX_CACHELINE_SIZE, size);
  dbg_assert_str(p, "parcel: failed to allocate %zu registered bytes.\n", size);
  return p;
}

hpx_parcel_t *parcel_new(hpx_addr_t target, hpx_action_t action,
                         hpx_addr_t c_target, hpx_action_t c_action,
                         hpx_pid_t pid, const void *data, size_t len) {
  hpx_parcel_t *p = parcel_alloc(len);
  parcel_init(target, action, c_target, c_action, pid, data, len, p);
  EVENT_PARCEL_CREATE(p, self->current);
  return p;
}

hpx_parcel_t *parcel_clone(const hpx_parcel_t *p) {
  dbg_assert(parcel_serialized(parcel_get_state(p)) || p->size == 0);
  size_t n = parcel_size(p);
  hpx_parcel_t *clone = as_memalign(AS_REGISTERED, HPX_CACHELINE_SIZE, n);
  memcpy(clone, p, n);
  clone->ustack = NULL;
  clone->next = NULL;
  parcel_set_state(clone, PARCEL_SERIALIZED);
  return clone;
}

void parcel_delete(hpx_parcel_t *p) {
  if (!p) {
    return;
  }

  parcel_state_t state = parcel_get_state(p);

  if (unlikely(parcel_retained(state))) {
    return;
  }

  if (unlikely(parcel_nested(state))) {
    size_t n = parcel_size(p);
    hpx_parcel_t *parent = (void*)((char*)p - sizeof(*p));
    size_t m = parent->size;
    if (n != m) {
      dbg_error("expected payload of %zu for parent, saw %zu\n", n, m);
    }
    parcel_delete(parent);
    return;
  }

  if (unlikely(parcel_pinned(state))) {
    state &= ~PARCEL_PINNED;
    state = parcel_exchange_state(p, state);
  }

  if (unlikely(parcel_pinned(state))) {
    return;
  }

  if (parcel_block_allocated(state)) {
    dbg_assert(parcel_serialized(state));
    parcel_block_delete_parcel(p);
    return;
  }

  as_free(AS_REGISTERED, p);
}

struct ustack* parcel_swap_stack(hpx_parcel_t *p, struct ustack *next) {
  assert((uintptr_t)next % sizeof(void*) == 0);
  // This can detect races in the scheduler when two threads try and process the
  // same parcel.
#ifdef ENABLE_DEBUG
    return sync_swap(&p->ustack, next, SYNC_ACQ_REL);
#else
    struct ustack *old = p->ustack;
    p->ustack = next;
    return old;
#endif
}
