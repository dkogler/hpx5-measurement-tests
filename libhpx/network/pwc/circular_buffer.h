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

#ifndef LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
#define LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H

#include <stdint.h>
#include <hpx/attributes.h>

typedef struct {
  uint32_t     capacity;
  uint32_t element_size;
  uint64_t          max;
  uint64_t          min;
  void  *records;
} circular_buffer_t;


/// Initialize a circular buffer.
///
/// @param            b The buffer to initialize.
/// @param        esize The size of a buffer element.
/// @param          cap The initial buffer capacity.
///
/// @returns  LIBHPX_OK The buffer was initialized successfully.
///       LIBHPX_ENOMEM There was not enough memory for @p cap elements.
///        LIBHPX_ERROR There was an unexpected error.
int circular_buffer_init(circular_buffer_t *b, uint32_t esize, uint32_t cap)
  HPX_NON_NULL(1);

/// Finalize a circular buffer.
void circular_buffer_fini(circular_buffer_t *b)
  HPX_NON_NULL(1);

/// Append an element to the circular buffer.
///
/// @param            b The buffer to append to.
///
/// @returns       NULL There was an error appending to the buffer.
///            NON-NULL The address of the start of the next record in the
///                       buffer.
void *circular_buffer_append(circular_buffer_t *b)
  HPX_NON_NULL(1);

/// Compute the number of elements in the buffer.
uint32_t circular_buffer_size(circular_buffer_t *b)
  HPX_NON_NULL(1);

/// Apply the callback closure to each element in the buffer, in a FIFO order.
///
///
/// @returns            The number of elements left in the buffer.
int circular_buffer_progress(circular_buffer_t *b,
                             int (*progress_callback)(void *env, void *record),
                             void *progress_env)
  HPX_NON_NULL(1, 2);

#endif // LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
