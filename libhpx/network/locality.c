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

/// Implement the locality actions.
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

locality_t *here = NULL;

/// The action that shuts down the HPX scheduler.
static int _locality_stop_handler(uint64_t code) {
  dbg_assert(code < UINT64_MAX);
  log_net("received shutdown (code %i)\n", (uint32_t)code);
  scheduler_stop(here->sched, (uint32_t)code);
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, locality_stop, _locality_stop_handler,
              HPX_UINT64);
