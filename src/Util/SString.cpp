// Copyright (c) 2023-2024, kiwec & 2025, WH, All rights reserved.

#include "SString.h"

#include <cstring>

namespace SString {
// alphanumeric string comparator that ignores special characters at the start of strings
bool alnum_comp(std::string_view a, std::string_view b) {
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

// alphanumeric string comparator that ignores special characters at the start of strings
template <typename R, typename S, split_ret_enabled_t<R>, split_join_enabled_t<S>>
std::vector<R> split(std::string_view s, S delim) {
    size_t len = 0;
    if constexpr(std::is_same_v<std::decay_t<S>, const char*>) {
        len = strlen(delim);
    } else if constexpr(std::is_same_v<std::decay_t<S>, std::string_view>) {
        len = delim.size();
    } else {  // single char
        len = 1;
    }

    std::vector<R> r;
    size_t i = 0, j = 0;
    if constexpr(std::is_same_v<std::decay_t<R>, std::string>) {
        while((j = s.find(delim, i)) != s.npos) r.emplace_back(s, i, j - i), i = j + len;
        r.emplace_back(s, i, s.size() - i);
    } else {  // string_view
        while((j = s.find(delim, i)) != s.npos) r.emplace_back(s.substr(i, j - i)), i = j + len;
        r.emplace_back(s.substr(i));
    }

    return r;
}

// explicit instantiations
template std::vector<std::string> split<std::string, char>(std::string_view, char);
template std::vector<std::string> split<std::string, const char*>(std::string_view, const char*);
template std::vector<std::string> split<std::string, std::string_view>(std::string_view, std::string_view);

template std::vector<std::string_view> split<std::string_view, char>(std::string_view, char);
template std::vector<std::string_view> split<std::string_view, const char*>(std::string_view, const char*);
template std::vector<std::string_view> split<std::string_view, std::string_view>(std::string_view, std::string_view);

template <typename S, split_join_enabled_t<S>>
std::string join(const std::vector<std::string>& strings, S delim) {
    if(strings.empty()) return {};

    std::string result = strings[0];

    for(size_t i = 1; i < strings.size(); ++i) {
        result += delim;
        result += strings[i];
    }

    return result;
}

// explicit instantiations
template std::string join<char>(const std::vector<std::string>&, char);
template std::string join<const char*>(const std::vector<std::string>&, const char*);
template std::string join<std::string_view>(const std::vector<std::string>&, std::string_view);

}  // namespace SString
