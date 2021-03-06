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

#include <libhpx/parcel.h>

void exit_action(const void *obj, hpx_parcel_t *p) {
}

void exit_pinned_action(const void *obj, hpx_parcel_t *p) {
  hpx_gas_unpin(p->target);
}
