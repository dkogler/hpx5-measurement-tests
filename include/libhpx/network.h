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

#ifndef LIBHPX_NETWORK_H
#define LIBHPX_NETWORK_H

/// @file include/libhpx/network.h
/// @brief Declare the network_t interface.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/string.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct transport;
/// @}

/// All network objects implement the network interface.
typedef struct network {
  int type;

  const class_string_t * string;

  void (*delete)(void*);

  int (*progress)(void*, int);

  int (*send)(void*, hpx_parcel_t *p);

  int (*lco_wait)(void *, hpx_addr_t lco, int reset);
  int (*lco_get)(void *, hpx_addr_t lco, size_t n, void *to, int reset);

  hpx_parcel_t *(*probe)(void*, int nrx);

  void (*flush)(void*);

  void (*register_dma)(void *, const void *base, size_t bytes, void *key);
  void (*release_dma)(void *, const void *base, size_t bytes);
} network_t;

/// Create a new network.
///
/// This depends on the current boot and transport object to be configured in
/// the "here" locality.
///
/// @param          cfg The current configuration.
/// @param         boot The bootstrap network object.
/// @param          gas The global address space.
///
/// @returns            The network object, or NULL if there was an issue.
network_t *
network_new(struct config *cfg, struct boot *boot, struct gas *gas)
  HPX_MALLOC;

/// Delete a network object.
///
/// This does not synchronize. The caller is required to ensure that no other
/// threads may be operating on the network before making this call.
///
/// @param      obj The network to delete.
static inline void
network_delete(void *obj) {
  network_t *network = obj;
  network->delete(network);
}

/// Perform one network progress operation.
///
/// This is not synchronized at this point, and must be synchronized
/// externally.
///
/// @param      obj The network to start.
/// @param       id The id to use when progressing the network.
///
/// @returns  LIBHPX_OK The network was progressed without error.
static inline int
network_progress(void *obj, int id) {
  network_t *network = obj;
  assert(network);
  return network->progress(network, id);
}

/// Initiate a parcel send over the network.
///
/// This is an asynchronous interface, the parcel will be sent at some point in
/// the future, and is guaranteed to be delivered. Delivery order is not
/// guaranteed. This transfers ownership of the parcel @p p, the client should
/// not access the sent parcel again---in particular, the client should not
/// release the parcel.
///
/// This call will block if there are not enough resources available to satisfy
/// the enqueue. It may progress the network as well, though that behavior is
/// not guaranteed.
///
/// @todo There is currently no way to test for send completion. We should add a
///       future parameter so that the sender can wait if necessary.
///
/// @param          obj The network to use for the send.
/// @param            p The parcel to send.
///
/// @returns  LIBHPX_OK The send was buffered successfully
static inline int
network_send(void *obj, hpx_parcel_t *p) {
  network_t *network = obj;
  return network->send(network, p);
}

/// Probe for received parcels.
static inline hpx_parcel_t *
network_probe(void *obj, int rank) {
  network_t *network = obj;
  return network->probe(network, rank);
}

/// Register a memory region for dma access.
///
/// Network registration is a limited resource. Currently, we handle
/// registration failures as unrecoverable. In the future it will make sense to
/// implement a registration cache or other mechanism for resource management.
///
/// @param          obj The network object.
/// @param         base The beginning of the region to register.
/// @param        bytes The number of bytes to register.
/// @param          key The key to use when registering dma.
static inline void
network_register_dma(void *obj, const void *base, size_t bytes, void *key) {
  network_t *network = obj;
  network->register_dma(network, base, bytes, key);
}

/// Release a registered memory region.
///
/// The region denotated by @p segment, @p bytes must correspond to a region
/// previously registered.
///
/// @param          obj The network object.
/// @param         base The beginning of the region to release.
/// @param        bytes The number of bytes to release.
static inline void
network_release_dma(void *obj, const void *base, size_t bytes) {
  network_t *network = obj;
  network->release_dma(network, base, bytes);
}

/// Perform an LCO get operation through the network.
static inline int
network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset) {
  network_t *network = obj;
  return network->lco_get(network, lco, n, out, reset);
}

/// Perform an LCO wait operation through the network.
static inline int
network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  network_t *network = obj;
  return network->lco_wait(network, lco, reset);
}

static inline int network_memget(void *obj, void *to, hpx_addr_t from,
                                 size_t size, hpx_addr_t lsync,
                                 hpx_addr_t rsync) {
  network_t *network = obj;
  return network->string->memget(network, to, from, size, lsync, rsync);
}

static inline int network_memget_rsync(void *obj, void *to, hpx_addr_t from,
                                       size_t size, hpx_addr_t lsync) {
  network_t *network = obj;
  return network->string->memget_rsync(network, to, from, size, lsync);
}

static inline int network_memget_lsync(void *obj, void *to, hpx_addr_t from,
                                       size_t size) {
  network_t *network = obj;
  return network->string->memget_lsync(network, to, from, size);
}

static inline int network_memput(void *obj, hpx_addr_t to, const void *from,
                                 size_t size, hpx_addr_t lsync,
                                 hpx_addr_t rsync) {
  network_t *network = obj;
  return network->string->memput(network, to, from, size, lsync, rsync);
}

static inline int network_memput_lsync(void *obj, hpx_addr_t to,
                                       const void *from, size_t size,
                                       hpx_addr_t rsync) {
  network_t *network = obj;
  return network->string->memput_lsync(network, to, from, size, rsync);
}

static inline int network_memput_rsync(void *obj, hpx_addr_t to,
                                       const void *from, size_t size) {
  network_t *network = obj;
  return network->string->memput_rsync(network, to, from, size);
}

static inline int network_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from,
                                 size_t size, hpx_addr_t sync) {
  network_t *network = obj;
  // use this call syntax do deal with issues on darwin with the memcpy symbol
  return (*network->string->memcpy)(network, to, from, size, sync);
}

static inline int network_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from,
                                      size_t size) {
  network_t *network = obj;
  return network->string->memcpy_sync(network, to, from, size);
}

#endif // LIBHPX_NETWORK_H
