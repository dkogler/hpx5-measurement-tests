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
#include <stdlib.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "agas.h"
#include "btt.h"

/// Free a block asynchronously.
///
/// This will remove the block's translation table mapping once its reference
/// count hits zero. If the block was relocated (i.e., the parcel to free block
/// wasn't sent to the home locality for the block), it then frees the cloned
/// memory and forwards the request on to the home locality for the block,
/// recursively.
///
/// The base case just removes the mapping once it has no reference counts. It
/// expects that the block itself will be deleted as part of a segment of
/// memory.
///
/// This implementation leverages the expectation that a block is only relocated
/// if its gva "home" is different than the locality running this handler. This
/// must remain true during the implementation of AGAS move.
static HPX_ACTION_DECL(_agas_free_block);
static int _agas_free_block_handler(hpx_addr_t block) {
  agas_t *agas = here->gas;
  gva_t    gva = { .addr = block };
  void    *lva = NULL;
  int        e = btt_remove_when_count_zero(agas->btt, gva, &lva);
  dbg_check(e, "No btt entry for %"PRIu64" at %d\n", gva.addr, here->rank);

  // This was the home for the block, ultimately the continuation of this action
  // will free the segment it is in.
  if (here->rank == gva.bits.home) {
    return HPX_SUCCESS;
  }

  // This was a relocated block, free the clone and then forward the request
  // back to the home locality.x
  free(lva);
  hpx_addr_t home = HPX_THERE(gva.bits.home);
  return hpx_call_cc(home, _agas_free_block, &block);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_free_block, _agas_free_block_handler,
                     HPX_ADDR);

/// Free a segment associated with a cyclic allocation.
///
/// This action is broadcast during cyclic de-allocation. Each locality will
/// clean up all of the blocks in the allocation that are "home" to the
/// locality, blocking until it is completely cleaned up, and then continue back
/// to the root.
///
/// This action is also used by non-cyclic array allocations to clean up the
/// blocks associated with the array.
static int _agas_free_segment_handler(hpx_addr_t base) {
  agas_t *agas = here->gas;
  gva_t    gva = { .addr = base };

  // This message is always sent to the right place, but the base address isn't
  // always correct. This happens because we are using this action to deal with
  // segments related to cyclic allocation in addition to segments related to
  // normal allocation. The cyclic allocation case has broadcast the base
  // segment's address, so we patch up the address to get the "right" segment.
  gva.bits.home = here->rank;

  // We need to know both the number of blocks in the segment, and the backing
  // virtual address of the segment. The number of blocks allows us to iterate
  // through them, and the backing virtual address will allow us to free the
  // backing memory for cyclic segments.
  void *    lva = NULL;
  size_t blocks = 0;
  int     found = btt_get_all(agas->btt, gva, &lva, &blocks, NULL);
  dbg_assert(found);
  size_t  bsize = UINT64_C(1) << gva.bits.size;

  // NB: par-for would be useful here if #blocks is large
  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (int i = 0, e = blocks; i < e; ++i) {
    // all blocks in a segment are contiguous so we can use local add here.
    hpx_addr_t block = agas_local_add(agas, gva, i * bsize, bsize);
    dbg_check( hpx_call(block, _agas_free_block, and, &block) );
  }
  dbg_check( hpx_lco_wait(and) );
  hpx_lco_delete(and, HPX_NULL);

  // We need to release the memory backing the segment if it is part of a cyclic
  // allocation and is not the rank 0 segment, which is dealt with by the
  // _agas_free_async handler. These asymmetries are a result of the way that we
  // do all cyclic memory management at rank 0.
  if (gva.bits.cyclic && here->rank != 0) {
    free(lva);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_free_segment,
                     _agas_free_segment_handler, HPX_ADDR);

/// Free an allocation asynchronously.
///
/// This action runs at the home for the base allocation, and runs through and
/// cleans up all of the blocks associated with the allocation in a hierarchical
/// way.
///
/// for each segment
///   for each block
///     free block
///   free segment
/// free address space
static int _agas_free_async_handler(hpx_addr_t base) {
  agas_t *gas = here->gas;
  gva_t   gva = { .addr = base };
  dbg_assert(gva.bits.home == here->rank);

  // We need to free this after everything has been cleaned up.
  void   *lva = btt_lookup(gas->btt, gva);
  dbg_assert_str(lva, "No btt entry for %"PRIu64" at %d\n", gva.addr, here->rank);

  // Cyclic allocations have segments at each rank, so we broadcast the command
  // to clean up the segment. Otherwise we just clean up the local segment. We
  // optimize here for single-block allocations by skipping the segment code.
  if (gva.bits.cyclic) {
    dbg_check( hpx_bcast_rsync(_agas_free_segment, &base) );
    cyclic_free(lva);
  }
  else if (btt_get_blocks(gas->btt, gva) == 1) {
    dbg_check( hpx_call_sync(base, _agas_free_block, NULL, 0, &base) );
    global_free(lva);
  }
  else {
    dbg_check( hpx_call_sync(HPX_HERE, _agas_free_segment, NULL, 0, &base) );
    global_free(lva);
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_free_async, _agas_free_async_handler, HPX_ADDR);

/// Free a global address.
///
/// There is one particular state that an allocation can be in where it is
/// important to fast-path the operation and do it synchronously. When there is
/// a non-cyclic allocation that is a single block, local, and has a reference
/// count of 0, it is important to give that memory back to the allocator as
/// quickly as possible.
///
/// All other allocation states require either messaging (cyclic or non-local
/// allocations), concurrency (array allocations), or blocking (reference counts
/// != 0). In those circumstances we will perform an asynchronous free.
///
void agas_free(void *obj, hpx_addr_t addr, hpx_addr_t rsync) {
  dbg_assert(obj == here->gas);

  agas_t *gas = obj;
  gva_t   gva = { .addr = addr };

  // Until we change the user API for free, we need to deal with the rsync
  // future. If synchronous free is disabled, then we go ahead and set rsync at
  // this point. Other branches will also try and set rsync for the synchronous
  // algorithm.
  if (!here->config->dbg_syncfree) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    rsync = HPX_NULL;
  }

  // We support free of HPX_NULL.
  if (addr == HPX_NULL) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  // If the allocation is a single-block allocation without any outstanding
  // references that hasn't been relocated, then we got ahead and fastpath the
  // operation. All other states are done asynchronously.
  //
  // It is crucial for correctness that we perform this optimization, as the
  // _agas_free_sync code depends on being able to allocate and free simple
  // blocks (lcos) on this fastpath. Without it, we can get an infinite loop.
  if (!gva.bits.cyclic && gva.bits.home == here->rank) {
    void *lva;
    size_t blocks;
    int32_t count;
    int found = btt_get_all(gas->btt, gva, &lva, &blocks, &count);
    dbg_assert(found);
    dbg_assert(blocks > 0);
    dbg_assert(count >= 0);
    dbg_assert(lva);

    if (!count && blocks == 1) {
      btt_remove(gas->btt, gva);
      global_free(lva);
      hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
      return;
    }
  }

  // We can't fast-path this free, so we go ahead and perform it asynchronously.
  hpx_addr_t home = HPX_THERE(gva.bits.home);
  dbg_check( hpx_call(home, _agas_free_async, rsync, &addr) );
}
