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

#ifndef LIBHPX_SCHEDULER_CVAR_H
#define LIBHPX_SCHEDULER_CVAR_H

#include <hpx/hpx.h>

/// Forward declarations.
/// @{
struct ustack;
/// @}

typedef struct cvar {
  hpx_parcel_t *top;
} cvar_t;

/// Reset a condition variable.
void cvar_reset(cvar_t *cvar)
  HPX_NON_NULL(1);

/// Check if a condition variable is empty---there are not waiting
/// threads associated with the condition variable.
bool cvar_empty(cvar_t *cvar)
  HPX_NON_NULL(1);

/// Set the condition with an error code.
///
/// This records the error in the condition, and returns the list of parcels
/// that were waiting. The error code will overwrite this list, so it must be
/// processed here.
///
/// @param         cvar The condition variable to set.
/// @param         code The user-defined error code.
///
/// @returns            The previous list of waiting parcels.
hpx_parcel_t *cvar_set_error(cvar_t *cvar, hpx_status_t code)
  HPX_NON_NULL(1);

/// Retrieve an error code from the condition variable.
///
/// This will check to see if an error is set on a condition variable, and
/// return it if there is one.
///
/// @param         cvar The condition variable to check.
///
/// @returns            The error state for the condition variable.
hpx_status_t cvar_get_error(const cvar_t *cvar)
  HPX_NON_NULL(1);

/// Clear an error condition.
///
/// If there is no error condition set, then this does not modify the condition
/// variable, otherwise it clears the error.
///
/// @param         cvar The condition variable to update.
void cvar_clear_error(cvar_t *cvar)
  HPX_NON_NULL(1);

/// Push an executing thread onto a condition variable.
///
/// If the condition is in an error condition, this will return that error
/// without pushing the thread, otherwise it will push the thread and return
/// HPX_SUCCESS.
///
/// @param         cvar The condition variable to modify.
/// @param       thread The thread to push.
///
/// @returns            HPX_SUCCESS or an error code
hpx_status_t cvar_push_thread(cvar_t *cvar, struct ustack *thread)
  HPX_NON_NULL(1, 2);

/// Push a parcel onto a condition variable directly.
///
/// If the condition is in an error condition, this will return that error
/// condition without pushing the parcel onto the condition's queue, otherwise
/// it will push the parcel and return HPX_SUCCESS.
///
/// @param         cvar The condition variable to modify.
/// @param       parcel The parcel to push.
///
/// @returns            HPX_SUCCESS or an error code
hpx_status_t cvar_attach(cvar_t *cvar, struct hpx_parcel *parcel)
  HPX_NON_NULL(1, 2);

/// Pop the top parcel from a condition variable.
///
/// @param         cvar The condition to pop.
///
/// @return             The top parcel, or NULL if the condition is empty or has
///                       an error.
hpx_parcel_t *cvar_pop(cvar_t *cvar)
  HPX_NON_NULL(1);

/// Pop the parcel list from a condition variable.
///
/// @param         cvar The condition to pop.
///
/// @return             The list of continuation parcels (NULL if there are
///                       none).
hpx_parcel_t *cvar_pop_all(cvar_t *cvar)
  HPX_NON_NULL(1);

#endif // LIBHPX_SCHEDULER_CVAR_H
