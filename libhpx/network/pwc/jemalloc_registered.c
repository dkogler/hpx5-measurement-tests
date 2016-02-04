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
#include <libhpx/system.h>
#include "registered.h"
#include "xport.h"

/// The transport we'll use for pinning. It's not ideal to stick it here, but we
/// need to pin before the network has been exposed through the
/// here->network. We can simply capture the transport since we know we'll need
/// it anyway.
static pwc_xport_t *_xport = NULL;

/// @file  libhpx/gas/pgas/registered.c
/// @brief This file implements the address-space allocator interface for
///        registered memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the registered_heap instance.
static void *_registered_chunk_alloc(void *addr, size_t n, size_t align, bool *zero,
                                     bool *commit, unsigned arena) {
  dbg_assert(zero);
  dbg_assert(commit);
  void *chunk = system_mmap_huge_pages(NULL, addr, n, align);
  if (!chunk) {
    return NULL;
  }

  // According to the jemalloc man page, if addr is set, then we *must* return a
  // chunk at that address.
  if (addr && addr != chunk) {
    system_munmap_huge_pages(NULL, chunk, n);
    return NULL;
  }

  // Pin the memory.
  _xport->pin(chunk, n, NULL);

  // If we are asked to zero a chunk, then we do so.
  if (*zero) {
    memset(chunk, 0, n);
  }

  // Commit is not relevant for linux/darwin.
  *commit = true;
  return chunk;
}

static bool _registered_chunk_free(void *chunk, size_t n, bool committed,
                                   unsigned arena) {
  _xport->unpin(chunk, n);
  system_munmap_huge_pages(NULL, chunk, n);
  return 0;
}

void registered_allocator_init(pwc_xport_t *xport) {
  static const chunk_hooks_t _registered_hooks = {
    .alloc    = _registered_chunk_alloc,
    .dalloc   = _registered_chunk_free,
    .commit   = as_null_commit,
    .decommit = as_null_decommit,
    .purge    = as_null_purge,
    .split    = as_null_split,
    .merge    = as_null_merge
  };

  _xport = xport;
  as_set_allocator(AS_REGISTERED, &_registered_hooks);
}
