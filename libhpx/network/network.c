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

/// @file libhpx/network/network.c
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/network.h>
#include "isir/isir.h"
#include "pwc/pwc.h"
#include "coalesced_network.h"
#include "inst.h"
#include "smp.h"

static const int LEVEL = HPX_LOG_CONFIG | HPX_LOG_NET | HPX_LOG_DEFAULT;

network_t *network_new(config_t *cfg, boot_t *boot, struct gas *gas) {
#ifndef HAVE_NETWORK
  // if we didn't build a network we need to default to SMP
  cfg->network = HPX_NETWORK_SMP;
#endif

  libhpx_network_t type = cfg->network;
  int ranks = boot_n_ranks(boot);
  network_t *network = NULL;

  // default to HPX_NETWORK_SMP for SMP execution
  if (ranks == 1 && cfg->opt_smp) {
    if (type != HPX_NETWORK_SMP && type != HPX_NETWORK_DEFAULT) {
      log_level(LEVEL, "%s overriden to SMP.\n", HPX_NETWORK_TO_STRING[type]);
    }
    type = HPX_NETWORK_SMP;
  }

  if (ranks > 1) {
#ifndef HAVE_NETWORK
    dbg_error("Launched on %d ranks but no network available\n", ranks);
#endif
  }

  if (ranks > 1 && type == HPX_NETWORK_SMP) {
    dbg_error("SMP network selection fails for %d ranks\n", ranks);
  }

  if (type == HPX_NETWORK_PWC) {
#ifndef HAVE_PHOTON
    dbg_error("PWC network selection fails (photon disabled in config)\n");
#endif
  }

  // handle default
  if (type == HPX_NETWORK_DEFAULT) {
#ifdef HAVE_PHOTON
    type = HPX_NETWORK_PWC;
#else
    type = HPX_NETWORK_ISIR;
#endif
  }

  switch (type) {
   case HPX_NETWORK_PWC:
#ifdef HAVE_NETWORK
    network = network_pwc_funneled_new(cfg, boot, gas);
    pwc_network = (pwc_network_t*) network;
#else
    log_level(LEVEL, "PWC network unavailable (no network configured)\n");
#endif
    break;

   case HPX_NETWORK_ISIR:
#ifdef HAVE_NETWORK
    network = network_isir_funneled_new(cfg, boot, gas);
#else
    log_level(LEVEL, "ISIR network unavailable (no network configured)\n");
#endif
    break;

   case HPX_NETWORK_SMP:
    network = network_smp_new(cfg, boot);
    break;

   default:
    log_level(LEVEL, "unknown network type\n");
    break;
  }

  if (!network) {
    dbg_error("%s did not initialize\n", HPX_NETWORK_TO_STRING[cfg->network]);
  }
  else {
    log_level(LEVEL, "%s network initialized\n", HPX_NETWORK_TO_STRING[type]);
  }

  if (cfg->coalescing_buffersize) {
    network =  coalesced_network_new(network, cfg);
  }

  if (!config_inst_at_isset(here->config, here->rank)) {
    return network;
  }

  if (!inst_trace_class(HPX_INST_SCHEDTIMES)) {
    return network;
  }

  return network_inst_new(network);
}
