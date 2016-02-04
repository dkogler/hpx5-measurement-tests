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

#ifndef LIBHPX_NETWORK_PWC_XPORT_H
#define LIBHPX_NETWORK_PWC_XPORT_H

#include <libhpx/config.h>
#include <libhpx/memory.h>
#include "commands.h"

#ifdef __cplusplus
extern "C" {
#define restrict
#endif

struct boot;
struct gas;

#define XPORT_ANY_SOURCE -1
#define XPORT_KEY_SIZE   16

typedef char xport_key_t[XPORT_KEY_SIZE];

typedef struct xport_op {
  int             rank;
  int   UNUSED_PADDING;
  size_t             n;
  void           *dest;
  const void *dest_key;
  const void      *src;
  const void  *src_key;
  command_t        lop;
  command_t        rop;
} xport_op_t HPX_ALIGNED(HPX_CACHELINE_SIZE);

typedef struct pwc_xport {
  libhpx_transport_t type;

  void (*dealloc)(void *xport);
  const void *(*key_find_ref)(void *xport, const void *addr, size_t n);
  void (*key_find)(void *xport, const void *addr, size_t n, void *key);
  void (*key_copy)(void *restrict dest, const void *restrict src);
  void (*key_clear)(void *key);
  int (*cmd)(int rank, command_t lcmd, command_t rcmd);
  int (*pwc)(xport_op_t *op);
  int (*gwc)(xport_op_t *op);
  int (*test)(command_t *op, int *remaining, int id, int *src);
  int (*probe)(command_t *op, int *remaining, int rank, int *src);
  void (*pin)(const void *base, size_t bytes, void *key);
  void (*unpin)(const void *base, size_t bytes);
} pwc_xport_t;

pwc_xport_t *pwc_xport_new_photon(const config_t *config, struct boot *boot,
                                  struct gas *gas);

pwc_xport_t *pwc_xport_new(const config_t *config, struct boot *boot,
                           struct gas *gas);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_PWC_XPORT_H
