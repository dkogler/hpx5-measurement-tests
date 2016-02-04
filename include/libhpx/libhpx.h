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

#ifndef LIBHPX_LIBHPX_H
#define LIBHPX_LIBHPX_H

#include <errno.h>
#include <hpx/attributes.h>
#include <libhpx/config.h>

enum {
  LIBHPX_ENOMEM = -(ENOMEM),
  LIBHPX_EINVAL = -(EINVAL),
  LIBHPX_ERROR = -2,
  LIBHPX_EUNIMPLEMENTED = -1,
  LIBHPX_OK = 0,
  LIBHPX_RETRY
};

/// Forward declare the libhpx configuration type.
/// @{
struct config;
typedef struct config libhpx_config_t;
/// @}

/// Get the current configuration.
const libhpx_config_t *libhpx_get_config(void)
  HPX_PUBLIC;

/// Print the version of libhpx to stdout.
void libhpx_print_version(void)
  HPX_PUBLIC;

#define LIBHPX_MAJOR 0
#define LIBHPX_MINOR 1
#define LIBHPX_PATCH 2

/// Get the current version.
///
/// The returned version can be inspected using the LIBHPX_VERSION indeces.
///
///   int version[3];
///   libhpx_get_version(version);
///
///   printf("major version %d\n, version[LIBHPX_MAJOR]);
///   printf("minor version %d\n, version[LIBHPX_MINOR]);
///   printf("patch version %d\n, version[LIBHPX_PATCH]);
///
/// @param[out]  version The output version.
void libhpx_get_version(int version[3])
  HPX_PUBLIC;

#endif  // LIBHPX_LIBHPX_Hx
