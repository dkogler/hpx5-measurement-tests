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

#ifndef HPX_RUNTIME_H
#define HPX_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <hpx/action.h>

/// @defgroup system System
/// @file include/hpx/runtime.h
/// @brief Functions that control the overall runtime
/// @{

/// Initializes the HPX runtime.
///
/// This must be called before other HPX functions.  hpx_init() initializes the
/// scheduler, network, and locality and should be called before any other HPX
/// functions.
///
/// @param         argc Count of command-line arguments.
/// @param         argv Array of command-line arguments.
/// @returns      HPX_SUCCESS on success
int hpx_init(int *argc, char ***argv)
  HPX_PUBLIC;

/// Finalize/cleanup from the HPX runtime.
///
/// This function will remove almost all data structures and allocations, and
/// will finalize the underlying network implementation. Note that hpx_run()
/// must never be called after hpx_finalize().
void hpx_finalize()
  HPX_PUBLIC;

/// Start the HPX runtime, and run a given action.
///
/// This creates an HPX "main" process, and calls the given action @p entry in
/// the context of this process. The @p entry action is invoked only on the root
/// locality. On termination, it deletes the main process and returns the status
/// returned by hpx_exit()
///
/// hpx_run finalizes action registration, starts up any scheduler and native
/// threads that need to run, and transfers all control into the HPX scheduler,
/// beginning execution of the top action in the scheduler queue.
///
/// The scheduler queue could be empty, in which case the entire scheduler
/// instance is running, waiting for a successful inter-locality steal operation
/// (if that is implemented) or a network parcel.
///
/// @param        entry An action to execute, or HPX_ACTION NULL to wait for an
///                     incoming parcel or a inter-locality steal (if
///                     implemented).
/// @param        nargs The number of arguments to pass to @p entry.
///
/// @returns            The status code passed to hpx_exit() upon termination.
int _hpx_run(hpx_action_t *entry, int nargs, ...)
  HPX_PUBLIC;

#define hpx_run(entry, ...)                                 \
  _hpx_run(entry, __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Exit the HPX runtime.
///
/// This causes the hpx_run() in the main native thread to return the @p
/// code. It is safe to call hpx_run() again after hpx_exit().
///
/// This call does not imply that the HPX runtime has shut down. In particular,
/// system threads may continue to run and execute HPX high-speed network
/// progress or outstanding lightweight threads. Users should ensure that such
/// concurrent activity will not create detrimental data races in their
/// applications.
///
/// @note This routine enacts non-local control flow, however the runtime must
///       correctly unwind the stack for languages that require such behavior
///       (e.g., C++).
///
/// @note While this routine does not guarantee to suspend the runtime,
///       high-performance implementations are expected to reduce their resource
///       consumption as a result of this call. In particular, runtime-spawned
///       system threads should be suspended.
///
/// @param         code A status code to be returned by hpx_run().
void hpx_exit(int code)
  HPX_PUBLIC HPX_NORETURN;

/// Abort the HPX runtime.
///
/// This is not cooperative, and will not clean up any resources.
void hpx_abort(void)
  HPX_PUBLIC HPX_NORETURN;

/// Print the help string associated with the runtime configuration
/// options supported by the HPX runtime.
void hpx_print_help(void)
  HPX_PUBLIC;

/// Print the version string associated with the HPX interface implemented by
/// the runtime.
void hpx_print_version(void)
  HPX_PUBLIC;

/// @}

#ifdef __cplusplus
}
#endif

#endif
