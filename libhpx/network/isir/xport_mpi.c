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
#include <mpi.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include "parcel_utils.h"
#include "xport.h"

extern MPI_Comm LIBHPX_COMM;

static void
_mpi_check_tag(int tag) {
  int *tag_ub;
  int flag = 0;
  int e = MPI_Comm_get_attr(LIBHPX_COMM, MPI_TAG_UB, &tag_ub, &flag);
  dbg_check(e, "Could not extract tag upper bound\n");
  dbg_assert_str(*tag_ub > tag, "tag value out of bounds (%d > %d)\n", tag,
                 *tag_ub);
}

static size_t
_mpi_sizeof_request(void) {
  return sizeof(MPI_Request);
}

static size_t
_mpi_sizeof_status(void) {
  return sizeof(MPI_Status);
}

static int
_mpi_isend(int to, const void *from, unsigned n, int tag, void *r) {
  int e = MPI_Isend((void *)from, n, MPI_BYTE, to, tag, LIBHPX_COMM, r);
  if (MPI_SUCCESS != e) {
    return log_error("failed MPI_Isend: %u bytes to %d\n", n, to);
  }

  log_net("started MPI_Isend: %u bytes to %d\n", n, to);
  return LIBHPX_OK;
}

static int
_mpi_irecv(void *to, size_t n, int tag, void *request) {
  const int src = MPI_ANY_SOURCE;
  const MPI_Comm com = LIBHPX_COMM;
  if (MPI_SUCCESS != MPI_Irecv(to, n, MPI_BYTE, src, tag, com, request)) {
    return log_error("could not start irecv\n");
  }
  return LIBHPX_OK;
}

static int
_mpi_iprobe(int *tag) {
  int flag;
  MPI_Status stat;
  int e = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, LIBHPX_COMM, &flag, &stat);
  if (MPI_SUCCESS != e) {
    return log_error("failed MPI_Iprobe\n");
  }

  *tag = -1;

  if (flag) {
    *tag = stat.MPI_TAG;
    log_net("probe detected irecv for %u-byte parcel\n",
            tag_to_payload_size(*tag));
  }
  return LIBHPX_OK;
}

static void
_mpi_testsome(int n, void *requests, int *nout, int *out, void *stats) {
  if (!stats) {
    stats = MPI_STATUS_IGNORE;
  }

  int e = MPI_Testsome(n, requests, nout, out, stats);
  dbg_assert_str(e == MPI_SUCCESS, "MPI_Testsome error is fatal.\n");
  dbg_assert_str(*nout != MPI_UNDEFINED, "silent MPI_Testsome() error.\n");
  (void)e;
}

static void
_mpi_clear(void *request) {
  MPI_Request *r = request;
  *r = MPI_REQUEST_NULL;
}

static int
_mpi_cancel(void *request, int *cancelled) {
  if (MPI_SUCCESS != MPI_Cancel(request)) {
    return log_error("could not cancel MPI request\n");
  }

  MPI_Status status;
  if (MPI_SUCCESS != MPI_Wait(request, &status)) {
    return log_error("could not cleanup a canceled MPI request\n");
  }

  int c;
  if (MPI_SUCCESS != MPI_Test_cancelled(&status, (cancelled) ? cancelled : &c)) {
    return log_error("could not test a status to see if a request was canceled\n");
  }

  return LIBHPX_OK;
}

static void
_mpi_finish(void *status, int *src, int *bytes) {
  MPI_Status *s = status;
  if (MPI_SUCCESS != MPI_Get_count(s, MPI_BYTE, bytes)) {
    dbg_error("could not extract the size of an irecv\n");
  }

  dbg_assert(*bytes > 0);
  *src = s->MPI_SOURCE;
}

static void
_mpi_delete(void *mpi) {
  free(mpi);
}

static void
_mpi_pin(const void *base, size_t bytes, void *key) {
}

static void
_mpi_unpin(const void *base, size_t bytes) {
}

static void
_init_mpi(void) {
  int init = 0;
  MPI_Initialized(&init);
  if (!init) {
    static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_SERIALIZED;
    int level;
    int e = MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level);
    if (e != MPI_SUCCESS) {
      dbg_error("mpi initialization failed\n");
    }

    if (level != LIBHPX_THREAD_LEVEL) {
      log_error("MPI thread level failed requested %d, received %d.\n",
                LIBHPX_THREAD_LEVEL, level);
    }

    log_trans("thread_support_provided = %d\n", level);
  }
}

isir_xport_t *
isir_xport_new_mpi(const config_t *cfg, gas_t *gas) {
  isir_xport_t *xport = malloc(sizeof(*xport));
  dbg_assert(xport);
  _init_mpi();

  xport->type           = HPX_TRANSPORT_MPI;
  xport->delete         = _mpi_delete;
  xport->check_tag      = _mpi_check_tag;
  xport->sizeof_request = _mpi_sizeof_request;
  xport->sizeof_status  = _mpi_sizeof_status;
  xport->isend          = _mpi_isend;
  xport->irecv          = _mpi_irecv;
  xport->iprobe         = _mpi_iprobe;
  xport->testsome       = _mpi_testsome;
  xport->clear          = _mpi_clear;
  xport->cancel         = _mpi_cancel;
  xport->finish         = _mpi_finish;
  xport->pin            = _mpi_pin;
  xport->unpin          = _mpi_unpin;

  // local = address_space_new_default(cfg);
  // registered = address_space_new_default(cfg);
  // global = address_space_new_jemalloc_global(cfg, xport, _mpi_pin, _mpi_unpin,
  //                                            gas, gas_mmap, gas_munmap);

  return xport;
}
