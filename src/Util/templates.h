#pragma once
// Copyright (c) 2023, kiwec & 2025, WH, All rights reserved.

// miscellaneous templates

#include <string>
#include <string_view>
#include <unordered_map>

// transparent hash and equality for heterogeneous lookup
struct StringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    std::size_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    std::size_t operator()(const char *s) const { return std::hash<std::string_view>{}(std::string_view(s)); }
};

template <typename T>
using sv_unordered_map = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;
