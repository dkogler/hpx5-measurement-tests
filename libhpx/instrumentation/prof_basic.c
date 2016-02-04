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

/// Each locality maintains a single profile log
extern profile_log_t _profile_log;

int prof_init(struct config *cfg) {
  _profile_log.counters = NULL;
  _profile_log.num_counters = 0;
  _profile_log.events = malloc(_profile_log.max_events *
                                sizeof(profile_list_t));
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(_profile_log);
  for (int i = 0; i < _profile_log.num_events; i++) {
    free(_profile_log.events[i].entries);
  }
  free(_profile_log.events);
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
  prof_start_timing(key, tag);
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key, int *tag) {
  prof_stop_timing(key, tag);
  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    return LIBHPX_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked &&
         !_profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG ||
     _profile_log.events[event].entries[*tag].marked ||
     _profile_log.events[event].entries[*tag].paused) {
    return LIBHPX_EINVAL;
  }

  // first store timing information
  hpx_time_t dur;
  hpx_time_diff(_profile_log.events[event].entries[*tag].ref_time, end, &dur);
  _profile_log.events[event].entries[*tag].run_time =
      hpx_time_add(_profile_log.events[event].entries[*tag].run_time, dur);

  _profile_log.events[event].entries[*tag].paused = true;
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag) {
  int event = profile_get_event(key);
  if (event < 0) {
    return LIBHPX_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked &&
         _profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return LIBHPX_EINVAL;
  }

  _profile_log.events[event].entries[*tag].paused = false;
  _profile_log.events[event].entries[*tag].start_time = hpx_time_now();
  _profile_log.events[event].entries[*tag].ref_time =
    _profile_log.events[event].entries[*tag].start_time;
  return LIBHPX_OK;
}

