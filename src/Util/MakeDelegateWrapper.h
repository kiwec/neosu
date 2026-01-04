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

template <auto Method, typename Class>
auto MakeDelegate(Class* instance) {
    using traits = member_function_traits<decltype(Method)>;
    using signature = typename traits::signature;
    using class_type = typename traits::class_type;

    return delegate<signature>::template create<class_type, Method>(instance);
}

template <auto Method, typename Class>
auto MakeDelegate(const Class* instance) {
    using traits = member_function_traits<decltype(Method)>;
    using signature = typename traits::signature;
    using class_type = typename traits::class_type;

    return delegate<signature>::template create<class_type, Method>(instance);
}

}  // namespace SA
