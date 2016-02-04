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
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <mpi.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>

// Did we initialize MPI? If not, we don't want to finalize it.
static bool _inited_mpi = false;
MPI_Comm LIBHPX_COMM;

static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "MPI";
}

static void _delete(boot_t *boot) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized && _inited_mpi) {
    MPI_Finalize();
  }
}

static int _rank(const boot_t *boot) {
  int rank;
  if (MPI_Comm_rank(LIBHPX_COMM, &rank) != MPI_SUCCESS) {
    hpx_abort();
  }
  return rank;
}

static int _n_ranks(const boot_t *boot) {
  int ranks;
  if (MPI_Comm_size(LIBHPX_COMM, &ranks) != MPI_SUCCESS) {
    hpx_abort();
  }
  return ranks;
}

static int _barrier(const boot_t *boot) {
  if (MPI_Barrier(LIBHPX_COMM) != MPI_SUCCESS) {
    return HPX_ERROR;
  }
  return LIBHPX_OK;
}

static int _allgather(const boot_t *boot, const void *restrict src,
                      void *restrict dest, int n) {
  int e = MPI_Allgather((void *)src, n, MPI_BYTE, dest, n, MPI_BYTE, LIBHPX_COMM);
  if (MPI_SUCCESS != e) {
    dbg_error("failed MPI_Allgather %d.\n", e);
  }
  return LIBHPX_OK;
}

static int _mpi_alltoall(const void *boot, void *restrict dest,
                         const void *restrict src, int n, int stride) {
  const boot_t *mpi = boot;
  int ranks = mpi->n_ranks(mpi);
  int *counts = calloc(ranks, sizeof(*counts));
  int *offsets = calloc(ranks, sizeof(*offsets));
  assert(offsets);
  assert(counts);
  for (int i = 0, e = ranks; i < e; ++i) {
    counts[i] = n;
    offsets[i] = i * stride;
  }
  if (MPI_SUCCESS != MPI_Alltoallv((void *)src, counts, offsets, MPI_BYTE,
                                   dest, counts, offsets, MPI_BYTE,
                                   LIBHPX_COMM)) {
    dbg_error("MPI_Alltoallv failed at bootstrap\n");
  }
  free(offsets);
  free(counts);
  return LIBHPX_OK;
}

static void _abort(const boot_t *boot) {
  MPI_Abort(LIBHPX_COMM, -6);
}

static boot_t _mpi_boot_class = {
  .type      = HPX_BOOT_MPI,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .alltoall  = _mpi_alltoall,
  .abort     = _abort
};

boot_t *boot_new_mpi(void) {
  int init;
  if (MPI_SUCCESS != MPI_Initialized(&init)) {
    log_error("mpi initialization failed\n");
    return NULL;
  }

  if (init) {
    return &_mpi_boot_class;
  }

  static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_SERIALIZED;

  int level;
  if (MPI_SUCCESS != MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level)) {
    log_error("mpi initialization failed\n");
    return NULL;
  }
  _inited_mpi = true;

  if (level != LIBHPX_THREAD_LEVEL) {
    log_boot("MPI thread level failed requested %d, received %d.\n",
         LIBHPX_THREAD_LEVEL, level);
  }

  if (MPI_SUCCESS != MPI_Comm_dup(MPI_COMM_WORLD, &LIBHPX_COMM)) {
    log_error("mpi communicator duplication failed\n");
    return NULL;
  }

  log_boot("thread_support_provided = %d\n", level);
  return &_mpi_boot_class;
}
