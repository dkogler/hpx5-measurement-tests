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

#ifndef LIBHPX_NETWORK_ISIR_ISIR_H
#define LIBHPX_NETWORK_ISIR_ISIR_H

#include <hpx/attributes.h>
#include <libhpx/network.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
/// @}

/// Allocate a new Isend/Irecv funneled network.
network_t *network_isir_funneled_new(const struct config *cfg,
                                     struct boot *boot, struct gas *gas)
  HPX_MALLOC;

/// Perform an LCO get operation through the Isend/Irecv network.
int isir_lco_wait(void *obj, hpx_addr_t lco, int reset);

/// Perform an LCO get operation through the Isend/Irecv network.
int isir_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);

extern const class_string_t isir_string_vtable;

#endif
