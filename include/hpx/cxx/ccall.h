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

#ifndef HPX_CXX_PROCESS_H
#define HPX_CXX_PROCESS_H

#include <hpx/rpc.h>
#include <hpx/cxx/errors.h>
#include <hpx/cxx/lco.h>

namespace hpx {
namespace c {
template <typename T, typename U, typename... Params>
void call(T&& target, hpx_action_t action, global_ptr<U>& sync,
          Params... params) {
  static_assert(lco::is_lco<U>::value, "lsync must be an LCO");
  const auto n = sizeof...(params);
  if (int e = _hpx_call(target.get(), action, sync.get(), n, &params...)) {
    throw Error(e);
  }
}

template <typename T, typename... Params>
void call(T&& target, hpx_action_t action, std::nullptr_t,
          Params... params) {
  const auto n = sizeof...(params);
  if (int e = _hpx_call(target.get(), action, HPX_NULL, n, &params...)) {
    throw Error(e);
  }
}

template <typename T, typename... Params>
void call(T&& target, hpx_action_t action, Params... params) {
  const auto n = sizeof...(params);
  if (int e = _hpx_call_sync(target.get(), action, NULL, 0, n, &params...)) {
    throw Error(e);
  }
}

template <typename U, typename... Params>
void call(hpx_addr_t target, hpx_action_t action, global_ptr<U>& sync,
          Params... params) {
  static_assert(lco::is_lco<U>::value, "lsync must be an LCO");
  const auto n = sizeof...(params);
  if (int e = _hpx_call(target, action, sync.get(), n, &params...)) {
    throw Error(e);
  }
}

template <typename... Params>
void call(hpx_addr_t target, hpx_action_t action, std::nullptr_t,
          Params... params) {
  const auto n = sizeof...(params);
  if (int e = _hpx_call(target, action, HPX_NULL, n, &params...)) {
    throw Error(e);
  }
}

template <typename... Params>
void call(hpx_addr_t target, hpx_action_t action, Params... params) {
  const auto n = sizeof...(params);
  if (int e = _hpx_call_sync(target, action, NULL, 0, n, &params...)) {
    throw Error(e);
  }
}
} // namespace c
} // namespace hpx

#endif // HPX_CXX_PROCESS_H
