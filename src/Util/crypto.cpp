// Copyright (c) 2025, WH & 2025, kiwec, All rights reserved.
#include "crypto.h"
#include "sha256.h"            // vendored library
#include "base64.h"            // vendored library
#include "MD5.h"               // vendored library
#include "ByteBufferedFile.h"  // for file hashing functions
#include <vector>
#include <cstring>

#if !defined(_MSC_VER) && __has_include(<openssl/crypto.h>)
#include <openssl/rand.h>
#include <openssl/evp.h>
#define USE_OPENSSL
#endif

#ifdef _WIN32
#include "WinDebloatDefs.h"
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/random.h>
#endif

namespace crypto {
namespace rng {

void get_bytes(u8* out, std::size_t s_out) {
#ifdef USE_OPENSSL
    if(RAND_bytes(out, static_cast<i32>(s_out)) == 1) {
        return;
    }
#endif

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hCryptProv, s_out, out);
        CryptReleaseContext(hCryptProv, 0);
    }
#else
    getrandom(out, s_out, 0);
#endif
}

}  // namespace rng

namespace hash {
void sha256(const void* data, size_t size, u8* hash) {
#ifdef USE_OPENSSL
    unsigned int hash_len;
    if(EVP_Digest(data, size, hash, &hash_len, EVP_sha256(), nullptr) == 1) {
        return;
    }
#endif

    // fallback to vendored library
    sha256_easy_hash(data, size, hash);
}

void md5(const void* data, size_t size, u8* hash) {
#ifdef USE_OPENSSL
    unsigned int hash_len;
    if(EVP_Digest(data, size, hash, &hash_len, EVP_md5(), nullptr) == 1) {
        return;
    }
#endif

    // fallback to vendored library
    MD5 hasher;
    hasher.update(static_cast<const unsigned char*>(data), size);
    hasher.finalize();
    std::memcpy(hash, hasher.getDigest(), 16);
}

void sha256_f(const UString& file_path, u8* hash) {
    constexpr size_t CHUNK_SIZE{32768};
    std::array<u8, CHUNK_SIZE> buffer{};
    size_t bytes_read{0};

#ifdef USE_OPENSSL
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if(ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1) {
        ByteBufferedFile::Reader reader(file_path);
        if(reader.good()) {
            bool success = true;

            while(reader.good() && (bytes_read = reader.read_bytes(buffer.data(), CHUNK_SIZE)) > 0) {
                if(EVP_DigestUpdate(ctx, buffer.data(), bytes_read) != 1) {
                    success = false;
                    break;
                }
            }

            if(success && reader.good()) {
                unsigned int hash_len = 0;
                if(EVP_DigestFinal_ex(ctx, hash, &hash_len) == 1) {
                    EVP_MD_CTX_free(ctx);
                    return;
                }
            }
        }
        EVP_MD_CTX_free(ctx);
    }
#endif

    // fallback to vendored library
    struct sha256_buff buff;
    sha256_init(&buff);

    ByteBufferedFile::Reader reader(file_path);
    if(reader.good()) {
        while(reader.good() && (bytes_read = reader.read_bytes(buffer.data(), CHUNK_SIZE)) > 0) {
            sha256_update(&buff, buffer.data(), bytes_read);
        }
    }

    sha256_finalize(&buff);
    sha256_read(&buff, hash);
}

void md5_f(const UString& file_path, u8* hash) {
    constexpr size_t CHUNK_SIZE{32768};
    std::array<u8, CHUNK_SIZE> buffer{};
    size_t bytes_read{0};

#ifdef USE_OPENSSL
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if(ctx && EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1) {
        ByteBufferedFile::Reader reader(file_path);
        if(reader.good()) {
            bool success = true;

            while(reader.good() && (bytes_read = reader.read_bytes(buffer.data(), CHUNK_SIZE)) > 0) {
                if(EVP_DigestUpdate(ctx, buffer.data(), bytes_read) != 1) {
                    success = false;
                    break;
                }
            }

            if(success && reader.good()) {
                unsigned int hash_len;
                if(EVP_DigestFinal_ex(ctx, hash, &hash_len) == 1) {
                    EVP_MD_CTX_free(ctx);
                    return;
                }
            }
        }
        EVP_MD_CTX_free(ctx);
    }
#endif

    // fallback to vendored library
    MD5 hasher;
    ByteBufferedFile::Reader reader(file_path);
    if(reader.good()) {
        while(reader.good() && (bytes_read = reader.read_bytes(buffer.data(), CHUNK_SIZE)) > 0) {
            hasher.update(buffer.data(), bytes_read);
        }
    }

    hasher.finalize();
    std::memcpy(hash, hasher.getDigest(), 16);
}
}  // namespace hash

namespace conv {
std::string encode64(const u8* src, size_t len) {
#ifdef USE_OPENSSL
    // calculate output size: base64 encoding produces 4 chars for every 3 input bytes
    size_t encoded_len = 4 * ((len + 2) / 3);
    std::vector<u8> temp(encoded_len + 1);

    int actual_len = EVP_EncodeBlock(temp.data(), src, static_cast<int>(len));
    if(actual_len > 0) {
        return std::string{(const char*)temp.data(), actual_len};
    }
#endif

    // fallback to vendored library
    size_t out_len;
    u8* result = base64_encode(src, len, &out_len);
    if(result) {
        std::string res{(const char*)result, out_len};
        delete[] result;
        return res;
    }

    return "";
}

std::vector<u8> decode64(const u8* src, size_t len) {
#ifdef USE_OPENSSL
    // calculate maximum output size
    size_t max_decoded_len = (len * 3) / 4 + 1;
    std::vector<u8> temp(max_decoded_len);

    int actual_len = EVP_DecodeBlock(temp.data(), src, static_cast<int>(len));
    if(actual_len >= 0) {
        // EVP_DecodeBlock doesn't account for padding, need to adjust
        int padding = 0;
        if(len >= 2) {
            if(src[len - 1] == '=') padding++;
            if(src[len - 2] == '=') padding++;
        }
        actual_len -= padding;

        temp.resize(static_cast<size_t>(actual_len));
        return temp;
    }
#endif

    // fallback to vendored library
    size_t out_len;
    u8* result = base64_decode(src, len, &out_len);
    if(result) {
        std::vector<u8> vec(result, result + out_len);
        delete[] result;
        return vec;
    }

    return {};
}
}  // namespace conv

}  // namespace crypto
