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

#ifndef LIBHPX_GAS_BITMAP_H
#define LIBHPX_GAS_BITMAP_H

#include <stddef.h>
#include <stdint.h>
#include <hpx/attributes.h>
#include <libsync/locks.h>

/// @file libhpx/gas/bitmap.h
/// @brief Implement a simple parallel bitmap allocator to manage chunk
///        allocation for jemalloc.
///
/// We use jemalloc directly to manage our symmetric heaps. Unlike dlmalloc,
/// which provides a "memory space" abstraction, jemalloc operates in terms of
/// large "chunks," which are typically provided through the system mmap/munmap
/// functionality.
///
/// In our case, we want jemalloc to use chunks that we provide from our
/// global heap.
///
/// @note Our implementation is currently single-lock based. If this becomes a
///       synchronization bottleneck we can move to a non-blocking or more
///       fine-grained approach.
///
typedef struct bitmap bitmap_t;

/// Allocate and initialize a bitmap.
///
/// @param        nbits The number of bits we need to manage.
/// @param    min_align The minimum alignment.
/// @param   base_align The base alignment.
///
/// @returns The new bitmap or NULL if there was an error.
bitmap_t *bitmap_new(uint32_t nbits, uint32_t min_align, uint32_t base_align)
  HPX_MALLOC;

/// Delete a bitmap that was previously allocated with bitmap_new().
///
/// @param       bitmap The bitmap to free,
void bitmap_delete(bitmap_t *bitmap);

/// Allocate @p nbits contiguous bits from the bitmap, aligned to a @p align
/// boundary.
///
/// This performs a least->most significant bit search for space.
///
/// @param[in]      map The bitmap to allocate from.
/// @param[in]    nbits The number of continuous bits to allocate.
/// @param[in]     align The alignment we need to return
/// @param[out]       i The offset of the start of the allocation.
///
/// @returns LIBHPX_OK, LIBHPX_ENOMEM
int bitmap_reserve(bitmap_t *map, uint32_t nbits, uint32_t align, uint32_t *i)
  HPX_NON_NULL(1, 4);

/// Allocate @p nbits contiguous bits from the bitmap, aligned to a @p align
/// boundary.
///
/// This performs a most->least significant bit search.
///
/// @param[in]      map The bitmap to allocate from.
/// @param[in]    nbits The number of continuous bits to allocate.
/// @param[in]    align The alignment we need to find.
/// @param[out]       i The offset of the start of the allocation.
///
/// @returns LIBHPX_OK, LIBHPX_ENOMEM
int bitmap_rreserve(bitmap_t *map, uint32_t nbits, uint32_t align, uint32_t *i)
  HPX_NON_NULL(1, 4);

/// Free @p nbits contiguous bits of memory, starting at offset @p i.
///
/// @param          map The bitmap to free from.
/// @param            i The offset to start freeing from.
/// @param        nbits The number of bits to free.
void bitmap_release(bitmap_t *map, uint32_t i, uint32_t nbits)
  HPX_NON_NULL(1);

/// Determine if a particular region has been allocated.
///
/// @param          map The bitmap to check.
/// @param          bit The bit offset to check.
/// @param        nbits The number of bits.
///
/// @returns true if the bit is set, false otherwise.
bool bitmap_is_set(const bitmap_t *map, uint32_t bit, uint32_t nbits)
  HPX_NON_NULL(1);

#endif
