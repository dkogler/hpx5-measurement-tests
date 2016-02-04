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
#include <papi.h>
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

static uint64_t _papi_events[] = {
  [HPX_L1_TCM]  = PAPI_L1_TCM,
  [HPX_L2_TCM]  = PAPI_L2_TCM,
  [HPX_L3_TCM]  = PAPI_L3_TCM,
  [HPX_TLB_TL]  = PAPI_TLB_TL,
  [HPX_TOT_INS] = PAPI_TOT_INS,
  [HPX_INT_INS] = PAPI_INT_INS,
  [HPX_FP_INS]  = PAPI_FP_INS,
  [HPX_LD_INS]  = PAPI_LD_INS,
  [HPX_SR_INS]  = PAPI_SR_INS,
  [HPX_BR_INS]  = PAPI_BR_INS,
  [HPX_TOT_CYC] = PAPI_TOT_CYC,
};

static int _set_event(int id, uint64_t papi_event, uint64_t event) {
  if (PAPI_query_event(papi_event) != PAPI_OK) {
    const char *counter_name = HPX_COUNTER_TO_STRING[event];
    log_dflt("Warning: %s is not available on this system\n", counter_name);
    return 0;
  }

  _profile_log.counters[id] = event;
  return 1;
}

int prof_init(config_t *cfg) {
  // initialize PAPI
  int e = PAPI_library_init(PAPI_VER_CURRENT);
  if (e != PAPI_VER_CURRENT) {
    log_error("unable to initialize PAPI (error code %d)\n", e);
  }

  // get the number of available hardware counters on the platform
  int max_counters = PAPI_num_counters();

  // get the number of counters requested by the user
  int counters = cfg->prof_counters;
  int req_counters = popcountl(counters);

  if (req_counters > max_counters) {
    log_dflt("WARNING: maximum available counters is %d\n", max_counters);
    req_counters = max_counters;
  }

  _profile_log.counters = calloc(req_counters, sizeof(int));
  _profile_log.events = calloc(_profile_log.max_events, sizeof(profile_list_t));

  int num_counters = 0;
  for (int c = 0; c < HPX_COUNTER_MAX && num_counters < req_counters; ++c) {
    if ((UINT64_C(1) << c) & counters) {
      uint64_t papi_event = _papi_events[c];
      num_counters += _set_event(num_counters, papi_event, c);
    }
  }

  _profile_log.num_counters = num_counters;
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(_profile_log);
  for (int i = 0; i < _profile_log.num_events; i++) {
    if (!_profile_log.events[i].simple) {
      for (int j = 0; j < _profile_log.events[i].num_entries; j++) {
        free(_profile_log.events[i].entries[j].counter_totals);
      }
    }
    free(_profile_log.events[i].entries);
  }
  free(_profile_log.events);
  free(_profile_log.counters);
}

int prof_get_averages(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    double divisor = 0;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked &&
         _profile_log.events[event].entries[j].counter_totals[i] >= 0) {
        values[i] += _profile_log.events[event].entries[j].counter_totals[i];
        divisor++;
      }
    }
    if (divisor > 0) {
      values[i] /= divisor;
    }
  }
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 || _profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked &&
         _profile_log.events[event].entries[j].counter_totals[i] >= 0) {
        values[i] += _profile_log.events[event].entries[j].counter_totals[i];
      }
    }
  }
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    int start = _profile_log.events[event].num_entries;
    int64_t temp;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[i].marked) {
        values[i] = _profile_log.events[event].entries[j].counter_totals[i];
        start = j + 1;
        break;
      }
    }
    for (int j = start; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked) {
        temp = _profile_log.events[event].entries[j].counter_totals[i];
        if (temp < values[i] && temp >= 0) {
          values[i] = temp;
        }
      }
    }
  }
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    int start = _profile_log.events[event].num_entries;
    int64_t temp;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[i].marked) {
        values[i] = _profile_log.events[event].entries[j].counter_totals[i];
        start = j + 1;
        break;
      }
    }
    for (int j = start; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked) {
        temp = _profile_log.events[event].entries[j].counter_totals[i];
        if (temp > values[i]) {
          values[i] = temp;
        }
      }
    }
  }
  return LIBHPX_OK;
}

static int _create_new_event(char *key) {
  int eventset = PAPI_NULL;
  int retval = PAPI_create_eventset(&eventset);
  if (retval != PAPI_OK) {
    log_error("unable to create eventset with error code %d\n", retval);
  } else {
    for (int i = 0; i < _profile_log.num_counters; i++) {
      PAPI_add_event(eventset, _papi_events[_profile_log.counters[i]]);
    }
  }

  return profile_new_event(key, false, eventset);
}

int prof_start_hardware_counters(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    event = _create_new_event(key);
  }
  if(event < 0){
    return LIBHPX_ERROR;
  }

  if (_profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }
  
  // update the current event and entry being recorded
  if (_profile_log.current_event >= 0 && 
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].marked &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].paused) {
    // left as long long instead of int64_t to suppress a warning
    // at compile time
    long long values[_profile_log.num_counters];
    for (int i = 0; i < _profile_log.num_counters; i++) {
      values[i] = -1;
    }
    PAPI_stop(_profile_log.events[_profile_log.current_event].eventset, 
              values);

    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[
                  _profile_log.current_event].entries[
                  _profile_log.current_entry].ref_time, end, &dur);
    _profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time =
             hpx_time_add(_profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time, dur);

    for (int i = 0; i < _profile_log.num_counters; i++) {
      _profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].counter_totals[i] 
                          += (int64_t) values[i];
    }
  }

  int index = profile_new_entry(event);
  
  _profile_log.events[event].entries[index].last_entry = _profile_log.current_entry;
  _profile_log.events[event].entries[index].last_event = _profile_log.current_event;
  _profile_log.current_entry = index;
  _profile_log.current_event = event;
  *tag = index;
  PAPI_reset(_profile_log.events[_profile_log.current_event].eventset);
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  _profile_log.events[event].entries[index].ref_time = 
    _profile_log.events[event].entries[index].start_time;
  return PAPI_start(_profile_log.events[event].eventset);
}

int prof_stop_hardware_counters(char *key, int *tag) {
  int event = prof_stop_timing(key, tag);
  if (event < 0 || *tag == HPX_PROF_NO_TAG) {
    return LIBHPX_EINVAL;
  }

  // left as long long instead of int64_t to suppress a warning
  // at compile time
  long long values[_profile_log.num_counters];
  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = -1;
  }
  int retval = PAPI_stop(_profile_log.events[event].eventset, values);
  PAPI_reset(_profile_log.events[event].eventset);
  if (retval != PAPI_OK) {
    return retval;
  }
  
  for (int i = 0; i < _profile_log.num_counters; i++) {
    _profile_log.events[event].entries[*tag].counter_totals[i] 
      += (int64_t) values[i];
  }

  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off (check current_event because it
  // was already updated in prof_stop_timing())
  if (_profile_log.current_event >= 0 && 
     !_profile_log.events[event].entries[_profile_log.current_entry].paused) {
    PAPI_start(_profile_log.events[_profile_log.current_event].eventset);
  }

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

  // then store counter information if necessary
  if (!_profile_log.events[event].simple) {
    // I leave this as type long long instead of int64_t to suppress a warning
    // at compile time that appears if I do otherwise
    long long values[_profile_log.num_counters];
    for (int i = 0; i < _profile_log.num_counters; i++) {
      values[i] = -1;
    }
    int retval = PAPI_stop(_profile_log.events[event].eventset, values);
    PAPI_reset(_profile_log.events[event].eventset);
    if (retval != PAPI_OK) {
      return retval;
    }
    
    for (int i = 0; i < _profile_log.num_counters; i++) {
      _profile_log.events[event].entries[*tag].counter_totals[i] 
        += (int64_t) values[i];
    }
  }
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
  _profile_log.events[event].entries[*tag].ref_time = hpx_time_now();
  if (!_profile_log.events[event].simple) {
    PAPI_reset(_profile_log.events[_profile_log.current_event].eventset);
    return PAPI_start(_profile_log.events[event].eventset);
  }
  return LIBHPX_OK;
}
