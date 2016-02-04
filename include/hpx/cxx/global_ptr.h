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

#ifndef HPX_CXX_GLOBAL_PTR_H
#define HPX_CXX_GLOBAL_PTR_H

#include <cstddef>
#include <memory>
#include <exception>
#include <hpx/addr.h>
#include <hpx/gas.h>

namespace hpx {

/// We throw this exception if a pin operation fails.
struct NotLocal : public std::exception {
  virtual const char* what() const throw() {
    return "Pin operation failed\n";
  }
};

template <typename T>
class global_ptr;

/// Special case global pointers to void with a template specialization.
///
/// These serve the same roll as void* pointers do. Other pointers can be cast
/// to global_ptr<void>, however we can't pin, unpin, or perform address
/// arithmetic on them, and we don't know their block size.
///
/// This specialization is defined first because we use it in the implementation
/// of the generic global_ptr template below (as part of an explicit cast
/// operation).
template <>
class global_ptr<void> {
 public:
  /// Default constructor.
  ///
  /// This constructs a global_ptr<void> to NULL.
  global_ptr<void>() : _impl(HPX_NULL) {
  }

  global_ptr<void>(std::nullptr_t) : _impl(HPX_NULL) {
  }

  /// Construct a generic global pointer from a generic HPX address.
  global_ptr<void>(hpx_addr_t addr) : _impl(addr) {
  }

  /// Construct a generic global pointer from a pointer to any other type---this
  /// serves to handle pointer casts as well.
  template <typename U>
  global_ptr<void>(const global_ptr<U>& ptr) : _impl(ptr.get()) {
  }

  /// Allow smart pointers to be used in (!ptr) style tests.
  operator bool() const {
    return (_impl != HPX_NULL);
  }

  /// Support any user that wants to get the underlying HPX address.
  hpx_addr_t get() const {
    return _impl;
  }

 private:
  hpx_addr_t _impl;
};


/// An HPX global address ptr.
///
/// Basically wraps hpx_addr_t and provides address arithmetic and pin/unpin
/// methods.
template <typename T>
class global_ptr {
  /// This helper class will allow users to use &ptr[x] to perform address
  /// arithmetic, without exposing the underlying T to manipulation.
  class reference {
   private:
    reference() = delete;

   public:
    /// The only thing we can do with a reference is to get its global address.
    const global_ptr<T>& operator&() const {
      return _gp;
    }

    ~reference() {
    }

   private:
    /// Global pointers create references in their array index operator.
    friend class global_ptr<T>;
    reference(const global_ptr<T>& gp) : _gp(gp) {
    }

    const global_ptr<T>& _gp;
  };

 public:
  typedef T value_type;

  /// The default constructor for a global_ptr initializes the pointer to null.
  global_ptr() : _gbl_ptr(HPX_NULL), _elems_per_blk(0) {
  }

  /// The nullptr can be implicitly converted to a global_ptr.
  global_ptr(std::nullptr_t) : _gbl_ptr(HPX_NULL), _elems_per_blk(0) {
  }

  /// A global pointer can be explicitly constructed from an hpx_addr_t.
  ///
  /// This constructor assumes that the block size for the allocation is simply
  /// a single element. Because this assumption is fundamentally unsafe we make
  /// this an explicit constructor so that we don't accidently get
  /// difficult-to-debug errors.
  ///
  /// @param       addr The hpx_addr_t.
  explicit global_ptr(hpx_addr_t addr) : _gbl_ptr(addr), _elems_per_blk(1) {
  }

  /// A global pointer can be constructed from an hpx_addr_t, block size pair.
  ///
  /// This constructs a global_ptr from an underlying hpx_addr_t allocation. Of
  /// course, to be valid @p n must match the block size that @p addr was
  /// allocated with. Note that @p n is in terms of sizeof(T), while the bsize
  /// for HPX allocation is in terms of bytes.
  ///
  /// @param       addr The hpx_addr_t.
  /// @param          n The number of elements per block.
  global_ptr(hpx_addr_t addr, uint32_t n) : _gbl_ptr(addr), _elems_per_blk(n) {
  }

  /// A typed global pointer can be constructed from a global_ptr<void>, as long
  /// as the user provides a block size.
  explicit global_ptr(const global_ptr<void>& gva, uint32_t n)
    : _gbl_ptr(gva.get()), _elems_per_blk(n) {
  }

  /// Allow smart pointers to be used in (!ptr) style tests.
  operator bool() const {
    return (_gbl_ptr != HPX_NULL);
  }

  /// Returns the raw hpx_addr_t that this smart pointer encapsulates.
  hpx_addr_t get() const {
    return _gbl_ptr;
  }

  /// Return the block size that this smart pointer encapsulates.
  uint32_t get_block_size() const {
    return _elems_per_blk;
  }

  /// The array-subscript operator is an alternative to explicit pointer
  /// arithmetic.
  reference operator[](size_t index) const {
    return reference(*this + index);
  }

  /// Standard pointer arithmetic returns another global pointer.
  template <typename U>
  global_ptr<T> operator+(U n) const {
    static_assert(std::is_integral<U>::value, "integer type required");
    ptrdiff_t bytes = n * sizeof(T);
    hpx_addr_t addr = hpx_addr_add(_gbl_ptr, bytes, bsize());
    return global_ptr<T>(addr, _elems_per_blk);
  }

  /// Standard self-update pointer arithmetic.
  template <typename U>
  global_ptr<T>& operator+=(U n) const {
    _gbl_ptr = (*this + n).get();
    return *this;
  }

  /// Standard pointer difference operation.
  ///
  /// Pointers can only be compared between the same allocation, which implies
  /// that they will have the same block size. Without more information we can't
  /// check this constraint more carefully.
  ptrdiff_t operator-(const global_ptr<T>& rhs) const {
    assert(_elems_per_blk == rhs.get_block_size());
    int64_t bytes = hpx_addr_sub(_gbl_ptr, rhs.get(), bsize());
    assert(bytes % sizeof(T) == 0);
    return bytes / sizeof(T);
  }

  /// Pin the global pointer.
  ///
  /// Pinning a global pointer allows the programmer to interact with its
  /// underlying block using its native type. This operation will attempt to pin
  /// the entire block pointed to by the global_ptr<T>. If it succeeds then the
  /// block will remain pinned until unpin() is called on the global_ptr<T>.
  ///
  /// HPX++ provides the pin_guard class to provide automatic lexically-scoped
  /// unpinning.
  ///
  /// @return           The local pointer associated with the global pointer.
  /// @throw   NotLocal If the pointer is not local.
  T* pin() const {
    T* lva = nullptr;
    if (!hpx_gas_try_pin(_gbl_ptr, reinterpret_cast<void**>(&lva))) {
      throw NotLocal();
    }
    return lva;
  }

  /// @overload pin()
  ///
  /// @param[out] local True if the pin succeeds, false otherwise.
  /// @return           The local pointer associated with the
  T* pin(bool &local) const {
    T *lva = nullptr;
    local = hpx_gas_try_pin(_gbl_ptr, reinterpret_cast<void**>(&lva));
    return lva;
  }

  /// Unpin a global pointer.
  ///
  /// The unpin() operation must match a pin() operation, but this requirement
  /// is not enforced by the runtime. A pin_guard object can help ensure that
  /// lexically-scoped use can be properly pinned and unpinned.
  void unpin() const {
    hpx_gas_unpin(_gbl_ptr);
  }

  /// Compute the HPX block size for the pointer. The HPX block size differs
  /// from the HPX++ block size because the HPX++ block size is in terms of T,
  /// while the HPX block size is in terms of bytes.
  size_t bsize() const {
    return _elems_per_blk * sizeof(T);
  }

 private:
  hpx_addr_t _gbl_ptr;
  size_t _elems_per_blk;
}; // template class global_ptr

/// The standard global pointer operators.
///
/// These are defined in their non-member forms so that we can use them for the
/// global_ptr<void> specialization.
///
/// @{
template <typename T, typename U>
bool operator==(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  return ((lhs - rhs) == 0);
}

template <typename T, typename U>
bool operator!=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  return !(lhs == rhs);
}

template <typename T, typename U>
bool operator<(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) < 0);
}

template <typename T, typename U>
bool operator>(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) > 0);
}

template <typename T, typename U>
bool operator<=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) <= 0);
}

template <typename T, typename U>
bool operator>=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) >= 0);
}

template <typename T>
bool operator==(const global_ptr<T>& lhs, std::nullptr_t) {
  return (lhs.get() == HPX_NULL);
}

template <typename T>
bool operator==(std::nullptr_t, const global_ptr<T>& rhs) {
  return (HPX_NULL == rhs.get());
}

template <typename T>
bool operator!=(const global_ptr<T>& lhs, std::nullptr_t) {
  return (lhs.get() != HPX_NULL);
}

template <typename T>
bool operator!=(std::nullptr_t, const global_ptr<T>& rhs) {
  return (rhs.get() != HPX_NULL);
}

template <typename T>
bool operator<(const global_ptr<T>& lhs, std::nullptr_t) {
  uintptr_t ptr = static_cast<uintptr_t>(lhs.get());
  T* p = reinterpret_cast<T*>(ptr);
  return std::less<T*>(p, nullptr);
}

template <typename T>
bool operator<(std::nullptr_t, const global_ptr<T>& rhs) {
  uintptr_t ptr = static_cast<uintptr_t>(rhs.get());
  return std::less<T*>(nullptr, reinterpret_cast<T*>(ptr));
}

template <typename T>
bool operator>(const global_ptr<T>& lhs, std::nullptr_t) {
  return nullptr < lhs;
}

template <typename T>
bool operator>(std::nullptr_t, const global_ptr<T>& rhs) {
  return rhs < nullptr;
}

template <typename T>
bool operator<=(const global_ptr<T>& lhs, std::nullptr_t) {
  return !(nullptr < lhs);
}

template <typename T>
bool operator<=(std::nullptr_t, const global_ptr<T>& rhs) {
  return !(rhs < nullptr);
}

template <typename T>
bool operator>=(const global_ptr<T>& lhs, std::nullptr_t) {
  return !(lhs < nullptr);
}

template <typename T>
bool operator>=(std::nullptr_t, const global_ptr<T>& rhs) {
  return !(nullptr < rhs);
}

/// @}

/// The pin_guard class.
///
/// A pin_guard will automatically unpin the address when it is destroyed. This
/// allows safe lexically scoped use of a pinned global pointer in circumstances
/// where that makes sense. It is unsafe to continue to access the provided
/// local pointer after the pin_guard has been destroyed.
///
/// The pin_guard is non-copyable.
template <typename T>
class pin_guard {
 private:
  pin_guard() = delete;
  pin_guard(const pin_guard<T>&) = delete;
  pin_guard<T>& operator=(const pin_guard<T>&) = delete;

 public:
  /// Construct a pin_guard from a global address.
  ///
  /// @param        gva The address we're trying to pin.
  /// @throw   NotLocal If the @p gva is not local.
  explicit pin_guard(const global_ptr<T>& gva)
    : _gva(gva), _local(true), _lva(gva.pin()) {
  }

  /// This version of the pin_guard constructor will return the success or
  /// failure of the pin operation through the @p local parameter, rather than
  /// throwing an exception.
  ///
  /// @param        gva The address we're trying to pin.
  /// @param[out] local A flag indication if the @p gva was local.
  pin_guard(const global_ptr<T>& gva, bool &local)
    : _gva(gva), _local(true), _lva(gva.pin(_local)) {
    local = _local;
  }

  /// The pin_guard destructor.
  ///
  /// The pin_guard will unpin the underlying gva if it was successfully pinned
  /// during construction.
  ~pin_guard() {
    if (_local) {
      _gva.unpin();
    }
  }

  /// Access the underlying local memory.
  T* get() const {
    assert(_local);
    return _lva;
  }

  /// Access the underlying local memory.
  operator T*() const {
    return get();
  }

 private:
  const global_ptr<T>& _gva;
  T*                   _lva;
  bool               _local;
}; // template pin_guard

} // namespace hpx

#endif // HPX_CXX_GLOBAL_PTR_H
