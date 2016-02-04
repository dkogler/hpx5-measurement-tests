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

#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include <hpx/hpx.h>
#include <libhpx/network.h>
#include <libhpx/padding.h>

#include "commands.h"

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct parcel_emulator;
struct pwc_xport;
struct send_buffer;
/// @}

typedef struct {
  network_t                   vtable;
  const struct config           *cfg;
  struct pwc_xport            *xport;
  struct parcel_emulator    *parcels;
  struct send_buffer   *send_buffers;
  struct heap_segment *heap_segments;
  PAD_TO_CACHELINE(sizeof(network_t) +
                   5 * sizeof(void*));
  volatile int probe_lock;
  PAD_TO_CACHELINE(sizeof(int));
  volatile int progress_lock;
  PAD_TO_CACHELINE(sizeof(int));
} pwc_network_t;

extern pwc_network_t *pwc_network;
/// Allocate and initialize a PWC network instance.
network_t *network_pwc_funneled_new(const struct config *cfg, struct boot *boot,
                                    struct gas *gas)
  HPX_MALLOC;

/// Perform an LCO wait operation through the PWC network.
///
/// This performs a wait operation on a remote LCO. It allows the calling thread
/// to wait without an intermediate "proxy" future allocation.
///
/// @param      network The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_wait(void *network, hpx_addr_t lco, int reset);

/// Perform an LCO get operation through the PWC network.
///
/// This performs an LCO get operation on a remote LCO. It allows the calling
/// thread to get an LCO value without having to allocate an intermediate
/// "proxy" future and its associated redundant copies.
///
/// @param          obj The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param            n The size of the output buffer, in bytes.
/// @param          out The output buffer---must be from registered memory.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);

/// Perform a rendezvous parcel send operation.
///
/// For normal size parcels, we use the set of one-to-one pre-allocated eager
/// parcel buffers to put the parcel data directly to the target rank. For
/// larger parcels that will either always overflow the eager buffer, or that
/// will use them up quickly and cause lots of re-allocation synchronization, we
/// use this rendezvous protocol.
///
/// @param      network The network object pointer.
/// @param            p The parcel to send.
///
/// @returns            The status of the operation.
int pwc_rendezvous_send(void *network, const hpx_parcel_t *p);

/// The asynchronous memget operation.
///
/// This operation will return before either the remote or the local operations
/// have completed. The user may specify either an @p lsync or @p rsync LCO to
/// detect the completion of the operations.
///
/// @param          obj The pwc network object.
/// @param           to The local address to memget into.
/// @param         from The global address we're memget-ing from
/// @param         size The number of bytes to get.
/// @param        lsync An LCO to set when @p to has been written.
/// @param        rsync An LCO to set when @p from has been read.
///
/// @returns            HPX_SUCCESS
int pwc_memget(void *obj, void *to, hpx_addr_t from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync);

/// The rsync memget operation.
///
/// This operation will not return until the remote read operation has
/// completed. The @p lsync LCO will be set once the local write operation has
/// completed.
///
/// @param          obj The pwc network object.
/// @param           to The local address to memget into.
/// @param         from The global address we're memget-ing from
/// @param         size The number of bytes to get.
/// @param        lsync An LCO to set when @p to has been written.
///
/// @returns            HPX_SUCCESS
int pwc_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
                     hpx_addr_t lsync);

/// The rsync memget operation.
///
/// This operation will not return until the @p to buffer has been written,
/// which also implies that the remote read has completed.
///
/// @param          obj The pwc network object.
/// @param           to The local address to memget into.
/// @param         from The global address we're memget-ing from
/// @param         size The number of bytes to get.
///
/// @returns            HPX_SUCCESS
int pwc_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size);

/// The asynchronous memput operation.
///
/// The @p lsync LCO will be set when it is safe to reuse or free the @p from
/// buffer. The @p rsync LCO will be set when the remote buffer has been
/// written.
///
/// @param          obj The pwc network object.
/// @param           to The global address to put into.
/// @param         from The local address we're putting from.
/// @param         size The number of bytes to put.
/// @param        lsync An LCO to set when @p from has been read.
/// @param        rsync An LCO to set when @p to has been written.
///
/// @returns            HPX_SUCCESS
int pwc_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync);

/// The locally synchronous memput operation.
///
/// This will not return until it is safe to modify or free the @p from
/// buffer. The @p rsync LCO will be set when the remote buffer has been
/// written.
///
/// @param          obj The pwc network object.
/// @param           to The global address to put into.
/// @param         from The local address we're putting from.
/// @param         size The number of bytes to put.
/// @param        rsync An LCO to set when @p to has been written.
///
/// @returns            HPX_SUCCESS
int pwc_memput_lsync(void *obj, hpx_addr_t to, const void *from, size_t size,
                     hpx_addr_t rsync);

/// The fully synchronous memput operation.
///
/// This will not return until the buffer has been written and is visible at the
/// remote size.
///
/// @param          obj The pwc network object.
/// @param           to The global address to put into.
/// @param         from The local address we're putting from.
/// @param         size The number of bytes to put.
///
/// @returns            HPX_SUCCESS
int pwc_memput_rsync(void *obj, hpx_addr_t to, const void *from, size_t size);

/// The asynchronous memcpy operation.
///
/// This will return immediately, and set the @p sync lco when the operation has
/// completed.
///
/// @param          obj The pwc network object.
/// @param           to The global address to write into.
/// @param         from The global address to read from (const).
/// @param         size The number of bytes to write.
/// @param         sync An optional LCO to signal remote completion.
///
/// @returns            HPX_SUCCESS;
int pwc_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size,
               hpx_addr_t sync);

/// The asynchronous memcpy operation.
///
/// This will not return until the operation has completed.
///
/// @param          obj The pwc network object.
/// @param           to The global address to write into.
/// @param         from The global address to read from (const).
/// @param         size The number of bytes to write.
///
/// @returns            HPX_SUCCESS;
int pwc_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size);

/// Initiate an rDMA get operation.
///
/// This will copy @p n bytes between the @p from buffer and the @p lva, running
/// the @p rcmd when the read completes remotely and running the @p lcmd when
/// the local write is complete.
///
/// @param          obj The network instance to use.
/// @param          lva The local target for the get.
/// @param         from The global source for the get.
/// @param            n The number of bytes to get.
/// @param         lcmd A local command, run when @p lva is written.
/// @param         rcmd A remote command, run when @p from has been read.
///
/// @returns            LIBHPX_OK
int pwc_get(void *obj, void *lva, hpx_addr_t from, size_t n,
            command_t lcmd, command_t rcmd);

/// Initiate an rDMA put operation with a remote continuation.
///
/// This will copy @p n bytes between the @p lca and the @p to buffer, running
/// the @p lcmd when the local buffer can be modified or deleted and the @p rcmd
/// when the remote write has completed.
///
/// @param          obj The transport instance to use.
/// @param           to The global target for the put.
/// @param          lva The local source for the put.
/// @param            n The number of bytes to put.
/// @param         lcmd The local command, run when @p lva can be reused.
/// @param         rcmd The remote command, run when @p to has be written.
///
/// @returns            LIBHPX_OK
int pwc_put(void *obj, hpx_addr_t to, const void *lva, size_t n,
            command_t lcmd, command_t rcmd);

/// Perform a PWC network command.
///
/// This sends a "pure" command to the scheduler at a different rank, without
/// any additional argument data. This can avoid using any additional eager
/// parcel buffer space, and can always be satisfied with one low-level "put"
/// operation.
///
/// @param          obj The network object pointer.
/// @param         rank The rank id to send the remote command to.
/// @param         lcmd A command to be run for local completion.
/// @param         rcmd A remote command to be run at @p rank.
///
/// @returns            The (local) status of the put operation.
int pwc_cmd(void *obj, int rank, command_t lcmd, command_t rcmd);

#endif
