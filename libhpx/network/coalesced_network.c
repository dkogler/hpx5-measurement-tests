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

#include <stdlib.h>
#include <string.h>
#include <libsync/queues.h>
#include <libsync/sync.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

typedef struct {
  network_t          vtable;
  network_t           *next;
  two_lock_queue_t    sends;
  int                 count;
  int        previous_count;
  const int coalescing_size;
  volatile int    syncflush;
} _coalesced_network_t;

static inline void _atomic_inc(volatile int *addr) {
  sync_fadd(addr, 1, SYNC_ACQ_REL);
}

static inline void _atomic_dec(volatile int *addr) {
  sync_fadd(addr, -1, SYNC_ACQ_REL);
}

static void _coalesced_network_delete(void *obj) {
  _coalesced_network_t *network = obj;
  network_delete(network->next);
  free(obj);
}

/// Demultiplex coalesced parcels on the receiver side.
///
/// @param       buffer The buffer of coalesced parcels.
/// @param            n The number of coalesced bytes.
static int _demux_handler(char* buffer, int n) {
  // retrieve the next parcel from the fat parcel
  while (n) {
    hpx_parcel_t *p = parcel_clone((void*)buffer);
    size_t bytes = parcel_size(p);
    buffer += bytes;
    n -= bytes;
    dbg_assert(n >= 0);
    parcel_launch(p);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demux, _demux_handler,
                     HPX_POINTER, HPX_INT);

/// Send parcels from the coalesced network.
static void _send_n(_coalesced_network_t *network, int n) {
  // 0) Allocate temporary storage.
  struct {
    hpx_parcel_t *fatp;
    char         *next;
    int        n_bytes;
  } *locs = calloc(HPX_LOCALITIES, sizeof(*locs));

  // 1) We'll pull n parcels off the global send queue, and store them
  //    temporarily in a local stack, so that we can accumulate the number of
  //    bytes we need to send to each rank.
  gas_t          *gas = here->gas;
  hpx_parcel_t *chain = NULL;
  hpx_parcel_t     *p = NULL;
  while (n--) {
    p = sync_two_lock_queue_dequeue(&network->sends);
    size_t bytes = parcel_size(p);
    uint32_t   l = gas_owner_of(gas, p->target);
    locs[l].n_bytes += bytes;
    parcel_stack_push(&chain, p);
  }

  // 2) Allocate a parcel for each rank that we have bytes going to, and grab a
  //    pointer to the beginning of its buffer.
  for (int l = 0, e = HPX_LOCALITIES; l < e; ++l) {
    int n = locs[l].n_bytes;
    if (n) {
      locs[l].fatp = action_new_parcel(_demux, HPX_THERE(l), 0, 0, 2, NULL, n);
      locs[l].next = hpx_parcel_get_data(locs[l].fatp);
    }
  }

  // 3) Copy the chained parcels to the appropriate buffers.
  while ((p = parcel_stack_pop(&chain))) {
    size_t bytes = parcel_size(p);
    uint32_t   l = gas_owner_of(gas, p->target);
    memcpy(locs[l].next, p, bytes);
    locs[l].next += bytes;
    parcel_delete(p);
  }

  // 4) Send the fat parcel to each target.
  for (int l = 0, e = HPX_LOCALITIES; l < e; ++l) {
    if (locs[l].fatp) {
      network_send(network->next, locs[l].fatp);
    }
  }

  // 5) Clean up the temporary array.
  free(locs);
}

static int _coalesced_network_send(void *obj, hpx_parcel_t *p) {
  _coalesced_network_t *network = obj;
  if (!action_is_coalesced(p->action)) {
    return network_send(network->next, p);
  }

  // Coalesce on demand as long as we have enough parcels available.
  int count = sync_load(&network->count, SYNC_RELAXED);
  while (count >= network->coalescing_size) {
    // Notify flush operations that we might be coalescing. This prevents a race
    // where a flusher thinks everything is gone, but we have partially
    // coalesced buffers to send.
    _atomic_inc(&network->syncflush);

    // The cas updates the count for the next loop iteration if it fails,
    // otherwise we manually update it.
    int n = count - network->coalescing_size;
    if (sync_cas(&network->count, &count, n, SYNC_RELAXED, SYNC_RELAXED)) {
      _send_n(network, network->coalescing_size);
      count = n;
    }

    // Notify flush operations that we're not in their way anymore.
    _atomic_dec(&network->syncflush);
  }

  // Prepare the parcel now, 1) to serialize it while its data is probably in
  // our cache and 2) to make sure it gets a pid from the right parent. Put the
  // parcel in the coalesced send queue.
  parcel_prepare(p);
  sync_two_lock_queue_enqueue(&network->sends, p);
  sync_fadd(&network->count, 1, SYNC_RELAXED);
  return LIBHPX_OK;
}

static int _coalesced_network_progress(void *obj, int id) {
  _coalesced_network_t *network = obj;

  // If the number of buffered parcels is the same as the previous time we
  // progressed, we'll do an eager send operation to reduce latency and make
  // sure we're not inducing deadlock.
  int current = sync_load(&network->count, SYNC_RELAXED);
  int previous = sync_load(&network->previous_count, SYNC_RELAXED);
  while (current && (current == previous)) {
    // Notify the flush operation that I might be coalescing---this prevents a
    // race during flush where I have taken some parcels out of the queue but
    // not submitted them to the underlying network yet.
    _atomic_inc(&network->syncflush);

    // Try and take all of the current parcels in the coalescing queue.
    if (sync_cas(&network->count, &current, 0, SYNC_RELAXED, SYNC_RELAXED)) {
      _send_n(network, current);
      current = 0;
    }

    // Notify any flush operations that we're no longer dangerous.
    _atomic_dec(&network->syncflush);

    // Our "previous" is what we just saw (or left behind).
    previous = current;
  }

  // Always post the last value that we saw.
  sync_store(&network->previous_count, current, SYNC_RELAXED);

  // Call the underlying network progress.
  return network_progress(network->next, id);
}

static hpx_parcel_t* _coalesced_network_probe(void *obj, int rank) {
  _coalesced_network_t *coalesced_network = obj;
  return network_probe(coalesced_network->next, rank);
}

static void _coalesced_network_flush(void *obj) {
  _coalesced_network_t *network = obj;

  // coalesce the rest of the buffered sends
  int count = sync_swap(&network->count, 0, SYNC_RELAXED);
  if (count > 0) {
    _send_n(network, count);
  }

  // wait for any concurrent flush operations to complete
  while (sync_load(&network->syncflush, SYNC_ACQUIRE)) {
    /* spin */;
  }

  // and flush the underlying network
  network->next->flush(network->next);
}

static void _coalesced_network_register_dma(void *obj, const void *base,
                                            size_t bytes, void *key) {
  _coalesced_network_t *network = obj;
  network_register_dma(network->next, base, bytes, key);
}

static void _coalesced_network_release_dma(void *obj, const void *base,
                                           size_t bytes) {
  _coalesced_network_t *network = obj;
  network_release_dma(network->next, base, bytes);
}

static int _coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n,
                                      void *out, int reset) {
  _coalesced_network_t *network = obj;
  return network_lco_get(network->next, lco, n, out, reset);
}

static int _coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _coalesced_network_t *network = obj;
  return network_lco_wait(network->next, lco, reset);
}

network_t* coalesced_network_new(network_t *next,  const struct config *cfg) {
  _coalesced_network_t *network = NULL;
  if (posix_memalign((void*)&network, HPX_CACHELINE_SIZE, sizeof(*network))) {
    log_error("could not allocate a coalesced network\n");
    return NULL;
  }

  // set the vtable
  network->vtable.string       = next->string;
  network->vtable.delete       = _coalesced_network_delete;
  network->vtable.progress     = _coalesced_network_progress;
  network->vtable.send         = _coalesced_network_send;
  network->vtable.probe        = _coalesced_network_probe;
  network->vtable.flush        = _coalesced_network_flush;
  network->vtable.register_dma = _coalesced_network_register_dma;
  network->vtable.release_dma  = _coalesced_network_release_dma;
  network->vtable.lco_get      = _coalesced_network_lco_get;
  network->vtable.lco_wait     = _coalesced_network_lco_wait;

  // set the next network
  network->next = next;

  // initialize the local coalescing queue for the parcels
  sync_two_lock_queue_init(&network->sends, NULL);

  // set coalescing size (this is const after the allocation)
  *(int*)&network->coalescing_size = cfg->coalescing_buffersize;

  network->count = 0;
  network->previous_count = 0;
  network->syncflush = 0;
  log_net("Created coalescing network\n");
  return &network->vtable;
}
