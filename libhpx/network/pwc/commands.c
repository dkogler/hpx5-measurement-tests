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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>

#include "commands.h"
#include "pwc.h"
#include "xport.h"

void handle_lco_set(int src, command_t cmd) {
  hpx_addr_t lco = offset_to_gpa(here->rank, cmd.packed);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
}

void handle_lco_set_source(int src, command_t cmd) {
  cmd.op = LCO_SET;
  dbg_check( pwc_cmd(pwc_network, src, (command_t){0}, cmd) );
}

void handle_delete_parcel(int src, command_t cmd) {
  hpx_parcel_t *p = (hpx_parcel_t *)(uintptr_t)cmd.arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  parcel_delete(p);
}

void handle_resume_parcel(int src, command_t cmd) {
  hpx_parcel_t *p = (hpx_parcel_t *)(uintptr_t)cmd.arg;
  log_net("resuming %s, (%p)\n", actions[p->action].key, (void*)p);
  parcel_launch(p);
}

void handle_resume_parcel_source(int src, command_t cmd) {
  cmd.op = RESUME_PARCEL;
  dbg_check( pwc_cmd(pwc_network, src, (command_t){0}, cmd) );
}

static HPX_USED const char *_straction(hpx_action_t id) {
  dbg_assert(here);
  CHECK_ACTION(id);
  return actions[id].key;
}

void command_run(int src, command_t cmd) {
  log_net("invoking command: %s from %d\n", _straction(cmd.op), src);

  static const command_handler_t commands[] = {
    NULL,
    handle_resume_parcel,
    handle_resume_parcel_source,
    handle_delete_parcel,
    handle_lco_set,
    handle_lco_set_source,
    handle_recv_parcel,
    handle_rendezvous_launch,
    handle_reload_request,
    handle_reload_reply
  };

  commands[cmd.op](src, cmd);
}
