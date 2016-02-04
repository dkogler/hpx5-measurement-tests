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

#include <libhpx/debug.h>
#include "../../thread.h"
#include "asm.h"

/// The fp control register state; we read this once at startup and then use it
/// to initialize thread state.
///
/// @{
static uint32_t  _mxcsr = 0;
static uint16_t  _fpucw = 0;

static void HPX_CONSTRUCTOR _init_x86_64(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
}
/// @}

/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner, but we are just worried
/// about x86-64 at the moment.
typedef struct {
  uint32_t     mxcsr;                           // 7
  uint16_t     fpucw;                           // 7.5
  uint16_t   padding;                           // 7.75 has to match transfer.S
  void          *r15;                           // 6
  void          *r14;                           // 5
  void          *r13;                           // 4
  hpx_parcel_t  *r12;                           // 3
  thread_entry_t rbx;                           // 2
  void          *rbp;                           // 1
  void         (*rip)(void);                    // 0
  void      *top_rbp;
  void     (*top_rip)(void);
} HPX_PACKED _frame_t;

void *transfer_frame_init(void *top, hpx_parcel_t *p, thread_entry_t f) {
  // x86_64 wants 16 byte alignment, so we adjust the top pointer if necessary
  top = (void*)((uintptr_t)top & ~(15));

  // Stack frame addresses go "down" while C struct addresses go "up, so compute
  // the frame base from the top of the frame using the size of the frame
  // structure. After this, we can just write values to the frame structure and
  // they'll be in the right place for the initial return from transfer.
  _frame_t *frame = (void*)((char*)top - sizeof(*frame));
  assert((uintptr_t)frame % 16 == 0);

  frame->mxcsr = _mxcsr;
  frame->fpucw = _fpucw;

#ifdef ENABLE_DEBUG
  frame->r15 = NULL;
  frame->r14 = NULL;
  frame->r13 = NULL;
#endif

  frame->r12 = p;
  frame->rbx = f;
  frame->rbp = &frame->rip;
  frame->rip = align_stack_trampoline;

#ifdef ENABLE_DEBUG
  frame->top_rbp = NULL;
  frame->top_rip = NULL;
#endif

  return frame;
}
