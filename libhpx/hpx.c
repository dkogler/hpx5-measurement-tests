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

/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/percolation.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/time.h>
#include <libhpx/topology.h>

#ifdef HAVE_APEX
#include "apex.h"
#endif

static hpx_addr_t _hpx_143;
static int _hpx_143_fix_handler(void) {
  _hpx_143 = hpx_gas_alloc_cyclic(sizeof(void*), HPX_LOCALITIES, 0);
  hpx_exit(HPX_SUCCESS);
  return LIBHPX_OK;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _hpx_143_fix, _hpx_143_fix_handler);

/// Stop HPX
///
/// This will stop HPX by stopping the network and scheduler, and cleaning up
/// anything that should not persist between hpx_run() calls.
static void _stop(locality_t *l) {
  if (!l)
    return;

  if (l->sched) {
    scheduler_delete(l->sched);
    l->sched = NULL;
  }

  if (l->network) {
    network_delete(l->network);
    l->network = NULL;
  }
}

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
  if (!l)
    return;

#ifdef HAVE_APEX
  apex_finalize();
#endif

  if (l->percolation) {
    percolation_delete(l->percolation);
    l->percolation = NULL;
  }

  if (l->gas) {
    gas_dealloc(l->gas);
    l->gas = NULL;
  }

  dbg_fini();

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  if (l->topology) {
    topology_delete(l->topology);
    l->topology = NULL;
  }

  action_table_finalize();
  inst_fini();

  if (l->config) {
    config_delete(l->config);
  }

  free(l);
}

int hpx_init(int *argc, char ***argv) {
  int status = HPX_SUCCESS;

  // Start the internal clock
  libhpx_time_start();

  here = malloc(sizeof(*here));
  if (!here) {
    status = log_error("failed to allocate a locality.\n");
    goto unwind0;
  }

  here->rank = -1;
  here->ranks = 0;
  here->epoch = 0;

  sigset_t set;
  sigemptyset(&set);
  dbg_check(pthread_sigmask(SIG_BLOCK, &set, &here->mask));

  here->config = config_new(argc, argv);
  if (!here->config) {
    status = log_error("failed to create a configuration.\n");
    goto unwind1;
  }

  // check to see if everyone is waiting
  if (config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
    dbg_wait();
  }

  // bootstrap
  here->boot = boot_new(here->config->boot);
  if (!here->boot) {
    status = log_error("failed to bootstrap.\n");
    goto unwind1;
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // initialize the debugging system
  // @todo We would like to do this earlier but MPI_init() for the bootstrap
  //       network overwrites our segv handler.
  if (LIBHPX_OK != dbg_init(here->config)) {
    goto unwind1;
  }

  // Now that we know our rank, we can be more specific about waiting.
  if (config_dbg_waitat_isset(here->config, here->rank)) {
    // Don't wait twice.
    if (!config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
      dbg_wait();
    }
  }

  // see if we're supposed to output the configuration, only do this at rank 0
  if (config_log_level_isset(here->config, HPX_LOG_CONFIG)) {
    if (here->rank == 0) {
      config_print(here->config, stdout);
    }
  }

  // topology discovery and initialization
  here->topology = topology_new(here->config);
  if (!here->topology) {
    status = log_error("failed to discover topology.\n");
    goto unwind1;
  }

  // Initialize our instrumentation.
  if (inst_init(here->config)) {
    log_dflt("error detected while initializing instrumentation\n");
  }

  // Allocate the global heap.
  here->gas = gas_new(here->config, here->boot);
  if (!here->gas) {
    status = log_error("failed to create the global address space.\n");
    goto unwind1;
  }
  HPX_HERE = HPX_THERE(here->rank);

  here->percolation = percolation_new();
  if (!here->percolation) {
    status = log_error("failed to activate percolation.\n");
    goto unwind1;
  }

  int cores = system_get_available_cores();
  dbg_assert(cores > 0);

  if (!here->config->threads) {
    here->config->threads = cores;
  }
  log_dflt("HPX running %d worker threads on %d cores\n", here->config->threads,
           cores);

  here->network = network_new(here->config, here->boot, here->gas);
  if (!here->network) {
    status = log_error("failed to create network.\n");
    goto unwind1;
  }

  // thread scheduler
  here->sched = scheduler_new(here->config);
  if (!here->sched) {
    status = log_error("failed to create scheduler.\n");
    goto unwind1;
  }

#ifdef HAVE_APEX
  // initialize APEX, give this main thread a name
  apex_init("HPX WORKER THREAD");
  apex_set_node_id(here->rank);
#endif

  action_registration_finalize();
  inst_start();

  // start the scheduler, this will return after scheduler_shutdown()
  if (scheduler_startup(here->sched, here->config) != LIBHPX_OK) {
    log_error("scheduler shut down with error.\n");
    goto unwind1;
  }

  if ((here->ranks > 1 && here->config->gas != HPX_GAS_AGAS) ||
      !here->config->opt_smp) {
    status = hpx_run(&_hpx_143_fix);
  }

  return status;
 unwind1:
  _stop(here);
  _cleanup(here);
 unwind0:
  return status;
}

/// Called to run HPX.
int _hpx_run(hpx_action_t *act, int n, ...) {
  if (here->rank == 0) {
    va_list args;
    va_start(args, n);
    hpx_parcel_t *p = action_new_parcel_va(*act, HPX_HERE, 0, 0, n, &args);
    va_end(args);
    dbg_check(hpx_parcel_send(p, HPX_NULL), "failed to spawn initial action\n");
  }
  log_dflt("hpx started running %"PRIu64"\n", here->epoch);
  int status = scheduler_restart(here->sched);
  log_dflt("hpx stopped running %"PRIu64"\n", here->epoch);

  // We need to flush the network here, because it might have messages that are
  // required for progress.
  here->network->flush(here->network);

  // Bump our epoch, and enforce the "collective" nature of run with a boot
  // barrier.
  here->epoch++;
  boot_barrier(here->boot);

  return status;
}

int hpx_get_my_rank(void) {
  return (here) ? here->rank : -1;
}

int hpx_get_num_ranks(void) {
  return (here && here->boot) ? here->ranks : -1;
}

int hpx_get_num_threads(void) {
  return (here && here->sched) ? here->sched->n_workers : -1;
}

int hpx_is_active(void) {
  return (self->current != NULL);
}

/// Called by the application to terminate the scheduler and network.
void hpx_exit(int code) {
  dbg_assert_str(here->ranks,
                 "hpx_exit can only be called when the system is running.\n");

  uint64_t c = (uint32_t)code;

  // Make sure we flush our local network when we stop, but don't send our own
  // shutdown here because it can "arrive" locally very quickly, before we've
  // even come close to sending the rest of the stop commands. This can cause
  // problems with flushing.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    if (i != here->rank) {
      int e = action_call_lsync(locality_stop, HPX_THERE(i), 0, 0, 1, &c);
      dbg_check(e);
    }
  }

  // Call our own shutdown through cc, which orders it locally after the effects
  // from the loop above.
  int e = hpx_call_cc(HPX_HERE, locality_stop, &c);
  hpx_thread_exit(e);
}

/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  inst_fini();

  if (here && here->config && here->config->dbg_waitonabort) {
    dbg_wait();
  }
  if (here && here->boot) {
    assert(here->boot);
    boot_abort(here->boot);
  }
  abort();
}

const char *hpx_strerror(hpx_status_t s) {
  switch (s) {
   case (HPX_ERROR): return "HPX_ERROR";
   case (HPX_SUCCESS): return "HPX_SUCCESS";
   case (HPX_RESEND): return "HPX_RESEND";
   case (HPX_LCO_ERROR): return "HPX_LCO_ERROR";
   case (HPX_LCO_CHAN_EMPTY): return "HPX_LCO_CHAN_EMPTY";
   case (HPX_LCO_TIMEOUT): return "HPX_LCO_TIMEOUT";
   case (HPX_LCO_RESET): return "HPX_LCO_RESET";
   case (HPX_USER): return "HPX_USER";
   default: return "HPX undefined error value";
  }
}

void hpx_finalize(void) {
  // clean up after _hpx_143
  if (_hpx_143 != HPX_NULL) {
    hpx_gas_free(_hpx_143, HPX_NULL);
  }

#if defined(HAVE_APEX)
  // this will add the stats to the APEX data set
  libhpx_save_apex_stats();
#endif
  _stop(here);

#if defined(ENABLE_PROFILING)
  libhpx_stats_print();
#endif
  _cleanup(here);
}
