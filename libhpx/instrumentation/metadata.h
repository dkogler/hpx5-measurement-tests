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

#ifndef METADATA_H
#define METADATA_H

#include <stddef.h>
#include <stdint.h>
#include <libhpx/instrumentation.h>
#include <libhpx/events.h>

// ==================== Event data =============================================

typedef struct record {
  int worker;
  uint64_t user[0];
} record_t;

#define _COL_OFFSET_WORKER    offsetof(record_t, worker)
#define _COL_OFFSET_NS        offsetof(record_t, user)
#define _COL_OFFSET_USER(off) offsetof(record_t, user) + ((off + 1) * 8)

// ==================== Event metadata =========================================
// Header file format:
//
// Magic file identifier bytes = 
//   {'h', 'p', 'x', ' ', 'l', 'o', 'g', '\0', 0xFF, 0x00, 0xAA, 0x55}
// table offset
// [metadata-id, length, data]*
//
//
//
// * "table offset" is 4 bytes 
// Then,
// * "metadata-id" is 4 bytes 
// * "length" is 4 bytes, indicates how many bytes to read for the data 
//   portion of this header entry
// * the data is a set of bytes, interpreted in a context-specific manner
// 
// Metadata-id numbers:
// -1 -- Named value
// 0 -- types
// 1 -- offsets (a list of ints)
// 2 -- names (pipe separated list of characters)
// 3 -- printf codes (pipe separated list)
// 4 -- min values per column
// 5 -- max values per column
//
// The 'data' portion of a named value is [type, value, label].  The 'length' of
// the entry indicates how long this triple is.  Type is a char, value is as
// long as indicated by the type. Label is an string of chars.
//
// Type details:
// i: int    -- 4 bytes
// l: long   -- 8 bytes
// s: short  -- 2 bytes
// d: double -- 8 bytes
// f: float  -- 4 bytes
// b: byte   -- 1 bytes
// c: char   -- 2 bytes

#define METADATA_TYPE_NAMED_VALUE  -1
#define METADATA_TYPE_DATA_TYPES   0
#define METADATA_TYPE_OFFSETS      1
#define METADATA_TYPE_NAMES        2
#define METADATA_TYPE_PRINTF_CODES 3
#define METADATA_TYPE_MINS         4
#define METADATA_TYPE_MAXS         5

// The signed types are also used for unsigned; there is not distinct unsigned
// type for these headers.
#define METADATA_TYPE_BYTE   'b'
#define METADATA_TYPE_CHAR   'c'
#define METADATA_TYPE_INT32  'i'
#define METADATA_TYPE_INT64  'l'
#define METADATA_TYPE_INT16  's'
#define METADATA_TYPE_DOUBLE 'd'
#define METADATA_TYPE_FLOAT  'f'

typedef struct inst_named_value {
  const char        type;
  const uint32_t   value;
  const char     name[8];
} HPX_PACKED inst_named_value_t;

typedef struct inst_event_col_metadata {
  const char mask;           //!< this should an OR of all the following values:
  const char data_type;      //!< mask 0x1
  const unsigned int offset; //!< mask 0x2
  const uint64_t min;        //!< mask 0x4
  const uint64_t max;        //!< mask 0x8
  const char printf_code[8]; //!< mask 0x10 (this value must be nul terminated)
  const char name[256];      //!< mask 0x20 (this value must be nul terminated)
} inst_event_col_metadata_t;

#define METADATA_WORKER                       \
  { .mask = 0x3f,                             \
    .data_type = METADATA_TYPE_INT32,         \
    .offset = _COL_OFFSET_WORKER,             \
    .min = 0,                                 \
    .max = INT_MAX,                           \
    .printf_code = "d",                       \
    .name = "worker"}

#define METADATA_NS                           \
  { .mask = 0x3f,                             \
    .data_type = METADATA_TYPE_INT64,         \
    .offset = _COL_OFFSET_NS,                 \
    .min = 0,                                 \
    .max = 1e9-1,                             \
    .printf_code = "zu",                      \
    .name = "nanoseconds"}

#define METADATA_UINT(width, off, _name)      \
  { .mask = 0x3f,                             \
    .data_type = METADATA_TYPE_INT64,         \
    .offset = _COL_OFFSET_USER(off),          \
    .min = 0,                                 \
    .max = UINT##width##_MAX,                 \
    .printf_code = "zu",                      \
    .name = _name}

#define METADATA_UINT16(off, _name) METADATA_UINT(16, off, _name)
#define METADATA_UINT32(off, _name) METADATA_UINT(32, off, _name)
#define METADATA_UINT64(off, _name) METADATA_UINT(64, off, _name)

/// Helper metadata generation macros for commonly used types

#define METADATA_EMPTY(off)         METADATA_UINT64(off, "empty")

#define METADATA_SIZE(off)          METADATA_UINT64(off, "size")
#define METADATA_ACTION(off)        METADATA_UINT64(off, "action")
#define METADATA_HPX_ADDR(off)      METADATA_UINT64(off, "global address")
#define METADATA_PTR(off)           METADATA_UINT64(off, "local address")

/// Event metadata struct.
/// In theory the number of columns need not match the number of fields in
/// an event. In practice, right now they do.
typedef struct inst_event_metadata {
  const int num_cols;
  const inst_event_col_metadata_t *col_metadata;
} inst_event_metadata_t;

#define _METADATA_ARGS(...) {                                       \
  .num_cols = __HPX_NARGS(__VA_ARGS__)+2,                           \
  .col_metadata =                                                   \
    (const inst_event_col_metadata_t[__HPX_NARGS(__VA_ARGS__)+2]) { \
    METADATA_WORKER,                                                \
    METADATA_NS,                                                    \
    __VA_ARGS__                                                     \
  }                                                                 \
}
#define _METADATA_NONE _METADATA_ARGS()

extern const inst_event_metadata_t INST_EVENT_METADATA[TRACE_NUM_EVENTS];

#define METADATA_PARCEL_ID(off)        METADATA_UINT64(off, "parcel id")
#define METADATA_PARCEL_SIZE(off)      METADATA_UINT64(off, "parcel size")
#define METADATA_PARCEL_SOURCE(off)    METADATA_UINT64(off, "source addr")
#define METADATA_PARCEL_PARENT_ID(off) METADATA_UINT64(off, "parent id")
#define METADATA_PARCEL_TARGET(off)    METADATA_UINT64(off, "target addr")

#define PARCEL_CREATE_METADATA                               \
  _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1),  \
                 METADATA_PARCEL_SIZE(2),                    \
                 METADATA_PARCEL_PARENT_ID(3))

#define PARCEL_SEND_METADATA                                 \
  _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1),  \
                 METADATA_PARCEL_SIZE(2),                    \
                 METADATA_PARCEL_SOURCE(3),                  \
                 METADATA_PARCEL_TARGET(4))

#define PARCEL_RECV_METADATA                                 \
 _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1),   \
                 METADATA_PARCEL_SIZE(2),                    \
                 METADATA_PARCEL_SOURCE(3),                  \
                 METADATA_PARCEL_TARGET(4))

#define PARCEL_RUN_METADATA                                  \
  _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1),  \
                 METADATA_PARCEL_SIZE(2))

#define PARCEL_END_METADATA                                  \
  _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1))

#define PARCEL_SUSPEND_METADATA                              \
 _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1))

#define PARCEL_RESUME_METADATA                               \
 _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1))

#define PARCEL_RESEND_METADATA                               \
 _METADATA_ARGS(METADATA_PARCEL_ID(0), METADATA_ACTION(1),   \
                METADATA_PARCEL_SIZE(2),                     \
                METADATA_PARCEL_TARGET(3))

#define NETWORK_PWC_SEND_METADATA _METADATA_NONE
#define NETWORK_PWC_RECV_METADATA _METADATA_NONE

#define SCHED_WQSIZE_METADATA                                \
  _METADATA_ARGS(METADATA_UINT64(0, "work_queue_size"))

#define SCHED_PUSH_LIFO_METADATA  _METADATA_NONE
#define SCHED_POP_LIFO_METADATA   _METADATA_NONE
#define SCHED_STEAL_LIFO_METADATA _METADATA_NONE
#define SCHED_ENTER_METADATA      _METADATA_NONE
#define SCHED_EXIT_METADATA       _METADATA_NONE

#define METADATA_LCO_ADDRESS(off) METADATA_UINT64(off, "lco address")
#define METADATA_LCO_STATE(off)   METADATA_UINT64(off, "lco state")

#define LCO_INIT_METADATA                                   \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_DELETE_METADATA                                 \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_SET_METADATA                                    \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_RESET_METADATA                                  \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_ATTACH_PARCEL_METADATA                          \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_WAIT_METADATA                                   \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))

#define LCO_TRIGGER_METADATA                                \
  _METADATA_ARGS(METADATA_LCO_ADDRESS(0),                   \
                 METADATA_LCO_STATE(1))


#define METADATA_PROCESS_ADDRESS(off) METADATA_UINT64(off, "process address")
#define METADATA_PROCESS_LCO(off)     METADATA_UINT64(off, "termination lco")

#define PROCESS_NEW_METADATA                                \
  _METADATA_ARGS(METADATA_PROCESS_ADDRESS(0),               \
                 METADATA_PROCESS_LCO(1))

#define PROCESS_CALL_METADATA                               \
  _METADATA_ARGS(METADATA_PROCESS_ADDRESS(0),               \
                 METADATA_PARCEL_ID(1))

#define PROCESS_DELETE_METADATA                             \
  _METADATA_ARGS(METADATA_PROCESS_ADDRESS(0))               \

#define MEMORY_REGISTERED_ALLOC_METADATA _METADATA_NONE
#define MEMORY_REGISTERED_FREE_METADATA  _METADATA_NONE
#define MEMORY_GLOBAL_ALLOC_METADATA     _METADATA_NONE
#define MEMORY_GLOBAL_FREE_METADATA      _METADATA_NONE
#define MEMORY_CYCLIC_ALLOC_METADATA     _METADATA_NONE
#define MEMORY_CYCLIC_FREE_METADATA      _METADATA_NONE
#define MEMORY_ENTER_ALLOC_FREE_METADATA _METADATA_NONE

#define METADATA_SCHEDTIMES_SCHED_SOURCE(off)   \
  METADATA_UINT64(off, "parcel source")

#define METADATA_SCHEDTIMES_SCHED_SPINS(off)   \
  METADATA_UINT64(off, "spins")

#define SCHEDTIMES_SCHED_METADATA                          \
  _METADATA_ARGS(METADATA_SCHEDTIMES_SCHED_SOURCE(0),      \
                     METADATA_SCHEDTIMES_SCHED_SPINS(0))

#define SCHEDTIMES_PROBE_METADATA _METADATA_NONE

#define SCHEDTIMES_PROGRESS_METADATA _METADATA_NONE

#define BOOKEND_BOOKEND_METADATA _METADATA_NONE

#define GAS_ACCESS_METADATA                                \
  _METADATA_ARGS(METADATA_UINT64(0, "source"),             \
                 METADATA_UINT64(1, "destination"),        \
                 METADATA_UINT64(2, "target"),             \
                 METADATA_SIZE(3))

#endif
