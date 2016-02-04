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
#include "agas.h"

using namespace rml;

static agas_t *_agas = NULL;

static void *
_global_chunk_alloc(intptr_t pool_id, size_t &bytes) {
  assert(pool_id == AS_GLOBAL);
  assert(_agas->bitmap);
  assert(_agas->chunk_size);
  void *chunk = agas_chunk_alloc(_agas,
                                 _agas->bitmap, NULL, bytes,
                                 _agas->chunk_size);
  assert(chunk);
  return chunk;
}

static int
_global_chunk_free(intptr_t pool_id, void* raw_ptr, size_t raw_bytes) {
  assert(pool_id == AS_GLOBAL);
  agas_chunk_dalloc(_agas, _agas->bitmap, raw_ptr, raw_bytes);
  return 0;
}

void
agas_global_allocator_init(agas_t *agas) {
  assert(agas);
  _agas = agas;

  int id = AS_GLOBAL;
  const MemPoolPolicy policy(_global_chunk_alloc, _global_chunk_free,
                             _agas->chunk_size);
  MemoryPool* pool = NULL;
  pool_create_v1(id, &policy, &pool);
  pools[id] = pool;
}
