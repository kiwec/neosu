// Copyright (c) 2025, WH, All rights reserved.
// miscellaneous utilities/macros which don't require transitive includes
#pragma once

#define SAFE_DELETE(p)  \
    {                   \
        if(p) {         \
            delete(p);  \
            (p) = NULL; \
        }               \
    }

#define PI 3.1415926535897932384626433832795
#define PIOVER180 0.01745329251994329576923690768489

inline bool isInt(float f) { return (f == static_cast<float>(static_cast<int>(f))); }

#ifdef APP_LIBRARY_BUILD
#ifdef _WIN32
#define EXPORT_NAME_ __declspec(dllexport)
#else
#define EXPORT_NAME_ __attribute__((visibility("default")))
#endif
#else
#define EXPORT_NAME_
#endif

// not copy or move constructable/assignable
// purely for clarifying intent
#define NOCOPY_NOMOVE(classname__)                        \
   private:                                               \
    classname__(const classname__ &) = delete;            \
    classname__ &operator=(const classname__ &) = delete; \
    classname__(classname__ &&) = delete;                 \
    classname__ &operator=(classname__ &&) = delete;

// create string view literal
#define MC_SV(string__) \
    std::string_view_literals::operator""sv(string__, (sizeof(string__) / sizeof((string__)[0]) - 1))

#if defined(__GNUC__) || defined(__clang__)
#define likely(x) __builtin_expect(bool(x), 1)
#define unlikely(x) __builtin_expect(bool(x), 0)
#define forceinline __attribute__((always_inline)) inline

// force all functions in the function body to be inlined into it
// different from "forceinline", because the function itself won't necessarily be inlined at all call sites
#define INLINE_BODY __attribute__((flatten))

#else
#define likely(x) (x)
#define unlikely(x) (x)
#ifdef _MSC_VER
#define forceinline __forceinline
#else
#define forceinline inline
#endif
#define INLINE_BODY
#endif

#if defined(__clang__)
#define MC_ASSUME(expr) __builtin_assume(expr)
#elif defined(__GNUC__)
#if defined(__has_attribute) && __has_attribute(assume)
#define MC_ASSUME(expr) __attribute__((assume(expr)))
#else
#define MC_ASSUME(expr)              \
    do {                             \
        if(expr) {                   \
        } else {                     \
            __builtin_unreachable(); \
        }                            \
    } while(false)
#endif
#elif defined(_MSC_VER)
#define MC_ASSUME(expr) __assume(expr)
#endif

#define MAKE_FLAG_ENUM(Enum_name__) \
    inline constexpr bool is_flag(Enum_name__) { return true; }

namespace flags {
namespace detail {
// minimal enable_if implementation
template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
    typedef T type;
};

// check if unsigned using the property that unsigned(-1) > 0
template <typename T>
struct is_unsigned {
    static constexpr inline bool value = T(-1) > T(0);
};

// check if type is an enum using compiler intrinsic
template <typename T>
struct is_enum {
    static constexpr inline bool value = __is_enum(T);
};

// get underlying type using compiler intrinsic (safe for non-enums)
template <typename T, bool is_enum>
struct underlying_type_impl {
    typedef T type;  // fallback for non-enums
};

template <typename T>
struct underlying_type_impl<T, true> {
    typedef __underlying_type(T) type;
};

template <typename T>
struct underlying_type {
    typedef typename underlying_type_impl<T, is_enum<T>::value>::type type;
};

// helper to check if underlying type is unsigned (only valid for enums)
template <typename T, bool is_enum>
struct is_unsigned_enum_impl {
    static const bool value = false;
};

template <typename T>
struct is_unsigned_enum_impl<T, true> {
    static const bool value = is_unsigned<typename underlying_type<T>::type>::value;
};

// check if type is an unsigned enum
template <typename T>
struct is_unsigned_enum {
    static const bool value = is_unsigned_enum_impl<T, is_enum<T>::value>::value;
};

// SFINAE check if is_flag(T{}) exists and returns true
template <typename T>
constexpr auto check_is_flag(int /**/) -> decltype(is_flag(T{}), bool()) {
    return is_flag(T{});
}

template <typename T>
// NOLINTNEXTLINE(cert-dcl50-cpp)
constexpr bool check_is_flag(...) {
    return false;
}

// check if type is a flag enum (unsigned enum with is_flag() returning true)
template <typename T>
struct is_flag_enum {
    static const bool value = is_unsigned_enum<T>::value && check_is_flag<T>(0);
};

template <typename T>
constexpr typename underlying_type<T>::type underlying_value(T enum_value) {
    return static_cast<typename underlying_type<T>::type>(enum_value);
}

}  // namespace detail

namespace operators {
// NOLINTBEGIN(modernize-use-constraints)
template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type operator~(T value) {
    return static_cast<T>(~detail::underlying_value(value));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, bool>::type operator!(T value) {
    return detail::underlying_value(value) == 0;
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type operator|(T lhs, T rhs) {
    return static_cast<T>(detail::underlying_value(lhs) | detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator|(typename detail::underlying_type<T>::type lhs, T rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(lhs | detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator|(T lhs, typename detail::underlying_type<T>::type rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(detail::underlying_value(lhs) | rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type &
operator|=(typename detail::underlying_type<T>::type &value, T const flag) {
    return value = value | flag;
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator|=(T &lhs, T const rhs) {
    return lhs = static_cast<T>(lhs | rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator|=(
    T &lhs, typename detail::underlying_type<T>::type const rhs) {
    return lhs = static_cast<T>(lhs | rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type operator&(T lhs, T rhs) {
    return static_cast<T>(detail::underlying_value(lhs) & detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator&(typename detail::underlying_type<T>::type lhs, T rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(lhs & detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator&(T lhs, typename detail::underlying_type<T>::type rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(detail::underlying_value(lhs) & rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type &
operator&=(typename detail::underlying_type<T>::type &value, T const flag) {
    return value = value & flag;
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator&=(T &lhs, T const rhs) {
    return lhs = static_cast<T>(lhs & rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator&=(
    T &lhs, typename detail::underlying_type<T>::type const rhs) {
    return lhs = static_cast<T>(lhs & rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type operator^(T lhs, T rhs) {
    return static_cast<T>(detail::underlying_value(lhs) ^ detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator^(typename detail::underlying_type<T>::type lhs, T rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(lhs ^ detail::underlying_value(rhs));
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type
operator^(T lhs, typename detail::underlying_type<T>::type rhs) {
    return static_cast<typename detail::underlying_type<T>::type>(detail::underlying_value(lhs) ^ rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, typename detail::underlying_type<T>::type>::type &
operator^=(typename detail::underlying_type<T>::type &value, T const flag) {
    return value = value ^ flag;
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator^=(T &lhs, T const rhs) {
    return lhs = static_cast<T>(lhs ^ rhs);
}

template <typename T>
constexpr typename detail::enable_if<detail::is_flag_enum<T>::value, T>::type &operator^=(
    T &lhs, typename detail::underlying_type<T>::type const rhs) {
    return lhs = static_cast<T>(lhs ^ rhs);
}
}  // namespace operators

template <auto mask>
constexpr typename detail::enable_if<detail::is_flag_enum<decltype(mask)>::value, bool>::type has(
    typename detail::underlying_type<decltype(mask)>::type value) {
    using namespace operators;
    return (value & mask) == detail::underlying_value(mask);
}

template <auto mask>
constexpr typename detail::enable_if<detail::is_flag_enum<decltype(mask)>::value, bool>::type has(
    decltype(mask) value) {
    using namespace operators;
    return (value & mask) == mask;
}

template <auto mask>
constexpr typename detail::enable_if<detail::is_flag_enum<decltype(mask)>::value, bool>::type any(
    decltype(mask) value) {
    using namespace operators;
    return !!(value & mask);
}

// NOLINTEND(modernize-use-constraints)

}  // namespace flags

using namespace flags::operators;
