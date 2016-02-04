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

#include "libhpx/action.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/worker.h"
#include "commands.h"
#include "pwc.h"
#include "xport.h"

/// This acts as a parcel_suspend transfer to allow _pwc_lco_get_request_handler
/// to wait for its pwc to complete.
static void _get_reply_continuation(hpx_parcel_t *p, void *env) {
  xport_op_t *op = env;

  // at this point we know which parcel to resume for local completion
  op->lop.op  = RESUME_PARCEL;
  op->lop.arg = (uintptr_t)p;
  dbg_check( pwc_network->xport->pwc(op) );
}

typedef struct {
  hpx_parcel_t *p;
  size_t n;
  void *out;
  int reset;
  xport_key_t key;
  int rank;
} _pwc_lco_get_request_args_t;

/// This function (*not* an action) consolidates the functionality to issue a
/// synchronous get reply via put-with-completion using the scheduler_suspend
/// interface.
///
/// We could actually do the xport_op_t construction in the reply transfer
/// continuation, but we'd need an environment to pass down there anyway, so we
/// use the xport_op_t for that.
static int _get_reply(_pwc_lco_get_request_args_t *args, pwc_network_t *pwc,
                      const void *ref, command_t remote) {
  // Create the transport operation to perform the rdma put operation
  xport_op_t op = {
    .rank = args->rank,
    .n = args->n,
    .dest = args->out,
    .dest_key = args->key,
    .src = ref,
    .src_key = pwc->xport->key_find_ref(pwc->xport, ref, args->n),
    .lop = {0},                                // set in _get_reply_continuation
    .rop = remote
  };
  dbg_assert_str(op.src_key, "LCO reference must point to registered memory\n");

  // Issue the pwc and wait for synchronous local completion so that the ref
  // buffer doesn't move during the underlying rdma, if there is any
  scheduler_suspend(_get_reply_continuation, &op, 0);
  return HPX_SUCCESS;
}

/// This function (*not* an action) performs a get request to a temporary stack
/// location.
static int _get_reply_stack(_pwc_lco_get_request_args_t *args,
                            pwc_network_t *pwc, hpx_addr_t lco) {
  char ref[args->n];

  int e = HPX_SUCCESS;
  if (args->reset) {
    e = hpx_lco_get_reset(lco, args->n, ref);
  }
  else {
    e = hpx_lco_get(lco, args->n, ref);
  }

  if (e != HPX_SUCCESS) {
    dbg_error("Cannot yet return an error from a remote get operation\n");
  }

  command_t resume = {
    .op  = RESUME_PARCEL,
    .arg = (uintptr_t)args->p
  };

  return _get_reply(args, pwc, ref, resume);
}

/// This function (*not* an action) performs a get request to a temporary
/// malloced location.
static int _get_reply_malloc(_pwc_lco_get_request_args_t *args,
                             pwc_network_t *pwc, hpx_addr_t lco) {
  void *ref = registered_malloc(args->n);
  dbg_assert(ref);

  int e = HPX_SUCCESS;
  if (args->reset) {
    e = hpx_lco_get_reset(lco, args->n, ref);
  }
  else {
    e = hpx_lco_get(lco, args->n, ref);
  }

  if (e != HPX_SUCCESS) {
    dbg_error("Cannot yet return an error from a remote get operation\n");
  }

  command_t resume = {
    .op  = RESUME_PARCEL,
    .arg = (uintptr_t)args->p
  };
  e = _get_reply(args, pwc, ref, resume);
  registered_free(ref);
  return e;
}

/// This function (*not* an action) performs a two-phase get request without any
/// temporary storage.
static int _get_reply_getref(_pwc_lco_get_request_args_t *args,
                             pwc_network_t *pwc, hpx_addr_t lco) {
  // Get a reference to the LCO data
  void *ref;
  int e = hpx_lco_getref(lco, args->n, &ref);

  if (e != HPX_SUCCESS) {
    dbg_error("Cannot yet return an error from a remote get operation\n");
  }

  // Send back the LCO data. This doesn't resume the remote thread because there
  // is a race where a delete can trigger a use-after-free during our subsequent
  // release.
  e = _get_reply(args, pwc, ref, (command_t){0});
  dbg_check(e, "Failed rendezvous put during remote lco get request.\n");

  // Release the reference.
  hpx_lco_release(lco, ref);

  // Wake the remote getter up.
  command_t rcmd = {
    .op  = RESUME_PARCEL,
    .arg = (uintptr_t)args->p
  };
  e = pwc_cmd(pwc, args->rank, (command_t){0}, rcmd);
  dbg_check(e, "Failed to start resume command during remote lco get.\n");
  return e;
}

/// This action is sent to execute the request half of a two-sided LCO get
/// operation.
static int _pwc_lco_get_request_handler(_pwc_lco_get_request_args_t *args,
                                        size_t n) {
  dbg_assert(n > 0);

  hpx_addr_t lco = hpx_thread_current_target();

  // We would like to rdma directly from the LCO's buffer, when
  // possible. Unfortunately, this induces a race where the returned put
  // operation completes at the get location before the rdma is detected as
  // completing here. This allows the user to correctly delete the LCO while the
  // local thread still has a reference to the buffer which leads to
  // use-after-free. At this point we can only do the getref() version using two
  // put operations, one to put back to the waiting buffer, and one to resume
  // the waiting thread after we drop our local reference.
  if (args->n > LIBHPX_SMALL_THRESHOLD && !args->reset) {
    return _get_reply_getref(args, pwc_network, lco);
  }

  // If there is enough space to stack allocate a buffer to copy, use the stack
  // version, otherwise malloc a buffer to copy to.
  else if (hpx_thread_can_alloca(args->n) >= HPX_PAGE_SIZE) {
    return _get_reply_stack(args, pwc_network, lco);
  }

  // Otherwise we get to a registered buffer and then do the put. The theory
  // here is that a single small-ish (< LIBHPX_SMALL_THRESHOLD) malloc, memcpy,
  // and free, is more efficient than doing two pwc() operations.
  //
  // NB: We need to verify that this heuristic is actually true, and that the
  //     LIBHPX_SMALL_THRESHOLD is appropriate. Honestly, given enough work to
  //     do, the latency of two puts might not be a big deal.
  else {
    return _get_reply_malloc(args, pwc_network, lco);
  }
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _pwc_lco_get_request,
                     _pwc_lco_get_request_handler, HPX_POINTER, HPX_SIZE_T);

typedef struct {
  _pwc_lco_get_request_args_t request;
  hpx_addr_t lco;
} _pwc_lco_get_continuation_env_t;

/// Issue the get request parcel from a transfer continuation.
static void _pwc_lco_get_continuation(hpx_parcel_t *p, void *env) {
  _pwc_lco_get_continuation_env_t *e = env;
  e->request.p = p;
  hpx_action_t act = _pwc_lco_get_request;
  size_t n = sizeof(e->request);
  dbg_check(action_call_lsync(act, e->lco, 0, 0, 2, &e->request, n));
}

/// This is the top-level LCO get handler that is called for (possibly) remote
/// LCOs. It builds and sends a get request parcel, that will reply using pwc()
/// to write directly to @p out.
///
/// This operation is synchronous and will block until the operation has
/// completed.
int pwc_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset) {
  _pwc_lco_get_continuation_env_t env = {
    .request = {
      .p = NULL,                             // set in _pwc_lco_get_continuation
      .n = n,
      .out = out,
      .reset = reset,
      .rank = here->rank,
      .key = {0}
    },
    .lco = lco
  };

  // If the output buffer is already registered, then we just need to copy the
  // key into the args structure, otherwise we need to register the region.
  const void *key = pwc_network->xport->key_find_ref(pwc_network->xport, out, n);
  if (key) {
    pwc_network->xport->key_copy(&env.request.key, key);
  }
  else {
    pwc_network->xport->pin(out, n, &env.request.key);
  }

  // Perform the get operation synchronously.
  scheduler_suspend(_pwc_lco_get_continuation, &env, 0);

  // If we registered the output buffer dynamically, then we need to de-register
  // it now.
  if (!key) {
    pwc_network->xport->unpin(out, n);
  }
  return HPX_SUCCESS;
}
