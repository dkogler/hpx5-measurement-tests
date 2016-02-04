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

#ifndef LIBHPX_BOOT_H
#define LIBHPX_BOOT_H

#include "hpx/attributes.h"
#include "hpx/hpx.h"
#include "libhpx/config.h"

typedef struct boot {
  libhpx_boot_t type;

  const char *(*id)(void);

  void (*delete)(struct boot*);
  int (*rank)(const struct boot*);
  int (*n_ranks)(const struct boot*);
  int (*barrier)(const struct boot*);
  int (*allgather)(const struct boot *boot, const void *restrict src,
                   void *restrict dest, int n);
  int (*alltoall)(const void *boot, void *restrict dest,
                  const void *restrict src, int n, int stride);
  void (*abort)(const struct boot*);
} boot_t;

boot_t *boot_new_mpi(void);
boot_t *boot_new_pmi(void);
boot_t *boot_new_smp(void);
boot_t *boot_new(libhpx_boot_t type);

static inline void boot_delete(boot_t *boot) {
  boot->delete(boot);
}

static inline libhpx_boot_t boot_type(boot_t *boot) {
  return boot->type;
}

static inline int boot_rank(const boot_t *boot) {
  return boot->rank(boot);
}

static inline int boot_n_ranks(const boot_t *boot) {
  return boot->n_ranks(boot);
}

static inline int boot_allgather(const boot_t *boot, const void *restrict in,
                                 void *restrict out, int n) {
  return boot->allgather(boot, in, out, n);
}

static inline int boot_alltoall(const boot_t *boot, void *restrict dest,
                                const void *restrict src, int n, int stride) {
  return boot->alltoall(boot, dest, src, n, stride);
}

static inline int boot_barrier(const boot_t *boot) {
  return boot->barrier(boot);
}

static inline void boot_abort(const boot_t *boot) {
  boot->abort(boot);
}

#endif
