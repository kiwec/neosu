/*
 * S.A.K. delegates extensions/wrappers
 *
 * Copyright (c) 2025-2026 William Horvath
 *
 * Based on Sergey Ryazanov's:
 * "The Impossibly Fast C++ Delegates", 18 Jul 2005
 * https://www.codeproject.com/articles/11015/the-impossibly-fast-c-delegates
 * 
 * And Sergey A Kryukov's:
 * "The Impossibly Fast C++ Delegates, Fixed", 13 Feb 2017
 * https://www.codeproject.com/Articles/1170503/The-Impossibly-Fast-Cplusplus-Delegates-Fixed
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "Delegate.h"

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace SA {

// Helper to extract function signature from member function pointer type
template <typename T>
struct member_function_traits;

template <typename R, typename C, typename... Args>
struct member_function_traits<R (C::*)(Args...)> {
    using return_type = R;
    using class_type = C;
    using signature = R(Args...);
    static constexpr bool is_const = false;
};

template <typename R, typename C, typename... Args>
struct member_function_traits<R (C::*)(Args...) const> {
    using return_type = R;
    using class_type = C;
    using signature = R(Args...);
    static constexpr bool is_const = true;
};

// Helper to extract signature from callable objects (lambdas, functors)
template <typename T>
struct callable_traits : member_function_traits<decltype(&T::operator())> {};

// Helper to extract signature from function pointers
template <typename T>
struct function_pointer_traits;

template <typename R, typename... Args>
struct function_pointer_traits<R (*)(Args...)> {
    using return_type = R;
    using signature = R(Args...);
};

template <typename R, typename... Args>
struct function_pointer_traits<R(Args...)> {
    using return_type = R;
    using signature = R(Args...);
};

// Traits for SA::delegate type inspection
template <typename T>
struct is_delegate : std::false_type {};

template <typename R, typename... Args>
struct is_delegate<delegate<R(Args...)>> : std::true_type {};

template <typename T>
inline constexpr bool is_delegate_v = is_delegate<T>::value;

template <typename T>
struct delegate_traits;

template <typename R, typename... Args>
struct delegate_traits<delegate<R(Args...)>> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template <size_t N>
    using nth_arg = std::tuple_element_t<N, args_tuple>;
};

// MakeDelegate for member functions (non-const)
template <auto Method, typename Class>
auto MakeDelegate(Class* instance) {
    using traits = member_function_traits<decltype(Method)>;
    using signature = typename traits::signature;
    using class_type = typename traits::class_type;

    return delegate<signature>::template create<class_type, Method>(instance);
}

// MakeDelegate for member functions (const)
template <auto Method, typename Class>
auto MakeDelegate(const Class* instance) {
    using traits = member_function_traits<decltype(Method)>;
    using signature = typename traits::signature;
    using class_type = typename traits::class_type;

    return delegate<signature>::template create<class_type, Method>(instance);
}

// MakeDelegate for free-standing/static functions (compile-time)
template <auto Func>
    requires std::is_function_v<std::remove_pointer_t<decltype(Func)>>
auto MakeDelegate() {
    using traits = function_pointer_traits<std::remove_pointer_t<decltype(Func)>>;
    using signature = typename traits::signature;
    return delegate<signature>::template create<Func>();
}

// MakeDelegate for lambdas and callable objects
// Forwards to delegate::create which handles stateless rvalues (safe, converts to funcptr)
// and stateful lvalues (caller manages lifetime), while rejecting stateful rvalues (dangling)
template <typename Lambda>
    requires requires { &std::decay_t<Lambda>::operator(); }
auto MakeDelegate(Lambda&& lambda) {
    using traits = callable_traits<std::decay_t<Lambda>>;
    using signature = typename traits::signature;
    return delegate<signature>::create(std::forward<Lambda>(lambda));
}

}  // namespace SA
