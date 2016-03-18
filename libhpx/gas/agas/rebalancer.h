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

#ifndef LIBHPX_GAS_AGAS_REBALANCER_H
#define LIBHPX_GAS_AGAS_REBALANCER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/hpx.h>
    
// Block Statistics Table (BST) API
void *bst_new(size_t size);
void bst_delete(void *bst);
void bst_upsert(void *obj, uint64_t block, uint64_t *counts, uint64_t *sizes);
size_t bst_serialize_to_parcel(void* obj, hpx_parcel_t **parcel);

// AGAS Graph Partitioning API
hpx_addr_t agas_graph_new(void);
void agas_graph_delete(hpx_addr_t graph);
int agas_graph_construct(hpx_addr_t graph, uint64_t *data, size_t size,
                         int owner);
size_t agas_graph_partition(void *g, int nparts, uint64_t **partition);
size_t agas_graph_get_vtxs(void *graph, uint64_t **vtxs);
size_t agas_graph_get_owner_count(void *graph);
int agas_graph_get_owner_entry(void *graph, uint64_t id, int *start,
                               int *end, int *owner);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_REBALANCER_H
