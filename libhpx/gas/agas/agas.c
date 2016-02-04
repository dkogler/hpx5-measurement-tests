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

#include <stdlib.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/bitmap.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "agas.h"
#include "btt.h"
#include "chunk_table.h"
#include "gva.h"

static const uint64_t AGAS_THERE_OFFSET = UINT64_C(4398046511103);

__thread size_t agas_alloc_bsize;

HPX_ACTION_DECL(agas_alloc_cyclic);
HPX_ACTION_DECL(agas_calloc_cyclic);

static void
_agas_dealloc(void *gas) {
  agas_t *agas = gas;
  if (agas->chunk_table) {
    chunk_table_delete(agas->chunk_table);
  }
  if (agas->btt) {
    btt_delete(agas->btt);
  }
  if (agas->bitmap) {
    bitmap_delete(agas->bitmap);
  }

  if (here->rank == 0) {
    if (agas->cyclic_bitmap) {
      bitmap_delete(agas->cyclic_bitmap);
    }
  }
  free(agas);
}

static int64_t
_agas_sub(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  gva_t l = { .addr = lhs };
  gva_t r = { .addr = rhs };

  uint32_t size = ceil_log2_32(bsize);
  if (l.bits.size != size || r.bits.size != size) {
    dbg_error("block size does not match\n");
  }

  if (l.bits.cyclic && r.bits.cyclic) {
    return gpa_sub_cyclic(lhs, rhs, bsize);
  }

  if (!l.bits.cyclic && !r.bits.cyclic) {
    if (l.bits.home == r.bits.home) {
      return agas_local_sub(gas, l, r, bsize);
    }
  }

  dbg_error("could not compare pointers between allocations\n");
}

static hpx_addr_t
_agas_add(const void *gas, hpx_addr_t addr, int64_t bytes, uint32_t bsize) {
  gva_t gva = { .addr = addr };
  uint32_t size = ceil_log2_32(bsize);
  if (gva.bits.size != size) {
    log_error("block size does not match\n");
  }

  if (gva.bits.cyclic) {
    gva.addr = gpa_add_cyclic(addr, bytes, bsize);
    gva.bits.size = size;
    gva.bits.cyclic = 1;
    return gva.addr;
  }
  else {
    return agas_local_add(gas, gva, bytes, bsize);
  }
}

static hpx_addr_t
_agas_there(void *gas, uint32_t i) {
  // We reserve a small range of addresses in the "large" allocation space that
  // will represent locality addresses.
  dbg_assert(i < here->ranks);
  gva_t gva = {
    .bits = {
      .offset = AGAS_THERE_OFFSET,
      .size   = 0,
      .cyclic  = 0,
      .home   = (uint16_t)i,
    }
  };
  return gva.addr;
}

static bool
_agas_try_pin(void *gas, hpx_addr_t addr, void **lva) {
  agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  return btt_try_pin(agas->btt, gva, lva);
}

static void
_agas_unpin(void *gas, hpx_addr_t addr) {
  agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  btt_unpin(agas->btt, gva);
}

static uint32_t
_agas_owner_of(const void *gas, hpx_addr_t addr) {
  const agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  return btt_owner_of(agas->btt, gva);
}

static int
_locality_alloc_cyclic_handler(uint64_t blocks, uint32_t align, uint64_t offset,
                               void *lva, uint32_t attr, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  uint32_t bsize = 1u << align;
  if (here->rank != 0) {
    uint32_t boundary = (bsize < 8) ? 8 : bsize;
    lva = NULL;
    int e = posix_memalign(&lva, boundary, blocks * bsize);
    dbg_check(e, "Failed memalign\n");
    (void)e;
  }

  if (zero) {
    lva = memset(lva, 0, blocks * bsize);
  }

  // and insert entries into our block translation table
  gva_t gva = {
    .bits = {
      .offset = offset,
      .cyclic = 1,
      .size = align,
      .home = here->rank
    }
  };

  for (int i = 0; i < blocks; i++) {
    btt_insert(agas->btt, gva, here->rank, lva, blocks, attr);
    lva += bsize;
    gva.bits.offset += bsize;
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _locality_alloc_cyclic,
                     _locality_alloc_cyclic_handler, HPX_UINT64, HPX_UINT32,
                     HPX_UINT64, HPX_POINTER, HPX_UINT32, HPX_INT);

hpx_addr_t _agas_alloc_cyclic_sync(size_t n, uint32_t bsize, uint32_t attr,
                                   int zero) {
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(here->rank == 0);

  // Figure out how many blocks per node we need.
  uint64_t blocks = ceil_div_64(n, here->ranks);
  uint32_t  align = ceil_log2_32(bsize);
  dbg_assert(align < 32);
  uint32_t padded = 1u << align;

  agas_alloc_bsize = padded;
  // Allocate the blocks as a contiguous, aligned array from cyclic memory.
  void *lva = cyclic_memalign(padded, blocks * padded);
  if (!lva) {
    dbg_error("failed cyclic allocation\n");
  }

  gva_t gva = agas_lva_to_gva(agas, lva, padded);
  gva.bits.cyclic = 1;
  uint64_t offset = gva.bits.offset;
  int e = hpx_bcast_rsync(_locality_alloc_cyclic, &blocks, &align, &offset,
                          &lva, &attr, &zero);
  dbg_check(e, "failed to insert btt entries.\n");

  // and return the address
  return gva.addr;
}

hpx_addr_t agas_alloc_cyclic_sync(size_t n, uint32_t bsize, uint32_t attr) {
  dbg_assert(here->rank == 0);
  return _agas_alloc_cyclic_sync(n, bsize, attr, 0);
}

static int _alloc_cyclic_handler(size_t n, size_t bsize, uint32_t attr) {
  hpx_addr_t addr = agas_alloc_cyclic_sync(n, bsize, attr);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, agas_alloc_cyclic, _alloc_cyclic_handler,
              HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32);

static hpx_addr_t
_agas_alloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary, uint32_t attr) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = agas_alloc_cyclic_sync(n, bsize, attr);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), agas_alloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize, &attr);
    dbg_check(e, "Failed to call agas_alloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

hpx_addr_t agas_calloc_cyclic_sync(size_t n, uint32_t bsize, uint32_t attr) {
  assert(here->rank == 0);
  return _agas_alloc_cyclic_sync(n, bsize, attr, 1);
}

static int _calloc_cyclic_handler(size_t n, size_t bsize, uint32_t attr) {
  hpx_addr_t addr = agas_calloc_cyclic_sync(n, bsize, attr);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, agas_calloc_cyclic, _calloc_cyclic_handler,
              HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32);

static hpx_addr_t
_agas_calloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary,
                    uint32_t attr) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = agas_calloc_cyclic_sync(n, bsize, attr);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), agas_calloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize, &attr);
    dbg_check(e, "Failed to call agas_calloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}


static gas_t _agas_vtable = {
  .type           = HPX_GAS_AGAS,
  .string = {
    .memget       = agas_memget,
    .memget_rsync = agas_memget_rsync,
    .memget_lsync = agas_memget_lsync,
    .memput       = agas_memput,
    .memput_lsync = agas_memput_lsync,
    .memput_rsync = agas_memput_rsync,
    .memcpy       = agas_memcpy,
    .memcpy_sync  = agas_memcpy_sync,
  },
  .dealloc        = _agas_dealloc,
  .local_size     = NULL,
  .local_base     = NULL,
  .sub            = _agas_sub,
  .add            = _agas_add,
  .there          = _agas_there,
  .try_pin        = _agas_try_pin,
  .unpin          = _agas_unpin,
  .alloc_cyclic   = _agas_alloc_cyclic,
  .calloc_cyclic  = _agas_calloc_cyclic,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = agas_local_alloc,
  .calloc_local   = agas_local_calloc,
  .free           = agas_free,
  .move           = agas_move,
  .owner_of       = _agas_owner_of
};

gas_t *gas_agas_new(const config_t *config, boot_t *boot) {
  agas_t *agas = malloc(sizeof(*agas));
  dbg_assert(agas);

  agas_alloc_bsize = 0;
  agas->vtable = _agas_vtable;
  agas->chunk_table = chunk_table_new(0);
  agas->btt = btt_new(0);

  // get the chunk size from jemalloc
  agas->chunk_size = as_bytes_per_chunk();

  size_t heap_size = 1lu << GVA_OFFSET_BITS;
  size_t nchunks = ceil_div_size_t(heap_size, agas->chunk_size);
  uint32_t min_align = ceil_log2_64(agas->chunk_size);
  uint32_t base_align = ceil_log2_64(heap_size);
  agas->bitmap = bitmap_new(nchunks, min_align, base_align);
  agas_global_allocator_init(agas);

  if (here->rank == 0) {
    size_t nchunks = ceil_div_size_t(here->ranks * heap_size, agas->chunk_size);
    uint32_t min_align = ceil_log2_64(agas->chunk_size);
    uint32_t base_align = ceil_log2_64(heap_size);
    agas->cyclic_bitmap = bitmap_new(nchunks, min_align, base_align);
    log_gas("allocated the arena to manage cyclic allocations.\n");
    agas_cyclic_allocator_init(agas);
  }

  gva_t there = { .addr = _agas_there(agas, here->rank) };
  btt_insert(agas->btt, there, here->rank, here, 1, HPX_GAS_ATTR_NONE);
  return &agas->vtable;
}


void *
agas_chunk_alloc(agas_t *agas, void *bitmap, void *addr, size_t n, size_t align)
{
  // 1) get gva placement for this allocation
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  uint32_t log2_align = ceil_log2_size_t(max_size_t(align, agas_alloc_bsize));
  uint32_t bit;
  int e = bitmap_reserve(bitmap, nbits, log2_align, &bit);
  dbg_check(e, "Could not reserve gva for %lu bytes\n", n);
  uint64_t offset = bit * agas->chunk_size;

  // 2) get backing memory
  align = 1 << log2_align;
  void *base = system_mmap(NULL, addr, n, align);
  dbg_assert(base);
  dbg_assert(((uintptr_t)base & (align - 1)) == 0);

  // 3) insert the inverse mappings
  char* chunk = base;
  for (int i = 0; i < nbits; i++) {
    chunk_table_insert(agas->chunk_table, chunk, offset);
    offset += agas->chunk_size;
    chunk += agas->chunk_size;
  }
  agas_alloc_bsize = 0;
  return base;
}

void
agas_chunk_dalloc(agas_t *agas, void *bitmap, void *addr, size_t n) {
  // 1) release the bits
  uint64_t offset = chunk_table_lookup(agas->chunk_table, addr);
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  uint32_t bit = offset / agas->chunk_size;
  bitmap_release(bitmap, bit, nbits);

  // 2) unmap the backing memory
  system_munmap(NULL, addr, n);

  // 3) remove the inverse mappings
  char *chunk = addr;
  for (int i = 0; i < nbits; ++i) {
    chunk_table_remove(agas->chunk_table, chunk);
    chunk += agas->chunk_size;
  }
}
