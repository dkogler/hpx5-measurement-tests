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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include "circular_buffer.h"

/// Compute the index into the buffer for an abstract index.
///
/// The size must be 2^k because we mask instead of %.
///
/// @param            i The abstract index.
/// @param     capacity The capacity of the buffer, in # elements.
///
/// @returns             @p capacity = 2^k; @p i % @p capacity
///                      @p capacity != 2^k; undefined
static int _index_of(uint32_t i, uint32_t capacity) {
  DEBUG_IF(capacity != (1u << ceil_log2_32(capacity))) {
    dbg_error("capacity %u is not 2^k\n", capacity);
  }
  uint32_t mask = capacity - 1;
  return (i & mask);
}

/// Compute the address of an element in the buffer.
///
/// @param       buffer The buffer itself.
/// @param            i The element index.
///
/// @returns            The address of the element in the buffer.
static void *_address_of(circular_buffer_t *buffer, uint32_t i) {
  size_t bytes = i * buffer->element_size;
  return (char*)buffer->records + bytes;
}

/// Reflow the elements in a buffer.
///
/// After resizing a buffer, existing elements are likely to be in the wrong
/// place, given the new size. This function will reflow the part of the buffer
/// that is in the wrong place.
///
/// @param       buffer The buffer to reflow.
/// @param old_capacity The size of the buffer prior to the resize operation.
///
/// @returns  LIBHPX_OK The reflow was successful.
///        LIBHPX_ERROR An unexpected error occurred.
static int _reflow(circular_buffer_t *buffer, uint32_t old_capacity) {
  if (!old_capacity) {
    return LIBHPX_OK;
  }

  // Resizing the buffer changes where our index mapping is, we need to move
  // data around in the arrays. We do that by memcpy-ing either the prefix or
  // suffix of a wrapped buffer into the new position. After resizing the buffer
  // should never be wrapped.
  int old_min = _index_of(buffer->min, old_capacity);
  int old_max = _index_of(buffer->max, old_capacity);
  int prefix = (old_min < old_max) ? old_max - old_min : old_capacity - old_min;
  int suffix = (old_min < old_max) ? 0 : old_max;

  uint32_t new_capacity = buffer->capacity;
  int new_min = _index_of(buffer->min, new_capacity);
  int new_max = _index_of(buffer->max, new_capacity);

  // This code is slightly tricky. We only need to move one of the ranges,
  // either [old_min, old_min + prefix) or [0, suffix). We determine which range
  // we need to move by seeing if the min or max index is different in the new
  // buffer, and then copying the appropriate bytes of the requests and records
  // arrays to the right place in the new buffer.
  if (old_min == new_min) {
    assert(old_max != new_max);
    assert(old_min + prefix == new_max - suffix);

    // copy the old suffix to the new location
    size_t bytes = suffix * buffer->element_size;
    void *to = _address_of(buffer, old_min + prefix);
    const void *from = _address_of(buffer, 0);
    memcpy(to, from, bytes);
  }
  else if (old_max == new_max) {
    assert(new_min + prefix <= new_capacity);

    // copy the prefix to the new location
    size_t bytes = prefix * buffer->element_size;
    void *to = _address_of(buffer, new_min);
    const void *from = _address_of(buffer, old_min);
    memcpy(to, from, bytes);
  }
  else {
    return log_error("unexpected shift\n");
  }

  log_net("reflowed a circular buffer from %u to %u\n", old_capacity,
              new_capacity);
  return LIBHPX_OK;
}

/// Expand the capacity of a buffer.
///
/// @param       buffer The buffer to expand.
/// @param     capacity The new size for the buffer.
///
/// @returns  LIBHPX_OK The buffer was expanded successfully.
///       LIBHPX_ENOMEM We ran out of memory during expansion.
///        LIBHPX_ERROR There was an unexpected error during expansion.
static int _expand(circular_buffer_t *buffer, uint32_t capacity) {
  assert(capacity != 0);

  if (capacity < buffer->capacity) {
    return log_error("cannot shrink a circular buffer\n");
  }

  if (capacity == buffer->capacity) {
    return LIBHPX_OK;
  }

  // realloc a new records array
  uint32_t old_capacity = buffer->capacity;
  buffer->capacity = capacity;
  buffer->records = realloc(buffer->records, capacity * buffer->element_size);
  if (!buffer->records) {
    log_error("failed to resize a circular buffer (%u to %u)\n", old_capacity,
              capacity);
    return LIBHPX_ENOMEM;
  }

  // the reallocated buffer has some of its elements in the wrong place, so we
  // need to reflow it
  return _reflow(buffer, old_capacity);
}

int circular_buffer_init(circular_buffer_t *b, uint32_t element_size,
                         uint32_t capacity)
{
  b->capacity = 0;
  b->element_size = element_size;
  b->min = 0;
  b->max = 0;
  b->records = NULL;

  capacity = 1 << ceil_log2_32(capacity);
  return _expand(b, capacity);
}

void circular_buffer_fini(circular_buffer_t *b) {
  if (b->records) {
    free(b->records);
  }
}

uint32_t circular_buffer_size(circular_buffer_t *b) {
  uint64_t size = b->max - b->min;
  dbg_assert_str(size < UINT32_MAX, "circular buffer size invalid\n");
  return (uint32_t)size;
}

void *circular_buffer_append(circular_buffer_t *buffer) {
  uint64_t next = buffer->max++;
  if (circular_buffer_size(buffer) >= buffer->capacity) {
    uint32_t capacity = buffer->capacity * 2;
    if (LIBHPX_OK != _expand(buffer, capacity)) {
      dbg_error("could not expand a circular buffer from %u to %u\n",
                buffer->capacity, capacity);
    }
  }
  uint32_t i = _index_of(next, buffer->capacity);
  return _address_of(buffer, i);
}

int circular_buffer_progress(circular_buffer_t *buffer,
                             int (*progress_callback)(void *env, void *record),
                             void *progress_env) {
  while (buffer->min < buffer->max) {
    uint32_t i = _index_of(buffer->min, buffer->capacity);
    void *record = _address_of(buffer, i);
    int e = progress_callback(progress_env, record);
    switch (e) {
     default:
      dbg_error("circular buffer could not progress\n");

     case LIBHPX_RETRY:
      return circular_buffer_size(buffer);

     case LIBHPX_OK:
      ++buffer->min;
      break;
    }
  }
  return 0;
}
