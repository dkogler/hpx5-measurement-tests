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

#ifndef HPX_CXX_GAS_H
#define HPX_CXX_GAS_H

#include <hpx/addr.h>
#include <hpx/cxx/errors.h>
#include <hpx/cxx/global_ptr.h>
#include <hpx/cxx/lco.h>

namespace hpx {
namespace gas {
template <typename T, typename A, typename... Params>
void broadcast(A act, const global_ptr<T>& base, int blocks, size_t offset,
               Params... ps) {
  size_t n = sizeof...(Params);
  hpx_addr_t gva = base.get();
  size_t bsize = base.bsize();
  if (int e = _hpx_gas_bcast_sync(act, gva, blocks, offset, bsize, n, ps...)) {
    throw Error(e);
  }
} // template broadcast
} // namespace gas
} // namespace hpx

#endif // HPX_CXX_GAS_H
