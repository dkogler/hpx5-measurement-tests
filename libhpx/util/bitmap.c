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

/// @file libhpx/gas/bitmap.c
/// @brief Implement a simple parallel bitmap.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/builtins.h>
#include <libsync/sync.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/bitmap.h>

/// We manage bitmaps in the granularity of chunks. In C++ this would be a
/// template parameter, here, we use a typedef to set up our block size.
/// @{

/// The block_t is currently a word.
///
/// @note We could use vector extensions to do this in the future, if we wanted
///       to write a best-fit algorithm that might make a difference.
typedef uintptr_t block_t;

/// Constants related to the block type.
static const block_t BLOCK_ALL_FREE = UINTPTR_MAX;
static const uint32_t   BLOCK_BYTES = sizeof(block_t);
static const uint32_t    BLOCK_BITS = sizeof(block_t) * 8;

/// Bitwise and between blocks.
static inline block_t block_and(block_t lhs, block_t rhs) {
  return (lhs & rhs);
}

/// Bitwise xor between blocks.
static inline block_t block_xor(block_t lhs, block_t rhs) {
  return (lhs ^ rhs);
}

/// Return the 0-based index of the first 1 bit in the block. At least one bit
/// in @p b must be set.
///
/// @code
/// block_ifs(0001 0001 0001 0000) -> 4
/// block_ifs(0001 0001 0000 0000) -> 8
/// @endcode
static inline uint32_t block_ifs(block_t b) {
  assert(b);
  return ctzl(b);
}

/// Return the 0-based index of the last 1 bit in the block. At least one bit in
/// @p b must be set.
///
/// @code
/// block_lfs(0001 0001 0001 0000) -> 12
/// block_lfs(0000 0001 0001 0000) -> 8
/// @endcode
static inline uint32_t block_ils(block_t b) {
  assert(b);
  return BLOCK_BITS - clzl(b) - 1;
}

/// Count the number of bits set in a block.
static inline uint32_t block_popcount(block_t b) {
  return popcountl(b);
}

/// Create a block bitmask of min(@p length, BLOCK_BITS - offset) 1s starting at
/// @p offset.
///
/// @param       offset The index of the least-significant 1 in the resulting
///                     mask.
/// @param       length The number of contiguous 1s in the mask.
///
/// @returns A constructed mask.
static inline block_t _create_mask(uint32_t offset, uint32_t length) {
  assert(offset < BLOCK_BITS);

  // We construct the mask by shifting the BLOCK_ALL_FREE constant right the
  // number of bits to result in the correct number of 1s, and then
  // left-shifting that string back up to the offset. If the left-shift happens
  // to push some bits back off the right end, that's fine and what we expect.
  //
  uint32_t rshift = (BLOCK_BITS < length) ? 0 : BLOCK_BITS - length;
  return (BLOCK_ALL_FREE >> rshift) << offset;
}
/// @}

/// @struct bitmap
/// @brief The bitmap structure.
///
/// The bitmap is fundamentally an array of bits chunked up into blocks combined
/// with some header data describing the block array and a lock for
/// concurrency.
///
/// @var  bitmap::lock 
/// A single lock to serialize access to the bitmap.
/// @var  bitmap::min
/// An index such that there are no free bits < min.
/// @var  bitmap::max
/// An index such that there are no free bits >= max.
/// @var  bitmap::nbits
/// The number of bits in the bitmap.
/// @var  bitmap::nblocks
/// The number of blocks in the block array.
/// @var  bitmap::blocks
/// The block array.
struct bitmap {
  tatas_lock_t      lock;
  uint32_t     min_align;
  uint32_t    base_align;
  uint32_t           min;
  uint32_t           max;
  uint32_t         nbits;
  uint32_t       nblocks;
  block_t       blocks[];
} HPX_ALIGNED(HPX_CACHELINE_SIZE);

/// Try to match the range of bits from [@p bit, @p bit + @p nbits).
///
/// This will scan the bitmap for @p nbits free bits starting at @p bit. If it
/// succeeds, it will return @p nbits. It failure mode is controlled by the @p f
/// parameter. If @p f is _block_ifs(), it will return the number of bits
/// successfully matched. If @p f is _block_ils(), it will return the number of
/// bits matched to get to the last failure.
///
/// This @p f customization is an optimization. The caller knows if it's going
/// to shift the test left or right in the presence of a failure, and the index
/// of the first failure or last failure can help avoid useless retries,
/// depending on which way the shift will be.
///
/// @param       blocks The bits we are testing.
/// @param          bit The base offset.
/// @param        nbits The number of bits to match.
/// @param            f On failure, count to the first or last mismatch.
///
/// @returns @p nbits for success, customizable for failure (see description).
static uint32_t _match(const block_t *blocks, uint32_t bit, uint32_t nbits,
                       uint32_t (*f)(block_t)) {
  assert(blocks);
  assert(nbits);

  uint32_t  block = bit / BLOCK_BITS;            // block index
  uint32_t offset = bit % BLOCK_BITS;            // initial offset in block
  block_t    mask = _create_mask(offset, nbits); // mask to match
  uint32_t      n = 0;                           // bits processed

  while (n < nbits) {
    block_t    match = block_and(mask, blocks[block++]);
    block_t mismatch = block_xor(mask, match);
    if (mismatch) {
      // return the number of bits we've matched so far, plus the number of bits
      // in the current word depending on if the user wants the first or last
      // mismatch. The trailing block_ifs(mask) deals with mismatches in the
      // first block only, the mask starts at index 0 for all other blocks.
      return n + f(mismatch) - block_ifs(mask);
    }

    // we matched each bit in the mask
    n += block_popcount(mask);
    mask = _create_mask(0, nbits - n);
  }

  assert(n == nbits);
  return n;
}

/// Scan through the blocks looking for the first free bit.
///
/// @param       blocks The blocks to scan.
/// @param          bit The starting bit index.
/// @param          max The end bit index.
///
/// @returns The first free bit, or max if there are no free bits.
static uint32_t _first_free(const block_t *blocks, uint32_t bit, uint32_t max) {
  assert(blocks);
  assert(bit < max);
  log_gas("finding the first free bit during allocation\n");

  uint32_t  block = bit / BLOCK_BITS;
  uint32_t offset = bit % BLOCK_BITS;
  block_t    mask = _create_mask(offset, BLOCK_BITS);

  while (bit < max) {
    block_t match = block_and(mask, blocks[block++]);
    if (match) {
      // For a match in the first block, we need to account for the fact that
      // the first set bit in mask might not be the 0th bit, so we subtract that
      // out.
      return bit + block_ifs(match) - block_ifs(mask);
    }

    // we matched each bit in the mask
    bit += block_popcount(mask);
    mask = BLOCK_ALL_FREE;
  }

  return max;
}

/// Set a contiguous number of bits in the bitmap.
///
/// @param       blocks The blocks of the bitmap we're updating.
/// @param          bit The absolute bit index we're starting with.
/// @param        nbits The number of bits that we're setting.
static void _set(block_t *blocks, uint32_t bit, uint32_t nbits) {
  assert(blocks);
  assert(nbits);

  uint32_t  block = bit / BLOCK_BITS;
  uint32_t offset = bit % BLOCK_BITS;
  block_t    mask = _create_mask(offset, nbits);

  while (nbits > 0) {
    //    val   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 0 0 1
    //
    // synchronized write so that it can be read non-blockingly in is_set, not a
    // release though since we're holding a lock
    block_t val = sync_load(&blocks[block], SYNC_RELAXED);
    sync_store(&blocks[block++], val & ~mask, SYNC_RELAXED);

    // we set each bit in the mask
    nbits -= block_popcount(mask);
    mask = _create_mask(0, nbits);
  }
}

/// Clear a continuous number of bits in the bitmap.
///
/// @param       blocks The blocks we're updating.
/// @param          bit The absolute bit index we're starting with.
/// @param        nbits The number of bits that we're clearing.
static void _clear(block_t *blocks, uint32_t bit, uint32_t nbits) {
  assert(blocks);
  assert(nbits);

  uint32_t  block = bit / BLOCK_BITS;
  uint32_t offset = bit % BLOCK_BITS;
  block_t    mask = _create_mask(offset, nbits);

  while (nbits > 0) {
    //    val   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 1 1 1
    //
    // synchronized write so that it can be read non-blockingly in is_set, not a
    // release though since we're holding a lock
    block_t val = sync_load(&blocks[block], SYNC_RELAXED);
    sync_store(&blocks[block++], val | mask, SYNC_RELAXED);

    // we cleared each bit in the mask
    nbits -= block_popcount(mask);
    mask = _create_mask(0, nbits);
  }
}

/// Count the number of unused blocks in the bitmap.
static int32_t _bitmap_unused_blocks(const bitmap_t *map) {
  uint32_t unused = 0;
  for (uint32_t i = 0, e = map->nblocks; i < e; ++i) {
    unused += block_popcount(map->blocks[i]);
  }

  uint32_t extra = BLOCK_BITS - (map->nbits % BLOCK_BITS);
  return (int32_t)(unused - extra);
}

/// Handle an out-of-memory condition.
///
/// @param          map The map that is full.
/// @param        nbits The request size that triggered the OOM.
/// @param        align The alignment that triggered OOM.
///
/// @returns LIBHPX_ENOMEM
static int _bitmap_oom(const bitmap_t *map, uint32_t nbits, uint32_t align) {
  int32_t unused = _bitmap_unused_blocks(map);
  dbg_error("Application ran out of global address space.\n"
            "\t-%u blocks requested with alignment %u\n"
            "\t-%u blocks available\n"
            "This space is used for all global allocation, as well as all\n"
            "stacks and parcel data. Pathological allocation may introduce\n"
            "fragmentation leading to unexpected counts above. Try adjusting\n"
            "your configuration.\n", nbits, align, unused);
  return LIBHPX_ENOMEM;
}

bitmap_t *bitmap_new(uint32_t nbits, uint32_t min_align, uint32_t base_align) {
  uint32_t nblocks = ceil_div_32(nbits, BLOCK_BITS);
  bitmap_t *map = NULL;
  int e = posix_memalign((void**)&map, HPX_CACHELINE_SIZE,
                         sizeof(*map) + nblocks * BLOCK_BYTES);
  if (e)
    dbg_error("failed to allocate a bitmap for %u bits\n", nbits);

  sync_tatas_init(&map->lock);
  map->min_align  = min_align;
  map->base_align = base_align;
  map->min        = 0;
  map->max        = nbits;
  map->nbits      = nbits;
  map->nblocks    = nblocks;
  memset(&map->blocks, BLOCK_ALL_FREE, nblocks * BLOCK_BYTES);
  return map;
}

void bitmap_delete(bitmap_t *map) {
  if (map) {
    free(map);
  }
}

int bitmap_reserve(bitmap_t *map, uint32_t nbits, uint32_t align, uint32_t *i) {
  log_gas("searching for %u blocks with alignment %u.\n", nbits, align);
  if (nbits == 0) {
    return LIBHPX_EINVAL;
  }

  uint32_t bit;
  sync_tatas_acquire(&map->lock);
  {
    // scan for a match, starting with the minimum available bit
    bit = map->min;
    while (true) {
      // crazy way of finding an aligned bit
      uint64_t val = bit * (1ul << map->min_align) + (1ul << map->base_align);
      uint32_t max = ctzl(val);
      while (align > max) {
        bit += 1;
        val = bit * (1ul << map->min_align) + (1ul << map->base_align);
        max = ctzl(val);
      }

      // make sure the match is inbounds
      if (bit + nbits > map->max) {
        return _bitmap_oom(map, nbits, align);
      }

      uint32_t matched = _match(map->blocks, bit, nbits, block_ils);
      if (matched == nbits) {
        break;
      }

      bit = _first_free(map->blocks, bit + matched, map->nbits);
    }

    // make sure we didn't run out of memory
    assert(bit + nbits <= map->max);

    if (map->min == bit) {
      uint32_t min = bit + nbits;
      log_gas("updated minimum free bit from %u to %u\n", map->min, min);
      map->min = min;
    }

    _set(map->blocks, bit, nbits);
  }
  sync_tatas_release(&map->lock);

  log_gas("found at offset %u.\n", bit);

  *i = bit;
  return LIBHPX_OK;
}

int bitmap_rreserve(bitmap_t *map, uint32_t nbits, uint32_t align, uint32_t *i)
{
  log_gas("reverse search for %u blocks with alignment %u.\n", nbits,
              align);
  if (nbits == 0)
    return LIBHPX_EINVAL;

  uint32_t bit;
  sync_tatas_acquire(&map->lock);
  {
    // scan for a match, starting with the max available bit
    bit = map->max;
    uint32_t matched = 0;

    while (matched != nbits) {
      assert(matched <= nbits);

      // shift down by the number of bits we matched in the last round
      uint32_t shift = nbits - matched;
      if (bit < shift) {
        return _bitmap_oom(map, nbits, align);
      }

      bit = bit - shift;

      // compute the closest aligned bit to the shifted bit
      uint64_t val = bit * (1ul << map->min_align) + (1ul << map->base_align);
      uint32_t max = ctzl(val);
      while (align > max) {
        if (bit == 0) {
          return _bitmap_oom(map, nbits, align);
        }
        bit -= 1;
        val = bit * (1ul << map->min_align) + (1ul << map->base_align);
        max = ctzl(val);
      }

      if (bit < map->min) {
        return _bitmap_oom(map, nbits, align);
      }

      // see how far we can match
      matched = _match(map->blocks, bit, nbits, block_ifs);
    }

    // make sure we didn't run out of memory
    assert(bit + nbits <= map->max);

    uint32_t max = bit + nbits;
    if (map->max == max) {
      log_gas("updated maximum bit from %u to %u\n", map->max, bit);
      map->max = bit;
    }

    _set(map->blocks, bit, nbits);
  }
  sync_tatas_release(&map->lock);

  log_gas("found at offset %u.\n", bit);

  *i = bit;
  return LIBHPX_OK;
}

void bitmap_release(bitmap_t *map, uint32_t bit, uint32_t nbits) {
  log_gas("release %u blocks at %u.\n", nbits, bit);

  sync_tatas_acquire(&map->lock);
  {
    _clear(map->blocks, bit, nbits);

    if (bit < map->min) {
      log_gas("updated minimum free bit from %u to %u\n", map->min, bit);
      map->min = bit;
    }

    uint32_t max = bit + nbits;
    if (max > map->max) {
      log_gas("updated maximum bit from %u to %u\n", map->max, max);
      map->max = max;
    }
  }
  sync_tatas_release(&map->lock);
}

bool bitmap_is_set(const bitmap_t *map, uint32_t bit, uint32_t nbits) {
  assert(map);
  dbg_assert_str(bit + nbits <= map->nbits,
                 "query out of range, %d + %d > %d\n", bit, nbits, map->nbits);

  uint32_t  block = bit / BLOCK_BITS;
  uint32_t offset = bit % BLOCK_BITS;
  block_t    mask = _create_mask(offset, nbits);

  while (nbits) {
    block_t    match = block_and(mask, map->blocks[block++]);
    block_t mismatch = block_xor(mask, match);
    if (mismatch)
      return true;

    nbits -= block_popcount(mask);
    mask   = _create_mask(0, nbits);
  }

  return false;
}
