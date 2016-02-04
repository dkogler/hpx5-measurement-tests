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

#ifndef HPX_CXX_LCO_H
#define HPX_CXX_LCO_H

#include <hpx/addr.h>
#include <hpx/lco.h>
#include <hpx/cxx/errors.h>
#include <hpx/cxx/global_ptr.h>

namespace hpx {
namespace lco {
/// This class is provides the lco interface common to all types of LCOs
/// extended by all LCO classes such as Future, Reduce, AndGate etc.
template <typename T>
class Base {
 protected:
  Base() {
  }

  virtual ~Base() {
  }
};

template <typename T, template <typename> class LCO>
void wait(const global_ptr<LCO<T>>& lco) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  if (int e = hpx_lco_wait(lco.get())) {
    throw Error(e);
  }
}

template <typename T, template <typename> class LCO>
void get(const global_ptr<LCO<T>>& lco, T& out) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  if (int e = hpx_lco_get(lco.get(), sizeof(out), &out)) {
    throw Error(e);
  }
}

template <template <typename> class LCO>
void get(const global_ptr<LCO<void>>& lco) {
  wait(lco);
}

template <typename T, template <typename> class LCO>
void set(const global_ptr<LCO<T>>& lco, const T& in) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  hpx_lco_set_rsync(lco.get(), sizeof(in), &in);
}

template <typename T, template <typename> class LCO>
void reset(const global_ptr<LCO<T>>& lco) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  hpx_lco_reset(lco.get());
}

template <typename T, template <typename> class LCO>
void dealloc(const global_ptr<LCO<T>>& lco) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  hpx_lco_delete_sync(lco.get());
}

template <typename T, template <typename> class LT,
          typename U, template <typename> class LU>
void dealloc(const global_ptr<LT<T>>& lco, const global_ptr<LU<U>>& sync) {
  static_assert(std::is_base_of<Base<T>, LT<T>>::value, "LCO type required");
  static_assert(std::is_base_of<Base<U>, LU<U>>::value, "LCO type required");
  hpx_lco_delete(lco.get(), sync.get());
}

template <typename T, template <typename> class LCO>
void dealloc(const global_ptr<LCO<T>>& lco, std::nullptr_t) {
  static_assert(std::is_base_of<Base<T>, LCO<T>>::value, "LCO type required");
  hpx_lco_delete_sync(lco.get(), HPX_NULL);
}

template <typename T>
class Future : public Base<T> {
 public:
  static global_ptr<Future<T>> Alloc() {
    return global_ptr<Future<T>>(hpx_lco_future_new(sizeof(T)));
  }

 private:
  Future() {
  }
};

/// Future to void is a 0-sized future that only contains control information.
template <>
global_ptr<Future<void>> Future<void>::Alloc() {
    return global_ptr<Future<void>>(hpx_lco_future_new(0));
}

template <typename T>
class And : public Base<T> {
  static global_ptr<And<T>> Alloc(size_t n) {
    return global_ptr<And<T>>(hpx_lco_and_new(n));
  }

 private:
  And() {
  }
};

} // namespace lco
} // namespace hpx

#endif // HPX_CXX_LCO_H
