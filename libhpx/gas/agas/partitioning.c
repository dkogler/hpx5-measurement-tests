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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libsync/locks.h>
#include <utstring.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

#ifdef HAVE_METIS
#include <metis.h>
#endif

#ifdef HAVE_PARMETIS
#include <parmetis.h>

extern void *LIBHPX_COMM;
#endif

static tatas_lock_t _lock = SYNC_TATAS_LOCK_INIT;

// The owner map for vertices in the graph.
typedef struct _owner_map {
  uint64_t start; //!< the starting vertex id of vertices mapped to an owner
  int      owner; //!< the partition owner
} _owner_map_t;

// A graph representing the GAS accesses.
typedef struct agas_graph {
  int nvtxs;
  int nedges;
  _owner_map_t *owner_map;
  UT_string *vtxs;
  UT_string *vtxdist;
  UT_string *vwgt;
  UT_string *vsizes;
  UT_string *xadj;
  UT_string *adjncy;
  UT_string *adjwgt;
  UT_string **lnbrs;
  int count;
} _agas_graph_t;

#define _UTBUF(s) ((void*)utstring_body(s))

// Add nodes associated with a locality to the graph.
//
// This adds the n nodes to the graph from index 0 to n. The weights
// of these nodes are initialized with INT_MAX since we don't ever
// want two locality nodes to fall into one partition. The sizes are
// presently initialized to 0.
static void _add_locality_nodes(_agas_graph_t *g) {
  int n = here->ranks;
  uint64_t vtxs[n];
  uint64_t vwgt[n];

  g->lnbrs  = malloc(n * sizeof(UT_string*));
  for (int i = 0; i < n; ++i) {
    vtxs[i] = i;
    vwgt[i] = INT_MAX;
    utstring_new(g->lnbrs[i]);
  }

  size_t size = n * sizeof(uint64_t);
  uint64_t *zeros = calloc(n+1, sizeof(*zeros));
  g->nvtxs += n;
  utstring_bincpy(g->vtxs,    vtxs, size);
  utstring_bincpy(g->vwgt,    vwgt, size);
  utstring_bincpy(g->vsizes, zeros, size);
  utstring_bincpy(g->xadj,   zeros, size + sizeof(uint64_t));
  free(zeros);
}

// Initialize a AGAS graph.
static void _init(_agas_graph_t *g) {
  g->nvtxs  = 0;
  g->nedges = 0;
  g->count  = 0;
  g->owner_map = calloc(here->ranks, sizeof(*g->owner_map));
  utstring_new(g->vtxs);
  utstring_new(g->vtxdist);
  utstring_new(g->vwgt);
  utstring_new(g->vsizes);
  utstring_new(g->xadj);
  utstring_new(g->adjncy);
  utstring_new(g->adjwgt);

  _add_locality_nodes(g);
}

// Free the AGAS graph.
static void _free(_agas_graph_t *g) {
  utstring_free(g->vtxs);
  utstring_free(g->vtxdist);
  utstring_free(g->vwgt);
  utstring_free(g->vsizes);
  utstring_free(g->xadj);
  utstring_free(g->adjncy);
  utstring_free(g->adjwgt);
  for (int i = 0; i < here->ranks; ++i) {
    utstring_free(g->lnbrs[i]);
  }
  free(g->lnbrs);
  free(g->owner_map);
}

// Constructor for an AGAS block graph. Note that this allocates a
// graph in the global address space and returns a global address for
// the graph.
hpx_addr_t agas_graph_new(void) {
  _agas_graph_t *g = NULL;
  hpx_addr_t graph = hpx_gas_alloc_local(1, sizeof(*g), 0);
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin newly allocated process.\n");
  }
  dbg_assert(graph != HPX_NULL && g);
  _init(g);
  hpx_gas_unpin(graph);
  return graph;
}

HPX_ACTION_DECL(agas_graph_delete_action);
void agas_graph_delete(hpx_addr_t graph) {
  _agas_graph_t *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    int e = hpx_call(graph, agas_graph_delete_action, HPX_NULL, &graph);
    dbg_check(e, "Could not forward agas_graph_delete\n");
    return;
  }
  _free(g);
  hpx_gas_unpin(graph);
  hpx_gas_free(graph, HPX_NULL);
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, agas_graph_delete_action,
              agas_graph_delete, HPX_ADDR);


#define _BUF(ptr, size)                         \
  ptr = (uint64_t*)buf; buf += (size);

// Deserialize the CSR representation of the BST that is pointed to by
// the @p buf buffer of size @p size bytes.
static void _deserialize_bst(char *buf, size_t size, uint64_t *nvtxs,
                             uint64_t *nedges, uint64_t **vtxs, uint64_t **vwgt,
                             uint64_t **vsizes, uint64_t **xadj,
                             uint64_t **adjncy, uint64_t **adjwgt,
                             uint64_t **lsizes, uint64_t **lnbrs) {
  *nvtxs = *(uint64_t*)buf; buf += sizeof(uint64_t);
  size_t nsize = *nvtxs * sizeof(uint64_t);

  _BUF(*vtxs,   nsize);
  _BUF(*vwgt,   nsize);
  _BUF(*vsizes, nsize);
  _BUF(*xadj,   nsize);

  *nedges = *(uint64_t*)buf; buf += sizeof(uint64_t);
  size_t esize = *nedges * sizeof(uint64_t);

  _BUF(*adjncy, esize);
  _BUF(*adjwgt, esize);

  int ranks = here->ranks;
  _BUF(*lsizes, ranks * sizeof(uint64_t));
  for (int i = 0; i < ranks; ++i) {
    _BUF(lnbrs[i], (*lsizes)[i] * sizeof(uint64_t));
  }
}

// Construct a new AGAS graph from a deserialized buffer @p buf of
// size @p size bytes.
int agas_graph_construct(hpx_addr_t graph, char *buf, size_t size,
                         int owner) {
  uint64_t nvtxs;
  uint64_t nedges;
  uint64_t *vtxs;
  uint64_t *vwgt;
  uint64_t *vsizes;
  uint64_t *xadj;
  uint64_t *adjncy;
  uint64_t *adjwgt;
  uint64_t *lsizes;

  int n = here->ranks;
  uint64_t *lnbrs[n];
  _deserialize_bst(buf, size, &nvtxs, &nedges, &vtxs, &vwgt, &vsizes,
                   &xadj, &adjncy, &adjwgt, &lsizes, lnbrs);

  _agas_graph_t *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin the graph. Did it move?\n");
    return HPX_ERROR;
  }

  sync_tatas_acquire(&_lock);

  // add owner map entry
  _owner_map_t *map = &g->owner_map[g->count++];
  map->start = g->nvtxs;
  map->owner = owner;

  uint64_t *gxadj = _UTBUF(g->xadj);
  for (int i = 0; i < n; ++i) {    
    gxadj[i+1] += lsizes[i];
    for (int j = 0; j < lsizes[i]; ++j) {
      lnbrs[i][j] += g->nvtxs;
    }
    
    utstring_bincpy(g->lnbrs[i], lnbrs[i], lsizes[i]*sizeof(uint64_t));
  }

  g->nvtxs  += nvtxs;
  g->nedges += nedges;

  size_t nsize = nvtxs  * sizeof(uint64_t);
  size_t esize = nedges * sizeof(uint64_t);
  utstring_bincpy(g->vtxs,     vtxs, nsize);
  utstring_bincpy(g->vwgt,     vwgt, nsize);
  utstring_bincpy(g->vsizes, vsizes, nsize);
  utstring_bincpy(g->xadj,     xadj, nsize);
  utstring_bincpy(g->adjncy, adjncy, esize);
  utstring_bincpy(g->adjwgt, adjwgt, esize);

  sync_tatas_release(&_lock);
  hpx_gas_unpin(graph);
  
  return HPX_SUCCESS;
}

static void _dump_agas_graph(_agas_graph_t *g) {
#ifdef ENABLE_INSTRUMENTATION
  char filename[256];
  snprintf(filename, 256, "rebalancer_%d.graph", HPX_LOCALITY_ID);
  FILE *file = fopen(filename, "w");
  if (!file) {
    log_error("failed to open action id file %s\n", filename);
  }

  uint64_t *vwgt   = _UTBUF(g->vwgt);
  uint64_t *vsizes = _UTBUF(g->vsizes);
  uint64_t *xadj   = _UTBUF(g->xadj);
  uint64_t *adjncy = _UTBUF(g->adjncy);

  fprintf(file, "%d %d 110\n", g->nvtxs, g->nedges);
  for (int i = 0; i < g->nvtxs; ++i) {
    fprintf(file, "%lu %lu ", vsizes[i], vwgt[i]);
    for (int j = xadj[i]; j < xadj[i+1]; ++j) {
      fprintf(file, "%lu ", adjncy[i]);
    }
  }
  fprintf(file, "\n");

  int e = fclose(file);
  if (e) {
    log_error("failed to dump the AGAS graph\n");
  }
#endif
}

#ifdef HAVE_METIS
static size_t _metis_partition(_agas_graph_t *g, idx_t nparts,
                               uint64_t **partition) {
  idx_t options[METIS_NOPTIONS];
  METIS_SetDefaultOptions(options);
  options[METIS_OPTION_NUMBERING] = 0;
  options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
  options[METIS_OPTION_CTYPE] = METIS_CTYPE_RM;

  idx_t ncon   = 1;
  idx_t objval = 0;

  idx_t nvtxs = g->nvtxs;
  *partition = calloc(nvtxs, sizeof(uint64_t));
  dbg_assert(partition);

  idx_t *vwgt   = _UTBUF(g->vwgt);
  idx_t *vsizes = _UTBUF(g->vsizes);
  idx_t *xadj   = _UTBUF(g->xadj);
  idx_t *adjncy = _UTBUF(g->adjncy);

  int e = METIS_PartGraphRecursive(&nvtxs, &ncon, xadj, adjncy, vwgt, vsizes,
            NULL, &nparts, NULL, NULL, options, &objval, (idx_t*)*partition);
  if (e != METIS_OK) {
    return 0;
  }
  return g->nvtxs;
}
#endif


#ifdef HAVE_PARMETIS
static size_t _parmetis_partition(_agas_graph_t *g, idx_t nparts,
                                  uint64_t **partition) {
  idx_t options[1] = { 0 };

  idx_t ncon    = 1;
  idx_t wgtflag = 2;
  idx_t numflag = 0;
  idx_t edgecut = 0;

  idx_t nvtxs = g->nvtxs;
  *partition = calloc(nvtxs, sizeof(uint64_t));
  dbg_assert(partition);

  idx_t *vtxdist = _UTBUF(g->vtxdist);
  idx_t *vwgt    = _UTBUF(g->vwgt);
  idx_t *xadj    = _UTBUF(g->xadj);
  idx_t *adjncy  = _UTBUF(g->adjncy);

  int e = ParMETIS_V3_PartKway(vtxdist, xadj, adjncy, vwgt, NULL, &wgtflag,
                               &numflag, &ncon, &nparts, NULL, NULL, options,
                               &edgecut, (idx_t*)*partition, &LIBHPX_COMM);
  if (e != METIS_OK) {
    return 0;
  }
  return g->nvtxs;
}
#endif

// Perform any post-processing operations on the graph. In particular,
// we merge the locality nbrs array with the adjacency array to get
// the final adjacency array. We also fix the xadj array to reflect
// the correct positions into the adjacency array.
static void _postprocess_graph(_agas_graph_t *g) {
  // aggregate the locality nbrs array  
  UT_string *tmp;
  utstring_new(tmp);
  for (int i = 0; i < here->ranks; ++i) {
    utstring_concat(tmp, g->lnbrs[i]);
  }
  utstring_concat(tmp, g->adjncy);
  utstring_free(g->adjncy);
  g->adjncy = tmp;
  g->nedges = utstring_len(g->adjncy)/sizeof(uint64_t);

  // fix the adj array
  uint64_t *xadj = _UTBUF(g->xadj);
  for (int i = 1; i < g->nvtxs+1; ++i) {
    xadj[i] += xadj[i-1];
  }
}

// Partition an AGAS graph into @p nparts number of partitions. The
// partitions are returned in the @p partition array which includes
// the indices of the nodes and their partition id.
size_t agas_graph_partition(void *g, int nparts, uint64_t **partition) {
  _postprocess_graph(g);
  _dump_agas_graph(g);
#ifdef HAVE_METIS
  return _metis_partition((_agas_graph_t*)g, nparts, partition);
#elif HAVE_PARMETIS
  return _parmetis_partition((_agas_graph_t*)g, nparts, partition);
#else
  log_dflt("No partitioner found.\n");
#endif
  return 0;
}

// Get tht number of vertices/nodes in the AGAS graph.
size_t agas_graph_get_vtxs(void *graph, uint64_t **vtxs) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  dbg_assert(g);
  *vtxs = _UTBUF(g->vtxs);
  return g->nvtxs;
}

// Get the count of the number of entries in the owner map of the AGAS
// graph.
size_t agas_graph_get_owner_count(void *graph) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  return g->count;
}

// Get the owner entries associated with the owner map of the AGAS
// graph.
int agas_graph_get_owner_entry(void *graph, uint64_t id, int *start,
                               int *end, int *owner) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  dbg_assert(g);
  dbg_assert(id >= 0 && id <= g->count);

  _owner_map_t entry = g->owner_map[id];
  *start = entry.start;
  *owner = entry.owner;

  if (id == g->count-1) {
    *end = g->nvtxs - 1;
  } else {
    _owner_map_t next_entry = g->owner_map[id+1];
    *end = next_entry.start - 1;
  }
  return HPX_SUCCESS;
}

