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

#include <cassert>
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include "rebalancer.h"

namespace {
  struct Entry {
    uint64_t *counts;
    uint64_t *sizes;
    Entry() : counts(NULL), sizes(NULL) {
    }
    Entry(uint64_t *c, uint64_t *s) : counts(c), sizes(s) {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BST : public Map {
   public:
    BST(size_t);
  };
}

BST::BST(size_t size) : Map(size) {
}

void *
bst_new(size_t size) {
  return new BST(size);
}

void
bst_delete(void* obj) {
  BST *bst = static_cast<BST*>(obj);
  bst->clear();
  delete bst;
}

#define _BUF(ptr, size)                         \
  uint64_t ptr = (uint64_t*)buf; buf += (size);

// This handler constructs a sparse graph in the compressed storage
// format (CSR) from the global BST.
static size_t
_bst_serialize(void *obj, char *buf) {
  BST *bst = static_cast<BST*>(obj);
  unsigned ranks = here->ranks;

  // store the vertex count first
  _BUF(*nvtxs, sizeof(uint64_t));
  *nvtxs = bst->size();

  size_t nsize = *nvtxs * sizeof(uint64_t);

  // next, store the vtxs, vwgt, vsizes and xadj array each nsize bytes long
  _BUF(*vtxs,   nsize);
  _BUF(*vwgt,   nsize);
  _BUF(*vsizes, nsize);
  _BUF(*xadj,   nsize);

  // store the number of edges
  _BUF(*nedges, sizeof(uint64_t));

  // the length of the next two arrays depends on the number of neighbors
  uint64_t *adjncy = (uint64_t*)buf;
  uint64_t *adjwgt = (uint64_t*)malloc(ranks*nsize);

  int id   = 0;
  int nbrs = 0;
  std::vector<uint64_t> lnbrs[ranks];
  {
    auto lt = bst->lock_table();
    for (const auto& item : lt) {
      Entry entry = item.second;
      uint64_t total_vwgt  = 0;
      uint64_t total_vsize = 0;
      int prev_nbrs = nbrs;
      for (unsigned k = 0; k < ranks; ++k) {
        if (entry.counts[k] != 0) {
          lnbrs[k].push_back(id);
          adjncy[nbrs] = k;
          adjwgt[nbrs] = entry.counts[k] * entry.sizes[k];
          total_vwgt  += entry.counts[k];
          total_vsize += entry.sizes[k];
          nbrs++;
        }
      }

      vtxs[id]   = item.first;
      vwgt[id]   = total_vwgt;
      vsizes[id] = total_vsize;
      xadj[id]   = nbrs - prev_nbrs;
      id++;
      free(entry.counts);
      free(entry.sizes);
    }
  }
  *nedges = nbrs;
  buf += (nbrs*sizeof(uint64_t));
  memcpy(buf, adjwgt, nbrs*sizeof(uint64_t));
  free(adjwgt);
  buf += (nbrs*sizeof(uint64_t));

  // size of the buffer excluding the locality neighbor graph
  size_t size = (4*nsize) + (3*sizeof(uint64_t))
    + (2*nbrs*sizeof(uint64_t)) + (ranks*sizeof(uint64_t));

  _BUF(*lsizes, ranks * sizeof(uint64_t));
  for (unsigned k = 0; k < ranks; ++k) {
    lsizes[k] = lnbrs[k].size();
    size_t bytes = lsizes[k]*sizeof(uint64_t);
    memcpy(buf, lnbrs[k].data(), bytes);
    buf  += bytes;
    size += bytes;
  }

  return size;
}

// Serialize the global BST into a parcel. This function allocates and
// returns a parcel.
size_t
bst_serialize_to_parcel(void* obj, hpx_parcel_t **parcel) {
  BST *bst = static_cast<BST*>(obj);
  int nvtxs = bst->size();
  if (!nvtxs) {
    return 0;
  }

  int ranks = here->ranks;
  size_t nsize = nvtxs*sizeof(uint64_t);

  // Serialization format:
  // nvtxs  : sizeof(uint64_t)
  // vtxs   : nsize
  // vwgts  : nsize
  // vsizes : nsize
  // xadj   : nsize
  // nedges : sizeof(uint64_t)
  // adjncy : ranks * nsize
  // adjwgt : ranks * nsize
  // lnbrs  : ranks * sizeof(uint64_t) + ranks * nsize

  size_t buf_size = (4*nsize) + (ranks+2)*sizeof(uint64_t)
    + (3*ranks*nsize);
  *parcel = hpx_parcel_acquire(NULL, buf_size);
  char *buf = static_cast<char*>(hpx_parcel_get_data(*parcel));
  size_t size = _bst_serialize(bst, buf);
  (*parcel)->size = size;
  return size;
}

void
bst_upsert(void *obj, uint64_t block, uint64_t *counts, uint64_t *sizes) {
  BST *bst = static_cast<BST*>(obj);
  auto updatefn = [&](Entry& entry) {
    for (unsigned i = 0; i < here->ranks; ++i) {
      entry.counts[i] += counts[i];
      entry.sizes[i]  += sizes[i];
    }
    free(counts);
    free(sizes);
  };
  bst->upsert(block, updatefn, Entry(counts, sizes));
}
