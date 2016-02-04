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
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>

#include "smp.h"

static const int _zero = 0;

// Define the transports allowed for the SMP network
static void _smp_delete(void *network) {
}

static int _smp_progress(void *network, int id) {
  return 0;
}

static int _smp_send(void *network, hpx_parcel_t *p) {
  hpx_abort();
}

static hpx_parcel_t *_smp_probe(void *network, int nrx) {
  return NULL;
}

static void _smp_flush(void *network) {
}

static void _smp_register_dma(void *obj, const void *addr, size_t n, void *key)
{
}

static void _smp_release_dma(void *obj, const void *addr, size_t n) {
}

static int _smp_lco_wait(void *o, hpx_addr_t lco, int reset) {
  return (reset) ? hpx_lco_wait_reset(lco) : hpx_lco_wait(lco);
}

static int _smp_lco_get(void *o, hpx_addr_t lco, size_t n, void *to, int reset) {
  return (reset) ? hpx_lco_get_reset(lco, n, to) : hpx_lco_get(lco, n, to);
}

static network_t _smp = {
  .type = HPX_NETWORK_SMP,
  .string = NULL,
  .delete = _smp_delete,
  .progress = _smp_progress,
  .send = _smp_send,
  .probe = _smp_probe,
  .flush = _smp_flush,
  .register_dma = _smp_register_dma,
  .release_dma = _smp_release_dma,
  .lco_get = _smp_lco_get,
  .lco_wait = _smp_lco_wait
};

network_t *network_smp_new(const struct config *cfg, boot_t *boot) {
  if (boot_n_ranks(boot) > 1) {
    dbg_error("SMP network does not support multiple ranks.\n");
    return NULL;
  }

  return &_smp;
}
