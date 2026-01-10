#pragma once

#include <cstring>
#include <array>
#include <string_view>
#include <string>

#ifndef BUILD_TOOLS_ONLY
#include "ankerl/unordered_dense.h"
namespace Hash {
namespace flat = ankerl::unordered_dense;
}
#endif

class UString;

#if defined(__GNUC__) && !defined(__clang__) && (defined(__MINGW32__) || defined(__MINGW64__))
// inexplicably broken in unpredictable ways with mingw-gcc
#define ALIGNED_TO(x)
#else
#define ALIGNED_TO(x) alignas(x)
#endif

struct ALIGNED_TO(16) MD5Hash final {
    MD5Hash() = default;
    MD5Hash(const char *str);

    // NOTE: not null-terminated
    [[nodiscard]] inline char *data() { return this->hash.data(); }
    [[nodiscard]] inline const char *data() const { return this->hash.data(); }

    [[nodiscard]] inline std::string_view string() const { return {this->hash.begin(), this->hash.end()}; }

    [[nodiscard]] inline size_t length() const { return this->hash.size(); }
    [[nodiscard]] inline bool operator==(const MD5Hash &other) const { return this->hash == other.hash; }
    [[nodiscard]] inline bool operator==(const std::string &other) const { return this->string() == other; }
    [[nodiscard]] bool operator==(const UString &other) const;

    inline void clear() { this->hash = {}; }

    // you'd have to be extremely unlucky to have an MD5 of all zeros
    inline bool empty() { return this->hash == std::array<char, 32>{}; }

    std::array<char, 32> hash{};
};

#undef ALIGNED_TO

namespace std {
template <>
struct hash<MD5Hash> {
    size_t operator()(const MD5Hash &md5) const { return std::hash<std::string_view>()(md5.string()); }
};
}  // namespace std


#ifndef BUILD_TOOLS_ONLY
template <>
struct Hash::flat::hash<MD5Hash> {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(const MD5Hash &md5) const noexcept -> uint64_t {
        return detail::wyhash::hash(md5.data(), md5.length());
    }
};
#endif