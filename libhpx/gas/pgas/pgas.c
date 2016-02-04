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
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <hpx/hpx.h>
#include <libhpx/bitmap.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include "cyclic.h"
#include "global.h"
#include "heap.h"
#include "pgas.h"

/// The PGAS type is a global address space that manages a shared heap.
///
/// @note This is sort of silly. The PGAS gas is basically an instance of an
///       object that uses the heap. A more normal approach to this would be to
///       make the heap an instance variable of the PGAS gas subtype. The reason
///       we're currently doing the global_heap as a static is that other files
///       in the pgas module want to interact with the global_heap directly, and
///       we didn't really want to expose the entire PGAS class. For a nicer
///       look, we could adjust this and make everyone in PGAS that needs the
///       heap go through a cast of the locality gas reference, e.g.,
///       (pgas_gas_t*)here->gas->heap.
///
heap_t *global_heap = NULL;

static void _pgas_dealloc(void *gas) {
  if (global_heap) {
    heap_fini(global_heap);
    free(global_heap);
    global_heap = NULL;
  }
}

static bool _gpa_is_cyclic(hpx_addr_t gpa) {
  return heap_offset_is_cyclic(global_heap, gpa_to_offset(gpa));
}

hpx_addr_t pgas_lva_to_gpa(const void *lva) {
  uint64_t offset = heap_lva_to_offset(global_heap, lva);
  return offset_to_gpa(here->rank, offset);
}

void *pgas_gpa_to_lva(hpx_addr_t gpa) {
  uint64_t offset = gpa_to_offset(gpa);
   return heap_offset_to_lva(global_heap, offset);
}

void *pgas_offset_to_lva(uint64_t offset) {
  return heap_offset_to_lva(global_heap, offset);
}

uint64_t pgas_max_offset(void) {
  return (1ull << GPA_OFFSET_BITS);
}

static int64_t
_pgas_sub(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  bool l = _gpa_is_cyclic(lhs);
  bool r = _gpa_is_cyclic(rhs);
  dbg_assert_str(l == r, "cannot compare addresses across allocations.\n");
  dbg_assert_str(!(l ^ r), "cannot compare cyclic with non-cyclic.\n");

  return (l && r) ? gpa_sub_cyclic(lhs, rhs, bsize) : gpa_sub(lhs, rhs);
}

static hpx_addr_t
_pgas_add(const void *gas, hpx_addr_t gpa, int64_t bytes, uint32_t bsize) {
  bool cyclic = _gpa_is_cyclic(gpa);
  return (cyclic) ? gpa_add_cyclic(gpa, bytes, bsize) : gpa_add(gpa, bytes);
}

// Compute a global address for a locality.
static hpx_addr_t _pgas_there(void *gas, uint32_t i) {
  hpx_addr_t there = offset_to_gpa(i, UINT64_MAX);
  if (DEBUG) {
    uint64_t offset = gpa_to_offset(there);
    dbg_assert_str(!heap_contains_offset(global_heap, offset),
                   "HPX_THERE() out of expected range\n");
    (void)offset;
  }
  return there;
}

/// Pin and translate an hpx address into a local virtual address. PGAS
/// addresses don't get pinned, so we're really only talking about translating
/// the address if its local.
static bool _pgas_try_pin(void *gas, hpx_addr_t gpa, void **local) {
  dbg_assert_str(gpa, "cannot pin HPX_NULL\n");

  // we're safe for HPX_HERE/THERE because gpa_to_rank doesn't range-check
  if (gpa_to_rank(gpa) != here->rank) {
    return false;
  }

  // special case messages to "here"
  if (local) {
    *local = (gpa != HPX_HERE) ? pgas_gpa_to_lva(gpa) : &here;
  }

  return true;
}

static void _pgas_unpin(void *gas, hpx_addr_t addr) {
  dbg_assert_str(_pgas_try_pin(gas, addr, NULL),
                 "%"PRIu64" is not local to %u\n", addr, here->rank);
}

static hpx_addr_t _pgas_gas_alloc_cyclic(size_t n, uint32_t bsize,
                                         uint32_t boundary, uint32_t attr) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = pgas_alloc_cyclic_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), pgas_alloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call pgas_alloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

static hpx_addr_t _pgas_gas_calloc_cyclic(size_t n, uint32_t bsize,
                                          uint32_t boundary, uint32_t attr) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = pgas_calloc_cyclic_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), pgas_calloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call pgas_calloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

/// Allocate global blocks from the global heap.
static hpx_addr_t _pgas_gas_alloc_local(size_t n, uint32_t bsize,
                                        uint32_t boundary, uint32_t attr) {
  size_t bytes = n * bsize;
  void *lva = NULL;
  if (boundary) {
    lva = global_memalign(boundary, bytes);
  } else {
    lva = global_malloc(bytes);
  }
  return (lva) ? pgas_lva_to_gpa(lva) : HPX_NULL;
}

/// Allocate global zero-filled blocks from the global heap.
static hpx_addr_t _pgas_gas_calloc_local(size_t n, uint32_t bsize,
                                         uint32_t boundary, uint32_t attr) {
  size_t bytes = n * bsize;
  void *lva = NULL;
  if (boundary) {
    lva = global_memalign(boundary, bytes);
    lva = memset(lva, 0, bytes);
  } else {
    lva = global_calloc(n, bsize);
  }
  return (lva) ? pgas_lva_to_gpa(lva) : HPX_NULL;
}

/// Free a global address.
///
/// This global address must either be the base of a cyclic allocation, or a
/// block allocated by _pgas_gas_alloc_local. At this time, we do not attempt to deal
/// with the cyclic allocations, as they are using a simple csbrk allocator.
static void _pgas_gas_free(void *gas, hpx_addr_t gpa, hpx_addr_t sync) {
  if (gpa == HPX_NULL) {
    return;
  }

  uint64_t offset = gpa_to_offset(gpa);
  void *lva = heap_offset_to_lva(global_heap, offset);
  dbg_assert_str(heap_contains_lva(global_heap, lva),
                 "attempt to free out of bounds offset %"PRIu64"", offset);
  (void)lva;

  if (heap_offset_is_cyclic(global_heap, offset)) {
    heap_free_cyclic(global_heap, offset);
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (gpa_to_rank(gpa) == here->rank) {
    global_free(pgas_gpa_to_lva(offset));
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else {
    dbg_check(hpx_call(gpa, pgas_free, sync), "free failed on %"PRIu64"", gpa);
  }
}

static void _pgas_move(void *gas, hpx_addr_t src, hpx_addr_t dst,
                       hpx_addr_t sync) {
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static size_t _pgas_local_size(void *gas) {
  return global_heap->nbytes;
}

static void *_pgas_local_base(void *gas) {
  return global_heap->base;
}

static uint32_t _pgas_owner_of(const void *pgas, hpx_addr_t addr) {
  return gpa_to_rank(addr);
}

static gas_t _pgas_vtable = {
  .type           = HPX_GAS_PGAS,
  .string = {
    .memget       = pgas_memget,
    .memget_rsync = pgas_memget_rsync,
    .memget_lsync = pgas_memget_lsync,
    .memput       = pgas_memput,
    .memput_lsync = pgas_memput_lsync,
    .memput_rsync = pgas_memput_rsync,
    .memcpy       = pgas_memcpy,
    .memcpy_sync  = pgas_memcpy_sync,
  },
  .dealloc        = _pgas_dealloc,
  .local_size     = _pgas_local_size,
  .local_base     = _pgas_local_base,
  .sub            = _pgas_sub,
  .add            = _pgas_add,
  .there          = _pgas_there,
  .try_pin        = _pgas_try_pin,
  .unpin          = _pgas_unpin,
  .alloc_cyclic   = _pgas_gas_alloc_cyclic,
  .calloc_cyclic  = _pgas_gas_calloc_cyclic,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = _pgas_gas_alloc_local,
  .calloc_local   = _pgas_gas_calloc_local,
  .free           = _pgas_gas_free,
  .move           = _pgas_move,
  .owner_of       = _pgas_owner_of
};

gas_t *gas_pgas_new(const config_t *cfg, boot_t *boot) {
  size_t heap_size = cfg->heapsize;

  if (global_heap) {
    return &_pgas_vtable;
  }

  global_heap = malloc(sizeof(*global_heap));
  if (!global_heap) {
    dbg_error("could not allocate global heap\n");
    return NULL;
  }

  if (heap_init(global_heap, heap_size) != LIBHPX_OK) {
    dbg_error("failed to allocate global heap\n");
    free(global_heap);
    return NULL;
  }

  global_allocator_init();
  if (here->rank == 0) {
    cyclic_allocator_init();
  }

  return &_pgas_vtable;
}
