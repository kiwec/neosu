#pragma once
#include "BaseEnvironment.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cassert>

// non-UString-related fast and small string manipulation helpers

namespace SString {

// alphanumeric string comparator that ignores special characters at the start of strings
inline bool alnum_comp(std::string_view a, std::string_view b) {
    int i = 0;
    int j = 0;
    while(i < a.length() && j < b.length()) {
        if(!isalnum((uint8_t)a[i])) {
            i++;
            continue;
        }
        if(!isalnum((uint8_t)b[j])) {
            j++;
            continue;
        }
        auto la = tolower(a[i]);
        auto lb = tolower(b[j]);
        if(la != lb) return la < lb;
        i++;
        j++;
    }
    return false;
}

// std string splitting, for if we don't want to create UStrings everywhere (slow and heavy)
template <typename S = char>
inline std::vector<std::string> split(std::string_view s, S d)
    requires(std::is_same_v<std::decay_t<S>, const char *>) || std::is_same_v<std::decay_t<S>, std::string_view> ||
            (std::is_same_v<std::decay_t<S>, char>)
{
    size_t len = 0;
    if constexpr(std::is_same_v<std::decay_t<S>, const char *>) {
        len = strlen(d);
    } else if constexpr(std::is_same_v<std::decay_t<S>, std::string_view>) {
        len = d.size();
    } else {  // single char
        len = 1;
    }

    std::vector<std::string> r;
    size_t i = 0, j = 0;
    while((j = s.find(d, i)) != s.npos) r.emplace_back(s, i, j - i), i = j + len;
    r.emplace_back(s, i, s.size() - i);
    return r;
};

inline std::string join(const std::vector<std::string> &strings, std::string_view delim = " ") {
    if(strings.empty()) return {};

    std::string result = strings[0];

    for(size_t i = 1; i < strings.size(); ++i) {
        result += delim;
        result += strings[i];
    }

    return result;
};

// in-place whitespace trimming
inline void trim(std::string *str) {
    assert(str && "null string passed to SString::trim()");
    if(str->empty()) return;
    str->erase(0, str->find_first_not_of(" \t\r\n"));
    str->erase(str->find_last_not_of(" \t\r\n") + 1);
}

inline bool contains_ncase(std::string_view haystack, std::string_view needle) {
    return !haystack.empty() && !std::ranges::search(haystack, needle, [](unsigned char ch1, unsigned char ch2) {
                                     return std::tolower(ch1) == std::tolower(ch2);
                                 }).empty();
}

inline bool whitespace_only(std::string_view str) {
    return str.empty() || std::ranges::all_of(str, [](unsigned char c) { return std::isspace(c) != 0; });
}

inline void to_lower(std::string &str) {
    if(str.empty()) return;
    std::ranges::transform(str, str.begin(), [](unsigned char c) { return std::tolower(c); });
}

inline std::string lower(std::string_view str) {
    std::string lstr{str.data(), str.length()};
    if(str.empty()) return lstr;
    to_lower(lstr);
    return lstr;
}

}  // namespace SString
