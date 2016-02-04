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

#ifndef LIBHPX_GAS_H
#define LIBHPX_GAS_H

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/string.h>
#include <libhpx/system.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Forward declarations.
/// @{
struct boot;
/// @}

/// Generic object oriented interface to the global address space.
typedef struct gas {
  libhpx_gas_t type;

  class_string_t string;

  void (*dealloc)(void *gas);
  size_t (*local_size)(void *gas);
  void *(*local_base)(void *gas);

  int64_t (*sub)(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs,
                 uint32_t bsize);
  hpx_addr_t (*add)(const void *gas, hpx_addr_t gva, int64_t bytes,
                    uint32_t bsize);

  hpx_addr_t (*there)(void *gas, uint32_t i);
  uint32_t (*owner_of)(const void *gas, hpx_addr_t gpa);
  bool (*try_pin)(void *gas, hpx_addr_t addr, void **local);
  void (*unpin)(void *gas, hpx_addr_t addr);
  void (*free)(void *gas, hpx_addr_t addr, hpx_addr_t rsync);

  void (*move)(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);

  // implement hpx/gas.h
  __typeof(hpx_gas_alloc_local_attr) *alloc_local;
  __typeof(hpx_gas_calloc_local_attr) *calloc_local;
  __typeof(hpx_gas_alloc_cyclic_attr) *alloc_cyclic;
  __typeof(hpx_gas_calloc_cyclic_attr) *calloc_cyclic;
  __typeof(hpx_gas_alloc_blocked_attr) *alloc_blocked;
  __typeof(hpx_gas_calloc_blocked_attr) *calloc_blocked;
} gas_t;

gas_t *gas_new(config_t *cfg, struct boot *boot)
  HPX_MALLOC HPX_NON_NULL(1,2);

inline static void gas_dealloc(gas_t *gas) {
  assert(gas && gas->dealloc);
  gas->dealloc(gas);
}

inline static uint32_t gas_owner_of(gas_t *gas, hpx_addr_t addr) {
  assert(gas && gas->owner_of);
  return gas->owner_of(gas, addr);
}

static inline size_t gas_local_size(gas_t *gas) {
  assert(gas && gas->local_size);
  return gas->local_size(gas);
}

inline static void *gas_local_base(gas_t *gas) {
  assert(gas && gas->local_base);
  return gas->local_base(gas);
}

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_H
