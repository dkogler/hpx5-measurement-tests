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

profile_log_t _profile_log = PROFILE_INIT;

int profile_new_event(char *key, bool simple, int eventset) {
  if (_profile_log.events == NULL) {
    return LIBHPX_ERROR;
  }
  if (_profile_log.num_events == _profile_log.max_events) {
    _profile_log.max_events *= 2;
    size_t bytes = _profile_log.max_events * sizeof(profile_list_t);
    _profile_log.events = realloc(_profile_log.events, bytes);
    dbg_assert(_profile_log.events);
  }
  int index = _profile_log.num_events++;
  profile_list_t *list = &_profile_log.events[index];
  if (list == NULL) {
    _profile_log.num_events--;
    return LIBHPX_ERROR;
  }
  list->entries = malloc(_profile_log.max_events * sizeof(profile_entry_t));
  dbg_assert(list->entries);
  list->num_entries = 0;
  list->max_entries = _profile_log.max_events;
  list->key = key;
  list->simple = simple;
  list->eventset = eventset;
  return index;
}

// Returns index of matching key or returns -1 if the event
// does not exist.
int profile_get_event(char *key) {
  for (int i = 0; i < _profile_log.num_events; i++) {
    if (strcmp(key, _profile_log.events[i].key) == 0) {
      return i;
    }
  }
  return LIBHPX_ERROR;
}

int profile_new_entry(int event) {
  profile_list_t *list = &_profile_log.events[event];
  dbg_assert(list->max_entries > 0);
  if (list->num_entries == list->max_entries) {
    list->max_entries *= 2;
    size_t bytes = list->max_entries * sizeof(profile_entry_t);
    list->entries = realloc(list->entries, bytes);
    dbg_assert(list->entries);
  }

  int index = list->num_entries++;
  list->entries[index].run_time = HPX_TIME_NULL;
  list->entries[index].marked = false;
  list->entries[index].paused = false;

  list->entries[index].counter_totals = NULL;
  list->entries[index].user_val = 0;
  if (list->simple) {
    list->entries[index].counter_totals = NULL;
  } else {
    list->entries[index].counter_totals =
        malloc(_profile_log.num_counters * sizeof(int64_t));
    for (int i = 0; i < _profile_log.num_counters; ++i) {
      list->entries[index].counter_totals[i] = -1;
    }
  }
  return index;
}

double prof_get_user_total(char *key) {
  int event = profile_get_event(key);
  if (event < 0) {
    return 0;
  }
  double total = 0;
  for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
    total += _profile_log.events[event].entries[i].user_val;
  }
  return total;
}

int prof_get_event_count(char *key) {
  int event = profile_get_event(key);
  if (event < 0) {
    return 0;
  }
  return _profile_log.events[event].num_entries;
}

void prof_get_average_time(char *key, hpx_time_t *avg) {
  int event = profile_get_event( key);
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, average = 0;
  double divisor = 0;
  int64_t wall_time = hpx_time_from_start_ns(hpx_time_now());
  for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      int64_t amount = hpx_time_diff_ns(HPX_TIME_NULL,
                                  _profile_log.events[event].entries[i].run_time);
      if (amount > 0 && amount < wall_time) {
        divisor++;
        average += amount;
      }
    }
  }
  if (divisor > 0) {
    average /= divisor;
  }
  seconds = average / 1e9;
  ns = average % (int64_t)1e9;

  *avg = hpx_time_construct(seconds, ns);
}

void prof_get_total_time(char *key, hpx_time_t *tot) {
  int event = profile_get_event(key);
  hpx_time_t average = HPX_TIME_NULL;
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, total;

  prof_get_average_time(key, &average);

  total = _profile_log.events[event].num_entries * hpx_time_diff_ns(HPX_TIME_NULL, average);
  seconds = total / 1e9;
  ns = total % (int64_t)1e9;

  *tot = hpx_time_construct(seconds, ns);
}

void prof_get_min_time(char *key, hpx_time_t *min) {
  int event = profile_get_event(key);
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, temp;
  int64_t minimum = 0;
  int start = _profile_log.events[event].num_entries;

  if (_profile_log.events[event].num_entries > 0 ) {
    for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
      if (_profile_log.events[event].entries[i].marked &&
         0 < hpx_time_diff_ns(HPX_TIME_NULL, _profile_log.events[event].entries[i].run_time)) {
        minimum = hpx_time_diff_ns(HPX_TIME_NULL, 
                                   _profile_log.events[event].entries[i].run_time);
        start = i+1;
        break;
      }
    }
  }
  for (int i = start; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      temp = hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if (temp < minimum && temp > 0) {
        minimum = temp;
      }
    }
  }
  seconds = minimum / 1e9;
  ns = minimum % (int64_t)1e9;

  *min = hpx_time_construct(seconds, ns);
}

void prof_get_max_time(char *key, hpx_time_t *max) {
  int event = profile_get_event(key);
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, temp;
  int64_t maximum = 0;
  int start = _profile_log.events[event].num_entries;

  int64_t wall_time = hpx_time_from_start_ns(hpx_time_now());
  if (_profile_log.events[event].num_entries > 0 ) {
    for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
      if (_profile_log.events[event].entries[i].marked) {
        temp = hpx_time_diff_ns(HPX_TIME_NULL,
                                _profile_log.events[event].entries[0].run_time);
        if (temp > maximum && temp < wall_time) {
          start = i+1;
          break;
        }
      }
    }
  }
  for (int i = start; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      temp = hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if (temp > maximum && temp < wall_time) {
        maximum = temp;
      }
    }
  }
  seconds = maximum / 1e9;
  ns = maximum % (int64_t)1e9;

  *max = hpx_time_construct(seconds, ns);
}

int prof_get_num_counters() {
  return _profile_log.num_counters;
}

void prof_record_user_val(char *key, double amount) {
  int event = profile_get_event(key);
  if (event < 0) {
    event = profile_new_event(key, true, 0);
  }
  if (event < 0) {
    return;
  }
  int index = profile_new_entry(event);
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  _profile_log.events[event].entries[index].user_val = amount;
}

void prof_mark(char *key) {
  int event = profile_get_event(key);
  if (event < 0) {
    event = profile_new_event(key, true, 0);
  }
  if (event < 0) {
    return;
  }
  int index = profile_new_entry(event);
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
}

void prof_start_timing(char *key, int *tag) {
  hpx_time_t now = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    event = profile_new_event(key, true, 0);
  }
  if (event < 0) {
    return;
  }

  // interrupt current timing
  if (_profile_log.current_event >= 0 && 
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].marked &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].paused) {
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[_profile_log.current_event].entries[
                  _profile_log.current_entry].ref_time, now, &dur);
    _profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time =
             hpx_time_add(_profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time, dur);
  }

  int index = profile_new_entry(event);
  _profile_log.events[event].entries[index].last_entry = _profile_log.current_entry;
  _profile_log.events[event].entries[index].last_event = _profile_log.current_event;
  _profile_log.current_entry = index;
  _profile_log.current_event = event;
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  _profile_log.events[event].entries[index].ref_time = 
    _profile_log.events[event].entries[index].start_time;
  *tag = index;
}

int prof_stop_timing(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    return event;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return event;
  }

  if (!_profile_log.events[event].entries[*tag].paused) {
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[event].entries[*tag].ref_time, end, &dur);

    _profile_log.events[event].entries[*tag].run_time = 
        hpx_time_add(_profile_log.events[event].entries[*tag].run_time, dur);
  }
  _profile_log.events[event].entries[*tag].marked = true;
  
  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off
  if (_profile_log.events[event].entries[*tag].last_event >= 0) {
    _profile_log.current_entry = _profile_log.events[event].entries[*tag].last_entry;
    _profile_log.current_event = _profile_log.events[event].entries[*tag].last_event;
    if (!_profile_log.events[event].entries[_profile_log.current_entry].paused) {
      _profile_log.events[_profile_log.current_event].entries
                       [_profile_log.current_entry].ref_time
                        = hpx_time_now();
    }
  }
  return event;
}
