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

#ifndef LIBHPX_INSTRUMENTATION_FILE_HEADER_H
#define LIBHPX_INSTRUMENTATION_FILE_HEADER_H

/// The header needed for our data file format
typedef struct {
  const char magic_number[8];
  const uint32_t order;
  uint32_t table_offset;
  char header_data[];
} logtable_header_t;

#define _LOGTABLE_HEADER                                       \
  {                                                            \
    .magic_number = {'h', 'p', 'x', ' ', 'l', 'o', 'g', '\0'}, \
    .order = 0xFF00AA55                                        \
  }

extern logtable_header_t LOGTABLE_HEADER;

size_t write_trace_header(void* base, int class, int id);

#endif // LIBHPX_INSTRUMENTATION_LOGTABLE_H
