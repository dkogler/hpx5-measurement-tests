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
#include "config.h"
#endif

#include <stdio.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include "libsync/locks.h"
#include "hpx/hpx.h"

#include "libhpx/action.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/system.h"

static char *_get_hostname(char *hostname, int size) {
  gethostname(hostname, size);
  return hostname;
}

// Used for debugging. Causes a process to wait for a debugger to
// attach, and set the value if i != 0.
HPX_NO_OPTIMIZE void dbg_wait(void) {
  int i = 0;
  char hostname[256];
  _get_hostname(hostname, 255);
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}

static void __print(FILE *file, unsigned line, const char *filename,
                    const char *func, const char *fmt, va_list *list) {
  int tid = hpx_get_my_thread_id();
  int rank = hpx_get_my_rank();
  fprintf(file, "LIBHPX<%d,%d>: (%s:%s:%u) ", rank, tid, basename((char*)filename), func, line);
  vfprintf(file, fmt, *list);
  fflush(file);
}

/// Helper macro, extracts the va-args and forwards to __print.
#define _print(file, line, filename, func, fmt)      \
  do {                                               \
    va_list args;                                    \
    va_start(args, fmt);                             \
    __print(file, line, filename, func, fmt, &args); \
    va_end(args);                                    \
  } while (0)

void dbg_error_internal(unsigned line, const char *filename, const char *func,
                       const char *fmt, ...) {
  _print(stderr, line, filename, func, fmt);
  hpx_abort();
}

uint64_t log_level = HPX_LOG_DEFAULT;

static tatas_lock_t _log_lock = SYNC_TATAS_LOCK_INIT;

void log_internal(unsigned line, const char *filename, const char *func,
                  const char *fmt, ...) {
  sync_tatas_acquire(&_log_lock);
  _print(stdout, line, filename, func, fmt);
  sync_tatas_release(&_log_lock);
}

int log_error_internal(unsigned line, const char *filename, const char *func,
                       const char *fmt, ...) {
  sync_tatas_acquire(&_log_lock);
  _print(stderr, line, filename, func, fmt);
  sync_tatas_release(&_log_lock);
  return HPX_ERROR;
}

/// This is unsafe because we can't use gethostname or printf in a signal
/// handler, particularly a SEGV handler.
static void _dbg_wait_on_sig(int signum) {
  dbg_wait();
}

static int _register_wait_on_sig(int signum) {
  struct sigaction segv = {
    .sa_handler = _dbg_wait_on_sig,
    .sa_flags = 0
  };
  sigemptyset (&segv.sa_mask);

  if (-1 == sigaction(signum, &segv, NULL)) {
    log_error("could not register _dbg_wait_on_sig for signal(%d)\n", signum);
    return LIBHPX_ERROR;
  }
  else {
    log_dflt("registered _dbg_wait_on_sig for signal(%d)\n", signum);
  }
  return LIBHPX_OK;
}

int dbg_init(config_t *config) {
  int e = LIBHPX_OK;
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGSEGV)) {
    e = _register_wait_on_sig(SIGSEGV);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGABRT)) {
    e = _register_wait_on_sig(SIGABRT);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGFPE)) {
    e = _register_wait_on_sig(SIGFPE);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGILL)) {
    e = _register_wait_on_sig(SIGILL);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGBUS)) {
    e = _register_wait_on_sig(SIGBUS);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGIOT)) {
    e = _register_wait_on_sig(SIGIOT);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGSYS)) {
    e = _register_wait_on_sig(SIGSYS);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  if (config_dbg_waitonsig_isset(config, HPX_WAITON_SIGTRAP)) {
    e = _register_wait_on_sig(SIGTRAP);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  return e;
}

void dbg_fini(void) {
}
