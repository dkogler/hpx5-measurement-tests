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
#include "../common/asm.h"

/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
typedef struct {
  void     *alignment; // keep 8-byte aligned stack
#ifdef __VFP_FP__
  void *vfp_alignment;
  void         *fpscr;
  void   *vfpregs[16];
#endif
  void       *regs[6]; // r6-r11
  void            *r5; // we use this to hold the parcel that is passed to f()
  thread_entry_t   r4; // used to hold f(), called by align_stack_trampoline
  void           (*lr)(void); // return address
  void        *top_r4;
  void       (*top_lr)(void);
} HPX_PACKED _frame_t;

void *transfer_frame_init(void *top, hpx_parcel_t *p, thread_entry_t f) {
  // arm wants 8 byte alignment, so we adjust the top pointer if necessary
  top = (void*)((uintptr_t)top & ~(7));

  // Stack frame addresses go "down" while C struct addresses go "up, so compute
  // the frame base from the top of the frame using the size of the frame
  // structure. After this, we can just write values to the frame structure and
  // they'll be in the right place for the initial return from transfer.
  _frame_t *frame = (void*)((char*)top - sizeof(*frame));
  assert((uintptr_t)frame % 8 == 0);

  // register must be the same as the one used in align_stack_trampoline
  frame->r4 = f;
  frame->r5 = p;
  frame->lr = align_stack_trampoline;

#ifdef ENABLE_DEBUG
  frame->top_r4 = NULL;
  frame->top_lr = NULL;
#endif

  return frame;
}
