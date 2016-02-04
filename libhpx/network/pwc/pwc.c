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
#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "parcel_emulation.h"
#include "pwc.h"
#include "send_buffer.h"
#include "xport.h"

pwc_network_t *pwc_network = NULL;

typedef struct heap_segment {
  size_t        n;
  char      *base;
  xport_key_t key;
} heap_segment_t;

static void _probe_local(pwc_network_t *pwc, int id) {
  int rank = here->rank;

  // Each time through the loop, we deal with local completions.
  command_t command;
  int src;
  while (pwc->xport->test(&command, NULL, XPORT_ANY_SOURCE, &src)) {
    command_run(rank, command);
  }
}

static hpx_parcel_t *_probe(pwc_network_t *pwc, int rank) {
  command_t command;
  int src;
  while (pwc->xport->probe(&command, NULL, rank, &src)) {
    command_run(src, command);
  }
  return NULL;
}

static int _pwc_progress(void *network, int id) {
  pwc_network_t *pwc = network;
  if (sync_swap(&pwc->progress_lock, 0, SYNC_ACQUIRE)) {
    _probe_local(pwc, id);
    sync_store(&pwc->progress_lock, 1, SYNC_RELEASE);
  }
  return 0;
}

static hpx_parcel_t *_pwc_probe(void *network, int rank) {
  pwc_network_t *pwc = network;
  if (sync_swap(&pwc->probe_lock, 0, SYNC_ACQUIRE)) {
    _probe(pwc, XPORT_ANY_SOURCE);
    sync_store(&pwc->probe_lock, 1, SYNC_RELEASE);
  }
  return NULL;
}

/// Create a network registration.
static void
_pwc_register_dma(void *network, const void *base, size_t n, void *key) {
  pwc_network_t *pwc = network;
  dbg_assert(pwc && pwc->xport && pwc->xport->pin);
  pwc->xport->pin(base, n, key);
}

/// Release a network registration.
static void _pwc_release_dma(void *network, const void* base, size_t n) {
  pwc_network_t *pwc = network;
  dbg_assert(pwc && pwc->xport && pwc->xport->unpin);
  pwc->xport->unpin(base, n);
}

static int _pwc_send(void *network, hpx_parcel_t *p) {
  if (parcel_size(p) >= here->config->pwc_parceleagerlimit) {
    return pwc_rendezvous_send(network, p);
  }

  pwc_network_t *pwc = network;
  int rank = gas_owner_of(here->gas, p->target);
  send_buffer_t *buffer = &pwc->send_buffers[rank];
  return send_buffer_send(buffer, HPX_NULL, p);
}

static void _pwc_flush(void *pwc) {
}

static void _pwc_delete(void *network) {
  dbg_assert(network);
  pwc_network_t *pwc = network;

  // Cleanup any remaining local work---this can leak memory and stuff, because
  // we aren't actually running the commands that we cleanup.
  int remaining, src;
  command_t command;
  do {
    pwc->xport->test(&command, &remaining, XPORT_ANY_SOURCE, &src);
  } while (remaining > 0);

  // Network deletion is effectively a collective, so this enforces that
  // everyone is done with rdma before we go and deregister anything.
  boot_barrier(here->boot);

  // Finalize send buffers.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_fini(&pwc->send_buffers[i]);
  }

  heap_segment_t *heap = &pwc->heap_segments[here->rank];
  _pwc_release_dma(pwc, heap->base, heap->n);
  free(pwc->heap_segments);
  free(pwc->send_buffers);

  parcel_emulator_delete(pwc->parcels);
  pwc->xport->dealloc(pwc->xport);
  free(pwc);
}

static const class_string_t _pwc_string_vtable = {
  .memget       = pwc_memget,
  .memget_lsync = pwc_memget_lsync,
  .memget_rsync = pwc_memget_rsync,
  .memput       = pwc_memput,
  .memput_lsync = pwc_memput_lsync,
  .memput_rsync = pwc_memput_rsync,
  .memcpy       = NULL,
  .memcpy_sync  = NULL
};

network_t *
network_pwc_funneled_new(const config_t *cfg, boot_t *boot, gas_t *gas) {
  // Validate parameters.
  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate PWC for the SMP boot network\n");
    return NULL;
  }

  if (cfg->gas == HPX_GAS_AGAS) {
    dbg_error("PWC is incompatible with AGAS, please run with --hpx-gas=pgas "
              "or with --hpx-network=isir\n");
  }

  // Validate configuration.
  if (cfg->pwc_parceleagerlimit > cfg->pwc_parcelbuffersize) {
    dbg_error(" --hpx-pwc-parceleagerlimit (%zu) must be less than "
              "--hpx-pwc-parcelbuffersize (%zu)\n",
              cfg->pwc_parceleagerlimit, cfg->pwc_parcelbuffersize);
  }

  // Allocate the network object and initialize its virtual function table.
  pwc_network_t *pwc;
  int e = posix_memalign((void*)&pwc, HPX_CACHELINE_SIZE, sizeof(*pwc));
  dbg_check(e, "failed to allocate the pwc network structure\n");
  dbg_assert(pwc);

  pwc->vtable.type = HPX_NETWORK_PWC;
  pwc->vtable.string = &_pwc_string_vtable;
  pwc->vtable.delete = _pwc_delete;
  pwc->vtable.progress = _pwc_progress;
  pwc->vtable.send = _pwc_send;
  pwc->vtable.probe = _pwc_probe;
  pwc->vtable.flush = _pwc_flush;
  pwc->vtable.register_dma = _pwc_register_dma;
  pwc->vtable.release_dma = _pwc_release_dma;
  pwc->vtable.lco_get = pwc_lco_get;
  pwc->vtable.lco_wait = pwc_lco_wait;

  // Initialize locks.
  sync_store(&pwc->probe_lock, 1, SYNC_RELEASE);
  sync_store(&pwc->progress_lock, 1, SYNC_RELEASE);

  // Initialize transports.
  pwc->cfg = cfg;
  pwc->xport = pwc_xport_new(cfg, boot, gas);
  pwc->parcels = parcel_emulator_new_reload(cfg, boot, pwc->xport);
  pwc->send_buffers = calloc(here->ranks, sizeof(send_buffer_t));
  pwc->heap_segments = calloc(here->ranks, sizeof(heap_segment_t));

  // Register the gas heap segment.
  heap_segment_t heap = {
    .n = gas_local_size(here->gas),
    .base = gas_local_base(here->gas)
  };
  _pwc_register_dma(pwc, heap.base, heap.n, &heap.key);

  // Exchange all the heap keys, and make sure it went okay
  boot_allgather(boot, &heap, pwc->heap_segments, sizeof(heap));

  heap_segment_t *segment = &pwc->heap_segments[here->rank];
  dbg_assert(heap.n == segment->n);
  dbg_assert(heap.base == segment->base);
  dbg_assert(!strncmp(heap.key, segment->key, XPORT_KEY_SIZE));

  // Initialize the send buffers.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_t *send = &pwc->send_buffers[i];
    int rc = send_buffer_init(send, i, pwc->parcels, pwc->xport, 8);
    dbg_check(rc, "failed to initialize send buffer %d of %u\n", i, e);
  }

  return &pwc->vtable;

  // avoid unused variable warnings
  (void)segment;
}

int pwc_get(void *obj, void *lva, hpx_addr_t from, size_t n,
            command_t lcmd, command_t rcmd) {
  pwc_network_t *pwc = obj;
  int rank = gpa_to_rank(from);

  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = lva,
    .dest_key = pwc->xport->key_find_ref(pwc->xport, lva, n),
    .src = pwc->heap_segments[rank].base + gpa_to_offset(from),
    .src_key = &pwc->heap_segments[rank].key,
    .lop = lcmd,
    .rop = rcmd
  };

  return pwc->xport->gwc(&op);
}

int pwc_put(void *obj, hpx_addr_t to, const void *lva, size_t n,
            command_t lcmd, command_t rcmd) {
  pwc_network_t *pwc = obj;
  int rank = gpa_to_rank(to);

  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = pwc->heap_segments[rank].base + gpa_to_offset(to),
    .dest_key = &pwc->heap_segments[rank].key,
    .src = lva,
    .src_key = pwc->xport->key_find_ref(pwc->xport, lva, n),
    .lop = lcmd,
    .rop = rcmd
  };

  return pwc->xport->pwc(&op);
}

int pwc_cmd(void *obj, int rank, command_t lcmd, command_t rcmd) {
  pwc_network_t *pwc = obj;
  return pwc->xport->cmd(rank, lcmd, rcmd);
}
