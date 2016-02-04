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

#include <libhpx/action.h>
#include "pwc.h"

///
static int _pwc_memcpy_handler(const void *from, hpx_addr_t to, size_t n,
                               hpx_addr_t sync) {
  return pwc_memput_lsync(pwc_network, to, from, n, sync);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _pwc_memcpy, _pwc_memcpy_handler,
                       HPX_POINTER, HPX_ADDR, HPX_SIZE_T, HPX_ADDR);

int pwc_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t n,
               hpx_addr_t sync) {
  return action_call_lsync(_pwc_memcpy, from, 0, 0, 3, &to, &n, &sync);
}

///
static int _pwc_memcpy_rsync_handler(const void *from, hpx_addr_t to, size_t n) {
  return pwc_memput_rsync(pwc_network, to, from, n);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _pwc_memcpy_rsync,
                       _pwc_memcpy_rsync_handler, HPX_POINTER, HPX_ADDR,
                       HPX_SIZE_T);

int pwc_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t n) {
  return action_call_rsync(_pwc_memcpy_rsync, from, NULL, 0, 2, &to, &n);
}
