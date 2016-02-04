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
#include <string.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "isend_buffer.h"
#include "parcel_utils.h"
#include "xport.h"

#define ISIR_TWIN_INC  10

/// Compute the buffer index of an abstract index.
///
/// @param            i The abstract index.
/// @param            n The size of the buffer (must be 2^k).
///
/// @returns            The index in the buffer for this abstract index.
static int _index_of(uint64_t i, uint32_t n) {
  return (i & (n - 1));
}

/// Figure out what tag I'm supposed to use for a particular payload size.
///
/// @param        isends The send buffer.
/// @param       payload The payload size.
///
/// @returns             The correct tag.
static int _payload_size_to_tag(isend_buffer_t *isends, uint32_t payload) {
  uint32_t parcel_size = payload + sizeof(hpx_parcel_t);
  int tag = ceil_div_32(parcel_size, HPX_CACHELINE_SIZE);
  if (DEBUG) {
    isends->xport->check_tag(tag);
  }
  return tag;
}

static void *_request_at(isend_buffer_t *buffer, int i) {
  dbg_assert(i >= 0);
  char *base = buffer->requests;
  int bytes = i * buffer->xport->sizeof_request();
  return base + bytes;
}

static void *_record_at(isend_buffer_t *buffer, int i) {
  dbg_assert(i >= 0);
  return buffer->records + i;
}

/// Re-size an isend buffer to the requested size.
///
/// Buffer sizes can only be increased in the current implementation. The size
/// must be a power of 2.
///
/// @param       buffer The buffer to re-size.
/// @param         size The new size.
///
/// @returns  LIBHPX_OK The buffer was resized correctly.
///       LIBHPX_ENOMEM There was not enough memory to resize the buffer.
static int _resize(isend_buffer_t *buffer, uint32_t size) {
  dbg_assert_str(size >= buffer->size, "cannot shrink send buffer\n");

  if (size == buffer->size) {
    return LIBHPX_OK;
  }

  // start by resizing the buffers
  uint32_t oldsize = buffer->size;
  buffer->size = size;
  buffer->requests = realloc(buffer->requests, size * buffer->xport->sizeof_request());
  buffer->out = realloc(buffer->out, size * sizeof(int));
  buffer->records = realloc(buffer->records, size * sizeof(*buffer->records));

  if (!buffer->requests || !buffer->out || !buffer->records) {
    return log_error("failed to resize a send buffer from %u to %u\n",
                     oldsize, size);
  }

  int n = buffer->max - buffer->min;
  if (!n) {
    goto exit;
  }

  // Resizing the buffer changes where our index mapping is, we need to move
  // data around in the arrays. We do that by memcpy-ing either the prefix or
  // suffix of a wrapped buffer into the new position. After resizing the buffer
  // should never be wrapped.
  int min = _index_of(buffer->min, oldsize);
  int max = _index_of(buffer->max, oldsize);
  int prefix = (min < max) ? max - min : oldsize - min;
  int suffix = (min < max) ? 0 : max;

  int nmin = _index_of(buffer->min, size);
  int nmax = _index_of(buffer->max, size);

  // This code is slightly tricky. We only need to move one of the ranges,
  // either [min, oldsize) or [0, max). We determine which range we need to move
  // by seeing if the min or max index is different in the new buffer, and then
  // copying the appropriate bytes of the requests and records arrays to the
  // right place in the new buffer.
  if (min == nmin) {
    assert(max != nmax);
    assert(min + prefix == nmax - suffix);
    {
      size_t bytes = suffix * buffer->xport->sizeof_request();
      void *to = _request_at(buffer, min + prefix);
      memcpy(to, buffer->requests, bytes);
    }
    {
      size_t bytes = suffix * sizeof(*buffer->records);
      void *to = _record_at(buffer, min + prefix);
      memcpy(to, buffer->records, bytes);
    }
  }
  else if (max == nmax) {
    assert(0 < prefix);
    assert(nmin + prefix <= size);
    {
      size_t bytes = prefix * buffer->xport->sizeof_request();
      void *to = _request_at(buffer, nmin);
      void *from = _request_at(buffer, min);
      memcpy(to, from, bytes);
    }
    {
      size_t bytes = prefix * sizeof(*buffer->records);
      void *to = _record_at(buffer, nmin);
      void *from = _record_at(buffer, min);
      memcpy(to, from, bytes);
    }
  }
  else {
    return log_error("unexpected shift in isend buffer _resize\n");
  }

 exit:
  log_net("resized a send buffer from %u to %u\n", oldsize, size);
  return LIBHPX_OK;
}

/// Start an isend operation.
///
/// A precondition of this function is that there must be a valid entry in the 
/// buffer that is not yet active.
///
/// @param       isends The buffer to start the send from.
/// @param            i The index to start.
///
/// @returns  LIBHPX_OK success
///        LIBHPX_ERROR MPI error
static int _start(isend_buffer_t *isends, int i) {
  assert(0 <= i && i < isends->size);

  hpx_parcel_t *p = isends->records[i].parcel;
  void *from = isir_network_offset(p);
  int to = gas_owner_of(here->gas, p->target);
  int n = payload_size_to_isir_bytes(p->size);
  int tag = _payload_size_to_tag(isends, p->size);
  void *r = _request_at(isends, i);
  return isends->xport->isend(to, from, n, tag, r);
}

/// Start as many isend operations as we can.
///
/// @param       isends The buffer from which to start isends.
///
/// @returns            The number of sends that remain unstarted.
int _start_all(isend_buffer_t *isends) {
  uint32_t size = isends->size;
  uint64_t max = isends->max;
  uint64_t limit = isends->limit;
  uint64_t end = (limit) ? min_u64(isends->min + limit, max) : max;

  for (uint64_t i = isends->active; i < end; ++i) {
    int j = _index_of(i, size);
    int e = _start(isends, j);
    dbg_check(e, "Failed to start an Isend operation.\n");
  }
  isends->active = end;
  return (max - end);
}

/// Test a contiguous range of the buffer.
///
/// This performs a single MPI_Testsome() on a range of requests covering
/// [i, i + n), and calls _finish() for each request that is completed.
///
/// @param       buffer The buffer to test.
/// @param            i The physical index at which the range starts.
/// @param            n The number of sends to test.
/// @param            o The index added to buffer->out.
///
/// @returns            The number of completed requests in this range.
static int _test_range(isend_buffer_t *buffer, uint32_t i, uint32_t n, int o) {
  assert(0 <= i && i + n <= buffer->size);

  if (n == 0)
    return 0;

  int cnt = 0;
  void *requests = _request_at(buffer, i);
  int *out = buffer->out + o;
  buffer->xport->testsome(n, requests, &cnt, out, NULL);

  for (int j = 0; j < cnt; ++j) {
    out[j] += i;
    int k = out[j];
    assert(i <= k && k < i + n);

    // handle each of the completed requests
    parcel_delete(buffer->records[k].parcel);
    hpx_gas_free(buffer->records[k].handler, HPX_NULL);
  }

  return cnt;
}

/// Compact the buffer after testing.
///
/// @param       buffer The buffer to compact.
/// @param            n The number of valid entries in buffer->out.
static void _compact(isend_buffer_t *buffer, int n) {
  uint32_t size = buffer->size;
  uint64_t m = buffer->active - buffer->min;
  if (n == m) {
    log_net("bulk compaction of %d/%"PRIu64" sends in buffer (%u)\n", n, m,
                size);
    buffer->min += n;
    return;
  }

  // incremental compaction
  for (int i = 0; i < n; ++i) {
    int j = buffer->out[i];
    DEBUG_IF(true) {
      void *request = _request_at(buffer, j);
      buffer->xport->clear(request);
    }

    uint64_t min = buffer->min++;
    int k = _index_of(min, size);
    void *to = _request_at(buffer, j);
    const void *from = _request_at(buffer, k);
    memmove(to, from, buffer->xport->sizeof_request());
    buffer->records[j] = buffer->records[k];
  }

  log_net("incremental compaction of %d/%"PRIu64" sends in buffer (%u)\n", n, m,
              size);
}


/// Test all of the active isend operations.
///
/// The isend buffer is a standard circular buffer, so we need to test one or
/// two ranges, depending on if the buffer is currently wrapped.
///
/// @param       buffer The buffer to test.
///
/// @returns            The number of sends completed.
static int _test_all(isend_buffer_t *buffer) {
  uint32_t twin = buffer->twin;
  uint32_t size = buffer->size;
  uint32_t i = _index_of(buffer->min, size);
  uint32_t j = _index_of(buffer->active, size);

  // might have to test two ranges, [i, size) and [0, j), or just [i, j)
  bool wrapped = (j < i);
  uint32_t n = (wrapped) ? buffer->size - i : j - i;
  uint32_t m = (wrapped) ? j : 0;

  // limit how many requests we test
  n = (n > twin) ? twin : n;
  m = (m > (twin - n)) ? (twin - n) : m;

  int total = 0;
  total += _test_range(buffer, i, n, total);
  total += _test_range(buffer, 0, m, total);
  if (total) {
    _compact(buffer, total);
  }
  if (total >= twin) {
    buffer->twin += ISIR_TWIN_INC;
    log_net("increased test window to %d\n", buffer->twin);
  }
  else if ((twin - total) > ISIR_TWIN_INC) {
    buffer->twin -= ISIR_TWIN_INC;
    log_net("decreased test window to %d\n", buffer->twin);
  }

  DEBUG_IF (total) {
    log_net("tested %u sends, completed %d\n", n+m, total);
  }
  return total;
}


/// Cancel an active request.
///
/// This is synchronous, and will wait until the request has been canceled.
///
/// @param       buffer The buffer.
/// @param            i The index to cancel.
///
/// @returns  LIBHPX_OK The request was successfully canceled.
///        LIBHPX_ERROR Three was an error during the operation.
static int _cancel(isend_buffer_t *buffer, int i) {
  assert(0 <= i && i < buffer->size);

  void *request = _request_at(buffer, i);
  int e = buffer->xport->cancel(request, NULL);
  if (LIBHPX_OK != e) {
    return e;
  }

  if (buffer->records) {
    hpx_gas_free(buffer->records[i].handler, HPX_NULL);
    parcel_delete(buffer->records[i].parcel);
  }
  return LIBHPX_OK;
}

/// Cancel and cleanup all outstanding requests in the buffer.
///
/// @param       buffer The buffer.
static void _cancel_all(isend_buffer_t *buffer) {
  if (!buffer->requests) {
    return;
  }

  uint32_t size = buffer->size;
  for (uint64_t i = buffer->min, e = buffer->active; i < e; ++i) {
    if (LIBHPX_OK != _cancel(buffer, _index_of(i, size))) {
      log_error("failed to cancel pending send\n");
    }
  }
}

int isend_buffer_init(isend_buffer_t *buffer, isir_xport_t *xport,
                      uint32_t size, uint32_t limit, uint32_t twin) {
  buffer->xport = xport;
  buffer->limit = limit;
  buffer->size = 0;
  buffer->min = 0;
  buffer->active = 0;
  buffer->max = 0;
  buffer->requests = NULL;
  buffer->out = NULL;
  buffer->records = NULL;
  buffer->twin = twin;

  size = 1 << ceil_log2_32(size);
  int e = _resize(buffer, size);
  if (LIBHPX_OK != e) {
    isend_buffer_fini(buffer);
  }
  return e;
}

void isend_buffer_fini(isend_buffer_t *buffer) {
  dbg_assert(buffer);

  _cancel_all(buffer);

  if (buffer->records) {
    free(buffer->records);
  }

  if (buffer->out) {
    free(buffer->out);
  }

  if (buffer->requests) {
    free(buffer->requests);
  }
}

int isend_buffer_append(isend_buffer_t *buffer, hpx_parcel_t *p, hpx_addr_t h) {
  uint64_t i = buffer->max++;
  uint32_t size = buffer->size;
  if (size <= buffer->max - buffer->min) {
    size = 2 * size;
    if (LIBHPX_OK != _resize(buffer, size)) {
      return log_error("failed to resize isend buffer during append\n");
    }
  }

  int j = _index_of(i, size);
  assert(0 <= j && j < buffer->size);
  if (DEBUG) {
    void *request = _request_at(buffer, j);
    buffer->xport->clear(request);
  }
  buffer->records[j].parcel = p;
  buffer->records[j].handler = h;
  return LIBHPX_OK;
}


int isend_buffer_flush(isend_buffer_t *buffer) {
  int total = 0;
  do {
    total += isend_buffer_progress(buffer);
  } while (buffer->min != buffer->max);
  return total;
}


int isend_buffer_progress(isend_buffer_t *isends) {
  int m = _test_all(isends);
  DEBUG_IF (m) {
    log_net("finished %d sends\n", m);
  }

  int n = _start_all(isends);
  DEBUG_IF (n) {
    log_net("failed to start %d sends\n", n);
  }

  // avoid unused errors.
  (void)n;

  return m;
}
