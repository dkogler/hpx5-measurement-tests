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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "allreduce.h"

struct continuation {
  size_t bytes;
  int capacity;
  int n;
  hpx_parcel_t *parcels[];
};

static int32_t _insert(continuation_t *c, hpx_action_t op, hpx_addr_t addr) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, c->bytes);
  p->action = op;
  p->target = addr;
  parcel_retain(p);

  // fill any spots left by leavers
  for (int i = 0, e = c->n; i < e; ++i) {
    if (c->parcels[i] == NULL) {
      c->parcels[i] = p;
      return i;
    }
  }

  int32_t i = c->n++;
  c->parcels[i] = p;
  return i;
}

static continuation_t *_expand(continuation_t *c, int capacity) {
  c = realloc(c, sizeof(*c) + capacity * sizeof(hpx_parcel_t *));
  dbg_assert(c);
  c->capacity = capacity;
  memset(&c->parcels[c->n], 0, (capacity - c->n) * sizeof(hpx_parcel_t *));
  return c;
}

static continuation_t *_try_expand(continuation_t *c) {
  return (c->n < c->capacity) ? c : _expand(c, 2 * c->capacity);
}

continuation_t *continuation_new(size_t bytes) {
  continuation_t *c = malloc(sizeof(*c));
  c->bytes = bytes;
  c->capacity = 0;
  c->n = 0;
  return _expand(c, 16);
}

void continuation_delete(continuation_t *c) {
  if (!c) {
    return;
  }

  for (int i = 0, e = c->n; i < e; ++i) {
    if (c->parcels[i]) {
      parcel_release(c->parcels[i]);
      parcel_delete(c->parcels[i]);
    }
  }
  free(c);
}

int32_t continuation_add(continuation_t **c, hpx_action_t op, hpx_addr_t addr) {
  log_coll("registering continuation (%d, %"PRIu64") at %p\n", op, addr, *c);
  int32_t i = _insert(*c, op, addr);
  *c = _try_expand(*c);
  return i;
}

void continuation_remove(continuation_t **c, int32_t id) {
  dbg_assert(0 <= id && id < (*c)->n);
  parcel_release((*c)->parcels[id]);
  parcel_delete((*c)->parcels[id]);
  (*c)->parcels[id] = NULL;
}

void continuation_trigger(continuation_t *c, const void *value) {
  log_coll("continuing %d from %p\n", c->n, c);

  for (int i = 0, e = c->n; i < e; ++i) {
    if (c->parcels[i]) {
      // hpx_parcel_set_data(c->parcels[i], value, c->bytes);
      // parcel_launch(c->parcels[i]);

      hpx_parcel_t *p = parcel_clone(c->parcels[i]);
      hpx_parcel_set_data(p, value, c->bytes);
      parcel_launch(p);
    }
  }
}

