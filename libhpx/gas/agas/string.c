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
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/lco.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>
#include <libsync/locks.h>
#include "agas.h"
#include "btt.h"

static int _insert_block_handler(int n, void *args[], size_t sizes[]) {
  agas_t *agas = (agas_t*)here->gas;

  dbg_assert(args[0] && sizes[0]);
  hpx_addr_t *src  = args[1];
  uint32_t   *attr = args[2];

  size_t bsize = sizes[0];
  char *lva = malloc(bsize);
  memcpy(lva, args[0], bsize);

  if (*attr & HPX_GAS_ATTR_LCO) {
    lco_t *lco = (lco_t*)lva;
    sync_tatas_release(&lco->lock);
  }

  gva_t gva = { .addr = *src };
  btt_insert(agas->btt, gva, here->rank, lva, 1, *attr);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED, _insert_block,
                     _insert_block_handler, HPX_INT, HPX_POINTER, HPX_POINTER);

/// Invalidate the remote block mapping. This action blocks until it
/// can safely invalidate the block.
static int _agas_invalidate_mapping_handler(hpx_addr_t dst, int rank) {
  agas_t *agas = (agas_t*)here->gas;
  hpx_addr_t src = hpx_thread_current_target();
  gva_t gva = { .addr = src };
  size_t bsize = UINT64_C(1) << gva.bits.size;

  uint32_t owner;
  dbg_assert(btt_get_owner(agas->btt, gva, &owner) && (here->rank == owner));
  (void)owner;

  void *block = NULL;
  uint32_t attr;
  int e = btt_try_move(agas->btt, gva, rank, &block, &attr);
  if (e != HPX_SUCCESS) {
    log_error("failed to invalidate remote mapping.\n");
    return e;
  }

  if (attr & HPX_GAS_ATTR_LCO) {
    lco_t *lco = block;
    sync_tatas_acquire(&lco->lock);
  }

  e = hpx_call_cc(dst, _insert_block, block, bsize, &src, sizeof(src), &attr,
                  sizeof(attr));

  // otherwise only free if the block is not at its home
  if (gva.bits.home != here->rank) {
    free(block);
  }

  return e;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_invalidate_mapping,
                     _agas_invalidate_mapping_handler, HPX_ADDR, HPX_INT);

static int _agas_move_handler(hpx_addr_t src) {
  int rank = here->rank;
  hpx_addr_t dst = hpx_thread_current_target();
  return hpx_call_cc(src, _agas_invalidate_mapping, &dst, &rank);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_move, _agas_move_handler, HPX_ADDR);

void agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  agas_t *agas = gas;
  libhpx_network_t net = here->config->network;
  if (net == HPX_NETWORK_PWC || net == HPX_NETWORK_SMP) {
    log_dflt("AGAS move not supported for network %s\n",
             HPX_NETWORK_TO_STRING[net]);
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  gva_t gva = { .addr = dst };
  uint32_t owner;
  bool found = btt_get_owner(agas->btt, gva, &owner);
  if (found) {
    hpx_call(src, _agas_invalidate_mapping, sync, &dst, &owner);
    return;
  }

  hpx_call(dst, _agas_move, sync, &src);
}

int agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memput(self->network, to, from, n, lsync, rsync);
}

int agas_memput_lsync(void *gas, hpx_addr_t to, const void *from, size_t n,
                      hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memput_lsync(self->network, to, from, n, rsync);
}

int agas_memput_rsync(void *gas, hpx_addr_t to, const void *from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }

  return network_memput_rsync(self->network, to, from, n);
}

int agas_memget(void *gas, void *to, hpx_addr_t from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memget(self->network, to, from, n, lsync, rsync);
}

int agas_memget_rsync(void *gas, void *to, hpx_addr_t from, size_t n,
                      hpx_addr_t lsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memget_rsync(self->network, to, from, n, lsync);
}

int agas_memget_lsync(void *gas, void *to, hpx_addr_t from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }

  return network_memget_lsync(self->network, to, from, n);
}

int agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
                hpx_addr_t sync) {
  if (!size) {
    return HPX_SUCCESS;
  }

  void *lto;
  void *lfrom;

  if (!hpx_gas_try_pin(to, &lto)) {
    return network_memcpy(self->network, to, from, size, sync);
  }

  if (!hpx_gas_try_pin(from, &lfrom)) {
    hpx_gas_unpin(to);
    return network_memcpy(self->network, to, from, size, sync);
  }

  memcpy(lto, lfrom, size);
  hpx_gas_unpin(to);
  hpx_gas_unpin(from);
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

int agas_memcpy_sync(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size) {
  int e = HPX_SUCCESS;
  if (!size) {
    return e;
  }

  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    log_error("could not allocate an LCO.\n");
    return HPX_ENOMEM;
  }

  e = agas_memcpy(gas, to, from, size, sync);

  if (HPX_SUCCESS != hpx_lco_wait(sync)) {
    dbg_error("failed agas_memcpy_sync\n");
  }

  hpx_lco_delete(sync, HPX_NULL);
  return e;
}
