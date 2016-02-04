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

#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libhpx/network.h>

struct config;
struct gas;

struct gas *gas_pgas_new(const struct config *cfg, struct boot *boot);

/// Called by each OS thread (pthread) to initialize and setup the thread local
/// structures required for it to use PGAS.
///
/// @note This currently only works with the global_heap object. A more flexible
///       solution might be appropriate some day.
///
/// @note Implemented in malloc.c.
///
/// @returns LIBHPX_OK, or LIBHPX_ERROR if there is an error.
int pgas_join(void);

/// Called by each OS thread (pthread) to clean up any thread local structures
/// that were initialized during pgas_join().
///
/// @note Implemented in malloc.c.
void pgas_leave(void);

/// Implementation of the distributed functionality that supports cyclic malloc
/// and calloc.
/// @{

/// Asynchronous entry point for alloc.
/// type hpx_addr_t (size_t bytes, size_t align)
extern HPX_ACTION_DECL(pgas_alloc_cyclic);

/// Asynchronous entry point for calloc.
/// type hpx_addr_t (size_t bytes, size_t align)
extern HPX_ACTION_DECL(pgas_calloc_cyclic);

/// Asynchronous entry point for free.
extern HPX_ACTION_DECL(pgas_free);

/// Synchronous entry point for alloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t pgas_alloc_cyclic_sync(size_t n, uint32_t bsize);

/// Synchronous entry point for calloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t pgas_calloc_cyclic_sync(size_t n, uint32_t bsize);

/// @}

/// Convert a global address into a local virtual address.
///
/// @param          gpa The global physical address.
///
/// @returns            The corresponding local virtual address.
void *pgas_gpa_to_lva(hpx_addr_t gpa);

/// Convert a heap offset into a local virtual address.
///
/// @param       offset The heap offset.
///
/// @returns            The corresponding local virtual address, or NULL if the
///                     offset is outside the bounds of the heap.
void *pgas_offset_to_lva(uint64_t offset);

/// Convert a local address into a global physical address.
///
/// @param          lva The local virtual address.
///
/// @returns            The corresponding local virtual address.
hpx_addr_t pgas_lva_to_gpa(const void *lva);

/// Get the current maximum heap offset.
uint64_t pgas_max_offset(void);

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
int pgas_memget(void *obj, void *to, hpx_addr_t from, size_t size,
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
int pgas_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
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
int pgas_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size);

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
int pgas_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
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
int pgas_memput_lsync(void *obj, hpx_addr_t to, const void *from, size_t size,
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
int pgas_memput_rsync(void *obj, hpx_addr_t to, const void *from, size_t size);

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
int pgas_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size,
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
int pgas_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size);


#endif // LIBHPX_GAS_PGAS_H
