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
#include <string.h>
#include <photon.h>

#include <libsync/locks.h>

#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/padding.h>
#include <libhpx/system.h>
#include "commands.h"
#include "registered.h"
#include "xport.h"

// check to make sure we can fit a photon key in the key size
_HPX_ASSERT(XPORT_KEY_SIZE == sizeof(struct photon_buffer_priv_t),
            incompatible_key_size);

typedef struct {
  pwc_xport_t  vtable;
  PAD_TO_CACHELINE(sizeof(pwc_xport_t));
} photon_pwc_xport_t;

static void
_init_photon_config(const config_t *cfg, boot_t *boot,
                    struct photon_config_t *pcfg) {
  pcfg->meta_exch               = PHOTON_EXCH_EXTERNAL;
  pcfg->nproc                   = boot_n_ranks(boot);
  pcfg->address                 = boot_rank(boot);
  pcfg->comm                    = NULL;
  pcfg->fi.provider             = cfg->photon_fiprov;
  pcfg->fi.eth_dev              = cfg->photon_fidev;
  pcfg->fi.node                 = NULL;
  pcfg->fi.service              = NULL;
  pcfg->fi.domain               = NULL;
  pcfg->ibv.use_cma             = cfg->photon_usecma;
  pcfg->ibv.eth_dev             = cfg->photon_ethdev;
  pcfg->ibv.ib_dev              = cfg->photon_ibdev;
  pcfg->ibv.num_srq             = cfg->photon_ibsrq;
  pcfg->ugni.eth_dev            = cfg->photon_ethdev;
  pcfg->ugni.bte_thresh         = cfg->photon_btethresh;
  pcfg->cap.eager_buf_size      = cfg->photon_eagerbufsize;
  pcfg->cap.small_pwc_size      = cfg->photon_smallpwcsize;
  pcfg->cap.ledger_entries      = cfg->photon_ledgersize;
  pcfg->cap.max_rd              = cfg->photon_maxrd;
  pcfg->cap.default_rd          = cfg->photon_defaultrd;
  pcfg->cap.num_cq              = cfg->photon_numcq;
  pcfg->cap.use_rcq             = cfg->photon_usercq;
  // static config not relevant for current HPX usage
  pcfg->forwarder.use_forwarder =  0;
  pcfg->cap.small_msg_size      = -1;  // default 4096 - not used for PWC
  pcfg->ibv.use_ud              =  0;  // don't enable this unless we're doing HW GAS
  pcfg->ibv.ud_gid_prefix       = "ff0e::ffff:0000:0000";
  pcfg->exch.allgather      = (__typeof__(pcfg->exch.allgather))boot->allgather;
  pcfg->exch.barrier        = (__typeof__(pcfg->exch.barrier))boot->barrier;
  pcfg->backend             = cfg->photon_backend;
}

static void
_init_photon(const config_t *cfg, boot_t *boot) {
  if (photon_initialized()) {
    return;
  }

  struct photon_config_t pcfg;
  _init_photon_config(cfg, boot, &pcfg);
  if (photon_init(&pcfg) != PHOTON_OK) {
    dbg_error("failed to initialize transport.\n");
  }
}

static void
_photon_key_clear(void *key) {
  memset(key, 0, sizeof(struct photon_buffer_priv_t));
}

static void
_photon_key_copy(void *restrict dest, const void *restrict src) {
  if (src) {
    dbg_assert(dest);
    memcpy(dest, src, sizeof(struct photon_buffer_priv_t));
  }
}

static const void *
_photon_key_find_ref(void *obj, const void *addr, size_t n) {
  const struct photon_buffer_priv_t *found = NULL;
  int e = photon_get_buffer_private((void*)addr, n, &found);
  if (PHOTON_OK != e) {
    dbg_assert(found == NULL);
    log_net("no rdma key for range (%p, %zu)\n", addr, n);
  }
  return found;
}

static void
_photon_key_find(void *obj, const void *addr, size_t n, void *key) {
  const void *found = _photon_key_find_ref(obj, addr, n);
  if (found)  {
    _photon_key_copy(key, found);
  }
  else {
    dbg_error("failed to find rdma key for (%p, %zu)\n", addr, n);
  }
}

static void
_photon_pin(const void *base, size_t n, void *key) {
  if (PHOTON_OK != photon_register_buffer((void*)base, n)) {
    dbg_error("failed to register segment with Photon\n");
  }
  else {
    log_net("registered segment (%p, %zu)\n", base, n);
  }

  if (key) {
    _photon_key_find(NULL, base, n, key);
  }
}

static void
_photon_unpin(const void *base, size_t n) {
  int e = photon_unregister_buffer((void*)base, n);
  if (PHOTON_OK != e) {
    dbg_error("unhandled error %d during release of segment (%p, %zu)\n", e,
              base, n);
  }
  log_net("released the segment (%p, %zu)\n", base, n);

}

// async entry point for unpin
static int
_photon_unpin_async(const void *base, size_t n, int src, uint64_t op) {
  _photon_unpin(base, n);
  command_run(src, (command_t){op});
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _unpin_async, _photon_unpin_async,
                     HPX_POINTER, HPX_SIZE_T, HPX_INT, HPX_UINT64);

/// Interpose a command to unpin a region before performing @p op.
///
/// We do not require that the local address for pwc/gwc be registered with
/// the network. When we get a region that is not yet registered we need to
/// dynamically register it, and then de-register it once the operation has
/// completed.
///
/// This operation attaches a parcel resume command to the local completion
/// event that will unpin the region, and then continue with the user-supplied
/// local command.
///
/// @param         addr The address we need to de-register.
/// @param            n The number of bytes to de-register.
/// @param           op The user's local completion operation.
///
/// @returns            The operation that should be used as the local
///                     completion event handler for the current operation
///                     (pwc/gwc instance).
static command_t _chain_unpin(const void *addr, size_t n, command_t op) {
  // we assume that this parcel doesn't need credit to run---technically it
  // not easy to account for this parcel because of the fact that pwc() can be
  // run as a scheduler_suspend() operation
  hpx_parcel_t *p = action_new_parcel(_unpin_async, // action
                                      HPX_HERE,     // target
                                      0,            // continuation target
                                      0,            // continuation action
                                      4,            // nargs
                                      &addr,        // buffer to unpin
                                      &n,           // length to unpin
                                      &here->rank,  // src for command
                                      &op.packed);  // command

  return (command_t){ .op = RESUME_PARCEL, .arg = (uintptr_t)p };
}

static int _photon_cmd(int rank, command_t lcmd, command_t rcmd) {
  int flags = ((lcmd.op) ? NOP : PHOTON_REQ_PWC_NO_LCE) |
              ((rcmd.op) ? NOP : PHOTON_REQ_PWC_NO_RCE);

  int e = photon_put_with_completion(rank, 0, NULL, NULL, lcmd.packed,
                                     rcmd.packed, flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  if (PHOTON_ERROR_RESOURCE == e) {
    log_error("could not initiate command due to resource constraint\n");
    return LIBHPX_RETRY;
  }

  dbg_error("could not initiate a put-with-completion\n");
}

static int _photon_pwc(xport_op_t *op) {
  int flags = ((op->lop.op) ? NOP : PHOTON_REQ_PWC_NO_LCE) |
              ((op->rop.op) ? NOP : PHOTON_REQ_PWC_NO_RCE);

  struct photon_buffer_t rbuf = {
    .addr = (uintptr_t)op->dest,
    .size = op->n
  };
  _photon_key_copy(&rbuf.priv, op->dest_key);

  struct photon_buffer_t lbuf = {
    .addr = (uintptr_t)op->src,
    .size = op->n
  };

  if (op->src_key) {
    _photon_key_copy(&lbuf.priv, op->src_key);
  }
  else {
    log_net("temporarily registering buffer (%p, %zu)\n", op->src, op->n);
    _photon_pin(op->src, op->n, &lbuf.priv);
    op->lop = _chain_unpin(op->src, op->n, op->lop);
  }

  int e = photon_put_with_completion(op->rank, op->n, &lbuf, &rbuf,
                                     op->lop.packed, op->rop.packed, flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  if (PHOTON_ERROR_RESOURCE == e) {
    log_error("could not initiate pwc due to resource constraint\n");
    return LIBHPX_RETRY;
  }

  dbg_error("could not initiate a put-with-completion\n");
}

static int
_photon_gwc(xport_op_t *op) {
  int flags = (op->rop.op) ? NOP : PHOTON_REQ_PWC_NO_RCE;

  struct photon_buffer_t lbuf = {
    .addr = (uintptr_t)op->dest,
    .size = op->n
  };

  if (op->dest_key) {
    _photon_key_copy(&lbuf.priv, op->dest_key);
  }
  else {
    log_net("temporarily registering buffer (%p, %zu)\n", op->dest, op->n);
    _photon_pin(op->dest, op->n, &lbuf.priv);
    op->lop = _chain_unpin(op->dest, op->n, op->lop);
  }

  struct photon_buffer_t rbuf = {
    .addr = (uintptr_t)op->src,
    .size = op->n
  };
  dbg_assert(op->src_key);
  _photon_key_copy(&rbuf.priv, op->src_key);

  int e = photon_get_with_completion(op->rank, op->n, &lbuf, &rbuf,
                                     op->lop.packed, op->rop.packed, flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  dbg_error("failed transport get operation\n");
}

static int
_poll(command_t *op, int *remaining, int rank, int *src, int type) {
  int flag = 0;
  int prank = (rank == XPORT_ANY_SOURCE) ? PHOTON_ANY_SOURCE : rank;
  int e = photon_probe_completion(prank, &flag, remaining, &op->packed, src, type);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static int
_photon_test(command_t *op, int *remaining, int id, int *src) {
  return _poll(op, remaining, id, src, PHOTON_PROBE_EVQ);
}

static int
_photon_probe(command_t *op, int *remaining, int rank, int *src) {
  return _poll(op, remaining, rank, src, PHOTON_PROBE_LEDGER);
}

static void
_photon_dealloc(void *photon) {
  free(photon);
}

pwc_xport_t *
pwc_xport_new_photon(const config_t *cfg, boot_t *boot, gas_t *gas) {
  photon_pwc_xport_t *photon = malloc(sizeof(*photon));
  dbg_assert(photon);
  _init_photon(cfg, boot);

  photon->vtable.type         = HPX_TRANSPORT_PHOTON;
  photon->vtable.dealloc      = _photon_dealloc;
  photon->vtable.key_find_ref = _photon_key_find_ref;
  photon->vtable.key_find     = _photon_key_find;
  photon->vtable.key_clear    = _photon_key_clear;
  photon->vtable.key_copy     = _photon_key_copy;
  photon->vtable.pin          = _photon_pin;
  photon->vtable.unpin        = _photon_unpin;
  photon->vtable.cmd          = _photon_cmd;
  photon->vtable.pwc          = _photon_pwc;
  photon->vtable.gwc          = _photon_gwc;
  photon->vtable.test         = _photon_test;
  photon->vtable.probe        = _photon_probe;

  // initialize the registered memory allocator
  registered_allocator_init(&photon->vtable);
  return &photon->vtable;
}
