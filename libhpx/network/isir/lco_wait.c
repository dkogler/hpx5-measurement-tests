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

#include <alloca.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "isir.h"

/// This action resumes a parcel that is suspended.
///
/// @param       parcel The parcel to resume.
///
/// @returns            HPX_SUCCESS
static int _isir_lco_launch_parcel_handler(void *parcel) {
  parcel_launch(parcel);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _isir_lco_launch_parcel,
                     _isir_lco_launch_parcel_handler, HPX_POINTER);

/// This action can be used by a thread to wait on an LCO through suspension.
///
/// @param        reset Flag saying if this is just a wait, or a wait + reset.
/// @param       parcel The address to be forwarded back to the caller.
///
/// @returns            HPX_SUCCESS
static int _isir_lco_wait_handler(int reset, void *parcel) {
  if (reset) {
    dbg_check( hpx_lco_wait_reset(self->current->target) );
  }
  else {
    dbg_check( hpx_lco_wait(self->current->target) );
  }

  return hpx_thread_continue(parcel);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _isir_lco_wait, _isir_lco_wait_handler,
                     HPX_INT, HPX_POINTER);

/// This scheduler_suspend continuation permits a thread to wait for a remote
/// LCO *without* allocating anything in the global address space.
/// @{
typedef struct {
  hpx_addr_t lco;
  int reset;
} _isir_lco_wait_env_t;

static void _isir_lco_wait_continuation(hpx_parcel_t *p, void *env) {
  _isir_lco_wait_env_t *e = env;
  hpx_action_t op = _isir_lco_wait;
  hpx_action_t rop = _isir_lco_launch_parcel;
  dbg_check( action_call_lsync(op, e->lco, HPX_HERE, rop, 2, &e->reset, &p) );
}
/// @}

int isir_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _isir_lco_wait_env_t env = {
    .lco = lco,
    .reset = reset
  };
  scheduler_suspend(_isir_lco_wait_continuation, &env, 0);
  return HPX_SUCCESS;
}
