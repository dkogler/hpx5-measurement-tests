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
#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>


#include "heap.h"
#include "pgas.h"

static HPX_ACTION_DECL(_calloc_init);
static HPX_ACTION_DECL(_set_csbrk);

/// Internal utility actions for dealing with calloc.
/// @{

/// Allocate from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0), and
/// doesn't need to be broadcast because the server controls this for
/// everyone. All global cyclic allocations are rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_alloc_cyclic_sync(size_t n, uint32_t bsize) {
  uint64_t offset = heap_alloc_cyclic(global_heap, n, bsize);
  assert(offset != 0);

  uint64_t csbrk = heap_get_csbrk(global_heap);
  int e = hpx_bcast_rsync(_set_csbrk, &csbrk);
  dbg_check(e, "\n");

  hpx_addr_t addr = offset_to_gpa(here->rank, offset);
  DEBUG_IF(addr == HPX_NULL) {
    dbg_error("should not get HPX_NULL during allocation\n");
  }
  return addr;
}

static int _alloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = pgas_alloc_cyclic_sync(n, bsize);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, pgas_alloc_cyclic, _alloc_cyclic_handler,
              HPX_SIZE_T, HPX_SIZE_T);

/// Allocate zeroed memory from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0) and is
/// broadcast to all of the ranks in the system using hpx_bcast(). It waits for
/// the broadcast to finish before returning. All global cyclic allocations are
/// rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_calloc_cyclic_sync(size_t n, uint32_t bsize) {
  assert(here->rank == 0);

  // Figure out how many blocks ber node that we need, and then allocate that
  // much cyclic space from the heap.
  uint64_t offset = heap_alloc_cyclic(global_heap, n, bsize);

  // We broadcast the csbrk to the system to make sure that people can do
  // effective heap_is_cyclic tests.
  uint64_t csbrk = heap_get_csbrk(global_heap);
  int e = hpx_bcast_rsync(_set_csbrk, &csbrk);
  dbg_check(e, "\n");

  // Broadcast the calloc so that each locality can zero the correct memory.
  e = hpx_bcast_rsync(_calloc_init, &offset, &n, &bsize);
  dbg_check(e, "\n");

  hpx_addr_t addr = offset_to_gpa(here->rank, offset);
  DEBUG_IF(addr == HPX_NULL) {
    dbg_error("should not get HPX_NULL during allocation\n");
  }
  return addr;
}

static int _calloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = pgas_calloc_cyclic_sync(n, bsize);
  return HPX_THREAD_CONTINUE(addr);
}
LIBHPX_ACTION(HPX_DEFAULT, 0, pgas_calloc_cyclic, _calloc_cyclic_handler,
              HPX_SIZE_T, HPX_SIZE_T);


/// This is the hpx_call_* target for doing a calloc initialization.
///
/// This essentially scans through the blocks on a particular rank associated
/// with the offset, and memsets them to 0. We can't just do one large memset
/// because we have alignment issues and may have internal padding.
///
/// @param       offset The base offset of the allocation.
/// @param        bytes The total number of bytes to allocate.
/// @param        bsize The block size.
///
/// @returns HPX_SUCCESS
static int _calloc_init_handler(uint64_t offset, uint32_t bytes, uint32_t bsize)
{
  // Create a global physical address from the offset so that we can perform
  // cyclic address arithmetic on it. This avoids any issues with internal
  // padding, since the addr_add already needs to be able to deal with that
  // correctly.
  //
  // Then compute the gpa for each local block, convert it to an lva, and then
  // memset it.
  uint32_t blocks = ceil_div_64(bytes, here->ranks);
  hpx_addr_t gpa = offset_to_gpa(here->rank, offset);
  for (int i = 0, e = blocks; i < e; ++i) {
    void *lva = pgas_gpa_to_lva(gpa);
    memset(lva, 0, bsize);
    // increment the global address by one cycle
    gpa = hpx_addr_add(gpa, bsize * here->ranks, bsize);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _calloc_init, _calloc_init_handler,
                     HPX_UINT64, HPX_UINT32, HPX_UINT32);

int pgas_free_handler(void) {
  hpx_addr_t gpa = hpx_thread_current_target();
  if (here->rank != gpa_to_rank(gpa)) {
    dbg_error("PGAS free operation for rank %u arrived at rank %u instead.\n",
              gpa_to_rank(gpa), here->rank);
    return HPX_ERROR;
  }
  void *lva = pgas_gpa_to_lva(gpa);
  global_free(lva);
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_DEFAULT, 0, pgas_free, pgas_free_handler);

static int _set_csbrk_handler(size_t offset) {
  int e = heap_set_csbrk(global_heap, offset);
  dbg_check(e, "cyclic allocation ran out of memory at rank %u", here->rank);
  return e;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _set_csbrk, _set_csbrk_handler,
                     HPX_SIZE_T);

static int _memput_rsync_handler(int src, uint64_t command) {
  hpx_addr_t rsync = offset_to_gpa(src, command);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, memput_rsync, _memput_rsync_handler,
                     HPX_INT, HPX_UINT64);
