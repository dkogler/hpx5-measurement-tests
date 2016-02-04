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

#ifndef INSTRUMENTATION_EVENTS_H
#define INSTRUMENTATION_EVENTS_H

#include <stddef.h>
#include <stdint.h>
#include <libhpx/instrumentation.h>

// ==================== Event data =============================================

typedef struct record {
  int worker;
  uint64_t ns;
  uint64_t user[4];
} record_t;

/// The number of columns for a recorded event; some may be unused
#define INST_EVENT_NUM_COLS 6

#define INST_EVENT_COL_OFFSET_WORKER offsetof(record_t, worker)
#define INST_EVENT_COL_OFFSET_NS     offsetof(record_t, ns)
#define INST_EVENT_COL_OFFSET_USER0  offsetof(record_t, user)
#define INST_EVENT_COL_OFFSET_USER1  offsetof(record_t, user) + 8
#define INST_EVENT_COL_OFFSET_USER2  offsetof(record_t, user) + 16
#define INST_EVENT_COL_OFFSET_USER3  offsetof(record_t, user) + 24

// ==================== Event metadata =========================================
// Header file format (for Abstract Rendering):
//
// Magic file identifier bytes = 
//   {'h', 'p', 'x', ' ', 'l', 'o', 'g', '\0', 0xFF, 0x00, 0xAA, 0x55}
// table offset
//  [metadata-id, length, data]*
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
// i: int -- 4 bytes
// l: long -- 8 bytes
// s: short -- 2 bytes
// d: double -- 8 bytes
// f: float -- 4 bytes
// b: byte -- 1 bytes
// c: char -- 2 bytes

#define METADATA_TYPE_NAMED_VALUE -1
#define METADATA_TYPE_DATA_TYPES 0
#define METADATA_TYPE_OFFSETS 1
#define METADATA_TYPE_NAMES 2
#define METADATA_TYPE_PRINTF_CODES 3
#define METADATA_TYPE_MINS 4
#define METADATA_TYPE_MAXS 5

// The signed types are also used for unsigned; there is not distinct unsigned
// type for these headers.
#define METADATA_TYPE_BYTE 'b'
#define METADATA_TYPE_CHAR 'c'
#define METADATA_TYPE_INT32 'i'
#define METADATA_TYPE_INT64 'l'
#define METADATA_TYPE_INT16 's'
#define METADATA_TYPE_DOUBLE 'd'
#define METADATA_TYPE_FLOAT 'f'

typedef struct inst_named_value {
  const char type;
  const uint32_t value;
  const char name[8];
} HPX_PACKED inst_named_value_t;

typedef struct inst_event_col_metadata {
  const char mask; // this should an OR of all the following values:
  const char data_type;      // mask 0x1
  const unsigned int offset; // mask 0x2
  const uint64_t min;        // mask 0x4
  const uint64_t max;        // mask 0x8
  const char printf_code[8]; // mask 0x10 (this value must be nul terminated)
  const char name[256];      // mask 0x20 (this value must be nul terminated)
} inst_event_col_metadata_t;

typedef struct inst_event_metadata {
  const int num_cols;
  // In theory the number of columns need not match the number of fields in
  // an event. In practice, right now they do.
  const  inst_event_col_metadata_t col_metadata[INST_EVENT_NUM_COLS];
} inst_event_metadata_t;

extern const inst_event_metadata_t INST_EVENT_METADATA[HPX_INST_NUM_EVENTS];

// typeof(INST_EVENT_COL_METADATA_WORKER) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_WORKER          \
  { .mask = 0x3f,                               \
    .data_type = METADATA_TYPE_INT32,           \
    .offset = INST_EVENT_COL_OFFSET_WORKER,     \
    .min = 0,                                   \
    .max = INT_MAX,                             \
    .printf_code = "d",                         \
    .name = "worker"}

// typeof(INST_EVENT_COL_METADATA_S) == inst_event_col_metadata_t
// inst_event_col_metadata_t INST_EVENT_COL_METADATA_NS
#define INST_EVENT_COL_METADATA_NS              \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_NS,       \
      .min = 0,                                 \
      .max = 1e9-1,                             \
      .printf_code = "zu",                      \
      .name = "nanoseconds"}

// typeof(INST_EVENT_COL_METADATA_EMPTY0) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY0          \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY1) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY1          \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER1,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY2) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY2          \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY3) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY3          \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER3,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_PARCEL_ID) == inst_event_col_metadata_t
#define METADATA_PARCEL_ID                      \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "parcel id"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_ACTION) == inst_event_col_metadata_t
#define METADATA_PARCEL_ACTION                  \
  { .mask = 0x3f,                               \
    .data_type = METADATA_TYPE_INT64,           \
    .offset = INST_EVENT_COL_OFFSET_USER1,      \
    .min = 0,                                   \
    .max = UINT16_MAX,                          \
      .printf_code = "zu",                      \
      .name = "action"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_SIZE) == inst_event_col_metadata_t
#define METADATA_PARCEL_SIZE                    \
  { .mask = 0x3f,                               \
    .data_type = METADATA_TYPE_INT64,           \
    .offset = INST_EVENT_COL_OFFSET_USER2,      \
    .min = 0,                                   \
    .max = UINT32_MAX,                          \
      .printf_code = "zu",                      \
      .name = "parcel size"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_TARGET) == inst_event_col_metadata_t
#define METADATA_PARCEL_TARGET                  \
  { .mask = 0x3f,                               \
    .data_type = METADATA_TYPE_INT64,           \
    .offset = INST_EVENT_COL_OFFSET_USER3,      \
    .min = 0,                                   \
    .max = UINT64_MAX,                          \
      .printf_code = "zu",                      \
      .name = "target address"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_SOURCE) == inst_event_col_metadata_t
#define METADATA_PARCEL_SOURCE                  \
  { .mask = 0x3f,                               \
    .data_type = METADATA_TYPE_INT64,           \
    .offset = INST_EVENT_COL_OFFSET_USER3,      \
    .min = 0,                                   \
    .max = UINT32_MAX,                          \
      .printf_code = "zu",                      \
      .name = "source rank"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_PARENT_ID) == inst_event_col_metadata_t
#define METADATA_PARCEL_PARENT_ID               \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER3,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "parent id"}

// typeof(METADATA_SCHEDULER_WQ_SIZE) == inst_event_col_metadata_t
#define METADATA_SCHEDULER_WQSIZE               \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "work_queue_size"}

// typeof(INST_EVENT_COL_METADATA_LCO_ADDRESS) == inst_event_col_metadata_t
#define METADATA_LCO_ADDRESS                    \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "lco address"}

// typeof(INST_EVENT_COL_METADATA_LCO_THREAD) == inst_event_col_metadata_t
#define METADATA_LCO_CURRENT_THREAD             \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER1,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "lco current thread"}

// typeof(INST_EVENT_COL_METADATA_LCO_THREAD) == inst_event_col_metadata_t
#define METADATA_LCO_STATE                      \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "lco state"}

// typeof(INST_EVENT_COL_METADATA_PROCESS_ADDRESS) == inst_event_col_metadata_t
#define METADATA_PROCESS_ADDRESS                \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "process address"}

// typeof(INST_EVENT_COL_METADATA_PROCESS_TERMINATION_LCO) == inst_event_col_metadata_t
#define METADATA_PROCESS_TERMINATION_LCO        \
  { .mask = 0x3f,                               \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "process termination lco"}


#define METADATA_SCHEDTIMES_STARTTIME           \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "start_time"                      \
      }

#define METADATA_SCHEDTIMES_SCHED_SOURCE        \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER1,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "parcel_source"                   \
      }


#define METADATA_SCHEDTIMES_SCHED_SPINS         \
  { .mask = 0x3,                                \
      .data_type = METADATA_TYPE_INT64,         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "spins"                           \
      }

// typeof(PARCEL_CREATE_METADATA) == inst_event_metadata_t 
#define PARCEL_CREATE_METADATA {                                   \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      METADATA_PARCEL_SIZE,                                        \
      METADATA_PARCEL_PARENT_ID                                    \
    }                                                              \
}

// typeof(PARCEL_SEND_METADATA) == inst_event_metadata_t 
#define PARCEL_SEND_METADATA {                                     \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      METADATA_PARCEL_SIZE,                                        \
      METADATA_PARCEL_TARGET                                       \
    }                                                              \
}

// typeof(PARCEL_RECV_METADATA) == inst_event_metadata_t 
#define PARCEL_RECV_METADATA {                                     \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      METADATA_PARCEL_SIZE,                                        \
      METADATA_PARCEL_SOURCE                                       \
    }                                                              \
}

// typeof(PARCEL_RUN_METADATA) == inst_event_metadata_t 
#define PARCEL_RUN_METADATA {                                      \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      METADATA_PARCEL_SIZE,                                        \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

// typeof(PARCEL_END_METADATA) == inst_event_metadata_t 
#define PARCEL_END_METADATA {                                      \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      INST_EVENT_COL_METADATA_EMPTY2,                              \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

// typeof(PARCEL_SUSPEND_METADATA) == inst_event_metadata_t 
#define PARCEL_SUSPEND_METADATA {                                  \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      INST_EVENT_COL_METADATA_EMPTY2,                              \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

// typeof(PARCEL_RESUME_METADATA) == inst_event_metadata_t 
#define PARCEL_RESUME_METADATA {                                   \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      INST_EVENT_COL_METADATA_EMPTY2,                              \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

// typeof(PARCEL_RESEND_METADATA) == inst_event_metadata_t 
#define PARCEL_RESEND_METADATA {                                   \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_PARCEL_ID,                                          \
      METADATA_PARCEL_ACTION,                                      \
      METADATA_PARCEL_SIZE,                                        \
      METADATA_PARCEL_TARGET                                       \
    }                                                              \
}

// typeof(SCHEDULER_WQSIZE_METADATA) == inst_event_metadata_t 
#define SCHEDULER_WQSIZE_METADATA {                              \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_SCHEDULER_WQSIZE,                                   \
    INST_EVENT_COL_METADATA_EMPTY1,                              \
    INST_EVENT_COL_METADATA_EMPTY2,                              \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_INIT_METADATA) == inst_event_metadata_t 
#define LCO_INIT_METADATA {                                      \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_DELETE_METADATA) == inst_event_metadata_t 
#define LCO_DELETE_METADATA {                                    \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_SET_METADATA) == inst_event_metadata_t 
#define LCO_SET_METADATA {                                       \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_RESET_METADATA) == inst_event_metadata_t 
#define LCO_RESET_METADATA {                                     \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_ASSIGN_PARCEL_METADATA) == inst_event_metadata_t 
#define LCO_ATTACH_PARCEL_METADATA {                             \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    METADATA_PARCEL_ID                                           \
  }                                                              \
}

// typeof(LCO_WAIT_METADATA) == inst_event_metadata_t 
#define LCO_WAIT_METADATA {                                      \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(LCO_TRIGGER_METADATA) == inst_event_metadata_t 
#define LCO_TRIGGER_METADATA {                                   \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_LCO_ADDRESS,                                        \
    METADATA_LCO_CURRENT_THREAD,                                 \
    METADATA_LCO_STATE,                                          \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(PROCESS_NEW_METADATA) == inst_event_metadata_t 
#define PROCESS_NEW_METADATA {                                   \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_PROCESS_ADDRESS,                                    \
    METADATA_PROCESS_TERMINATION_LCO,                            \
    INST_EVENT_COL_METADATA_EMPTY2,                              \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(PROCESS_CALL_METADATA) == inst_event_metadata_t 
#define PROCESS_CALL_METADATA {                                  \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_PROCESS_ADDRESS,                                    \
    METADATA_PARCEL_ID,                                          \
    INST_EVENT_COL_METADATA_EMPTY2,                              \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

// typeof(PROCESS_DELETE_METADATA) == inst_event_metadata_t 
#define PROCESS_DELETE_METADATA {                                \
  .num_cols = 6,                                                 \
  .col_metadata = {                                              \
    INST_EVENT_COL_METADATA_WORKER,                              \
    INST_EVENT_COL_METADATA_NS,                                  \
    METADATA_PROCESS_ADDRESS,                                    \
    INST_EVENT_COL_METADATA_EMPTY1,                              \
    INST_EVENT_COL_METADATA_EMPTY2,                              \
    INST_EVENT_COL_METADATA_EMPTY3                               \
  }                                                              \
}

#define SCHEDTIMES_SCHED_METADATA {                                \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_SCHEDTIMES_STARTTIME,                               \
      METADATA_SCHEDTIMES_SCHED_SOURCE,                            \
      METADATA_SCHEDTIMES_SCHED_SPINS,                             \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

#define SCHEDTIMES_PROBE_METADATA {                                \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_SCHEDTIMES_STARTTIME,                               \
      INST_EVENT_COL_METADATA_EMPTY1,                              \
      INST_EVENT_COL_METADATA_EMPTY2,                              \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

#define SCHEDTIMES_PROGRESS_METADATA {                             \
    .num_cols = 6,                                                 \
    .col_metadata = {                                              \
      INST_EVENT_COL_METADATA_WORKER,                              \
      INST_EVENT_COL_METADATA_NS,                                  \
      METADATA_SCHEDTIMES_STARTTIME,                               \
      INST_EVENT_COL_METADATA_EMPTY1,                              \
      INST_EVENT_COL_METADATA_EMPTY2,                              \
      INST_EVENT_COL_METADATA_EMPTY3                               \
    }                                                              \
}

#endif
