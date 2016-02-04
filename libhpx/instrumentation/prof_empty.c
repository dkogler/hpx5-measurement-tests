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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>
#include <libsync/sync.h>

int prof_init(struct config *cfg) {
  return LIBHPX_OK;
}

void prof_fini(void) {
}

int prof_get_averages(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_start_hardware_counters(char *key, int *tag) {
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key, int *tag) {
  prof_stop_timing(key, tag);
  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag) {
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag) {
  return LIBHPX_OK;
}

