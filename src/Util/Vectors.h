// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "fmt/format.h"

using glm::vec2;
using glm::vec3;
using glm::vec4;
using vec2d = glm::dvec2;
using vec3d = glm::dvec3;
using vec4d = glm::dvec4;

namespace vec {

static constexpr auto FLOAT_NORMALIZE_EPSILON = 0.000001f;
static constexpr auto DOUBLE_NORMALIZE_EPSILON = FLOAT_NORMALIZE_EPSILON / 10e6;

using glm::all;
using glm::any;
using glm::cross;
using glm::distance;
using glm::dot;
using glm::round;
using glm::equal;
using glm::greaterThan;
using glm::greaterThanEqual;
using glm::length;
using glm::lessThan;
using glm::lessThanEqual;
using glm::max;
using glm::min;
using glm::normalize;

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
    requires(std::is_same_v<V, vec2> || std::is_same_v<V, vec3> || std::is_same_v<V, vec4> || std::is_same_v<V, vec2d> || std::is_same_v<V, vec3d> || std::is_same_v<V, vec4d>)
inline constexpr bool allEqual(const V &vec1, const V vec2) {
    return vec::all(vec::equal(vec1, vec2));
}

}  // namespace vec

namespace fmt {
template <>
struct formatter<vec2> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec2 &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f})", p.x, p.y);
    }
};

template <>
struct formatter<vec2d> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec2d &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f})", p.x, p.y);
    }
};

template <>
struct formatter<vec3> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec3 &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
    }
};

template <>
struct formatter<vec3d> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec3d &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z);
    }
};

template <>
struct formatter<vec4> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec4 &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z, p.w);
    }
};

template <>
struct formatter<vec4d> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const vec4d &p, FormatContext &ctx) const {
        return format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})", p.x, p.y, p.z, p.w);
    }
};

}  // namespace fmt
