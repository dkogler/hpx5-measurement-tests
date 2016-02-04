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

#ifndef LIBHPX_NETWORK_ISIR_IRECV_BUFFER_H
#define LIBHPX_NETWORK_ISIR_IRECV_BUFFER_H

#include <hpx/hpx.h>

struct isir_xport;

typedef struct {
  struct isir_xport *xport;
  uint32_t           limit;
  uint32_t            size;
  uint32_t               n;
  uint32_t          UNUSED;
  void           *requests;
  void           *statuses;
  int                 *out;
  struct {
    int              tag;
    hpx_parcel_t *parcel;
  } *records;
} irecv_buffer_t;

/// Initialize an irecv buffer.
///
/// @param       buffer The buffer to initialize.
/// @param        xport The isir xport to use.
/// @param         size The initial size for the buffer.
/// @param        limit The limit of the number of active requests.
///
/// @returns            LIBHPX_OK or an error code.
int irecv_buffer_init(irecv_buffer_t *buffer, struct isir_xport *xport,
                      uint32_t size, uint32_t limit)
  HPX_NON_NULL(1, 2);

/// Finalize an irecv buffer.
///
/// This will cancel all outstanding requests, and free any outstanding parcel
/// buffers.
///
/// @param       buffer The buffer to finalize.
void irecv_buffer_fini(irecv_buffer_t *buffer)
  HPX_NON_NULL(1);

/// Progress an irecv buffer.
///
/// This is a non-blocking call, and uses MPI_Iprobe(), MPI_Irecv(), and
/// MPI_Test{some}() internally, so it will progress MPI. It is not thread
/// safe.
///
/// @param       buffer The buffer to progress.
///
/// @returns            This will return a parcel chain that consists of all of
///                     the parcels that were received in this epoch.
hpx_parcel_t *irecv_buffer_progress(irecv_buffer_t *buffer)
  HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_ISIR_IRECV_BUFFER_H
