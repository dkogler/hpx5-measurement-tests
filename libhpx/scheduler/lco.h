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

#ifndef LIBHPX_SCHEDULER_LCO_H
#define LIBHPX_SCHEDULER_LCO_H

#include <inttypes.h>
#include <hpx/attributes.h>
#include <libhpx/lco.h>
#include "cvar.h"

#define LCO_LOG_NEW(gva, lva) do {                              \
    dbg_assert_str(gva, "Could not malloc global memory\n");    \
    log_lco("allocated lco %"PRIu64" (%p)\n", gva, (void*)lva); \
  } while (0)

/// This constant is used to determine when a set should be performed
/// asynchronously, even if the set is actually local.
static const int HPX_LCO_SET_ASYNC = 512;

typedef enum {
  LCO_ALLREDUCE = 0,
  LCO_ALLTOALL,
  LCO_AND,
  LCO_FUTURE,
  LCO_GATHER,
  LCO_GENCOUNT,
  LCO_REDUCE,
  LCO_SEMA,
  LCO_USER,
  LCO_MAX
} lco_type_t;

/// The action used to propagate an LCO error.
extern HPX_ACTION_DECL(lco_error);

/// The LCO abstract class interface.
///
/// All LCOs will implement this interface, which is accessible through the
/// hpx_lco set of generic actions. Concrete classes may implement extended
/// interfaces as well.
///
/// This interface is locally synchronous, but will be invoked externally
/// through the set of hpx_lco_* operations that may use them asynchronously.
typedef void (*lco_fini_t)(lco_t *lco);
typedef int (*lco_set_t)(lco_t *lco, int size, const void *value);
typedef void (*lco_error_t)(lco_t *lco, hpx_status_t code);
typedef hpx_status_t (*lco_get_t)(lco_t *lco, int size, void *value, int reset);
typedef hpx_status_t (*lco_getref_t)(lco_t *lco, int size, void **out, int *unpin);
typedef int (*lco_release_t)(lco_t *lco, void *out);
typedef hpx_status_t (*lco_wait_t)(lco_t *lco, int reset);
typedef hpx_status_t (*lco_attach_t)(lco_t *lco, hpx_parcel_t *p);
typedef void (*lco_reset_t)(lco_t *lco);
typedef size_t (*lco_size_t)(lco_t *lco);

typedef struct lco_class lco_class_t;
struct lco_class {
  lco_type_t            type;
  lco_fini_t         on_fini;
  lco_error_t       on_error;
  lco_set_t           on_set;
  lco_attach_t     on_attach;
  lco_get_t           on_get;
  lco_getref_t     on_getref;
  lco_release_t   on_release;
  lco_wait_t         on_wait;
  lco_reset_t       on_reset;
  lco_size_t         on_size;
} HPX_ALIGNED(16);

extern const lco_class_t *lco_vtables[];

/// Lock an LCO.
///
/// @param lco  The LCO to lock
void lco_lock(lco_t *lco)
  HPX_NON_NULL(1);

/// Unlock an LCO.
///
/// The calling thread must currently hold the LCO's lock.
///
/// @param lco - the LCO to unlock
void lco_unlock(lco_t* lco)
  HPX_NON_NULL(1);

/// Initialize an LCO vtable pointer.
///
/// @param           lco The pointer to initialize
/// @param         class The class pointer for this LCO instance
void lco_init(lco_t *lco, const lco_class_t *class)
  HPX_NON_NULL(1,2);

/// Finalize an LCO vtable pointer.
///
/// @param           lco The pointer to finalize.
void lco_fini(lco_t *lco)
  HPX_NON_NULL(1);

/// Set the triggered state to true.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to trigger.
void lco_set_triggered(lco_t *lco)
  HPX_NON_NULL(1);

/// Reset the triggered state to false.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to trigger.
void lco_reset_triggered(lco_t *lco)
  HPX_NON_NULL(1);

/// Get the triggered state.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to read.
///
/// @returns Non-zero if the triggered bit is set, zero otherwise.
uintptr_t lco_get_triggered(const lco_t *lco)
  HPX_NON_NULL(1);

/// Set the user state to true.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The target LCO.
void lco_set_user(lco_t *lco)
  HPX_NON_NULL(1);

/// Get the user state of an LCO.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to read.
///
/// @returns Non-zero if the user bit is set, zero otherwise.
uintptr_t lco_get_user(const lco_t *lco)
  HPX_NON_NULL(1);

// Helper macros to allocate LCOs in the global address space
#define lco_alloc_local(n, size, boundary)                      \
  hpx_gas_alloc_local_attr(n, size, boundary, HPX_GAS_ATTR_LCO)

#define lco_alloc_cyclic(n, size, boundary)                      \
  hpx_gas_alloc_cyclic_attr(n, size, boundary, HPX_GAS_ATTR_LCO)

#endif // LIBHPX_SCHEDULER_LCO_H
