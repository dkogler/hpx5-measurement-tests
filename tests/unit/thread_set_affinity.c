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

#include <hpx/hpx.h>
#include "tests.h"

static int _test_thread_set_affinity_handler(void) {
  printf("running on %d\n", HPX_THREAD_ID);
  int to = (HPX_THREAD_ID + 1) % HPX_THREADS;
  hpx_thread_set_affinity(to);
  printf("running on %d\n", HPX_THREAD_ID);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _test_thread_set_affinity,
                  _test_thread_set_affinity_handler);

TEST_MAIN({
    ADD_TEST(_test_thread_set_affinity, 0);
  });
