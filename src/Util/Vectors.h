// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#ifndef BUILD_TOOLS_ONLY
#include "fmt/format.h"
#include "fmt/compile.h"
#endif

using glm::vec2;
using glm::vec3;
using glm::vec4;
using vec2d = glm::dvec2;
using vec3d = glm::dvec3;
using vec4d = glm::dvec4;
using ivec2 = glm::i32vec2;
using ivec3 = glm::i32vec3;
using ivec4 = glm::i32vec4;
using lvec2 = glm::i64vec2;
using lvec3 = glm::i64vec3;
using lvec4 = glm::i64vec4;

namespace vec {

inline constexpr auto FLOAT_NORMALIZE_EPSILON = 0.000001f;
inline constexpr auto DOUBLE_NORMALIZE_EPSILON = FLOAT_NORMALIZE_EPSILON / 10e6;

using glm::all;
using glm::any;
using glm::cross;
using glm::distance;
using glm::dot;
using glm::equal;
using glm::greaterThan;
using glm::greaterThanEqual;
using glm::length;
using glm::lessThan;
using glm::lessThanEqual;
using glm::max;
using glm::min;
using glm::normalize;
using glm::round;

template <typename T, typename V>
    requires(std::is_floating_point_v<V>) &&
            (std::is_same_v<T, vec2> || std::is_same_v<T, vec3> || std::is_same_v<T, vec4>)
void setLength(T &vec, const V &len) {
    if(length(vec) > FLOAT_NORMALIZE_EPSILON) {
        vec = normalize(vec) * static_cast<float>(len);
    }
}

template <typename T, typename V>
    requires(std::is_floating_point_v<V>) &&
            (std::is_same_v<T, vec2d> || std::is_same_v<T, vec3d> || std::is_same_v<T, vec4d>)
void setLength(T &vec, const V &len) {
    if(length(vec) > DOUBLE_NORMALIZE_EPSILON) {
        vec = normalize(vec) * static_cast<double>(len);
    }
}

template <typename V>
    requires(std::is_same_v<V, vec2> || std::is_same_v<V, vec3> || std::is_same_v<V, vec4> ||
             std::is_same_v<V, vec2d> || std::is_same_v<V, vec3d> || std::is_same_v<V, vec4d> ||
             std::is_same_v<V, ivec2> || std::is_same_v<V, ivec3> || std::is_same_v<V, ivec4> ||
             std::is_same_v<V, lvec2> || std::is_same_v<V, lvec3> || std::is_same_v<V, lvec4>)
inline constexpr bool allEqual(const V &vec1, const V vec2) {
    return vec::all(vec::equal(vec1, vec2));
}

}  // namespace vec

#ifndef BUILD_TOOLS_ONLY
namespace fmt {
template <typename Vec, int N>
struct float_vec_formatter {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Vec &p, FormatContext &ctx) const {
        if constexpr(N == 2) {
            return format_to(ctx.out(), "({:.2f}, {:.2f})"_cf, p.x, p.y);
        } else if constexpr(N == 3) {
            return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})"_cf, p.x, p.y, p.z);
        } else {
            return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})"_cf, p.x, p.y, p.z, p.w);
        }
    }
};

template <typename Vec, int N>
struct int_vec_formatter {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Vec &p, FormatContext &ctx) const {
        if constexpr(N == 2) {
            return format_to(ctx.out(), "({}, {})"_cf, p.x, p.y);
        } else if constexpr(N == 3) {
            return format_to(ctx.out(), "({}, {}, {})"_cf, p.x, p.y, p.z);
        } else {
            return format_to(ctx.out(), "({}, {}, {}, {})"_cf, p.x, p.y, p.z, p.w);
        }
    }
};

template <>
struct formatter<vec2> : float_vec_formatter<vec2, 2> {};

template <>
struct formatter<vec2d> : float_vec_formatter<vec2d, 2> {};

template <>
struct formatter<vec3> : float_vec_formatter<vec3, 3> {};

template <>
struct formatter<vec3d> : float_vec_formatter<vec3d, 3> {};

template <>
struct formatter<vec4> : float_vec_formatter<vec4, 4> {};

template <>
struct formatter<vec4d> : float_vec_formatter<vec4d, 4> {};

template <>
struct formatter<ivec2> : int_vec_formatter<ivec2, 2> {};

template <>
struct formatter<ivec3> : int_vec_formatter<ivec3, 3> {};

template <>
struct formatter<ivec4> : int_vec_formatter<ivec4, 4> {};

template <>
struct formatter<lvec2> : int_vec_formatter<lvec2, 2> {};

template <>
struct formatter<lvec3> : int_vec_formatter<lvec3, 3> {};

template <>
struct formatter<lvec4> : int_vec_formatter<lvec4, 4> {};

}  // namespace fmt
#endif
