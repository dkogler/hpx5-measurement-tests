// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_CONTRIB_DLMALLOC_MMAP_WRAPPER
#define LIBHPX_CONTRIB_DLMALLOC_MMAP_WRAPPER

#include <sys/mman.h>

void *dl_mmap_wrapper(size_t length);
void *dl_munmap_wrapper(void *ptr, size_t length);

#endif
