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

#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdint.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <libhpx/config.h>
#include <libhpx/locality.h> // for here object inlined in inst_trace_type
#include <libhpx/profiling.h>

struct config;

//hostnames can only be 63 characters in length, so
#define HOSTNAME_LENGTH 64

#ifdef ENABLE_INSTRUMENTATION
/// INSTRUMENTATION is true if and only if instrumentation is enabled
# define INSTRUMENTATION 1
/// INST will do @p stmt only if instrumentation is enabled
# define INST(stmt) stmt;
#else
/// INSTRUMENTATION is true if and only if instrumentation is enabled
# define INSTRUMENTATION 0
/// INST will do @p stmt only if instrumentation is enabled
# define INST(stmt)
#endif

/// Initialize instrumentation. This is usually called in hpx_init().
int inst_init(struct config *cfg)
  HPX_NON_NULL(1);

/// "Start" instrumentation. This is usually called in hpx_run(). This takes
/// care of some things that must be done after initialization is complete,
/// specifically action registration.
int inst_start();


void inst_fini(void);

/// Dump all of the profiling information to file
void inst_prof_dump(profile_log_t profile_log);

/// Record an event to the log
/// @param        type Type this event is part of (see hpx_inst_class_type_t)
/// @param           id The event id (see hpx_inst_event_type_t)
/// @param            n The number of user arguments to log, < 5.
void inst_vtrace(int type, int n, int id, ...);

#ifdef ENABLE_INSTRUMENTATION
# define inst_trace(type, ...)                                 \
  inst_vtrace(type, __HPX_NARGS(__VA_ARGS__) - 1, __VA_ARGS__)
#else
# define inst_trace(type, id, ...)             \
  do {                                          \
    (void)type;                                \
    (void)id;                                   \
  } while (0)
#endif

// This matches the order in config.h trace_t.
#define      HPX_INST_CLASS_PARCEL INT32_C(0)
#define HPX_INST_CLASS_NETWORK_PWC INT32_C(1)
#define                 INST_SCHED INT32_C(2)
#define         HPX_INST_CLASS_LCO INT32_C(3)
#define     HPX_INST_CLASS_PROCESS INT32_C(4)
#define      HPX_INST_CLASS_MEMORY INT32_C(5)
#define        HPX_INST_SCHEDTIMES INT32_C(6)
#define       HPX_INST_NUM_CLASSES INT32_C(7)

static const char * const INST_CLASS_TO_STRING[] = {
  "CLASS_PARCEL",
  "CLASS_NETWORK_PWC",
  "CLASS_SCHED",
  "CLASS_LCO",
  "CLASS_PROCESS",
  "CLASS_MEMORY",
  "CLASS_SCHEDTIMES"
};

#define  HPX_INST_EVENT_PARCEL_CREATE INT32_C(0)
#define    HPX_INST_EVENT_PARCEL_SEND INT32_C(1)
#define    HPX_INST_EVENT_PARCEL_RECV INT32_C(2)
#define     HPX_INST_EVENT_PARCEL_RUN INT32_C(3)
#define     HPX_INST_EVENT_PARCEL_END INT32_C(4)
#define HPX_INST_EVENT_PARCEL_SUSPEND INT32_C(5)
#define  HPX_INST_EVENT_PARCEL_RESUME INT32_C(6)
#define  HPX_INST_EVENT_PARCEL_RESEND INT32_C(7)

#define HPX_INST_EVENT_NETWORK_PWC_SEND INT32_C(8)
#define HPX_INST_EVENT_NETWORK_PWC_RECV INT32_C(9)

#define     HPX_INST_EVENT_SCHED_WQSIZE INT32_C(10)
#define  HPX_INST_EVENT_SCHED_PUSH_LIFO INT32_C(11)
#define   HPX_INST_EVENT_SCHED_POP_LIFO INT32_C(12)
#define HPX_INST_EVENT_SCHED_STEAL_LIFO INT32_C(13)

#define          HPX_INST_EVENT_LCO_INIT INT32_C(14)
#define        HPX_INST_EVENT_LCO_DELETE INT32_C(15)
#define           HPX_INST_EVENT_LCO_SET INT32_C(16)
#define         HPX_INST_EVENT_LCO_RESET INT32_C(17)
#define HPX_INST_EVENT_LCO_ATTACH_PARCEL INT32_C(18)
#define          HPX_INST_EVENT_LCO_WAIT INT32_C(19)
#define       HPX_INST_EVENT_LCO_TRIGGER INT32_C(20)

#define    HPX_INST_EVENT_PROCESS_NEW INT32_C(21)
#define   HPX_INST_EVENT_PROCESS_CALL INT32_C(22)
#define HPX_INST_EVENT_PROCESS_DELETE INT32_C(23)

#define HPX_INST_EVENT_MEMORY_REGISTERED_ALLOC INT32_C(24)
#define  HPX_INST_EVENT_MEMORY_REGISTERED_FREE INT32_C(25)
#define     HPX_INST_EVENT_MEMORY_GLOBAL_ALLOC INT32_C(26)
#define      HPX_INST_EVENT_MEMORY_GLOBAL_FREE INT32_C(27)
#define     HPX_INST_EVENT_MEMORY_CYCLIC_ALLOC INT32_C(28)
#define      HPX_INST_EVENT_MEMORY_CYCLIC_FREE INT32_C(29)

#define              HPX_INST_SCHEDTIMES_SCHED INT32_C(30)
#define              HPX_INST_SCHEDTIMES_PROBE INT32_C(31)
#define           HPX_INST_SCHEDTIMES_PROGRESS INT32_C(32)

#define                    HPX_INST_NUM_EVENTS INT32_C(33)

static const char * const INST_EVENT_TO_STRING[] = {
  "EVENT_PARCEL_CREATE",
  "EVENT_PARCEL_SEND",
  "EVENT_PARCEL_RECV",
  "EVENT_PARCEL_RUN",
  "EVENT_PARCEL_END",
  "EVENT_PARCEL_SUSPEND",
  "EVENT_PARCEL_RESUME",
  "EVENT_PARCEL_RESEND",

  "EVENT_NETWORK_PWC_SEND",
  "EVENT_NETWORK_PWC_RECV",

  "EVENT_SCHED_WQSIZE",
  "EVENT_SCHED_PUSH_LIFO",
  "EVENT_SCHED_POP_LIFO",
  "EVENT_SCHED_STEAL_LIFO",

  "EVENT_LCO_INIT",
  "EVENT_LCO_DELETE",
  "EVENT_LCO_SET",
  "EVENT_LCO_RESET",
  "EVENT_LCO_ATTACH_PARCEL",
  "EVENT_LCO_WAIT",
  "EVENT_LCO_TRIGGER",

  "EVENT_PROCESS_NEW",
  "EVENT_PROCESS_CALL",
  "EVENT_PROCESS_DELETE",

  "EVENT_MEMORY_REGISTERED_ALLOC",
  "EVENT_MEMORY_REGISTERED_FREE",
  "EVENT_MEMORY_GLOBAL_ALLOC",
  "EVENT_MEMORY_GLOBAL_FREE",
  "EVENT_MEMORY_CYCLIC_ALLOC",
  "EVENT_MEMORY_CYCLIC_FREE",

  "EVENT_SCHEDTIMES_SCHED",
  "EVENT_SCHEDTIMES_PROBE",
  "EVENT_SCHEDTIMES_PROGRESS"
};

static const int INST_OFFSETS[] = {
  HPX_INST_EVENT_PARCEL_CREATE,
  HPX_INST_EVENT_NETWORK_PWC_SEND,
  HPX_INST_EVENT_SCHED_WQSIZE,
  HPX_INST_EVENT_LCO_INIT,
  HPX_INST_EVENT_PROCESS_NEW,
  HPX_INST_EVENT_MEMORY_REGISTERED_ALLOC,
  HPX_INST_SCHEDTIMES_SCHED,
  HPX_INST_NUM_EVENTS
};

static inline bool inst_trace_class(int type) {
  return config_trace_classes_isset(here->config, 1 << type);
}

#endif
