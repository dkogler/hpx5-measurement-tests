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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#include <pwd.h>

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/profiling.h>
#include "logtable.h"

/// complete path to the directory to which log files, etc. will be written
static const char *_log_path = NULL;

/// when profiling, setting detailed_prof = true will dump each individual
/// measurement to file
static bool _detailed_prof = false;

/// We're keeping one log per event per locality. Here are their headers.
static logtable_t _logs[HPX_INST_NUM_EVENTS] = {LOGTABLE_INIT};

/// Concatenate two paths. Callee must free returned char*.
static char *_get_complete_path(const char *path, const char *filename) {
  int len_path = strlen(path);
  int len_filename = strlen(filename);
  int len_complete_path = len_path + len_filename + 2;
  // length is +1 for '/' and +1 for \00
  char *complete_path = malloc(len_complete_path + 1);
  snprintf(complete_path, len_complete_path, "%s/%s", path, filename);
  return complete_path;
}

/// This will output a list of action ids and names as a two-column csv file
/// This is so that traced parcels can be interpreted more easily.
static void _dump_actions(void) {
  char filename[256];
  snprintf(filename, 256, "actions.%d.csv", hpx_get_my_rank());
  char *file_path = _get_complete_path(_log_path, filename);

  FILE *file = fopen(file_path, "w");
  if (file == NULL) {
    log_error("failed to open action id file %s\n", file_path);
  }

  free(file_path);

  for (int i = 0, e = action_table_size(); i < e; i++) {
    if (action_is_internal(i)) {
      fprintf(file, "%d,%s,INTERNAL\n", i, actions[i].key);
    }
    else {
      fprintf(file, "%d,%s,USER\n", i, actions[i].key);
    }
  }

  int e = fclose(file);
  if (e != 0) {
    log_error("failed to write actions\n");
  }
}

static void _log_create(int class, int id, size_t size, hpx_time_t now) {
  char filename[256];
  snprintf(filename, 256, "event.%d.%d.%d.%s.%s.log",
           class, id, hpx_get_my_rank(),
           INST_CLASS_TO_STRING[class],
           INST_EVENT_TO_STRING[id]);

  char *file_path = _get_complete_path(_log_path, filename);

  int e = logtable_init(&_logs[id], file_path, size, class, id);
  if (e) {
    log_error("failed to initialize log file %s\n", file_path);
  }

  free(file_path);
}

static const char *_mkdir(const char *dir) {
  // try and create the directory---we don't care if it's already there
  int e = mkdir(dir, 0777);
  if (e) {
    if (errno != EEXIST) {
      log_error("Could not create %s for instrumentation\n", dir);
    }
  }
  return dir;
}

static const char *_mktmp(void) {
  // create directory name
  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);
  char dirname[256];
  struct passwd *pwd = getpwuid(getuid());
  const char *username = pwd->pw_name;
  snprintf(dirname, 256, "hpx-%s.%.4d%.2d%.2d.%.2d%.2d", username,
           lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);

  return _mkdir(_get_complete_path("/tmp", dirname));
}

static const char *_get_log_path(const char *dir) {
  if (!dir) {
    return _mktmp();
  }

  struct stat sb;

  // if the path doesn't exist, then we make it
  if (stat(dir, &sb) != 0) {
    return _mkdir(dir);
  }

  // if the path is a directory, we just return it
  if (S_ISDIR(sb.st_mode)) {
    return strndup(dir, 256);
  }

  // path exists but isn't a directory.
  log_error("--with-inst-dir=%s does not point to a directory\n", dir);
  return NULL;
}

int inst_init(config_t *cfg) {
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif
  if (!config_inst_at_isset(cfg, hpx_get_my_rank())) {
    return LIBHPX_OK;
  }

  _log_path = _get_log_path(cfg->inst_dir);
  if (_log_path == NULL) {
    return LIBHPX_OK;
  }

  // create log files
  hpx_time_t start = hpx_time_now();
  for (int cl = 0, e = HPX_INST_NUM_CLASSES; cl < e; ++cl) {
    for (int id = INST_OFFSETS[cl], e = INST_OFFSETS[cl + 1]; id < e; ++id) {
      if (inst_trace_class(cl)) {
        _log_create(cl, id, cfg->trace_filesize, start);
      }
    }
  }

  // enable profiling
  if (prof_init(cfg)) {
    log_dflt("error detected while initializing profiling\n");
  }
  _detailed_prof = cfg->prof_detailed;

  return LIBHPX_OK;
}

static void _dump_hostnames(void) {
  if (_log_path == NULL) {
    return;
  }
  char hostname[HOSTNAME_LENGTH];
  gethostname(hostname, HOSTNAME_LENGTH);

  char filename[256];
  snprintf(filename, 256, "hostname.%d", hpx_get_my_rank());
  char *filepath = _get_complete_path(_log_path, filename);
  FILE *f = fopen(filepath, "w");
  if (f == NULL) {
    log_error("failed to open hostname file %s\n", filepath);
    free(filepath);
    return;
  }

  fprintf(f, "%s\n", hostname);
  int e = fclose(f);
  if (e != 0) {
    log_error("failed to write hostname to %s\n", filepath);
  }

  free(filepath);
}

/// This is for things that can only happen once hpx_run has started.
/// Specifically, actions must have been finalized. There may be additional
/// restrictions in the future.
/// Right now the only thing inst_start() does is write the action table.
int inst_start(void) {
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif
  // write action table for tracing
  if (inst_trace_class(HPX_INST_CLASS_PARCEL)) {
    _dump_actions();
    _dump_hostnames();
  }

  return 0;
}

void inst_fini(void) {
  prof_fini();
  for (int i = 0, e = HPX_INST_NUM_EVENTS; i < e; ++i) {
    logtable_fini(&_logs[i]);
  }
  free((void*)_log_path);
}

void inst_prof_dump(profile_log_t log) {
  if (!_log_path) {
    return;
  }

  char filename[256];
  snprintf(filename, 256, "profile.%d", hpx_get_my_rank());
  char *filepath = _get_complete_path(_log_path, filename);
  FILE *f = fopen(filepath, "w");
  if (!f) {
    log_error("failed to open profiling output file %s\n", filepath);
    free(filepath);
    return;
  }

  double duration = hpx_time_from_start_ns(hpx_time_now())/1e9;
  fprintf(f, "Total time: %.3f seconds \n", duration);
  for (int i = 0; i < log.num_events; i++) {
    fprintf(f, "\nEvent: %s\n", log.events[i].key);
    fprintf(f, "Count: %d\n", log.events[i].num_entries);
    double total = prof_get_user_total(log.events[i].key);
    double total_time_ms;
    hpx_time_t total_time;
    prof_get_total_time(log.events[i].key, &total_time);
    total_time_ms = hpx_time_ms(total_time);
    if(total != 0){
      fprintf(f, "Total recorded user value: %f\n", total);
    }

    if(total_time_ms != 0 || (log.num_counters >0 && !log.events[i].simple)){
      fprintf(f, "Statistics:\n");
      fprintf(f, "%-24s%-24s%-24s%-24s\n",
             "Type", "Average", "Minimum", "Maximum");
    }
    if (log.num_counters > 0 && !log.events[i].simple) {
      int64_t averages[log.num_counters];
      int64_t minimums[log.num_counters];
      int64_t maximums[log.num_counters];

      prof_get_averages(averages, log.events[i].key);
      prof_get_minimums(minimums, log.events[i].key);
      prof_get_maximums(maximums, log.events[i].key);
      for (int j = 0; j < log.num_counters; j++) {
        fprintf(f, "%-24s%-24"PRIu64"%-24"PRIu64"%-24"PRIu64"\n",
                HPX_COUNTER_TO_STRING[log.counters[j]],
                averages[j], minimums[j], maximums[j]);
      }
    }

    hpx_time_t average, min, max;
    prof_get_average_time(log.events[i].key, &average);
    prof_get_min_time(log.events[i].key, &min);
    prof_get_max_time(log.events[i].key, &max);

    if(total_time_ms != 0){
      fprintf(f, "%-24s%-24.6f%-24.6f%-24.6f\n", "Time (ms)",
              hpx_time_ms(average),
              hpx_time_ms(min),
              hpx_time_ms(max));
    }

    if (_detailed_prof) {
      fprintf(f, "\nDUMP:\n\n%-24s", "Entry #");
      if(!log.events[i].simple){
        for (int j = 0; j < log.num_counters; j++) {
          fprintf(f, "%-24s", HPX_COUNTER_TO_STRING[log.counters[j]]);
        }
      }
      fprintf(f, "%-24s", "Start Time (ms)");
      if(total_time_ms != 0){
        fprintf(f, "%-24s", "CPU Time (ms)");
      }
      if(total != 0){
        fprintf(f, "%-24s", "User Value (ms)");
      }
      fprintf(f, "\n");
      for (int j = 0; j < log.events[i].num_entries; j++) {
        fprintf(f, "%-24d", j);
        if(!log.events[i].simple){
          for (int k = 0; k < log.num_counters; k++) {
            fprintf(f, "%-24"PRIu64, log.events[i].entries[j].counter_totals[k]);
          }
        }
        fprintf(f, "%-24f", hpx_time_ms(log.events[i].entries[j].start_time));
        if(total_time_ms != 0){
          fprintf(f, "%-24f", hpx_time_ms(log.events[i].entries[j].run_time));
        }
        if(total != 0){
          fprintf(f, "%-24f", log.events[i].entries[j].user_val);
        }
        fprintf(f, "\n");
      }
    }
  }

  int e = fclose(f);
  if (e != 0) {
    log_error("failed to write profiling output to %s\n", filepath);
  }

  free(filepath);
}

void inst_vtrace(int UNUNSED, int n, int id, ...) {
  dbg_assert_str(n < 5, "can only trace up to 4 user values\n");
  logtable_t *log = &_logs[id];
  if (!log->records) {
    return;
  }

  uint64_t args[4];
  va_list vargs;
  va_start(vargs, id);
  for (int i = 0; i < n; ++i) {
    args[i] = va_arg(vargs, uint64_t);
  }
  va_end(vargs);
  for (int i = n; i < 4; ++i) {
    args[i] = 0;
  }
  logtable_append(log, args[0], args[1], args[2], args[3]);
}

