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

#ifndef LIBHPX_NETWORK_COMPRESSED_H
#define LIBHPX_NETWORK_COMPRESSED_H

#include <hpx/attributes.h>
#include <libhpx/network.h>

/// Forward declarations.
/// @{
struct config;
/// @}


network_t* compressed_network_new (network_t *network)
  HPX_MALLOC;

#endif // LIBHPX_NETWORK_COMPRESSED_H
