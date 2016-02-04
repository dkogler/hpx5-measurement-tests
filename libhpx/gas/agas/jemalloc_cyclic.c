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

/// @file  libhpx/gas/agas/cyclic.c
/// @brief This file implements the address-space allocator interface for cyclic
///        memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the cyclic bitmap instance.
#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include "agas.h"

static void *_cyclic_chunk_alloc(void *addr, size_t n, size_t align, bool *zero,
                                 bool *commit, unsigned arena) {
  dbg_assert(here && here->gas);
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->cyclic_bitmap);
  void *chunk = agas_chunk_alloc(agas, agas->cyclic_bitmap, addr, n, align);
  if (!chunk) {
    return NULL;
  }

  // According to the jemalloc man page, if addr is set, then we *must* return a
  // chunk at that address.
  if (addr && addr != chunk) {
    agas_chunk_dalloc(agas, agas->cyclic_bitmap, addr, n);
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

static bool _cyclic_chunk_free(void *addr, size_t n, bool commited,
                               unsigned arena) {
  dbg_assert(here && here->gas);
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->cyclic_bitmap);
  agas_chunk_dalloc(agas, agas->cyclic_bitmap, addr, n);
  return 0;
}

void agas_cyclic_allocator_init(agas_t *agas) {
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
