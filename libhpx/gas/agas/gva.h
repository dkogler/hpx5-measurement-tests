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

#ifndef LIBHPX_GAS_AGAS_GVA_H
#define LIBHPX_GAS_AGAS_GVA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <hpx/hpx.h>

/// Set up some limitations for the AGAS implementation for now.
#define   GVA_RANK_BITS 16
#define   GVA_SIZE_BITS 5
#define GVA_OFFSET_BITS 42

typedef union {
  hpx_addr_t addr;
  struct {
    uint64_t offset : GVA_OFFSET_BITS;
    uint64_t   size : GVA_SIZE_BITS;
    uint64_t cyclic : 1;
    uint64_t   home : GVA_RANK_BITS;
  } bits;
} gva_t;

_HPX_ASSERT(sizeof(gva_t) == sizeof(hpx_addr_t), gva_bitfield_packing_failed);

static inline uint64_t gva_to_key(gva_t gva) {
  uint64_t mask = ~((UINT64_C(1) << gva.bits.size) - 1);
  return gva.addr & mask;
}

static inline uint64_t gva_to_block_offset(gva_t gva) {
  uint64_t mask = ((UINT64_C(1) << gva.bits.size) - 1);
  return gva.addr & mask;
}

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_GVA_H
