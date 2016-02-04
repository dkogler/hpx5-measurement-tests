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

#include <libhpx/debug.h>
#include "xport.h"

isir_xport_t *isir_xport_new(const config_t *cfg, struct gas *gas) {
  switch (cfg->transport) {
   case (HPX_TRANSPORT_PHOTON):
    dbg_error("Photon support for the ISIR network is not yet available.\n");

   case (HPX_TRANSPORT_MPI):
#ifdef HAVE_MPI
    return isir_xport_new_mpi(cfg, gas);
#else
    dbg_error("MPI transport not enabled in current configuration.\n");
#endif

   default:
#ifdef HAVE_MPI
    return isir_xport_new_mpi(cfg, gas);
#else
    dbg_error("MPI transport required for ISIR network.\n");
#endif
  }
  unreachable();
}

