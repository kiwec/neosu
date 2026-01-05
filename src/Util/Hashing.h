#pragma once
// Copyright (c) 2023, kiwec & 2025-2026, WH, All rights reserved.

#include "ankerl/unordered_dense.h"

#include <string>
#include <string_view>

namespace Hash {
namespace flat = ankerl::unordered_dense;

// transparent hash and equality for heterogeneous lookup
struct UnstableStringHash {
    using is_transparent = void;  // enable heterogeneous overloads
    using is_avalanching = void;  // mark class as high quality avalanching hash

    [[nodiscard]] auto operator()(std::string_view str) const noexcept -> uint64_t {
        return flat::hash<std::string_view>{}(str);
    }
};

// unstable because unordered_dense doesn't guarantee iterator/reference stability
template <typename T>
using unstable_stringmap = flat::map<std::string, T, UnstableStringHash, std::equal_to<>>;

struct StableStringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    std::size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    std::size_t operator()(const char *s) const { return std::hash<std::string_view>{}(std::string_view(s)); }
};

template <typename T>
using stable_stringmap = std::unordered_map<std::string, T, StableStringHash, std::equal_to<>>;

}  // namespace Hash
