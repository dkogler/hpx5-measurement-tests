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
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <malloc-2.8.6.h>
#include "agas.h"
#include "chunk_table.h"

void _agas_allocator_init(agas_t *agas, int id) {
  dbg_assert(id < AS_COUNT);
  const libhpx_config_t *cfg = libhpx_get_config();
  size_t bytes = ceil_div_size_t(cfg->heapsize, 2);
  void *base = system_mmap(NULL, NULL, bytes, agas->chunk_size);
  dbg_assert(base);
  chunk_table_insert(agas->chunk_table, base, 0);
  mspaces[id] = create_mspace_with_base(base, bytes, 1);
}

void
agas_cyclic_allocator_init(agas_t *agas) {
  _agas_allocator_init(agas, AS_CYCLIC);
}

void
agas_global_allocator_init(agas_t *agas) {
  _agas_allocator_init(agas, AS_GLOBAL);
}

