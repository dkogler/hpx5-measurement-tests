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

#ifndef LIBHPX_SCHEDULER_EVENTS_H
#define LIBHPX_SCHEDULER_EVENTS_H

/// @file libhpx/scheduler/events.h
/// @short Define the set of scheduler events that we know how to trace.

#ifdef __cplusplus
extern "C" {
#endif

#include <libhpx/action.h>
#include <libhpx/instrumentation.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>

static inline void EVENT_WQSIZE(worker_t *w) {
  static const int type = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_WQSIZE;
  inst_trace(type, id,
             sync_chase_lev_ws_deque_size(&w->queues[w->work_id].work));
}

static inline void EVENT_PUSH_LIFO(hpx_parcel_t *p) {
  static const int type = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_PUSH_LIFO;
  inst_trace(type, id, p);
}

static inline void EVENT_POP_LIFO(hpx_parcel_t *p) {
  static const int type = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_POP_LIFO;
  inst_trace(type, id, p);
}

static inline void EVENT_STEAL_LIFO(hpx_parcel_t *p, const worker_t *victim) {
  static const int type = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_STEAL_LIFO;
  inst_trace(type, id, p, victim->id);
}

static inline void EVENT_THREAD_RUN(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    CHECK_ACTION(p->action);
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RUN;
  inst_trace(type, id, p->id, p->action, p->size);
}

static inline void EVENT_THREAD_END(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_END;
  inst_trace(type, id, p->id, p->action);
}

static inline void EVENT_THREAD_SUSPEND(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_SUSPEND;
  inst_trace(type, id, p->id, p->action);
}

static inline void EVENT_THREAD_RESUME(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RESUME;
  inst_trace(type, id, p->id, p->action);
}

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_SCHEDULER_EVENTS_H
