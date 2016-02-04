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
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "commands.h"
#include "pwc.h"
#include "xport.h"

/// Wait for an LCO to be set, and then resume a remote parcel.
///
/// NB: We could do this through the normal parcel continuation infrastructure
///     without sending the parcel pointer as an argument.
static int _pwc_lco_wait_handler(uint64_t p, int reset) {
  hpx_parcel_t *curr = self->current;
  hpx_addr_t lco = curr->target;
  int e = (reset) ? hpx_lco_wait_reset(lco) : hpx_lco_wait(lco);

  if (e != HPX_SUCCESS) {
    dbg_error("Cannot yet return an error from a remote wait operation\n");
  }
  command_t rcmd = { .op = RESUME_PARCEL, .arg = p };
  return pwc_cmd(pwc_network, curr->src, (command_t){0}, rcmd);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _pwc_lco_wait, _pwc_lco_wait_handler,
                     HPX_POINTER, HPX_INT);

typedef struct {
  hpx_addr_t lco;
  int reset;
} _pwc_lco_wait_env_t;

static void _pwc_lco_wait_continuation(hpx_parcel_t *p, void *env) {
  _pwc_lco_wait_env_t *e = env;
  uint64_t arg = (uint64_t)(uintptr_t)p;
  hpx_action_t act = _pwc_lco_wait;
  dbg_check(action_call_lsync(act, e->lco, 0, 0, 2, &arg, &e->reset));
}

int pwc_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _pwc_lco_wait_env_t env = {
    .lco = lco,
    .reset = reset
  };
  scheduler_suspend(_pwc_lco_wait_continuation, &env, 0);
  // NB: we could return self->current->error
  return HPX_SUCCESS;
}
