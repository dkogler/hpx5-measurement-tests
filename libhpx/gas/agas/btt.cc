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

#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include "btt.h"

namespace {
  struct Entry {
    int32_t count;
    uint32_t owner;
    void *lva;
    size_t blocks;
    hpx_parcel_t *onunpin;
    uint32_t attr;
    Entry() : count(0), owner(0), lva(NULL), blocks(1), onunpin(NULL), attr(0) {
    }
    Entry(int32_t o, void *l, size_t b, hpx_parcel_t *p, uint32_t a) : count(0), owner(o), lva(l), blocks(b), onunpin(p), attr(a)
    {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BTT : public Map {
   public:
    BTT(size_t);
    hpx_parcel_t *attachParcel(gva_t gva, hpx_parcel_t *p);
    bool tryPin(gva_t gva, void** lva);
    hpx_parcel_t *unpin(gva_t gva);
    void *lookup(gva_t gva) const;
    uint32_t getOwner(gva_t gva) const;
    void setOwner(gva_t gva, uint32_t owner) const;
    size_t getBlocks(gva_t gva) const;
  };
}

BTT::BTT(size_t size) : Map(size) {
}

hpx_parcel_t *
BTT::attachParcel(gva_t gva, hpx_parcel_t *p) {
  uint64_t key = gva_to_key(gva);
  bool ret = false;
  bool found = update_fn(key, [&](Entry& entry) {
      if (entry.owner != here->rank || entry.count == 0) {
        ret = true;
        return;
      }

      // If there are pending pins on this block, we register a parcel
      // that is launched when the reference count goes to 0.
      assert(entry.onunpin == NULL);
      entry.onunpin = p;
    });
  assert(found);
  return (ret ? p : NULL);
}

bool
BTT::tryPin(gva_t gva, void** lva) {
  uint64_t key = gva_to_key(gva);
  bool ret = true;
  bool found = update_fn(key, [&](Entry& entry) {
      if (!lva) {
        return;
      }

      // If we do not own the block or if there is a pending delete on
      // this block, the try-pin operation fails.
      if (entry.owner != here->rank || entry.onunpin != NULL) {
        ret = false;
        return;
      }

      assert(entry.count >= 0);
      entry.count++;
      // printf("%lu %d ++\n", key, entry.count);
      *lva = (char*)(entry.lva) + gva_to_block_offset(gva);
    });
  return found && ret;
}

hpx_parcel_t *
BTT::unpin(gva_t gva) {
  uint64_t key = gva_to_key(gva);
  bool ret = false;
  hpx_parcel_t *p;
  bool found = update_fn(key, [&](Entry& entry) {
      assert(entry.owner == here->rank);
      assert(entry.count > 0);
      entry.count--;
      // printf("%lu %d --\n", key, entry.count);
      if (entry.count == 0 && entry.onunpin) {
        p = entry.onunpin;
        entry.onunpin = NULL;
        ret = true;
      }
    });
  assert(found);
  return (ret ? p : NULL);
}

void *
BTT::lookup(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.lva;
  }
  else {
    return NULL;
  }
}

uint32_t
BTT::getOwner(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.owner;
  }
  else {
    return gva.bits.home;
  }
}

void
BTT::setOwner(gva_t gva, uint32_t owner) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    entry.owner = owner;
  }
}

size_t
BTT::getBlocks(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.blocks;
  }
  else {
    return 0;
  }
}

void *
btt_new(size_t size) {
  return new BTT(size);
}

void
btt_delete(void* obj) {
  BTT *btt = static_cast<BTT*>(obj);
  delete btt;
}

void
btt_insert(void *obj, gva_t gva, uint32_t owner, void *lva, size_t blocks,
           uint32_t attr) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool inserted = btt->insert(key, Entry(owner, lva, blocks, NULL, attr));
  assert(inserted);
  (void)inserted;
}

void
btt_remove(void *obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool erased = btt->erase(key);
  assert(erased);
  (void)erased;
}

bool
btt_try_pin(void* obj, gva_t gva, void** lva) {
  BTT *btt = static_cast<BTT*>(obj);
  return btt->tryPin(gva, lva);
}

void
btt_unpin(void* obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  hpx_parcel_t *p = btt->unpin(gva);
  if (p) {
    parcel_launch(p);
  }
}

void *
btt_lookup(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->lookup(gva);
}

uint32_t
btt_owner_of(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getOwner(gva);
}

size_t
btt_get_blocks(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getBlocks(gva);
}

int
btt_get_all(const void *o, gva_t gva, void **lva, size_t *blocks,
            int32_t *cnt) {
  const BTT *btt = static_cast<const BTT*>(o);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  if (found) {
    if (lva) {
      *lva = entry.lva;
    }
    if (blocks) {
      *blocks = entry.blocks;
    }
    if (cnt) {
      *cnt = entry.count;
    }
  }
  return found;
}

typedef struct {
  BTT        *btt;
  gva_t       gva;
} _btt_attach_parcel_env_t;

static void _btt_attach_parcel_continuation(hpx_parcel_t *p, void *e) {
  _btt_attach_parcel_env_t *env = static_cast<_btt_attach_parcel_env_t*>(e);
  BTT *btt = env->btt;
  gva_t gva = env->gva;
  if ((p = btt->attachParcel(gva, p))) {
    parcel_launch(p);
  }
}

static int
_btt_wait_until_count_zero(void *obj, gva_t gva, void **lva, uint32_t *attr) {
  BTT *btt = static_cast<BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  if (!btt->find(key, entry)) {
    return HPX_ERROR;
  }

  if (lva) {
    *lva = entry.lva;
  }

  if (attr) {
    *attr = entry.attr;
  }

  // Wait until the reference count hits zero.
  if (entry.count) {
    _btt_attach_parcel_env_t env = {
      .btt = btt,
      .gva = gva
    };

    scheduler_suspend(_btt_attach_parcel_continuation, &env, 0);
  }
  return HPX_SUCCESS;
}

int btt_remove_when_count_zero(void *obj, gva_t gva, void **lva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  int e = _btt_wait_until_count_zero(obj, gva, lva, NULL);
  bool erased = btt->erase(key);
  assert(erased);
  return e;
  (void)erased;
}

int btt_try_move(void *obj, gva_t gva, int rank, void **lva, uint32_t *attr) {
  BTT *btt = static_cast<BTT*>(obj);
  int e = _btt_wait_until_count_zero(obj, gva, lva, attr);
  btt->setOwner(gva, rank);
  return e;
}
