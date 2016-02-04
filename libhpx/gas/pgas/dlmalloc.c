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

#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <malloc-2.8.6.h>
#include "cyclic.h"
#include "global.h"
#include "heap.h"

void
cyclic_allocator_init(void) {
  dbg_assert(global_heap);
  size_t bytes = ceil_div_size_t(global_heap->nbytes, 2);
  heap_set_csbrk(global_heap, bytes);
  mspaces[AS_CYCLIC] = create_mspace_with_base(global_heap->base, bytes, 1);
}

void
global_allocator_init(void) {
  dbg_assert(global_heap);
  size_t offset = ceil_div_size_t(global_heap->nbytes, 2);
  size_t bytes = global_heap->nbytes - offset;
  mspaces[AS_GLOBAL] = create_mspace_with_base(global_heap->base+offset, bytes, 1);
}

