#pragma once
#include "Delegate.h"

#include <cstdint>
#include <type_traits>
#include <utility>

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
