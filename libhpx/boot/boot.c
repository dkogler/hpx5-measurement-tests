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
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/locality/boot.c
/// @brief Handles boot initialization.
/// ----------------------------------------------------------------------------
#include <libhpx/boot.h>
#include <libhpx/debug.h>

static boot_t *_default(void) {
#ifdef HAVE_PMI
  return boot_new_pmi();
#endif

#ifdef HAVE_MPI
  return boot_new_mpi();
#endif

  return boot_new_smp();
}

boot_t *boot_new(libhpx_boot_t type) {
  boot_t *boot = NULL;

  switch (type) {
   case (HPX_BOOT_PMI):
#ifdef HAVE_PMI
    boot = boot_new_pmi();
    if (boot)
      log_boot("initialized PMI bootstrapper.\n");
#else
    dbg_error("PMI bootstrap not supported in current configuration.\n");
#endif
    break;

   case (HPX_BOOT_MPI):
#ifdef HAVE_MPI
    boot = boot_new_mpi();
    if (boot)
      log_boot("initialized mpirun bootstrapper.\n");
#else
    dbg_error("MPI bootstrap not supported in current configuration.\n");
#endif
    break;

   case (HPX_BOOT_SMP):
    boot = boot_new_smp();
    if (boot)
      log_boot("initialized the SMP bootstrapper.\n");
    break;

   case HPX_BOOT_DEFAULT:
   default:
    boot = _default();
    break;
  }

  if (!boot) {
    boot = _default();
  }

  if (!boot) {
    dbg_error("failed to initialize the bootstrapper.\n");
  }
  else {
    log_dflt("bootstrapped using %s.\n", boot->id());
  }

  return boot;
}

