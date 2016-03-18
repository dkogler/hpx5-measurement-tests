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

#ifndef LIBHPX_INSTRUMENTATION_LOGTABLE_H
#define LIBHPX_INSTRUMENTATION_LOGTABLE_H

struct record;

/// All of the data needed to keep the state of an individual event log
typedef struct {
  int                 fd;       //!< file backing the log
  int              class;       //!< the class we're logging
  int                 id;       //!< the event we're logging
  int        record_size;       //!< record size
  size_t        max_size;       //!< max size in bytes
  void           *header;       //!< pointer to file header
  struct record *records;       //!< pointer to data for log
  volatile size_t   next;       //!< the next element to write
  volatile size_t   last;       //!< the last element written
} logtable_t;

#define LOGTABLE_INIT {             \
  .fd          = -1,                \
  .class       = -1,                \
  .id          = -1,                \
  .record_size = 0,                 \
  .max_size    = 0,                 \
  .header      = NULL,              \
  .records     = NULL,              \
  .next        = 0,                 \
  .last        = 0                  \
}

/// Initialize a logtable.
///
/// If filename is NULL or size == 0 this will not generate a file.
int logtable_init(logtable_t *lt, const char* filename, size_t size,
                  int class, int event);

void logtable_fini(logtable_t *lt);

/// Append a record to a log table.
void logtable_vappend(logtable_t *log, int n, va_list *args);

#endif // LIBHPX_INSTRUMENTATION_LOGTABLE_H
