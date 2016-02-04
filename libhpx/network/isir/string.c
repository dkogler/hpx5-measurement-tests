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

#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <libhpx/string.h>

typedef struct {
  void  *to;
  char from[];
} _isir_memget_reply_args_t;

/// This handler implements the memget reply operation.
///
/// This reply is sent to the rank that triggered the request, since the target
/// address isn't a global address. It encodes the target address in the
/// argument type.
///
/// @param         args The marshaled argument type.
/// @param            n The size of the arguments.
///
/// @returns            HPX_SUCCESS
static int _isir_memget_reply_handler(_isir_memget_reply_args_t *args, size_t n)
{
  memcpy(args->to, args->from, n - sizeof(*args));
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_MARSHALLED, _isir_memget_reply,
                     _isir_memget_reply_handler, HPX_POINTER, HPX_SIZE_T);


/// This handler satisfies the memget request operation.
///
/// The purpose of this action is to copy the data to the continuation, and then
/// signal the rsync future once the copy has completed.
///
/// @param         from The pinned buffer we are copying out of.
/// @param           to The local virtual address we're copying back to.
/// @param            n The number of bytes to copy.
/// @param        lsync The lsync LCO to signal completion.
///
/// @returns            HPX_SUCCESS from hpx_thread_continue.
static int _isir_memget_request_handler(const void *from, void *to, size_t n,
                                        hpx_addr_t lsync) {
  // Allocate a generic "large enough" parcel to send the data back to the
  // source. We do it like this in order to avoid a temporary copy of the data.
  hpx_parcel_t *current = self->current;
  size_t          bytes = sizeof(_isir_memget_reply_args_t) + n;
  hpx_addr_t     target = HPX_THERE(current->src);
  hpx_action_t       op = _isir_memget_reply;
  hpx_action_t      rop = hpx_lco_set_action;
  hpx_pid_t         pid = current->pid;

  hpx_parcel_t *p = parcel_new(target, op, lsync, rop, pid, NULL, bytes);
  _isir_memget_reply_args_t *args = hpx_parcel_get_data(p);
  args->to = to;
  memcpy(args->from, from, n);
  parcel_launch(p);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, _isir_memget_request,
                     _isir_memget_request_handler, HPX_POINTER, HPX_POINTER,
                     HPX_SIZE_T, HPX_ADDR);

/// The memget operation.
///
/// The memget operation uses the request/reply mechanism to perform an
/// asynchronous memget. This interface allows the programmer to attach both an
/// rsync LCO to determine when the remote read has completed, and an lsync LCO
/// to determine when the local copy has completed.
static int _isir_memget(void *obj, void *to, hpx_addr_t from, size_t size,
                        hpx_addr_t lsync, hpx_addr_t rsync) {
  hpx_action_t op  = _isir_memget_request;
  hpx_action_t rop = hpx_lco_set_action;
  return action_call_lsync(op, from, rsync, rop, 3, &to, &size, &lsync);
}

/// The memget operation.
///
/// The memget operation uses the request/reply mechanism to perform an
/// asynchronous memget. This version of memget will not return until the remote
/// read has completed.
static int _isir_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
                              hpx_addr_t lsync) {
  hpx_addr_t rsync = hpx_lco_future_new(0);
  dbg_assert(rsync);
  dbg_check( _isir_memget(obj, to, from, size, lsync, rsync) );
  int e = hpx_lco_wait(rsync);
  hpx_lco_delete_sync(rsync);
  return e;
}

/// The synchronous memget handler.
///
/// A synchronous memget doesn't have any rsync to worry about, so we can just
/// continue the data on to the continuation.
///
/// @param         from The pinned target buffer to copy out of.
/// @param            n The number of bytes to copy.
///
/// @returns            HPX_SUCCESS
static int _isir_memget_sync_handler(const void *from, size_t n) {
  return hpx_thread_continue(from, n);
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, _isir_memget_sync,
                     _isir_memget_sync_handler, HPX_POINTER, HPX_SIZE_T);

/// The fully synchronous memget operation.
///
/// This memget operation will not return until the operation has completed
/// locally, which implies that the read has completed on the remote side. This
/// version of memget can use the simplified rsync form of memget without
/// relying on the asynchronous version.
static int _isir_memget_lsync(void *obj, void *to, hpx_addr_t from,
                              size_t size) {
  return action_call_rsync(_isir_memget_sync, from, to, size, 1, &size);
}

/// The memput handler.
///
/// This just copies the passed buffer to the target address.
///
/// @param           to The pinned target buffer to copy into.
/// @param         from The temporary buffer we're copying from.
/// @param            n The size of the memory object being put.
///
/// @returns            HPX_SUCCESS
static int _isir_memput_request_handler(void *to, const void *from, size_t n) {
  memcpy(to, from, n);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
                     _isir_memput_request, _isir_memput_request_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int _isir_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
                        hpx_addr_t lsync, hpx_addr_t rsync) {
  hpx_action_t  op = _isir_memput_request;
  hpx_action_t set = hpx_lco_set_action;
  return action_call_async(op, to, lsync, set, rsync, set, 2, from, size);
}

static int _isir_memput_lsync(void *obj, hpx_addr_t to, const void *from,
                              size_t size, hpx_addr_t rsync) {
  hpx_action_t  op = _isir_memput_request;
  hpx_action_t set = hpx_lco_set_action;
  return action_call_lsync(op, to, rsync, set, 2, from, size);
}

static int _isir_memput_rsync(void *obj, hpx_addr_t to, const void *from,
                              size_t size) {
  return action_call_rsync(_isir_memput_request, to, NULL, 0, 2, from, size);
}

/// The memcpy reply handler.
///
/// The second half of a memcpy is a pinned, marshalled action that writes the
/// data out to the target address.
///
/// @param           to The pinned target buffer we're writing to.
/// @param         data The data we're writing.
/// @param            n The size of the data we're writing.
///
/// @returns            HPX_SUCCESS
static int _isir_memcpy_reply_handler(void *to, const void *data, size_t n) {
  memcpy(to, data, n);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
                     _isir_memcpy_reply, _isir_memcpy_reply_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// The memcpy request handler.
///
/// The first half of a memcpy is a pinned action that copies the data out of
/// the buffer into its continuation. It leverages call_cc to do this entirely
/// within the standard call interface.
///
/// @param         from The pinned buffer we're copying out of.
/// @param            n The number of bytes we want to copy.
/// @param           to The target buffer.
///
/// @returns            HPX_SUCCESS
static int _isir_memcpy_request_handler(const void *from, size_t n,
                                        hpx_addr_t to) {
  return hpx_call_cc(to, _isir_memcpy_reply, from, n);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _isir_memcpy_request,
                     _isir_memcpy_request_handler, HPX_POINTER, HPX_SIZE_T,
                     HPX_ADDR);

static int _isir_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t n,
                        hpx_addr_t rsync) {
  hpx_action_t  op = _isir_memcpy_request;
  hpx_action_t set = hpx_lco_set_action;
  return action_call_lsync(op, from, rsync, set, 2, &n, &to);
}

static int _isir_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from,
                             size_t n) {
  return action_call_rsync(_isir_memcpy_request, from, NULL, 0, 2, &n, &to);
}

const class_string_t isir_string_vtable = {
  .memget       = _isir_memget,
  .memget_rsync = _isir_memget_rsync,
  .memget_lsync = _isir_memget_lsync,
  .memput       = _isir_memput,
  .memput_lsync = _isir_memput_lsync,
  .memput_rsync = _isir_memput_rsync,
  .memcpy       = _isir_memcpy,
  .memcpy_sync  = _isir_memcpy_sync
};
