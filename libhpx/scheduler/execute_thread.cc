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

/// @file libhpx/scheduler/execute_thread.c
/// @brief Implementation of the worker_execute_thread function.
///
/// This is implemented in its own file because of its dependency on C++
/// exception handling to deal with non-local return. An alternative to this
/// technique is to rely on the unwind.h header directly along with
/// _Unwind_ForcedUnwind, however that technique had problems with some of our
/// arm platforms.
#include <libhpx/action.h>
#include <libhpx/events.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>


namespace {
  /// A local exception type used to propagate the status from
  /// hpx_thread_exit().
  struct ThreadExitStatus {
    int status;
    ThreadExitStatus(int status) : status(status) {}
  };
}

void HPX_NORETURN worker_execute_thread(hpx_parcel_t *p) {
#ifdef ENABLE_INSTRUMENTATION
  EVENT_THREAD_RUN(p, self);
#endif
  int status = HPX_SUCCESS;
  try {
    status = action_exec_parcel(p->action, p);
  } catch (const ThreadExitStatus &e) {
    status = e.status;
  }
  worker_finish_thread(p, status);
}

/// Exit a thread through a non-local control transfer.
void HPX_NORETURN hpx_thread_exit(int status) {
  throw ThreadExitStatus(status);
}
