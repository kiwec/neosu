#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

#include <charconv>  // from_chars
#include <cstring>   // strlen, strncmp
#include <string_view>
#include <optional>

#include "types.h"
#include "Vectors.h"
#include "SString.h"

namespace Parsing {

// NOLINTBEGIN(cppcoreguidelines-init-variables)
namespace detail {

template <typename T>
const char* parse_str(const char* begin, const char* end, T* arg) {
    if constexpr(std::is_same_v<T, f32>) {
        f32 f;
        auto [ptr, ec] = std::from_chars(begin, end, f);
        if(ec != std::errc()) return nullptr;
        *arg = f;
        return ptr;
    } else if constexpr(std::is_same_v<T, f64>) {
        f64 d;
        auto [ptr, ec] = std::from_chars(begin, end, d);
        if(ec != std::errc()) return nullptr;
        *arg = d;
        return ptr;
    } else if constexpr(std::is_same_v<T, i32>) {
        i32 i;
        auto [ptr, ec] = std::from_chars(begin, end, i);
        if(ec != std::errc()) return nullptr;
        *arg = i;
        return ptr;
    } else if constexpr(std::is_same_v<T, i64>) {
        i64 ll;
        auto [ptr, ec] = std::from_chars(begin, end, ll);
        if(ec != std::errc()) return nullptr;
        *arg = ll;
        return ptr;
    } else if constexpr(std::is_same_v<T, u32>) {
        u32 u;
        auto [ptr, ec] = std::from_chars(begin, end, u);
        if(ec != std::errc()) return nullptr;
        *arg = u;
        return ptr;
    } else if constexpr(std::is_same_v<T, u64>) {
        u64 ull;
        auto [ptr, ec] = std::from_chars(begin, end, ull);
        if(ec != std::errc()) return nullptr;
        *arg = ull;
        return ptr;
    } else if constexpr(std::is_same_v<T, bool>) {
        long l;
        auto [ptr, ec] = std::from_chars(begin, end, l);
        if(ec != std::errc()) return nullptr;
        *arg = (l > 0);
        return ptr;
    } else if constexpr(std::is_same_v<T, u8>) {
        unsigned int b;
        auto [ptr, ec] = std::from_chars(begin, end, b);
        if(ec != std::errc() || b > 255) return nullptr;
        *arg = static_cast<u8>(b);
        return ptr;
    } else if constexpr(std::is_same_v<T, std::string>) {
        // special handling for quoted string values
        if(begin < end && *begin == '"') {
            begin++;
            const char* start = begin;
            while(begin < end && *begin != '"') {
                begin++;
            }

            // expected closing '"', reached end instead
            if(begin == end) return nullptr;

            *arg = std::string(start, begin - start);
            begin++;
            return begin;
        } else {
            *arg = std::string(begin, end);
            SString::trim_inplace(*arg);
            return end;
        }
    } else {
        static_assert(Env::always_false_v<T>, "parsing for this type is not implemented");
        return nullptr;
    }
}

// base case for recursive parse_impl
inline const char* parse_impl(const char* begin, const char* /* end */) { return begin; }

template <typename T, typename... Extra>
const char* parse_impl(const char* begin, const char* end, T arg, Extra... extra) {
    // always skip whitespace
    while(begin < end && (*begin == ' ' || *begin == '\t')) begin++;

    if constexpr(std::is_same_v<T, std::string*> && sizeof...(extra) > 0) {
        // you can only parse an std::string if it is the LAST parameter,
        // because it will consume the WHOLE string.
        static_assert(Env::always_false_v<T>, "cannot parse an std::string in the middle of the parsing chain");
        return nullptr;
    } else if constexpr(std::is_same_v<T, char>) {
        // assert char separator. return position after separator.
        if(begin >= end || *begin != arg) return nullptr;
        return parse_impl(begin + 1, end, extra...);
    } else if constexpr(std::is_same_v<T, const char*>) {
        // assert string label. return position after label.
        auto arg_len = strlen(arg);
        if(end - begin < static_cast<ptrdiff_t>(arg_len)) return nullptr;
        if(strncmp(begin, arg, arg_len) != 0) return nullptr;
        return parse_impl(begin + arg_len, end, extra...);
    } else if constexpr(std::is_pointer_v<T>) {
        // storing result in tmp var, so we only modify *arg once parsing fully succeeded
        using T_val = std::remove_pointer_t<T>;
        T_val arg_tmp;
        begin = parse_str(begin, end, &arg_tmp);
        if(begin == nullptr) return nullptr;

        begin = parse_impl(begin, end, extra...);
        if(begin == nullptr) return nullptr;

        *arg = std::move(arg_tmp);
        return begin;
    } else {
        static_assert(Env::always_false_v<T>, "expected pointer parameter");
        return nullptr;
    }
}

}  // namespace detail

template <typename S = const char*, typename T, typename... Extra>
bool parse(S str, T arg, Extra... extra)
    requires(std::is_same_v<std::decay_t<S>, std::string> || std::is_same_v<std::decay_t<S>, std::string_view> ||
             std::is_same_v<std::decay_t<S>, const char*>)
{
    const char *begin, *end;

    if constexpr(std::is_same_v<std::decay_t<S>, std::string_view>) {
        begin = str.data();
        end = str.data() + str.size();
    } else if constexpr(std::is_same_v<std::decay_t<S>, std::string>) {
        begin = str.data();
        end = str.data() + str.size();
    } else if constexpr(std::is_same_v<std::decay_t<S>, const char*>) {
        begin = str;
        end = str + strlen(str);
    } else {
        static_assert(Env::always_false_v<S>, "invalid first parameter type");
    }

    return !!detail::parse_impl(begin, end, arg, extra...);
}

// NOLINTEND(cppcoreguidelines-init-variables)

// Since strtok_r SUCKS I'll just make my own
// Returns the token start, and edits str to after the token end (unless '\0').
inline char* strtok_x(char d, char** str) {
    char* old = *str;
    while(**str != '\0' && **str != d) {
        (*str)++;
    }
    if(**str != '\0') {
        **str = '\0';
        (*str)++;
    }
    return old;
}

// this is commonly used in a few places to parse some arbitrary width x height string, might as well make it a function
inline std::optional<ivec2> parse_resolution(std::string_view width_x_height) {
    // don't allow e.g. < 100x100 or > 10000x10000
    if(width_x_height.length() < 7 || width_x_height.length() > 9) {
        return std::nullopt;
    }

    auto resolution = SString::split(width_x_height, 'x');
    if(resolution.size() != 2) {
        return std::nullopt;
    }

    bool good = false;
    i32 width{0}, height{0};
    do {
        {
            auto [ptr, ec] = std::from_chars(resolution[0].data(), resolution[0].data() + resolution[0].size(), width);
            if(ec != std::errc() || width < 320) break;  // 320x240 sanity check
        }
        {
            auto [ptr, ec] = std::from_chars(resolution[1].data(), resolution[1].data() + resolution[1].size(), height);
            if(ec != std::errc() || height < 240) break;
        }
        good = true;
    } while(false);

    if(!good) {
        return std::nullopt;
    }

    // success
    return ivec2{width, height};
}

}  // namespace Parsing
