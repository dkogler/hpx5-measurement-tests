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

#ifndef LIBHPX_NETWORK_PWC_SEND_BUFFER_H
#define LIBHPX_NETWORK_PWC_SEND_BUFFER_H

#include <hpx/hpx.h>
#include <libsync/locks.h>
#include "circular_buffer.h"

struct parcel_emulator;
struct pwc_xport;

typedef struct send_buffer {
  struct tatas_lock       lock;
  int                     rank;
  int           UNUSED_PADDING;
  struct parcel_emulator *emul;
  struct pwc_xport      *xport;
  circular_buffer_t    pending;
} send_buffer_t;

/// Initialize a send buffer.
int send_buffer_init(send_buffer_t *sends, int rank,
                     struct parcel_emulator *emul, struct pwc_xport *xport,
                     uint32_t size);

/// Finalize a send buffer.
void send_buffer_fini(send_buffer_t *sends);

/// Perform a parcel send operation.
///
/// The parcel send is a fundamentally asynchronous operation. If @p lsync is
/// not HPX_NULL then it will be signaled when the local send operation
/// completes at the network level.
///
/// This send operation must be properly synchronized with other sends, and with
/// send_buffer_progress() since they may be called concurrently from more than
/// one thread.
///
/// NB: We don't currently support the @p lsync operation. When a send
///     completes, it generates a local completion event that is returned
///     through a local probe (local progress) operation. This completion event
///     will delete the parcel.
///
/// @param        sends The send buffer.
/// @param        lsync An event to signal when the send completes locally.
/// @param            p The parcel to send.
///
/// @returns  LIBHPX_OK The send operation was successful (i.e., it was passed
///                       to the network or it was buffered).
///       LIBHPX_ENOMEM The send operation could not complete because it needed
///                       to be buffered but the system was out of memory.
///        LIBHPX_ERROR An unexpected error occurred.
int send_buffer_send(send_buffer_t *sends, hpx_addr_t lsync,
                     const hpx_parcel_t *p);

int send_buffer_progress(send_buffer_t *sends);

#endif // LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
