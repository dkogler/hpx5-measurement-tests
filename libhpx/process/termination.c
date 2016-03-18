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
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <libsync/sync.h>
#include <libsync/lockable_ptr.h>
#include <libhpx/debug.h>
#include <libhpx/termination.h>

lockable_ptr_t ptr;

void _bitmap_bounds_check(bitmap_t *b, uint32_t page, uint32_t word) {
  assert(word < _bitmap_num_words);

  // check if the page has been allocated or not
  for (;;) {
    _bitmap_page_t p = sync_load(&b[page].page, SYNC_ACQUIRE);
    if (!p) {
      void *newp = NULL;
      int e = posix_memalign((void**)&newp, HPX_CACHELINE_SIZE,
                             _bitmap_num_words * sizeof(_bitmap_word_t));
      if (e) {
        dbg_error("failed to allocate a page for %u words\n",
                  _bitmap_num_words);
      }
      memset(newp, 0, _bitmap_num_words * sizeof(_bitmap_word_t));

      if (!sync_cas(&b[page].page, &p, newp, SYNC_RELEASE, SYNC_RELAXED)) {
        free(newp);
        // try again..
        continue;
      } else {
        // move on to the previous page
        page--;
        continue;
      }
    }
    break;
  }
}


bitmap_t *cr_bitmap_new(void) {
  // allocate _bitmap_num_pages
  bitmap_t *b = NULL;
  int e = posix_memalign((void**)&b, HPX_CACHELINE_SIZE,
                         _bitmap_num_pages * sizeof(_bitmap_page_t));
  if (e) {
    dbg_error("failed to allocate a bitmap for %u pages\n", _bitmap_num_pages);
  }
  memset(b, 0, _bitmap_num_pages * sizeof(_bitmap_page_t));

  // allocate the first page
  void *p = NULL;
  e = posix_memalign((void**)&p, HPX_CACHELINE_SIZE,
                     _bitmap_num_words * sizeof(_bitmap_word_t));
  if (e) {
    dbg_error("failed to allocate a page for %u words\n", _bitmap_num_words);
  }

  memset(p, 0, _bitmap_num_words * sizeof(_bitmap_word_t));
  sync_store(&b[0].page, p, SYNC_RELEASE);
  return b;
}


void cr_bitmap_delete(bitmap_t *b) {
  // delete all pages
  for (int i = 0; i < _bitmap_num_pages; ++i) {
    _bitmap_page_t p = sync_load(&b[i].page, SYNC_ACQUIRE);
    if (p) {
      free((void*)p);
    }
  }
  free(b);
}


void _bitmap_add_at(bitmap_t *b, uint32_t page, uint32_t word, uint32_t offset) {
  _bitmap_bounds_check(b, page, word);
  uint64_t mask = (UINT64_C(1) << offset);

  _bitmap_word_t old = sync_fadd(&(b[page].page[word]), mask, SYNC_ACQ_REL);
  // if there was an overflow, add to the previous word
  if ((old + mask) < mask) {
    if (word == 0) {
      if (page > 0) {
        page--;
      }
      word = _bitmap_num_words;
    }
    _bitmap_add_at(b, page, word-1, 0);
  }
}

// set a bit at index i and then test if the bit at the 0th
// position (page 0 word 0) is set
uint64_t cr_bitmap_add_and_test(bitmap_t *b, int64_t i) {
  if (!i) {
    return 0;
  }
  uint32_t page      = (i-1)/_bitmap_num_pages;
  uint32_t pg_offset = (i-1)%_bitmap_page_size;
  uint32_t word      = pg_offset/_bitmap_word_size;

  int word_offset = (_bitmap_word_size-1) - (pg_offset%_bitmap_word_size);
  _bitmap_add_at(b, page, word, word_offset);
  return (uint64_t)sync_load(&b[0].page[0], SYNC_ACQUIRE);
}
