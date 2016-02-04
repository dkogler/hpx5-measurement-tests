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

#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include "chunk_table.h"

namespace {
  typedef cuckoohash_map<const void*, uint64_t, CityHasher<const void*> > ChunkTable;
}

void *
chunk_table_new(size_t size) {
  return new ChunkTable(size);
}

void
chunk_table_delete(void *obj) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  delete table;
}

uint64_t
chunk_table_lookup(void *obj, void *chunk) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  uint64_t base = 0;
  bool found = table->find(chunk, base);
  assert(found);
  return base;
}

void
chunk_table_insert(void *obj, void *chunk, uint64_t base) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  bool inserted = table->insert(chunk, base);
  assert(inserted);
  (void)inserted;
}

void
chunk_table_remove(void *obj, void *chunk) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  bool erased = table->erase(chunk);
  assert(erased);
  (void)erased;
}

