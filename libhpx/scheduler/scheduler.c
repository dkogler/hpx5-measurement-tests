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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_APEX
# include <apex.h>
#endif

#include "hpx/builtins.h"
#include "libhpx/action.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "thread.h"

static void _bind_self(worker_t *worker) {
  dbg_assert(worker);

  if (self && self != worker) {
    dbg_error("HPX does not permit worker structure switching.\n");
  }
  self = worker;
  self->thread = pthread_self();
}

/// The pthread entry function for dedicated worker threads.
///
/// This is used by _create().
static void *_run(void *worker) {
  dbg_assert(here);
  dbg_assert(here->gas);
  dbg_assert(worker);

  _bind_self(worker);

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

#ifdef HAVE_APEX
  // let APEX know there is a new thread
  apex_register_thread("HPX WORKER THREAD");
#endif

  // wait for the other threads to join
  system_barrier_wait(&here->sched->barrier);

  if (worker_start()) {
    dbg_error("failed to start processing lightweight threads.\n");
    return NULL;
  }

#ifdef HAVE_APEX
  // let APEX know the thread is exiting
  apex_exit_thread();
#endif

  // leave the global address space
  as_leave();

  // unbind self and return NULL
  return (self = NULL);
}

static int _create(worker_t *worker, const config_t *cfg) {
  pthread_t thread;

  int e = pthread_create(&thread, NULL, _run, worker);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    return e;
  }
  return LIBHPX_OK;
}

static void _join(worker_t *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  int e = pthread_join(worker->thread, NULL);
  if (e) {
    dbg_error("cannot join worker thread %d (%s).\n", worker->id, strerror(e));
  }
}

/// Cancel a worker thread.
static void _cancel(const worker_t *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
}

struct scheduler *scheduler_new(const config_t *cfg) {
  const int workers = cfg->threads;

  struct scheduler *s = malloc(sizeof(*s));
  if (!s) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }

  size_t r = HPX_CACHELINE_SIZE - sizeof(s->workers[0]) % HPX_CACHELINE_SIZE;
  size_t padded_size = sizeof(s->workers[0]) + r;
  size_t total = workers * padded_size;
  int e = posix_memalign((void**)&s->workers, HPX_CACHELINE_SIZE, total);
  if (e) {
    dbg_error("could not allocate a worker array.\n");
    scheduler_delete(s);
    return NULL;
  }

  for (int i = 0; i < workers; ++i) {
    e = worker_init(&s->workers[i], i, i, 64);
    if (e) {
      dbg_error("failed to initialize a worker.\n");
      scheduler_delete(s);
      return NULL;
    }
  }

  e = system_barrier_init(&s->barrier, NULL, workers);
  if (e) {
    dbg_error("failed to allocate the scheduler barrier.\n");
    scheduler_delete(s);
    return NULL;
  }

  // initialize the run state.
  sync_store(&s->stopped, SCHED_STOP, SYNC_RELEASE);
  s->run_state.state = SCHED_STOP;
  s->run_state.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  s->run_state.running = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

  sync_store(&s->next_tls_id, 0, SYNC_RELEASE);
  s->n_workers    = workers;
  s->n_active_workers = workers;
  s->wf_threshold = cfg->sched_wfthreshold;

  thread_set_stack_size(cfg->stacksize);
  log_sched("initialized a new scheduler.\n");

  // bind a worker for this thread so that we can spawn lightweight threads
  _bind_self(&s->workers[0]);

  log_sched("worker 0 ready.\n");
  return s;
}

void scheduler_delete(struct scheduler *sched) {
  if (!sched) {
    return;
  }

  // shut everyone down
  pthread_mutex_lock(&sched->run_state.lock);
  sched->run_state.state = SCHED_SHUTDOWN;
  pthread_cond_broadcast(&sched->run_state.running);
  pthread_mutex_unlock(&sched->run_state.lock);

  worker_t *worker = NULL;
  for (int i = 1; i < sched->n_workers; ++i) {
    worker = scheduler_get_worker(sched, i);
    _join(worker);
  }
  log_sched("joined worker threads.\n");
  // unbind this thread's worker
  self = NULL;

  system_barrier_destroy(&sched->barrier);

  if (sched->workers) {
    for (int i = 0, e = sched->n_workers; i < e; ++i) {
      worker_t *worker = scheduler_get_worker(sched, i);
      worker_fini(worker);
    }
    free(sched->workers);
  }

  free(sched);
}

worker_t *scheduler_get_worker(struct scheduler *sched, int id) {
  assert(id >= 0);
  assert(id < sched->n_workers);
  return &sched->workers[id];
}

int scheduler_restart(struct scheduler *sched) {
  int status;

  // notify everyone that they don't have to keep checking the lock.
  sync_store(&sched->stopped, SCHED_RUN, SYNC_RELEASE);

  // now switch the state of the running condition
  pthread_mutex_lock(&sched->run_state.lock);
  sched->run_state.state = SCHED_RUN;
  pthread_cond_broadcast(&sched->run_state.running);
  pthread_mutex_unlock(&sched->run_state.lock);

  status = worker_start();

  // now switch the state to stopped
  pthread_mutex_lock(&sched->run_state.lock);
  sched->run_state.state = SCHED_STOP;
  pthread_mutex_unlock(&sched->run_state.lock);

  return status;
}

int scheduler_startup(struct scheduler *sched, const config_t *cfg) {
  worker_t *worker = NULL;
  int status = LIBHPX_OK;

  // start all of the other worker threads
  for (int i = 1, e = sched->n_workers; i < e; ++i) {
    worker = scheduler_get_worker(sched, i);
    status = _create(worker, cfg);

    if (status != LIBHPX_OK) {
      dbg_error("could not start worker %d.\n", i);

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        _cancel(worker);
      }

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        _join(worker);
      }

      return status;
    }
  }

  // wait for the other slave worker threads to launch
  system_barrier_wait(&sched->barrier);

  return status;
}

void scheduler_stop(struct scheduler *sched, int code) {
  sync_store(&sched->stopped, code, SYNC_RELEASE);
}

int scheduler_is_stopped(struct scheduler *sched) {
  int stopped = sync_load(&sched->stopped, SYNC_ACQUIRE);
  return (stopped != SCHED_RUN);
}
