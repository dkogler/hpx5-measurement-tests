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

#ifndef HPX_CXX_ACTION_H
#define HPX_CXX_ACTION_H

#include <type_traits>

#include <hpx/addr.h>
#include <hpx/types.h>
#include <hpx/action.h>
#include <hpx/rpc.h>
#include <hpx/par.h>
#include <hpx/cxx/lco.h>
#include <hpx/cxx/runtime.h>

namespace hpx {

namespace detail {

// used to check sameness of type lists
template <typename... Ts>
struct tlist {};

// reference: https://functionalcpp.wordpress.com/2013/08/05/function-traits/
template<class F>
struct function_traits;

// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

template<class R, class... Args>
struct function_traits<R(&)(Args...)> : public function_traits<R(Args...)>
{};

template<class R, class... Args>
struct function_traits<R(Args...)> {
  using return_type = R;
  static constexpr std::size_t arity = sizeof...(Args);
  using arg_types = tlist<Args...>;
}; // template struct function_traits

/// HPX basic datatypes
/// HPX_CHAR, HPX_UCHAR, HPX_SCHAR, HPX_SHORT, HPX_USHORT, HPX_SSHORT, HPX_INT,
/// HPX_UINT, HPX_SINT, HPX_LONG, HPX_ULONG, HPX_SLONG, HPX_VOID, HPX_UINT8,
/// HPX_SINT8, HPX_UINT16, HPX_SINT16, HPX_UINT32, HPX_SINT32, HPX_UINT64,
/// HPX_SINT64, HPX_FLOAT, HPX_DOUBLE, HPX_POINTER, HPX_LONGDOUBLE,
/// HPX_COMPLEX_FLOAT, HPX_COMPLEX_DOUBLE, HPX_COMPLEX_LONGDOUBLE

template <typename T>
struct _convert_arg_type;

#define DEF_CONVERT_TYPE(cpptype, hpxtype)          \
template<> struct _convert_arg_type<cpptype> {          \
  constexpr static auto type = hpxtype;             \
};                              \
constexpr decltype(hpxtype) _convert_arg_type<cpptype>::type;

DEF_CONVERT_TYPE(char, HPX_CHAR)
DEF_CONVERT_TYPE(short, HPX_SHORT)
DEF_CONVERT_TYPE(int, HPX_INT)
DEF_CONVERT_TYPE(float, HPX_FLOAT)
DEF_CONVERT_TYPE(double, HPX_DOUBLE)
DEF_CONVERT_TYPE(std::size_t, HPX_SIZE_T)

// global ptr conversion
template <typename T>
struct _convert_arg_type< hpx::global_ptr<T> > {
  constexpr static auto type = HPX_ADDR;
};
template <typename T>
constexpr decltype(HPX_ADDR) _convert_arg_type< hpx::global_ptr<T> >::type;

// pointer convesion
template <typename T>
struct _convert_arg_type<T*> {
  constexpr static auto type = HPX_POINTER;
};
template <typename T>
constexpr decltype(HPX_POINTER) _convert_arg_type<T*>::type;

} // namespace detail
} // namspace hpx

namespace hpx {

// The action type
template <hpx_action_type_t TYPE, uint32_t ATTR, typename F, typename... ContTs>
class Action;

template <typename A>
struct Action_traits;

template <typename A>
struct Action_traits<A&> : public Action_traits<A>
{};

template <hpx_action_type_t TYPE, uint32_t ATTR, typename F, typename... ContTs>
struct Action_traits< Action<TYPE, ATTR, F, ContTs...> > {
  using ftype = F;
  using ctypes = hpx::detail::tlist<ContTs...>;
  static constexpr std::size_t arity = sizeof...(ContTs);
};

// The action type
template <hpx_action_type_t TYPE, uint32_t ATTR, typename F, typename... ContTs>
class Action {
private:
  hpx_action_t _id;
  bool _is_registerd;

  template <typename R, typename... Args>
  int _register_helper(R(&f)(Args...)) {
    return hpx_register_action(HPX_DEFAULT, HPX_ATTR_NONE, __FILE__ ":" _HPX_XSTR(_id),
                               &(_id), sizeof...(Args) + 1 , f,
                               hpx::detail::_convert_arg_type<Args>::type...);
  }

public:

  Action() : _is_registerd(false) {}

  Action(F& f) { // Should the parameter be always a reference?
    static_assert(std::is_function<F>::value, "Action constructor argument is not a function.");
    _register_helper(f); // What to do with return value? Maybe throw an exception when the registration is not successful?
    _is_registerd = true;
  }

  ~Action() {}

  hpx_action_t get_id() const {
    return _id;
  }

/// Fully synchronous call interface.
///
/// Performs action on @p args at @p addr, and sets @p out with the
/// resulting value. The output value @p out can be NULL, in which case no return
/// value is generated. The type of @p out must be the same as output type of the calling action.
///
/// @param         addr The address that defines where the action is executed.
/// @param       action The action to perform.
/// @param          out Reference to the output variable.
/// @param            n The number of arguments for @p action.
///
/// @returns            HPX_SUCCESS, or an error code if the action generated an
///                     error that could not be handled remotely.
  template <typename R, typename... Args>
  int call_sync(hpx_addr_t& addr, R& out, Args&... args) {
    // TODO check the continuation type with R
    using traits = hpx::detail::function_traits<F>;
    static_assert(::std::is_same< typename traits::arg_types, hpx::detail::tlist<Args...> >::value,
                  "action and argument types do not match");
    return _hpx_call_sync(addr, _id, &out, sizeof(R), sizeof...(Args), &args...);
  }

/// Locally synchronous call interface.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface. If @p result is not HPX_NULL,
/// hpx_call puts the the resulting value in @p result at some point
/// in the future.
///
/// @param         addr The address that defines where the action is executed.
/// @param       result An address of an LCO to trigger with the result.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename... Args>
  int call(hpx_addr_t addr, hpx_addr_t result, Args&... args) {
    return _hpx_call(addr, _id, result, sizeof...(Args), &args...);
  }
  template <typename T1, typename LCO, typename... Args>
  int call(const ::hpx::global_ptr<T1>& addr, const ::hpx::global_ptr<LCO>& result, Args&... args) {
    return _hpx_call(addr.get(), _id, result.get(), sizeof...(Args), &args...);
  }


/// Locally synchronous call interface when LCO is set.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface which implements the hpx_parcel_send_
/// through() function. The gate must be non-HPX_NULL.
///
/// @param         gate The LCO that will serve as the gate (not HPX_NULL).
/// @param         addr The address that defines where the action is executed.
/// @param       result An address of an LCO to trigger with the result.
/// @param            n The number of arguments for @p action.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename... Args>
  int call_when(hpx_addr_t gate, hpx_addr_t addr, hpx_addr_t result, Args&... args) {
    return _hpx_call_when(gate, addr, _id, result, sizeof...(Args), &args...);
  }
  template <typename LCO, typename T1, typename T2, typename... Args>
  int call_when(const ::hpx::global_ptr<LCO>& gate,
        const ::hpx::global_ptr<T1>& addr,
        const ::hpx::global_ptr<T1>& result, Args&... args) {
    return _hpx_call_when(gate.get(), addr.get(), _id, result.get(), sizeof...(Args), &args...);
  }

/// Fully synchronous call interface which implements hpx_parcel_send_through()
/// when an LCO is set.
///
/// Performs @p action on @p args at @p addr, and sets @p out with the resulting
/// value. The output value @p out can be NULL (or the corresponding @p olen
/// could be zero), in which case no return value is generated.
///
/// The gate must be non-HPX_NULL.
///
/// @param         gate The LCO that will serve as the gate (non HPX_NULL).
/// @param         addr The address that defines where the action is executed.
/// @param       action The action to perform.
/// @param          out Address of the output buffer.
/// @param         olen The length of the @p output buffer.
/// @param            n The number of arguments for @p action.
///
/// @returns            HPX_SUCCESS, or an error code if the action generated an
///                     error that could not be handled remotely.
  template <typename R, typename... Args>
  int call_when_sync(hpx_addr_t gate, hpx_addr_t addr, R& out, Args&... args) {
    return _hpx_call_when_sync(gate, addr, _id, &out, sizeof(R), sizeof...(Args), &args...);
  }
  template <typename LCO, typename T, typename R, typename... Args>
  int call_when_sync(const ::hpx::global_ptr<LCO>& gate,
                const ::hpx::global_ptr<T>& addr, R& out, Args&... args) {
    return _hpx_call_when_sync(gate.get(), addr.get(), _id, &out, sizeof(R), sizeof...(Args), &args...);
  }

/// Locally synchronous call with continuation interface.
///
/// This is similar to hpx_call with additional parameters to specify the
/// continuation action @p c_action to be executed at a continuation address @p
/// c_target.
///
/// @param         addr The address that defines where the action is executed.
/// @param     c_target The address where the continuation action is executed.
/// @param     c_action The continuation action to perform.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename Cont, typename... Args>
  int call_with_continuation(hpx_addr_t addr, hpx_addr_t c_target, Cont&& c_action, Args&... args) {
    // check if the continued types of current action (caller) match input arguments of c_action (callee)
    static_assert(std::is_same<hpx::detail::tlist<ContTs...>, hpx::detail::tlist<Args...>>::value,
          "continued types and continuation argument types must match...");
    return _hpx_call_with_continuation(addr, _id, c_target, c_action.get_id(), sizeof...(Args), &args...);
  }
  template <typename T1, typename T2, typename Cont, typename... Args>
  int call_with_continuation(const ::hpx::global_ptr<T1>& addr, const ::hpx::global_ptr<T2>& c_target,
                 Cont&& c_action, Args&... args) {
    static_assert(std::is_same<hpx::detail::tlist<ContTs...>, hpx::detail::tlist<Args...>>::value,
          "continued types and continuation argument types must match...");
    return _hpx_call_with_continuation(addr.get(), _id, c_target.get(), c_action.get_id(), sizeof...(Args), &args...);
  }

/// Fully asynchronous call interface.
///
/// This is a completely asynchronous variant of the remote-procedure
/// call interface. If @p result is not HPX_NULL, hpx_call puts the
/// the resulting value in @p result at some point in the future. This
/// function returns even before the argument buffer has been copied
/// and is free to reuse. If @p lsync is not HPX_NULL, it is set
/// when @p args is safe to be reused or freed.
///
/// @param         addr The address that defines where the action is executed.
/// @param        lsync The global address of an LCO to signal local completion
///                     (i.e., R/W access to, or free of @p args is safe),
///                     HPX_NULL if we don't care.
/// @param       result The global address of an LCO to signal with the result.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call_async invocation.
  template <typename... Args>
  int call_async(hpx_addr_t addr, hpx_addr_t lsync, hpx_addr_t result, Args&... args) {
    return _hpx_call_async(addr, _id, lsync, result, sizeof...(Args), &args...);
  }
  template <typename T, typename LSYNC, typename RES, typename... Args>
  int call_async(const ::hpx::global_ptr<T>& addr, const ::hpx::global_ptr<LSYNC>& lsync,
            const ::hpx::global_ptr<RES>& result, Args&... args) {
    return _hpx_call_async(addr, _id, lsync, result, sizeof...(Args), &args...);
  }

/// Locally synchronous call_when with continuation interface.
///
/// The gate must be non-HPX_NULL.
///
/// @param         gate The LCO that will serve as the gate (not HPX_NULL).
/// @param         addr The address that defines where the action is executed.
/// @param     c_target The address where the continuation action is executed.
/// @param         Cont The continuation action to perform.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename Cont, typename...Args>
  int call_when_with_continuation(hpx_addr_t gate, hpx_addr_t addr, hpx_addr_t c_target, Args&... args) {

    return _hpx_call_when_with_continuation(gate, addr, _id, c_target, Cont::id, sizeof...(Args), &args...);
  }
  template <typename LCO, typename T1, typename T2, typename Cont, typename...Args>
  int call_when_with_continuation(const ::hpx::global_ptr<LCO>& gate, const ::hpx::global_ptr<T1>& addr,
                     const ::hpx::global_ptr<T2>& c_target, Cont&& c_action, Args&... args) {
    return _hpx_call_when_with_continuation(gate.get(), addr.get(), _id, c_target.get(), c_action.get_id(), sizeof...(Args), &args...);
  }

/// Call with current continuation.
///
/// This calls an action passing the currrent thread's continuation as the
/// continuation for the called action.
///
/// The gate must be non-HPX_NULL.
///
/// @param         gate An LCO for a dependent call (must be non HPX_NULL).
/// @param         addr The address where the action is executed.
/// @param          ... The arguments for the call.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     during the hpx_call_cc invocation.
  template <typename...Args>
  int call_when_cc(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action, Args&... args) {
    return _hpx_call_when_cc(gate, addr, _id, sizeof...(Args), &args...);
  }
  template <typename LCO, typename T, typename...Args>
  int call_when_cc(const ::hpx::global_ptr<LCO>& gate, const ::hpx::global_ptr<T>& addr, Args&... args) {
    return _hpx_call_when_cc(gate.get(), addr.get(), _id, sizeof...(Args), &args...);
  }

/// Call with current continuation.
///
/// This calls an action passing the currrent thread's continuation as the
/// continuation for the called action.
///
/// @param         addr The address where the action is executed.
/// @param          ... The arguments for the call.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     during the hpx_call_cc invocation.
  template <typename... Args>
  int _hpx_call_cc(hpx_addr_t addr, Args&... args) {
    return _hpx_call_cc(addr, _id, sizeof...(Args), &args...);
  }
  template <typename T, typename... Args>
  int _hpx_call_cc(const ::hpx::global_ptr<T>& addr, Args&... args) {
    return _hpx_call_cc(addr.get(), _id, sizeof...(Args), &args...);
  }

//   template <typename... Args>
//   int thread_continue(Args&... args) {
//     return _hpx_thread_continue(sizeof...(Args), &args...);
//   };

  template <typename T>
  int thread_continue(T& arg) {
    return _hpx_thread_continue(2, &arg, sizeof(T));
  };

/// hpx parallel loop interface
/// Perform a "for" loop in parallel.
///
/// This encapsulates a simple local parallel for loop:
///
/// @code
/// for (int i = min, e = max; i < e; ++i) {
///   hpx_call(HPX_HERE, action, sync, &i, &args);
/// }
/// @endcode
///
/// The work is divided in equal chunks among the number of "worker"
/// threads available. Work is actively pushed to each worker thread
/// but is not affinitized and can be stolen by other worker threads.
///
/// @param      min The minimum index in the loop.
/// @param      max The maximum index in the loop.
/// @param     args The arguments to the for function @p f.
/// @param     sync An LCO that indicates the completion of all iterations.
///
//// @returns An error code, or HPX_SUCCESS.
template <typename LCO, typename... Args>
int par_for(const hpx::global_ptr<LCO>& sync, int min, int max, Args&... args) {
  return hpx_par_for(_id, min, max, &args..., sync.get());
}
template <typename... Args>
int par_for_sync(int min, int max, Args&... args) {
  return hpx_par_for_sync(_id, min, max, &args...);
}

/// Perform a parallel call.
///
/// This encapsulates a simple parallel for loop with the following semantics.
///
/// @code
/// for (int i = min, e = max; i < e; ++i) {
///   char args[arg_size];
///   arg_init(args, i, env);
///   hpx_call(HPX_HERE, action, sync, args, arg_size);
/// }
/// @endcode
///
/// The loop may actually be spawned as a tree, in which case @p
/// branching_factor controls how many chunks each range is partitioned into,
/// and @p cutoff controls the smalled chunk that is split.
///
/// @param              min The minimum index in the loop.
/// @param              max The maximum index in the loop.
/// @param branching_factor The tree branching factor for tree-spawn.
/// @param           cutoff The largest chunk we won't split.
/// @param         arg_size The size of the arguments to action.
/// @param         arg_init A callback to initialize the arguments
/// @param         env_size The size of the environment of arg_init.
/// @param              env An environment to pass to arg_init.
/// @param             sync An LCO to set as the continuation for each iteration.
///
/// @returns An error code, or HPX_SUCCESS.
template <typename LCO>
int par_call(const int min, const int max,
         const int branching_factor, const int cutoff,
         const size_t arg_size,
         void (*arg_init)(void*, const int, const void*),
         const size_t env_size, const void *env,
         const hpx::global_ptr<LCO>& sync) {
  return hpx_par_call(_id, min, max, branching_factor, cutoff, arg_size, arg_init, env_size, env, sync.get());
}

int par_call_sync(const int min, const int max,
          const int branching_factor, const int cutoff,
          const size_t arg_size,
          void (*arg_init)(void*, const int, const void*),
          const size_t env_size,
          const void *env) {
  return hpx_par_call_sync(_id, min, max, branching_factor, cutoff, arg_size, arg_init, env_size, env);
}

template <typename T>
int count_range_call(const hpx::global_ptr<T>& addr,
             const size_t count,
             const size_t increment,
             const uint32_t bsize,
             const size_t arg_size,
             void *const arg) {
  return hpx_count_range_call(addr.get(), count, increment, bsize, arg_size, arg);
}

/// Collective calls.
///

  template <typename... Args>
  int process_broadcast(hpx_addr_t lsync, hpx_addr_t rsync, Args&... args) {
    return _hpx_process_broadcast(hpx_thread_current_pid(), _id, lsync, rsync, sizeof...(Args), &args...);
  }
  template <typename LSYNC, typename RSYNC, typename... Args>
  int process_broadcast(const ::hpx::global_ptr<LSYNC>& lsync,
                   const ::hpx::global_ptr<RSYNC>& rsync,
                   Args&... args) {
    return _hpx_process_broadcast(hpx_thread_current_pid(), _id, lsync.get(), rsync.get(), sizeof...(Args), &args...);
  }
  template <typename... Args>
  int process_broadcast_lsync(hpx_addr_t rsync, Args&... args) {
    return _hpx_process_broadcast_lsync(hpx_thread_current_pid(), _id, rsync, sizeof...(Args), &args...);
  }
  template <typename RSYNC, typename... Args>
  int process_broadcast_lsync(::hpx::global_ptr<RSYNC>& rsync, Args&... args) {
    return _hpx_process_broadcast_lsync(hpx_thread_current_pid(), _id, rsync.get(), sizeof...(Args), &args...);
  }
  template <typename... Args>
  int process_broadcast_rsync(Args&... args) {
    return _hpx_process_broadcast_rsync(hpx_thread_current_pid(), _id, sizeof...(Args), &args...);
  }

  template <typename... Args>
  int run(Args&... args) {
    using traits = hpx::detail::function_traits<F>;
    static_assert(::std::is_same< typename traits::arg_types, hpx::detail::tlist<Args...> >::value,
                  "action and argument types do not match");
    return hpx::run(_id, std::forward<Args>(args)...);
  }

  template <typename R, typename... Args>
  int _register(R(&f)(Args...)) {
//     return hpx_register_action(HPX_DEFAULT, HPX_ATTR_NONE, __FILE__ ":" _HPX_XSTR(A::id),
//                                &(A::id), sizeof...(Args) + 1 , f,
//                                _convert_arg_type<Args>::type...);
    return _register_helper(f);
  }

}; // template class Action

// helper methods to create action object
template <hpx_action_type_t T, uint32_t ATTR, typename F, typename... ContTs>
Action<T, ATTR, F, ContTs...>
make_action(F& f) {
  return Action<T, ATTR, F, ContTs...>(f);
}
template <typename F, typename... ContTs>
Action<HPX_DEFAULT, HPX_ATTR_NONE, F, ContTs...>
make_action(F& f) {
  return Action<HPX_DEFAULT, HPX_ATTR_NONE, F, ContTs...>(f);
}

}

#endif // HPX_CXX_ACTION_H
