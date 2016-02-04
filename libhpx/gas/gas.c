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

#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include "agas/agas.h"
#include "smp/smp.h"
#include "pgas/pgas.h"

static const int LEVEL = HPX_LOG_CONFIG | HPX_LOG_GAS;

gas_t *gas_new(config_t *cfg, struct boot *boot) {
#ifndef HAVE_NETWORK
  // if we didn't build a network we need to default to SMP
  cfg->gas = HPX_GAS_SMP;
#endif

  int ranks = boot_n_ranks(boot);
  gas_t *gas = NULL;
  libhpx_gas_t type = cfg->gas;

  // if we built a network, we might want to optimize for SMP
  if (ranks == 1 && cfg->opt_smp) {
    if (type != HPX_GAS_SMP && type != HPX_GAS_DEFAULT) {
      log_level(LEVEL, "GAS %s overriden to SMP.\n", HPX_GAS_TO_STRING[type]);
    }
    type = HPX_GAS_SMP;
  }

  if (ranks > 1 && type == HPX_GAS_SMP) {
    dbg_error("SMP GAS selection fails for %d ranks\n", ranks);
  }

  switch (type) {
   case HPX_GAS_SMP:
    gas = gas_smp_new();
    break;

   case HPX_GAS_AGAS:
#if defined(HAVE_AGAS) && defined(HAVE_NETWORK)
    gas = gas_agas_new(cfg, boot);
#endif
    break;

   case HPX_GAS_PGAS:
#ifdef HAVE_NETWORK
    gas = gas_pgas_new(cfg, boot);
#endif
    break;

   default:
    dbg_error("unexpected configuration value for --hpx-gas\n");
  }

  if (!gas) {
    log_error("GAS %s failed to initialize\n", HPX_GAS_TO_STRING[type]);
  }
  else {
    log_gas("GAS %s initialized\n", HPX_GAS_TO_STRING[type]);
  }

  return gas;
}
