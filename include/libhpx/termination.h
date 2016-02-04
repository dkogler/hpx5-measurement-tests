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
#ifndef LIBHPX_TERMINATION_H
#define LIBHPX_TERMINATION_H

/// ----------------------------------------------------------------------------
/// @file include/libhpx/termination.h
/// ----------------------------------------------------------------------------
#include <stdint.h>
#include <hpx/attributes.h>

/// Credit-Recovery for detecting termination using a variant of
/// Mattern's credit-recovery scheme as described in:
/// "Global Quiescence Detection Based On Credit Distribution and
/// Recovery".

// a bit-vector word
typedef uint64_t _bitmap_word_t;

// a bit-vector page
typedef _bitmap_word_t *_bitmap_page_t;

// number of bits in a bitmap word
#define _bitmap_word_size  (sizeof(_bitmap_word_t) * CHAR_BIT)

// default number of words in a page
#define _bitmap_num_words 1024

// size of a bitmap page (in bits)
#define _bitmap_page_size  (_bitmap_num_words * _bitmap_word_size)

// default number of pages (of _bitmap_num_words) at initialization
#define _bitmap_num_pages 512

// the bitmap structure used for credit tracking
typedef struct {
  _bitmap_page_t volatile(page);
} bitmap_t;

///

bitmap_t *cr_bitmap_new(void) HPX_MALLOC;
void cr_bitmap_delete(bitmap_t *b) HPX_NON_NULL(1);
uint64_t cr_bitmap_add_and_test(bitmap_t *b, int64_t i) HPX_NON_NULL(1);

#endif // LIBHPX_TERMINATION_H
