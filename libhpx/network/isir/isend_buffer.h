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

#ifndef LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H
#define LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H

#include <hpx/hpx.h>

struct isir_xport;

typedef struct {
  struct isir_xport *xport;
  uint32_t  limit;
  uint32_t   twin;
  uint32_t   size;
  uint64_t    min;
  uint64_t active;
  uint64_t    max;
  void  *requests;
  int        *out;
  struct {
    hpx_parcel_t *parcel;
    hpx_addr_t   handler;
  } *records;
} isend_buffer_t;

/// Initialize a send buffer.
///
/// @param       buffer The buffer to initialize.
/// @param        xport The isir xport to use.
/// @param         size The initial size for the buffer.
/// @param        limit The limit of the number of active requests.
/// @param         twin The initial number of requests tested.
///
/// @returns            LIBHPX_OK or an error code.
int isend_buffer_init(isend_buffer_t *buffer, struct isir_xport *xport,
                      uint32_t size, uint32_t limit, uint32_t twin)
  HPX_NON_NULL(1);

/// Finalize a send buffer.
///
/// @param       buffer The buffer to initialize.
void isend_buffer_fini(isend_buffer_t *buffer)
  HPX_NON_NULL(1);

/// Append a send to the buffer.
///
/// This may or may not start the send immediately.
///
/// @param       buffer The buffer to initialize.
/// @param            p The stack of parcels to send.
/// @param            h The handler for local completion.
///
/// @returns  LIBHPX_OK The message was appended successfully.
///        LIBHXP_ERROR There was an error in this operation.
int isend_buffer_append(isend_buffer_t *buffer, hpx_parcel_t *p, hpx_addr_t h)
  HPX_NON_NULL(1,2);

/// Progress the sends in the buffer.
///
/// @param       buffer The buffer to progress().
///
/// @returns            The number of completed requests.
int isend_buffer_progress(isend_buffer_t *buffer)
  HPX_NON_NULL(1);

/// Flush all outstanding sends.
///
/// This is synchronous and will not return until all of the buffered sends and
/// sends in the parcel queue have completed. It is not thread safe.
///
/// @param       buffer The send buffer to flush().
///
/// @returns            The number of completed requests during the flush.
int isend_buffer_flush(isend_buffer_t *buffer)
  HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H
