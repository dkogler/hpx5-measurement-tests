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
#include "cyclic.h"
#include "heap.h"

/// @file  libhpx/gas/pgas/cyclic.c
/// @brief This file implements the address-space allocator interface for cyclic
///        memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the global_heap instance.
static void *_cyclic_chunk_alloc(void *addr, size_t n, size_t align, bool *zero,
                                 bool *commit, unsigned arena) {
  dbg_assert(global_heap);
  dbg_assert(zero);
  dbg_assert(commit);
  void *chunk = heap_cyclic_chunk_alloc(global_heap, addr, n, align);
  if (!chunk) {
    return NULL;
  }

  // According to the jemalloc man page, if addr is set, then we *must* return a
  // chunk at that address.
  if (addr && addr != chunk) {
    heap_chunk_dalloc(global_heap, chunk, n);
    return NULL;
  }

  // If we are asked to zero a chunk, then we do so.
  if (*zero) {
    memset(chunk, 0, n);
  }

  // Commit is not relevant for linux/darwin.
  *commit = true;
  return chunk;
}

static bool _cyclic_chunk_free(void *addr, size_t n, bool committed,
                               unsigned arena) {
  dbg_assert(global_heap);
  heap_chunk_dalloc(global_heap, addr, n);
  return 0;
}

void cyclic_allocator_init(void) {
  static const chunk_hooks_t _cyclic_hooks = {
    .alloc    = _cyclic_chunk_alloc,
    .dalloc   = _cyclic_chunk_free,
    .commit   = as_null_commit,
    .decommit = as_null_decommit,
    .purge    = as_null_purge,
    .split    = as_null_split,
    .merge    = as_null_merge
  };

  as_set_allocator(AS_CYCLIC, &_cyclic_hooks);
}
