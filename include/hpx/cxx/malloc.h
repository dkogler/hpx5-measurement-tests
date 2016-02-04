// ================================================================= -*- C++ -*-
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

#ifndef HPX_CXX_MALLOC_H
#define HPX_CXX_MALLOC_H

#include <hpx/cxx/global_ptr.h>
#include <hpx/cxx/lco.h>

namespace hpx {

void free(const global_ptr<void>& gva) {
  hpx_gas_free_sync(gva.get());
}

template <template <typename> class LCO>
void free(const global_ptr<void>& gva, const global_ptr<LCO<void>>& rsync) {
  static_assert(std::is_base_of<lco::Base<void>, LCO<void>>::value,
                "rsync must be a control-only LCO");
  hpx_gas_free(gva.get(), rsync.get());
}

void free(const global_ptr<void>& gva, std::nullptr_t) {
  hpx_gas_free(gva.get(), HPX_NULL);
}

/// The generic gas allocation routine.
template <typename T>
global_ptr<T> malloc(size_t n, unsigned block, unsigned boundary,
                     hpx_gas_dist_t dist, unsigned attr) {
  hpx_addr_t gva = hpx_gas_alloc(n, block * sizeof(T), boundary, dist, attr);
  return global_ptr<T>(gva, block);
}

/// The generic gas allocation routine.
template <typename T>
global_ptr<T> calloc(size_t n, unsigned block, unsigned boundary,
                     hpx_gas_dist_t dist, unsigned attr) {
  hpx_addr_t gva = hpx_gas_calloc(n, block * sizeof(T), boundary, dist, attr);
  return global_ptr<T>(gva, block);
}

/// Allocate a local array of elements.
template <typename T, typename V>
global_ptr<T> alloc_local(size_t n, unsigned boundary) {
  return malloc<T>(n, 1, boundary, HPX_GAS_DIST_LOCAL, HPX_GAS_ATTR_NONE);
}

template <typename T>
global_ptr<T> alloc_local(size_t n) {
  return malloc<T>(n, 1, 0, HPX_GAS_DIST_LOCAL, HPX_GAS_ATTR_NONE);
}

template <typename T>
global_ptr<T> alloc_cyclic(size_t n, unsigned block, unsigned boundary) {
  return malloc<T>(n, block, boundary, HPX_GAS_DIST_CYCLIC, HPX_GAS_ATTR_NONE);
}

template <typename T>
global_ptr<T> alloc_cyclic(size_t n, unsigned block) {
  return malloc<T>(n, block, 0, HPX_GAS_DIST_CYCLIC, HPX_GAS_ATTR_NONE);
}

template <typename T>
T* malloc(size_t bytes) {
  return static_cast<T*>(hpx_malloc_registered(bytes));
}

void free(void* registered) {
  hpx_free_registered(registered);
}

} // namespace hpx

#endif // HPX_CXX_MALLOC_H
