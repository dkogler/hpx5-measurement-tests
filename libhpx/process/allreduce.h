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

#ifndef LIBHPX_PROCESS_ALLREDUCE_H
#define LIBHPX_PROCESS_ALLREDUCE_H

#include <hpx/hpx.h>
#include <libsync/locks.h>

typedef struct continuation continuation_t;

continuation_t *continuation_new(size_t bytes);
void continuation_delete(continuation_t *obj);
int32_t continuation_add(continuation_t **obj, hpx_action_t op, hpx_addr_t addr);
void continuation_remove(continuation_t **obj, int32_t id);
void continuation_trigger(continuation_t *obj, const void *value);

typedef struct reduce reduce_t;

reduce_t *reduce_new(size_t bytes, hpx_monoid_id_t id, hpx_monoid_op_t op);
void reduce_delete(reduce_t *obj);
int reduce_add(reduce_t *obj);
int reduce_remove(reduce_t *obj);
int reduce_join(reduce_t *obj, const void *in);
void reduce_reset(reduce_t *obj, void *out);

typedef struct {
  hpx_addr_t              lock;           // semaphore synchronizes add/remove
  size_t                 bytes;           // the size of the value being reduced
  hpx_addr_t            parent;           // our parent node
  continuation_t *continuation;           // our continuation data
  reduce_t             *reduce;           // the local reduction
  int32_t                   id;           // our identifier for our parent
} allreduce_t;

void allreduce_init(allreduce_t *obj, size_t bytes, hpx_addr_t parent,
                    hpx_monoid_id_t id, hpx_monoid_op_t op);
void allreduce_fini(allreduce_t *obj);
int32_t allreduce_add(allreduce_t *obj, hpx_action_t op, hpx_addr_t addr);
void allreduce_remove(allreduce_t *obj, int32_t id);
void allreduce_reduce(allreduce_t *obj, const void *in);
void allreduce_bcast(allreduce_t *obj, const void *val);

/// void allreduce_init_async(allreduce_t *, size_t bytes, hpx_addr_t parent,
///                           hpx_action_t id, hpx_action_t op);
extern HPX_ACTION_DECL(allreduce_init_async);

/// void allreduce_fini_async(allreduce_t *);
extern HPX_ACTION_DECL(allreduce_fini_async);

/// int32_t allreduce_add_async(allreduce_t *, hpx_action_t op,
///                             hpx_addr_t addr);
extern HPX_ACTION_DECL(allreduce_add_async);

/// void allreduce_remove_async(allreduce_t *, int32_t id);
extern HPX_ACTION_DECL(allreduce_remove_async);

/// void allreduce_join_async(allreduce_t *, const void *value, size_t bytes);
extern HPX_ACTION_DECL(allreduce_join_async);

/// void allreduce_bcast_async(allreduce_t *, const void *value, size_t bytes);
extern HPX_ACTION_DECL(allreduce_bcast_async);

#endif // LIBHPX_PROCESS_ALLREDUCE_H
