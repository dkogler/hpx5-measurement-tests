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

/// User level stacks.
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

#include <hpx/builtins.h>
#include <valgrind/valgrind.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "thread.h"

/// The size of the thread structure. This is set during initialization based on
/// the current configuration.
/// @{
static int _thread_size = 0;
/// @}

/// The size of the thread buffer. This is the number of bytes we actually
/// allocate for a thread, and will account for boundary pages when we're
/// running with dbg_mprotectstacks.
/// @{
static int _buffer_size = 0;
/// @}

/// Determine if we're supposed to be protecting the stack.
///
/// This uses preprocessor macros and will be optimized out when either 1) we
/// have huge pages that prevent us from allocating boundary pages or 2) we're
/// not in debug mode. In that case, the compiler will statically eliminate lots
/// of extraneous stuff. Otherwise, we check the value of the dbg option.
static int _protect_stacks(void) {
#ifndef ENABLE_DEBUG
  return 0;
#endif

#ifdef HAVE_HUGETLBFS
  if (here->config->dbg_mprotectstacks) {
    log_error("cannot mprotect stacks when using huge pages\n");
  }
  return 0;
#endif

  dbg_assert(here && here->config);
  return here->config->dbg_mprotectstacks;
}

/// Update the protections on the first and last page in the stack.
///
/// @param         base The base address.
/// @param         prot The new permissions.
static void _mprotect_boundary_pages(void *base, int prot) {
  dbg_assert(_protect_stacks());

  if ((uintptr_t)base & (HPX_PAGE_SIZE - 1)) {
    dbg_error("stack must be page aligned for mprotect\n");
  }

  char *p1 = base;
  char *p2 = p1 + _thread_size + HPX_PAGE_SIZE;
  int e1 = mprotect(p1, HPX_PAGE_SIZE, prot);
  int e2 = mprotect(p2, HPX_PAGE_SIZE, prot);

  if (e1 || e2) {
    dbg_error("Mprotect error: %d (EACCES %d, EINVAL %d, ENOMEM %d)\n", errno,
              EACCES, EINVAL, ENOMEM);
  }
}

/// Protect the stack so that stack over/underflow will result in a segfault.
///
/// @param       buffer The base of the stack buffer.
///
/// @returns            The page-aligned, writable base of the stack structure.
static ustack_t *_protect(void *buffer) {
  if (!_protect_stacks()) {
    return buffer;
  }

  _mprotect_boundary_pages(buffer, PROT_NONE);
  return (void*)((char*)buffer + HPX_PAGE_SIZE);
}

/// Unprotect the stack so that the pages can be reused.
///
/// @param        stack The base of the stack.
///
/// @returns            The base address of the original allocation so that it
///                     can be freed by the caller.
static void *_unprotect(ustack_t *stack) {
  if (!_protect_stacks()) {
    return stack;
  }

  void *buffer = (char*)stack - HPX_PAGE_SIZE;
  _mprotect_boundary_pages(buffer, PROT_READ | PROT_WRITE);
  return buffer;
}

/// Register a stack with valgrind, so that it doesn't incorrectly complain
/// about stack accesses.
static int _register(ustack_t *thread) {
  void *begin = &thread->stack[0];
  void *end = &thread->stack[_thread_size - sizeof(*thread)];
  return VALGRIND_STACK_REGISTER(begin, end);
}

/// Deregister a stack with valgrind so that it can be reused.
static void _deregister(ustack_t *thread) {
  VALGRIND_STACK_DEREGISTER(thread->stack_id);
}

void thread_set_stack_size(int stack_bytes) {
  dbg_assert(stack_bytes);
  if (!_protect_stacks()) {
    _buffer_size = _thread_size = stack_bytes;
  }
  else {
    // allocate boundary pages when we want to protect the stack
    int pages = ceil_div_32(stack_bytes, HPX_PAGE_SIZE);
    _thread_size = pages * HPX_PAGE_SIZE;
    _buffer_size = _thread_size + 2 * HPX_PAGE_SIZE;
  }

  if (_thread_size != stack_bytes) {
    log_sched("Adjusted stack size to %d bytes\n", _thread_size);
  }
}

/// Architecture-specific transfer frame initialization.
///
/// Each architecture will provide its own functionality for initialing a
/// stack stack frame, typically as an asm file. The result is a thread that we
/// can start running through  thread_transfer.
///
/// @param          top The highest address in the stack.
/// @param            p The parcel that we want to "pass" to @p f.
/// @param            f The initial function to run after the transfer.
///
/// @return             The stack address to use during the first transfer.
extern void *transfer_frame_init(void *top, hpx_parcel_t *p, thread_entry_t f)
  HPX_NON_NULL(1, 2, 3);

void thread_init(ustack_t *thread, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) {
  // Initialize the architecture-independent bit of the stack.
  thread->parcel    = parcel;
  thread->next      = NULL;
  thread->lco_depth = 0;
  thread->tls_id    = -1;
  thread->size      = size;
  thread->cont      = 0;
  thread->affinity  = -1;
  thread->masked    = 0;

  // Initialize the top stack frame so that we can correctly "return" from it
  // during the first transfer to this thread.
  thread->sp = transfer_frame_init((char*)thread + size, parcel, f);
}

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  void *base = NULL;
  if (_protect_stacks()) {
    base = as_memalign(AS_REGISTERED, HPX_PAGE_SIZE, _buffer_size);
  }
  else {
    base = as_malloc(AS_REGISTERED, _buffer_size);
  }
  dbg_assert(base);

  ustack_t *thread = _protect(base);
  thread->stack_id = _register(thread);
  thread_init(thread, parcel, f, _thread_size);
  return thread;
}

void thread_delete(ustack_t *thread) {
  _deregister(thread);
  void *base = _unprotect(thread);
  as_free(AS_REGISTERED, base);
}

