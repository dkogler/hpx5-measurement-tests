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

#include <stdio.h>
#include <stdlib.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <libhpx/locality.h>
#include <malloc-2.8.6.h>

mspace mspaces[AS_COUNT] = {NULL};

void
as_join(int id) {
}

void
as_leave(void) {
}

size_t
as_bytes_per_chunk(void) {
  return ceil_div_size_t(here->config->heapsize, 2);
}

void *
as_malloc(int id, size_t bytes) {
  mspace msp = mspaces[id];
  if (msp) {
    return mspace_malloc(msp, bytes);
  }
  else {
    return malloc(bytes);
  }
}

void *
as_calloc(int id, size_t nmemb, size_t bytes) {
  mspace msp = mspaces[id];
  if (msp) {
    return mspace_calloc(msp, nmemb, bytes);
  }
  else {
    return calloc(nmemb, bytes);
  }
}

void *
as_memalign(int id, size_t boundary, size_t size) {
  mspace msp = mspaces[id];
  if (msp) {
    return mspace_memalign(msp, boundary, size);
  }
  else {
    void *p = NULL;
    posix_memalign(&p, boundary, size);
    return p;
  }
}

void
as_free(int id, void *ptr)  {
  mspace msp = mspaces[id];
  if (msp) {
    mspace_free(msp, ptr);
  }
  else {
    free(ptr);
  }
}
