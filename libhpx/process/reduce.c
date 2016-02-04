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
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/scheduler.h>
#include "allreduce.h"

struct reduce {
  volatile int i;
  int n;
  size_t bytes;
  size_t padded;
  hpx_monoid_id_t id;
  hpx_monoid_op_t op;
  PAD_TO_CACHELINE(2 * sizeof(int) +
                   2 * sizeof(size_t) +
                   2 * sizeof(void(*)()));
  char values[];
};

reduce_t *reduce_new(size_t bytes, hpx_monoid_id_t id, hpx_monoid_op_t op) {
  reduce_t *r = NULL;

  // allocate enough space so that each
  int workers = here->sched->n_workers;
  size_t padded = _BYTES(HPX_CACHELINE_SIZE, bytes) + bytes;
  size_t size = sizeof(*r) + padded * workers;
  if (posix_memalign((void*)&r, HPX_CACHELINE_SIZE, size)) {
    dbg_error("could not allocate aligned reduction\n");
  }

  r->n = 0;
  r->bytes = bytes;
  r->padded = padded;
  r->id = id;
  r->op = op;
  sync_store(&r->i, 0, SYNC_RELEASE);
  for (int i = 0, e = workers; i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->id(value, r->bytes);
  }

  return r;
}

void reduce_delete(reduce_t *r) {
  if (r) {
    free(r);
  }
}

int reduce_add(reduce_t *r) {
  dbg_assert(sync_load(&r->i, SYNC_ACQUIRE) == r->n);
  int i = ++r->n;
  sync_store(&r->i, i, SYNC_RELEASE);
  return (i == 1);
}

int reduce_remove(reduce_t *r) {
  dbg_assert(sync_load(&r->i, SYNC_ACQUIRE) == r->n);
  int i = --r->n;
  sync_store(&r->i, i, SYNC_RELEASE);
  return (i == 0);
}

int reduce_join(reduce_t *r, const void *in) {
  void *buffer = r->values + self->id * r->padded;
  r->op(buffer, in, r->bytes);
  return (sync_addf(&r->i, -1, SYNC_ACQ_REL) == 0);
}

void reduce_reset(reduce_t *r, void *out) {
  r->id(out, r->bytes);
  for (int i = 0, e = here->sched->n_workers; i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->op(out, value, r->bytes);
    r->id(value, r->bytes);
  }

  sync_store(&r->i, r->n, SYNC_RELEASE);
}
