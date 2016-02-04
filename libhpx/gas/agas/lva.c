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
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include "agas.h"
#include "btt.h"
#include "chunk_table.h"
#include "gva.h"

static void*
_lva_to_chunk(agas_t *gas, void *lva) {
  uintptr_t mask = ~(gas->chunk_size - 1);
  return (void*)((uintptr_t)lva & mask);
}

gva_t agas_lva_to_gva(agas_t *gas, void *lva, uint32_t bsize) {
  // we need to reverse map this address to an offset into the local portion of
  // the global address space
  void *chunk = _lva_to_chunk(gas, lva);
  uint64_t base = chunk_table_lookup(gas->chunk_table, chunk);
  uint64_t offset = base + ((char*)lva - (char*)chunk);

  // and construct a gva for this
  gva_t gva = {
    .bits = {
      .offset = offset,
      .size   = ceil_log2_32(bsize),
      .cyclic  = 0,
      .home   = here->rank,
    }
  };

  // and return the address
  return gva;
}
