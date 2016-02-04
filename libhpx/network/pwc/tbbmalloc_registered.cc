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
#include <cstdlib>
#include <iostream>
#include <tbb/scalable_allocator.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "registered.h"
#include "xport.h"

using namespace rml;

/// The transport we'll use for pinning. It's not ideal to stick it here, but we
/// need to pin before the network has been exposed through the
/// here->network. We can simply capture the transport since we know we'll need
/// it anyway.
static pwc_xport_t *_xport = NULL;

static void *
_registered_chunk_alloc(intptr_t pool_id, size_t &bytes) {
  assert(pool_id == AS_REGISTERED);
  void *chunk = system_mmap_huge_pages(NULL, NULL, bytes, HPX_PAGE_SIZE);
  if (!chunk) {
    std::cerr << "failed to mmap " << bytes << " bytes anywhere in memory\n";
    abort();
  }
  _xport->pin(chunk, bytes, NULL);

  return chunk;
}

static int
_registered_chunk_free(intptr_t pool_id, void* raw_ptr, size_t raw_bytes) {
  assert(pool_id == AS_REGISTERED);
  _xport->unpin(raw_ptr, raw_bytes);
  system_munmap_huge_pages(NULL, raw_ptr, raw_bytes);
  return 0;
}

void
registered_allocator_init(pwc_xport_t *xport) {
  _xport = xport;

  int id = AS_REGISTERED;
  size_t granularity = as_bytes_per_chunk();
  const MemPoolPolicy policy(_registered_chunk_alloc, _registered_chunk_free,
                             granularity);
  MemoryPool* pool = NULL;
  pool_create_v1(id, &policy, &pool);
  pools[id] = pool;
}
