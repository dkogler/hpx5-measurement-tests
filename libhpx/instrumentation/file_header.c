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

#include <stdlib.h>
#include <string.h>

#include <hpx/hpx.h>
#include <libhpx/instrumentation_events.h>
#include "file_header.h"

struct cols_metadata {
  int kind;
  int length;
  char metadata[];
};

logtable_header_t LOGTABLE_HEADER = _LOGTABLE_HEADER;

static size_t _write_event_metadata_named_value(void* base,
                                                inst_named_value_t const *nv_md)
{
  // nv_md = [type, value, label] (e.g. [METADATA_TYPE_INT32, 4, "rank"]
  // metadata = [type, length, nv_md] where
  //   type = METADATA_TYPE_NAMED_VALUE and 
  //   length = sizeof(metadata) + sizeof(nv_md)
  int nv_size = sizeof(inst_named_value_t);
  int md_size = sizeof(struct cols_metadata) + nv_size;
  struct cols_metadata *md = malloc(md_size);
  md->kind = METADATA_TYPE_NAMED_VALUE;
  md->length = nv_size;
  memcpy(md->metadata, nv_md, nv_size);
  memcpy(base, md, md_size);
  free(md);
  return md_size;
}

#define METADATA_HANDLER(name, md_kind, ctype)                          \
  static size_t _write_event_metadata_ ## name(void* base,              \
                                  inst_event_metadata_t const *event_md \
                                               ) {                      \
    int el_size = sizeof(ctype);                                        \
    int md_size = sizeof(struct cols_metadata) + event_md->num_cols * el_size; \
    struct cols_metadata *md = malloc(md_size);                         \
    md->kind = (md_kind);                                               \
    md->length = event_md->num_cols * el_size;                          \
    ctype data[event_md->num_cols];                                     \
    for (int i = 0; i < event_md->num_cols; i++) {                      \
      data[i] = event_md->col_metadata[i].name;                         \
    }                                                                   \
    memcpy(md->metadata, data, sizeof(data));                           \
    memcpy(base, md, md_size);                                          \
    free(md);                                                           \
    return md_size;                                                     \
  }

#define METADATA_HANDLER_STR(name, md_kind, _length)                    \
  static size_t _write_event_metadata_ ## name(void* base,              \
                                  inst_event_metadata_t const *event_md \
                                               ) {                      \
    if(event_md->num_cols == 0) {                                       \
        return 0;                                                       \
    }                                                                   \
    int md_data_size = event_md->num_cols * ((_length) + 1) + 1;        \
    int md_size = sizeof(struct cols_metadata) + md_data_size;          \
    struct cols_metadata *md = malloc(md_size);                         \
    md->kind = (md_kind);                                               \
    char *data = malloc(md_data_size);                                  \
    strncpy(data, event_md->col_metadata[0].name, (_length));           \
    for (int i = 1; i < event_md->num_cols; i++) {                      \
      strncat(data, "|", 1);                                            \
      strncat(data, event_md->col_metadata[i].name, (_length));         \
    }                                                                   \
    strncpy(md->metadata, data, md_data_size);                          \
    md->length = strlen(data);                                          \
    md_size = sizeof(struct cols_metadata) + md->length;                \
    memcpy(base, md, md_size);                                          \
    free(data);                                                         \
    free(md);                                                           \
    return md_size;                                                     \
  }

METADATA_HANDLER(data_type, METADATA_TYPE_DATA_TYPES, char)
METADATA_HANDLER(offset, METADATA_TYPE_OFFSETS, int)
METADATA_HANDLER(min, METADATA_TYPE_MINS, uint64_t)
METADATA_HANDLER(max, METADATA_TYPE_MAXS, uint64_t)
METADATA_HANDLER_STR(printf_code, METADATA_TYPE_PRINTF_CODES, 8)
METADATA_HANDLER_STR(name, METADATA_TYPE_NAMES, 256)

/// Write the metadata for the event to the header portion of the log
static size_t _write_event_metadata(void* base, int class, int id) {
  inst_event_metadata_t const *event_md = &INST_EVENT_METADATA[id];
  size_t bytes;
  uintptr_t curr = (uintptr_t)base;
  // 8 byte aligned data first
  bytes = _write_event_metadata_min((void*)curr, event_md);
  curr += bytes;
  bytes = _write_event_metadata_max((void*)curr, event_md);
  curr += bytes;
  // 4 byte-aligned
  bytes = _write_event_metadata_offset((void*)curr, event_md);
  curr += bytes;
  // 1 byte-aligned
  bytes = _write_event_metadata_data_type((void*)curr, event_md);
  curr += bytes;
  bytes = _write_event_metadata_printf_code((void*)curr, event_md);
  curr += bytes;
  bytes = _write_event_metadata_name((void*)curr, event_md);
  curr += bytes;
  // 1 byte-aligned named values
  // record rank
  inst_named_value_t rank_md = {
    .type = METADATA_TYPE_INT32,
    .value = hpx_get_my_rank(),
    .name = "rank"
  };
  bytes = _write_event_metadata_named_value((void*)curr, &rank_md);
  curr += bytes;
  // event class
  inst_named_value_t class_md = {.type = METADATA_TYPE_INT32, .value = class,
                                .name = "class"};
  bytes = _write_event_metadata_named_value((void*)curr, &class_md);
  curr += bytes;
  inst_named_value_t id_md = {.type = METADATA_TYPE_INT32,
                              .value = id, .name = "id"};
  bytes = _write_event_metadata_named_value((void*)curr, &id_md);
  curr += bytes;

  return curr - (uintptr_t)base;
}

// Write the metadata for this event to the header of the log file
// Header is padded out to 8 byte multiple length
size_t write_trace_header(void* base, int class, int id) {
  logtable_header_t *header = (logtable_header_t*)base;
  memcpy(header, &LOGTABLE_HEADER, sizeof(LOGTABLE_HEADER));
  size_t metadata_size = _write_event_metadata(header->header_data, class, id);
  metadata_size += 8 - (metadata_size % 8);
  header->table_offset = offsetof(logtable_header_t, header_data) +
    metadata_size;
  return header->table_offset;
}
