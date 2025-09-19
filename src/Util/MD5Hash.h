#pragma once

#include "UString.h"

struct MD5Hash final {
    MD5Hash() = default;
    MD5Hash(const char *str) {
        strncpy(this->string(), str, 32);
        this->hash[32] = '\0';
    }

    [[nodiscard]] inline char *string() { return this->hash.data(); }
    [[nodiscard]] inline const char *string() const { return this->hash.data(); }

    [[nodiscard]] inline char *data() { return this->hash.data(); }
    [[nodiscard]] inline const char *data() const { return this->hash.data(); }

    [[nodiscard]] inline size_t length() const { return strnlen(this->string(), 32); }
    [[nodiscard]] inline bool operator==(const MD5Hash &other) const {
        return memcmp(this->string(), other.string(), 32) == 0;
    }
    [[nodiscard]] inline bool operator==(const std::string &other) const {
        return strncmp(this->string(), other.c_str(), 32) == 0;
    }
    [[nodiscard]] inline bool operator==(const UString &other) const {
        return strncmp(this->string(), other.toUtf8(), 32) == 0;
    }

    inline void clear() { this->hash = {}; }

    std::array<char, 33> hash{};
};

namespace std {
template <>
struct hash<MD5Hash> {
    size_t operator()(const MD5Hash &md5) const { return std::hash<std::string_view>()({md5.string(), 32}); }
};
}  // namespace std
