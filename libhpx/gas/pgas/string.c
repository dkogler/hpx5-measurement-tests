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

#include <string.h>
#include <hpx/hpx.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include "pgas.h"

int pgas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t n,
                hpx_addr_t sync) {
  if (!n) {
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(to) != here->rank) {
    return network_memcpy(here->network, to, from, n, sync);
  }

  if (gpa_to_rank(from) != here->rank) {
    return network_memcpy(here->network, to, from, n, sync);
  }

  void *lto = pgas_gpa_to_lva(to);
  void *lfrom = pgas_gpa_to_lva(from);
  memcpy(lto, lfrom, n);
  hpx_lco_error(sync, HPX_SUCCESS, HPX_NULL);
  return HPX_SUCCESS;
}

int pgas_memcpy_sync(void *gas, hpx_addr_t to, hpx_addr_t from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  hpx_addr_t sync = hpx_lco_future_new(0);
  dbg_assert_str(sync, "could not allocate an LCO for memcpy_sync.\n");

  int e = pgas_memcpy(gas, to, from, n, sync);
  dbg_check(hpx_lco_wait(sync), "failed agas_memcpy_sync\n");
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}

int pgas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_error(lsync, HPX_SUCCESS, HPX_NULL);
    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(to) == here->rank) {
    void *lto = pgas_gpa_to_lva(to);
    memcpy(lto, from, n);
    hpx_lco_error(lsync, HPX_SUCCESS, HPX_NULL);
    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memput(here->network, to, from, n, lsync, rsync);
}

int pgas_memput_lsync(void *gas, hpx_addr_t to, const void *from, size_t n,
                      hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(to) == here->rank) {
    void *lto = pgas_gpa_to_lva(to);
    memcpy(lto, from, n);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memput_lsync(here->network, to, from, n, rsync);
}

int pgas_memput_rsync(void *gas, hpx_addr_t to, const void *from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(to) == here->rank) {
    void *lto = pgas_gpa_to_lva(to);
    memcpy(lto, from, n);
    return HPX_SUCCESS;
  }

  return network_memput_rsync(here->network, to, from, n);
}

int pgas_memget(void *gas, void *to, hpx_addr_t from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_error(lsync, HPX_SUCCESS, HPX_NULL);
    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(from) == here->rank) {
    void *lfrom = pgas_gpa_to_lva(from);
    memcpy(to, lfrom, n);
    hpx_lco_error(lsync, HPX_SUCCESS, HPX_NULL);
    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memget(here->network, to, from, n, lsync, rsync);
}

int pgas_memget_rsync(void *gas, void *to, hpx_addr_t from, size_t n,
                      hpx_addr_t lsync) {
  if (!n) {
    hpx_lco_error(lsync, HPX_SUCCESS, HPX_NULL);
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(from) == here->rank) {
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(to, lfrom, n);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  return network_memget_rsync(here->network, to, from, n, lsync);
}

int pgas_memget_lsync(void *gas, void *to, hpx_addr_t from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  if (gpa_to_rank(from) == here->rank) {
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(to, lfrom, n);
    return HPX_SUCCESS;
  }

  return network_memget_lsync(here->network, to, from, n);
}
