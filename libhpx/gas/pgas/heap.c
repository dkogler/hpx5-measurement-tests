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

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <libsync/sync.h>
#include <hpx/builtins.h>
#include <libhpx/bitmap.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "heap.h"
#include "pgas.h"

const uint64_t MAX_HEAP_BYTES = UINT64_C(1) << GPA_OFFSET_BITS;

///
static bitmap_t *
_new_bitmap(const heap_t *heap) {
  size_t nchunks = heap->nchunks;
  assert(nchunks <= UINT32_MAX);
  uint32_t min_align = ceil_log2_size_t(heap->bytes_per_chunk);
  uint32_t base_align = ctzl((uintptr_t)heap->base);
  bitmap_t *bitmap = bitmap_new((uint32_t)nchunks, min_align, base_align);
  if (!bitmap) {
    dbg_error("failed to allocate a bitmap to track free chunks.\n");
  }
  return bitmap;
}

static bool
_chunks_are_used(const heap_t *heap, uint64_t offset, size_t n) {
  uint32_t from = offset / heap->bytes_per_chunk;
  uint32_t to = (offset + n) / heap->bytes_per_chunk + 1;
  return bitmap_is_set(heap->chunks, from, to - from);
}

int
heap_init(heap_t *heap, size_t size) {
  assert(heap);
  assert(size);

  sync_store(&heap->csbrk, 0, SYNC_RELEASE);

  // align size to bytes-per-chunk boundary
  heap->bytes_per_chunk = as_bytes_per_chunk();
  log_gas("heap bytes per chunk is %zu\n", heap->bytes_per_chunk);

  heap->nbytes = size - (size % heap->bytes_per_chunk);
  log_gas("heap nbytes is aligned as %zu\n", heap->nbytes);
  if (heap->nbytes > MAX_HEAP_BYTES) {
    dbg_error("%zu > max heap bytes of %"PRIu64"\n", heap->nbytes, MAX_HEAP_BYTES);
  }

  heap->nchunks = ceil_div_size_t(heap->nbytes, heap->bytes_per_chunk);
  log_gas("heap nchunks is %zu\n", heap->nchunks);

  // mmap a properly aligned heap. The heap tends to be larger with large
  // alignment requirements, so we try and help out by suggesting a "good"
  // starting address.

  heap->max_block_lg_size = min_int(GPA_MAX_LG_BSIZE, ceil_log2_size_t(heap->nbytes));
  size_t align = (1lu << heap->max_block_lg_size);
  void *addr = (void*)align;
  heap->base = system_mmap_huge_pages(NULL, addr, heap->nbytes, align);
  if (!heap->base) {
    log_error("could not allocate %zu bytes for the global heap\n",
              heap->nbytes);
    return LIBHPX_ENOMEM;
  }
  log_gas("allocated %zu bytes for the global heap\n", heap->nbytes);

  assert((uintptr_t)heap->base % heap->bytes_per_chunk == 0);

  heap->chunks = _new_bitmap(heap);
  log_gas("allocated chunk bitmap to manage %zu chunks.\n", heap->nchunks);

  log_gas("allocated heap.\n");
  return LIBHPX_OK;
}

void
heap_fini(heap_t *heap) {
  if (heap->chunks)
    bitmap_delete(heap->chunks);

  if (heap->base) {
    system_munmap_huge_pages(NULL, heap->base, heap->nbytes);
  }
}

void *
heap_chunk_alloc(heap_t *heap, void *addr, size_t bytes, size_t align) {
  assert(bytes % heap->bytes_per_chunk == 0);
  assert(bytes / heap->bytes_per_chunk < UINT32_MAX);
  uint32_t bits = bytes / heap->bytes_per_chunk;
  uint32_t log2_align = ceil_log2_size_t(align);

  uint32_t bit = 0;
  if (bitmap_rreserve(heap->chunks, bits, log2_align, &bit)) {
    dbg_error("out-of-memory detected\n");
  }

  uint64_t offset = bit * heap->bytes_per_chunk;
  assert(offset % align == 0);

  if (offset < heap->csbrk) {
    dbg_error("out-of-memory detected\n");
  }

  return heap->base + offset;
}

void *
heap_cyclic_chunk_alloc(heap_t *heap, void *addr, size_t bytes, size_t align) {
  assert(bytes % heap->bytes_per_chunk == 0);
  assert(bytes / heap->bytes_per_chunk < UINT32_MAX);
  uint32_t bits = bytes / heap->bytes_per_chunk;
  uint32_t log2_align = ceil_log2_size_t(align);

  uint32_t bit = 0;
  if (bitmap_reserve(heap->chunks, bits, log2_align, &bit)) {
    dbg_error("out-of-memory detected\n");
  }

  uint64_t offset = bit * heap->bytes_per_chunk;
  heap_set_csbrk(heap, offset + bytes);
  void *p = heap_offset_to_lva(heap, offset);
  dbg_assert(((uintptr_t)p & (align - 1)) == 0);
  return p;
}

bool
heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size) {
  const uint64_t offset = (char*)chunk - heap->base;
  assert(offset % heap->bytes_per_chunk == 0);
  assert(size % heap->bytes_per_chunk == 0);

  const uint64_t    bit = offset / heap->bytes_per_chunk;
  const uint64_t  nbits = size / heap->bytes_per_chunk;

  bitmap_release(heap->chunks, bit, nbits);
  return true;
}

bool
heap_contains_lva(const heap_t *heap, const void *lva) {
  const ptrdiff_t d = (char*)lva - heap->base;
  return (0 <= d && d < heap->nbytes);
}

bool
heap_contains_offset(const heap_t *heap, uint64_t gpa) {
  return (gpa < heap->nbytes);
}

uint64_t
heap_lva_to_offset(const heap_t *heap, const void *lva) {
  DEBUG_IF (!heap_contains_lva(heap, lva)) {
    dbg_error("local virtual address %p is not in the global heap\n", lva);
  }
  return ((char*)lva - heap->base);
}

void *
heap_offset_to_lva(const heap_t *heap, uint64_t offset) {
  DEBUG_IF (heap->nbytes < offset) {
    dbg_error("offset %"PRIu64" out of range (0,%zu)\n", offset, heap->nbytes);
  }

  return heap->base + offset;
}

uint64_t
heap_alloc_cyclic(heap_t *heap, size_t n, uint32_t bsize) {
  dbg_assert_str(ceil_log2_32(bsize) <= heap_max_block_lg_size(heap),
                 "Attempting to allocate block with alignment %"PRIu32
                 " while the maximum alignment is %"PRIu32".\n",
                 ceil_log2_32(bsize), heap_max_block_lg_size(heap));

  // Figure out how many blocks per node that we need, and the base alignment.
  uint64_t blocks = ceil_div_64(n, here->ranks);
  uint32_t  align = ceil_log2_32(bsize);
  dbg_assert(align < 32);
  uint32_t padded = 1u << align;

  // Allocate the blocks as a contiguous, aligned array from cyclic memory.
  void *base = cyclic_memalign(padded, blocks * padded);
  if (!base) {
    dbg_error("failed cyclic allocation\n");
  }

  // And transform the result into a global address.
  uint64_t ret = heap_lva_to_offset(heap, base);

  // sanity check, make sure we have a good alignment in the heap
  dbg_assert((ret & ((uint64_t)padded - 1)) == 0);
  dbg_assert(heap_offset_is_cyclic(heap, ret));

  return ret;
}

void
heap_free_cyclic(heap_t *heap, uint64_t offset) {
  dbg_assert(heap_offset_is_cyclic(heap, offset));
  void *lva = heap_offset_to_lva(heap, offset);
  cyclic_free(lva);
}

bool
heap_offset_is_cyclic(const heap_t *heap, uint64_t offset) {
  if (offset >= heap->nbytes) {
    log_gas("offset %"PRIu64" is not in the heap\n", offset);
    return false;
  }

  return (offset < heap->csbrk);
}

uint64_t
heap_get_csbrk(const heap_t *heap) {
  return sync_load(&heap->csbrk, SYNC_ACQUIRE);
}

int
heap_set_csbrk(heap_t *heap, uint64_t offset) {
  // csbrk is monotonically increasing, so if we see a value in the csbrk field
  // larger than the new offset, it means that this is happening out of order
  uint64_t old = sync_load(&heap->csbrk, SYNC_RELAXED);
  if (old < offset) {
    sync_cas(&heap->csbrk, &old, offset, SYNC_RELAXED, SYNC_RELAXED);
    int used = _chunks_are_used(heap, old, offset - old);
    return (used) ? HPX_ERROR : HPX_SUCCESS;
  }
  // otherwise we have an old allocation and it's fine
  return HPX_SUCCESS;
}

uint32_t
heap_max_block_lg_size(const heap_t *heap) {
  return heap->max_block_lg_size;
}
