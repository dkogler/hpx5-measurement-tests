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

#ifndef LIBHPX_NETWORK_PWC_REGISTERED_H
#define LIBHPX_NETWORK_PWC_REGISTERED_H

#ifdef __cplusplus
extern "C" {
#endif

/// @file  libhpx/network/pwc/registered.h

/// Forward declarations
/// @{
struct pwc_xport;
/// @}

void registered_allocator_init(struct pwc_xport *xport);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_PWC_REGISTERED_H
