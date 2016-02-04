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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include <libsync/sync.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/instrumentation_events.h>
#include "file_header.h"
#include "logtable.h"

static size_t _header_size(logtable_t *log) {
  return (size_t)((uintptr_t)log->records - (uintptr_t)log->header);
}

static int _create_file(const char *filename, size_t size) {
  static const int flags = O_RDWR | O_CREAT;
  static const int perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = open(filename, flags, perm);
  if (fd == -1) {
    log_error("failed to open a log file for %s\n", filename);
    return -1;
  }

  lseek(fd, size - 1, SEEK_SET);
  if (write(fd, "", 1) != 1) {
    log_error("could not create log file %s\n", filename);
    close(fd);
    return -1;
  }

  return fd;
}

static void *_create_mmap(size_t size, int file) {
  static const int prot = PROT_WRITE;
  static const int flags = MAP_SHARED | MAP_NORESERVE;
  void *base = mmap(NULL, size * sizeof(record_t), prot, flags, file, 0);
  if (base == MAP_FAILED) {
    log_error("could not mmap log file\n");
    return NULL;
  }
  else {
    log_dflt("mapped %zu byte trace file at %p.\n", size * sizeof(record_t), base);
  }

  if ((uintptr_t)base % HPX_CACHELINE_SIZE) {
    log_dflt("log records are not cacheline aligned\n");
  }

  return base;
}

int logtable_init(logtable_t *log, const char* filename, size_t size,
                  int class, int id) {
  log->fd = -1;
  log->class = class;
  log->id = id;
  sync_store(&log->next, 0, SYNC_RELEASE);
  sync_store(&log->last, 0, SYNC_RELEASE);
  log->max_size = size;
  log->records = NULL;

  if (filename == NULL || size == 0) {
    return LIBHPX_OK;
  }

  log->fd = _create_file(filename, size);
  if (log->fd == -1) {
    goto unwind;
  }

  log->header = _create_mmap(size, log->fd);
  if (!log->header) {
    goto unwind;
  }

  size_t header_size = write_trace_header(log->header, class, id);
  assert(((uintptr_t)log->header + header_size) % 8 == 0);
  log->records = (void*)((uintptr_t)log->header + header_size);

  return LIBHPX_OK;

 unwind:
  logtable_fini(log);
  return LIBHPX_ERROR;
}

void logtable_fini(logtable_t *log) {
  if (!log->max_size) {
    return;
  }

  if (log->header) {
    int e = munmap(log->header, log->max_size);
    if (e) {
      log_error("failed to unmap trace file\n");
    }
  }

  if (log->fd != -1) {
    size_t filesize =
      (uintptr_t)&log->records[log->last] - (uintptr_t)log->header;
    int e = ftruncate(log->fd, filesize);
    if (e) {
      log_error("failed to truncate trace file\n");
    }
    e = close(log->fd);
    if (e) {
      log_error("failed to close trace file\n");
    }
  }
}

void logtable_append(logtable_t *log, uint64_t u1, uint64_t u2, uint64_t u3,
                     uint64_t u4) {
  size_t i = sync_fadd(&log->next, 1, SYNC_ACQ_REL);
  if (_header_size(log) + (i + 1) * sizeof(record_t) > log->max_size) {
    return;
  }
  sync_fadd(&log->last, 1, SYNC_ACQ_REL); // update size

  record_t *r = &log->records[i];
  r->worker = hpx_get_my_thread_id();
  r->ns = hpx_time_from_start_ns(hpx_time_now());
  r->user[0] = u1;
  r->user[1] = u2;
  r->user[2] = u3;
  r->user[3] = u4;
}
