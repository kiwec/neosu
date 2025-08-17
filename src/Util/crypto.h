// Copyright (c) 2025, WH & 2025, kiwec, All rights reserved.
#pragma once

#include "types.h"
#include <string>
#include <vector>

class UString;

namespace crypto {

namespace rng {
void get_bytes(u8* out, size_t s_out);
}  // namespace rng

namespace hash {
void sha256(const void* data, size_t size, u8* hash);
void md5(const void* data, size_t size, u8* hash);

// takes a file directly
void sha256_f(const UString& file_path, u8* hash);
void md5_f(const UString& file_path, u8* hash);
}  // namespace hash

namespace conv {
std::string encode64(const u8* src, size_t len);
std::vector<u8> decode64(const u8* src, size_t len);

template <size_t N>
std::string encodehex(const std::array<u8, N>& src) {
    const char* hex_chars = "0123456789abcdef";

    std::string out;
    out.reserve(N * 2);

    for(u8 byte : src) {
        out.push_back(hex_chars[byte >> 4]);
        out.push_back(hex_chars[byte & 0x0F]);
    }

    return out;
}

}  // namespace conv

}  // namespace crypto
