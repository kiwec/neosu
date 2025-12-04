#pragma once

#include <cstring>
#include <array>
#include <string_view>
#include <string>

class UString;

struct alignas(16) MD5Hash final {
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

    std::array<char, 32> hash{};
};

namespace std {
template <>
struct hash<MD5Hash> {
    size_t operator()(const MD5Hash &md5) const { return std::hash<std::string_view>()(md5.string()); }
};
}  // namespace std
