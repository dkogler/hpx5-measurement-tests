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

/// @file libhpx/scheduler/apex.c

#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>

#ifndef HAVE_APEX
int worker_is_active(void) {
  return 1;
}

int worker_is_stopped(void) {
  return scheduler_is_stopped(here->sched);
}
#else
# include <pthread.h>
# include <apex.h>
# include <apex_policies.h>

/// This is the condition that "sleeping" threads will wait on
static pthread_cond_t _release = PTHREAD_COND_INITIALIZER;

/// Mutex for the pthread_cond_wait() call. Typically, it is used
/// to save the shared state, but we don't need it for our use.
static pthread_mutex_t _release_mutex = PTHREAD_MUTEX_INITIALIZER;

static void _apex_wait(void) {
  pthread_mutex_lock(&_release_mutex);
  pthread_cond_wait(&_release, &_release_mutex);
  pthread_mutex_unlock(&_release_mutex);
}

static void _apex_signal(void) {
  pthread_cond_signal(&_release);
}

/// Try to deactivate a worker.
///
/// @returns          1 If the worker remains active, 0 if it was deactivated
static int _apex_try_deactivate(volatile int *n_active_workers) {
  if (sync_fadd(n_active_workers, -1, SYNC_ACQ_REL) > apex_get_thread_cap()) {
    self->active = false;
    apex_set_state(APEX_THROTTLED);
    return 0;
  }

  sync_fadd(n_active_workers, 1, SYNC_ACQ_REL);
  return 1;
}

/// Try to reactivate an inactive worker.
///
/// @returns          1 If the thread reactivated, 0 if it is still inactive.
static int _apex_try_reactivate(volatile int *n_active_workers) {
  if (sync_fadd(n_active_workers, 1, SYNC_ACQ_REL) <= apex_get_thread_cap()) {
    // I fit inside the cap!
    self->active = true;
    apex_set_state(APEX_BUSY);
    return 1;
  }

  sync_fadd(n_active_workers, -1, SYNC_ACQ_REL);
  return 0;
}

/// This function will check whether the current thread in the
/// scheduling loop should be throttled.
///
/// @returns          1 If the thread is active, 0 if it is inactive.
static int _apex_check_active(void) {
  // akp: throttling code -- if the throttling flag is set don't use
  // some threads NB: There is a possible race condition here. It's
  // possible that mail needs to be checked again just before
  // throttling.
  if (!apex_get_throttle_concurrency()) {
    return 1;
  }

  // we use this address a bunch of times, so just remember it
  volatile int * n_active_workers = &(here->sched->n_active_workers);

  if (!self->active) {
    // because I can't change the power level, sleep instead.
    _apex_wait();
    return _apex_try_reactivate(n_active_workers);
  }

  if (!apex_throttleOn || self->yielded) {
    return 1;
  }

  // If there are too many threads running, then try and become inactive.
  if (sync_load(n_active_workers, SYNC_ACQUIRE) > apex_get_thread_cap()) {
    return _apex_try_deactivate(n_active_workers);
  }

  // Go ahead and signal the condition variable if we need to
  if (sync_load(n_active_workers, SYNC_ACQUIRE) < apex_get_thread_cap()) {
    _apex_signal();
  }

  return 1;
}

/// release idle threads, stop any running timers, and exit the thread from APEX
static void _apex_worker_shutdown(void) {
  pthread_cond_broadcast(&_release);
  worker_t *w = self;
  if (w->profiler != NULL) {
    apex_stop(w->profiler);
    w->profiler = NULL;
  }
}

int worker_is_active(void) {
  return _apex_check_active();
}

int worker_is_stopped(void) {
  int e = scheduler_is_stopped(here->sched);
  if (e) {
    _apex_worker_shutdown();
  }
  return e;
}
#endif
