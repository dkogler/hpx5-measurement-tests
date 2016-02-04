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
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "registered.h"
#include "xport.h"

static pwc_xport_t *_xport = NULL;

void *dl_mmap_wrapper(size_t length) {
  void *base = system_mmap_huge_pages(NULL, NULL, length, 1);
  if (!base) {
    dbg_error("failed to mmap %zu bytes anywhere in memory\n", length);
  }
  _xport->pin(base, length, NULL);
  log_mem("mapped %zu registered bytes at %p\n", length, base);
  return base;
}

void dl_munmap_wrapper(void *ptr, size_t length) {
  if (!length) {
    return;
  }
  _xport->unpin(ptr, length);
  system_munmap_huge_pages(NULL, ptr, length);
}

void
registered_allocator_init(pwc_xport_t *xport) {
  dbg_assert_str(!_xport, "registered allocator already initialized\n");
  _xport = xport;
  mspaces[AS_REGISTERED] = create_mspace(0, 1);
}
