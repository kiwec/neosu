#pragma once
// Copyright (c) 2023, kiwec & 2025-2026, WH, All rights reserved.

#include "ankerl/unordered_dense.h"

#include <string>
#include <string_view>
#include <cctype>
#include <algorithm>

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

    uint64_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    uint64_t operator()(const std::string &s) const { return std::hash<std::string>{}(s); }
    uint64_t operator()(const char *s) const { return std::hash<std::string_view>{}(std::string_view(s)); }
};

template <typename T>
using stable_stringmap = std::unordered_map<std::string, T, StableStringHash, std::equal_to<>>;

struct StringHashNcase {
   private:
    [[nodiscard]] inline uint64_t hashFunc(std::string_view str) const {
        uint64_t hash = 0;
        for(auto &c : str) {
            hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
        }
        return hash;
    }

   public:
    using is_transparent = void;

    uint64_t operator()(const std::string &s) const { return hashFunc(s); }
    uint64_t operator()(std::string_view sv) const { return hashFunc(sv); }
};

struct StringEqualNcase {
   private:
    [[nodiscard]] inline bool equality(std::string_view lhs, std::string_view rhs) const {
        return std::ranges::equal(
            lhs, rhs, [](unsigned char a, unsigned char b) -> bool { return std::tolower(a) == std::tolower(b); });
    }

   public:
    using is_transparent = void;

    bool operator()(const std::string &lhs, const std::string &rhs) const { return equality(lhs, rhs); }
    bool operator()(const std::string &lhs, std::string_view rhs) const { return equality(lhs, rhs); }
    bool operator()(std::string_view lhs, const std::string &rhs) const { return equality(lhs, rhs); }
};

// case-insensitive string map
template <typename T>
using unstable_ncase_stringmap = Hash::flat::map<std::string, T, StringHashNcase, StringEqualNcase>;

template <typename T>
using unstable_ncase_set = Hash::flat::set<T, StringHashNcase, StringEqualNcase>;

}  // namespace Hash

// forward declaration tutorial (saving for later):

// #include <type_traits>

// namespace ankerl::unordered_dense::inline v4_8_1 {

// namespace bucket_type {
// struct standard;
// }  // namespace bucket_type

// namespace detail {
// template <class Key, class T, class Hash, class KeyEqual, class AllocatorOrContainer, class Bucket,
//           class BucketContainer, bool IsSegmented>
// class table;
// struct default_container_t;

// }  // namespace detail
// template <typename T, typename Enable>
// struct hash;

// template <class Key, class T, class Hash, class KeyEqual, class AllocatorOrContainer, class Bucket,
//           class BucketContainer>
// using map = detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, BucketContainer, false>;
// } // namespace ankerl::unordered_dense::inline v4_8_1

// namespace Hash {
// namespace flat = ankerl::unordered_dense;
// }

// using HashToScoreMap =
//     Hash::flat::map<MD5Hash, std::vector<FinishedScore>, Hash::flat::hash<MD5Hash, void>, std::equal_to<MD5Hash>,
//                     std::allocator<std::pair<MD5Hash, std::vector<FinishedScore>>>, Hash::flat::bucket_type::standard,
//                     Hash::flat::detail::default_container_t>;

// using HashToDiffMap = Hash::flat::map<MD5Hash, BeatmapDifficulty*, Hash::flat::hash<MD5Hash, void>,
//                                       std::equal_to<MD5Hash>, std::allocator<std::pair<MD5Hash, BeatmapDifficulty*>>,
//                                       Hash::flat::bucket_type::standard, Hash::flat::detail::default_container_t>;
