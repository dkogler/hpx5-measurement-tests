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

#ifndef LIBHPX_NETWORK_PWC_COMMANDS_H
#define LIBHPX_NETWORK_PWC_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint8_t op_t;
typedef uint64_t arg_t;

typedef union {
  uint64_t packed;
  struct {
    uint64_t   arg : 48;
    uint64_t    op : 16;
  };
} command_t;

typedef void (*command_handler_t)(int, command_t);

void handle_resume_parcel(int src, command_t cmd);
void handle_resume_parcel_source(int src, command_t cmd);
void handle_delete_parcel(int src, command_t cmd);
void handle_lco_set(int src, command_t cmd);
void handle_lco_set_source(int src, command_t cmd);
void handle_recv_parcel(int src, command_t cmd);
void handle_rendezvous_launch(int src, command_t cmd);
void handle_reload_request(int src, command_t cmd);
void handle_reload_reply(int src, command_t cmd);

enum {
  NOP = 0,
  RESUME_PARCEL,
  RESUME_PARCEL_SOURCE,
  DELETE_PARCEL,
  LCO_SET,
  LCO_SET_SOURCE,
  RECV_PARCEL,
  RENDEZVOUS_LAUNCH,
  RELOAD_REQUEST,
  RELOAD_REPLY,
  COMMAND_COUNT
};

/// Handle a command.
void command_run(int src, command_t cmd);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
