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

#ifndef LIBHPX_EVENTS_H
#define LIBHPX_EVENTS_H

/// @file include/libhpx/events.h
/// @short Define the set of instrumentation events that we know how to trace.

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/builtins.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/instrumentation.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>

/// Tracing events.
typedef enum {
#define LIBHPX_EVENT(class, event, ...) TRACE_EVENT_##class##_##event,
# include "events.def"
#undef LIBHPX_EVENT
} libhpx_trace_events_t;

static const char *const TRACE_EVENT_TO_STRING[] = {
#define LIBHPX_EVENT(class, event, ...) _HPX_XSTR(class##_##event),
# include "events.def"
#undef LIBHPX_EVENT
};

static const int TRACE_EVENT_NUM_FIELDS[] = {
#define LIBHPX_EVENT(class, event, ...) __HPX_NARGS(__VA_ARGS__) + 1,
# include "events.def"
#undef LIBHPX_EVENT
};

#define TRACE_NUM_EVENTS _HPX_NELEM(TRACE_EVENT_TO_STRING)

static const int TRACE_OFFSETS[] = {
  TRACE_EVENT_PARCEL_CREATE,
  TRACE_EVENT_NETWORK_PWC_SEND,
  TRACE_EVENT_SCHED_WQSIZE,
  TRACE_EVENT_LCO_INIT,
  TRACE_EVENT_PROCESS_NEW,
  TRACE_EVENT_MEMORY_REGISTERED_ALLOC,
  TRACE_EVENT_SCHEDTIMES_SCHED,
  TRACE_EVENT_BOOKEND_BOOKEND,
  TRACE_EVENT_GAS_ACCESS,
  TRACE_NUM_EVENTS
};

/// Trace event macros.
#ifndef ENABLE_INSTRUMENTATION
# include <libhpx/event_stubs.h>

# define EVENT_THREAD_RUN(...)
# define EVENT_THREAD_END(...)
# define EVENT_THREAD_SUSPEND(...)
# define EVENT_THREAD_RESUME(...)

#else

#define _DECL0() void
#define _DECL1(t0) t0 u0
#define _DECL2(t0,t1) t0 u0, t1 u1
#define _DECL3(t0,t1,t2) t0 u0, t1 u1, t2 u2
#define _DECL4(t0,t1,t2,t3) t0 u0, t1 u1, t2 u2, t3 u3
#define _DECL5(t0,t1,t2,t3,t4) t0 u0, t1 u1, t2 u2, t3 u3, t4 u4
#define _ARGS0
#define _ARGS1 , u0
#define _ARGS2 , u0, u1
#define _ARGS3 , u0, u1, u2
#define _ARGS4 , u0, u1, u2, u3
#define _ARGS5 , u0, u1, u2, u3, u4
#define LIBHPX_EVENT(class, event, ...)                                 \
  static inline void                                                    \
  EVENT_##class##_##event(_HPX_CAT2(_DECL, __HPX_NARGS(__VA_ARGS__))(__VA_ARGS__)) { \
    inst_trace(HPX_TRACE_##class, TRACE_EVENT_##class##_##event         \
              _HPX_CAT2(_ARGS, __HPX_NARGS(__VA_ARGS__)));              \
  }
# include "events.def"
#undef LIBHPX_EVENT
#undef _ARGS0
#undef _ARGS1
#undef _ARGS2
#undef _ARGS3
#undef _ARGS4
#undef _ARGS5
#undef _DECL0
#undef _DECL1
#undef _DECL2
#undef _DECL3
#undef _DECL4
#undef _DECL5

/// Thread tracing events.
/// @{
static inline void EVENT_THREAD_RUN(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    CHECK_ACTION(p->action);
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RUN(p->id, p->action, p->size);
}

static inline void EVENT_THREAD_END(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  EVENT_PARCEL_END(p->id, p->action);
}

static inline void EVENT_THREAD_SUSPEND(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  EVENT_PARCEL_SUSPEND(p->id, p->action);
}

static inline void EVENT_THREAD_RESUME(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RESUME(p->id, p->action);
}
/// @}
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_EVENTS_H
