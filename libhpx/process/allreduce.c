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
#include <libhpx/parcel.h>
#include "allreduce.h"

void allreduce_init(allreduce_t *r, size_t bytes, hpx_addr_t parent,
                    hpx_monoid_id_t id, hpx_monoid_op_t op) {
  r->lock = hpx_lco_sema_new(1);
  r->bytes = bytes;
  r->parent = parent;
  r->continuation = continuation_new(bytes);
  r->reduce = reduce_new(bytes, id, op);
  r->id = -1;
}

void allreduce_fini(allreduce_t *r) {
  hpx_lco_delete_sync(r->lock);
  continuation_delete(r->continuation);
  reduce_delete(r->reduce);
}

int32_t allreduce_add(allreduce_t *r, hpx_action_t op, hpx_addr_t addr) {
  int32_t i = 0;
  // acquire the semaphore
  hpx_lco_sema_p(r->lock);

  // extend the local continuation structure and get and id for this input
  i = continuation_add(&r->continuation, op, addr);

  // extend the local reduction, if this is the first input then we need to
  // recursively tell our parent (if we have one) that we exist, and that we
  // need to have our bcast action run as a continuation
  if (reduce_add(r->reduce) && r->parent) {
    hpx_addr_t allreduce = hpx_thread_current_target();
    dbg_check( hpx_call_sync(r->parent, allreduce_add_async, &r->id,
                             sizeof(r->id), &allreduce_bcast_async,
                             &allreduce) );
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
  return i;
}

void allreduce_remove(allreduce_t *r, int32_t id) {
  // acquire the lock
  hpx_lco_sema_p(r->lock);

  // remove the continuation that is leaving
  continuation_remove(&r->continuation, id);

  // remove this input from our allreduce, if this is the last input then tell
  // our parent that we are no longer participating
  if (reduce_remove(r->reduce) && r->parent) {
    dbg_check( hpx_call_sync(r->parent, allreduce_remove_async, NULL, 0,
                             &r->id) );
    r->id = -1;
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
}

void allreduce_reduce(allreduce_t *r, const void *val) {
  log_coll("reducing at %p\n", r);
  // if this isn't the last local value then just continue
  if (!reduce_join(r->reduce, val)) {
    return;
  }

  // the local continuation is done, join the parent node asynchronously
  if (r->parent) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, r->bytes);
    p->target = r->parent;
    p->action = allreduce_join_async;
    reduce_reset(r->reduce, hpx_parcel_get_data(p));
    parcel_launch(p);
    return;
  }

  // this is a root node, so turn around and run all of our continuations
  void *result = malloc(r->bytes);
  reduce_reset(r->reduce, result);
  allreduce_bcast(r, result);
  free(result);
}

void allreduce_bcast(allreduce_t *r, const void *value) {
  log_coll("broadcasting at %p\n", r);
  // just trigger the continuation stored in this node
  continuation_trigger(r->continuation, value);
}
