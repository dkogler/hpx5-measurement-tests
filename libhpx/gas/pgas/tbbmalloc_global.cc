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

#include <cassert>
#include <iostream>
#include <tbb/scalable_allocator.h>
#include <libhpx/memory.h>
#include "global.h"
#include "heap.h"

using namespace rml;

static void *
_global_chunk_alloc(intptr_t pool_id, size_t &bytes) {
  assert(pool_id == AS_GLOBAL);
  void *chunk = heap_chunk_alloc(global_heap, NULL, bytes,
                          global_heap->bytes_per_chunk);
  // std::cout << "(" << chunk << ", " << bytes << ")\n";
  return chunk;
}

static int
_global_chunk_free(intptr_t pool_id, void* raw_ptr, size_t raw_bytes) {
  assert(pool_id == AS_GLOBAL);
  heap_chunk_dalloc(global_heap, raw_ptr, raw_bytes);
  return 0;
}

void
global_allocator_init(void) {
  int id = AS_GLOBAL;
  size_t granularity = as_bytes_per_chunk();
  const MemPoolPolicy policy(_global_chunk_alloc, _global_chunk_free,
                             granularity);
  MemoryPool* pool = NULL;
  pool_create_v1(id, &policy, &pool);
  pools[id] = pool;
}
