// Copyright (c) 2025, WH & 2025, kiwec, All rights reserved.
#pragma once

#include "types.h"
#include <string>
#include <vector>

class UString;

namespace crypto {

namespace rng {
void get_bytes(u8* out, size_t s_out);

// generate a random integral value
template <typename T = u64>
T get_rand() {
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    T result;
    get_bytes(reinterpret_cast<u8*>(&result), sizeof(T));
    return result;
}

// fill an array with random bytes
template <typename T, std::size_t N>
void get_rand(std::array<T, N>& arr) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    get_bytes(reinterpret_cast<u8*>(arr.data()), sizeof(T) * N);
}

template <typename T, std::size_t N>
void get_rand(T (&arr)[N]) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    get_bytes(reinterpret_cast<u8*>(arr), sizeof(T) * N);
}

// fill a vector with random bytes
template <typename T>
void get_rand(std::vector<T>& vec) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    if(!vec.empty()) {
        get_bytes(reinterpret_cast<u8*>(vec.data()), vec.size() * sizeof(T));
    }
}

}  // namespace rng

namespace hash {
void sha256(const void* data, size_t size, u8* hash);
void md5(const void* data, size_t size, u8* hash);

// takes a file directly
void sha256_f(std::string_view file_path, u8* hash);
void md5_f(std::string_view file_path, u8* hash);
}  // namespace hash

namespace conv {
std::string encode64(const u8* src, size_t len);

template <size_t N>
std::string encode64(const std::array<u8, N>& src) {
    return encode64(src.data(), N);
}

std::vector<u8> decode64(std::string src);

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
