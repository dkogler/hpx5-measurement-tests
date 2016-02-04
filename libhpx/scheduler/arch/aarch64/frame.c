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

#include <stddef.h>
#include <stdio.h>
#include <libhpx/debug.h>
#include "../../thread.h"
#include "../common/asm.h"

/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
typedef struct {
  thread_entry_t    x19; // used to hold f(), called by align_stack_trampoline
  void             *x20; // we use this to hold the parcel that is passed to f()
  void         *regs[8]; // x21-x28
  void             *x29; // The frame pointer
  void     (*x30)(void); // return address - set to align_stack_trampoline
  void   *vfp_alignment;
  void           *fpscr;
  void      *vfpregs[8];
  void         *top_x19;
  void (*top_x30)(void);
} HPX_PACKED _frame_t;

void *transfer_frame_init(void *top, hpx_parcel_t *p, thread_entry_t f) {
  // wants 16 byte alignment, so we adjust the top pointer if necessary
  top = (void*)((uintptr_t)top & ~(15));

  // Stack frame addresses go "down" while C struct addresses go "up, so compute
  // the frame base from the top of the frame using the size of the frame
  // structure. After this, we can just write values to the frame structure and
  // they'll be in the right place for the initial return from transfer.
  _frame_t *frame = (void*)((char*)top - sizeof(*frame));
  assert((uintptr_t)frame % 16 == 0);

  // register must be the same as the one in align_stack_trampoline
  frame->x19      = f;
  frame->x20      = p;
  frame->x30      = align_stack_trampoline;

#ifdef ENABLE_DEBUG
  frame->top_x19 = NULL;
  frame->top_x30 = NULL;
#endif

  return frame;
}
