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

#ifndef LIBHPX_NETWORK_SMP_H
#define LIBHPX_NETWORK_SMP_H

/// Forward declarations.
/// @{
struct boot;
struct config;
struct network;
/// @}

struct network *network_smp_new(const struct config *cfg, struct boot *boot);

#endif
