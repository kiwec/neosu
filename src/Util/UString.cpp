// Copyright (c) 2009, 2D Boy & PG & 2025, WH, All rights reserved.
#include "UString.h"

#include "simdutf.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <ranges>
#include <utility>

static constexpr char ESCAPE_CHAR = '\\';

UString::UString(const char16_t *str) {
    if(!str) return;
    this->sUnicode = str;
    updateUtf8();
}

UString::UString(const char16_t *str, int length) {
    if(!str || length <= 0) return;
    this->sUnicode.assign(str, length);
    updateUtf8();
}

UString::UString(const wchar_t *str) {
    if(!str) return;
#if WCHAR_MAX <= 0xFFFF
    this->sUnicode.assign(reinterpret_cast<const char16_t *>(str), std::wcslen(str));
#else
    fromUtf32(reinterpret_cast<const char32_t *>(str), std::wcslen(str));
#endif
    updateUtf8();
}

UString::UString(const wchar_t *str, int length) {
    if(!str || length <= 0) return;
#if WCHAR_MAX <= 0xFFFF
    this->sUnicode.assign(reinterpret_cast<const char16_t *>(str), length);
#else
    fromUtf32(reinterpret_cast<const char32_t *>(str), length);
#endif
    updateUtf8();
}

UString::UString(const char *utf8) {
    if(!utf8) return;
    this->sUtf8.assign(utf8, std::strlen(utf8));
    fromSupposedUtf8(this->sUtf8.data(), this->sUtf8.size());
}

UString::UString(const char *utf8, int length) {
    if(!utf8 || length <= 0) return;
    this->sUtf8.assign(utf8, length);
    fromSupposedUtf8(this->sUtf8.data(), this->sUtf8.size());
}

UString::UString(const std::string &utf8) {
    if(utf8.empty()) return;
    this->sUtf8 = utf8;
    fromSupposedUtf8(this->sUtf8.data(), this->sUtf8.size());
}

UString::UString(std::string_view utf8) {
    if(utf8.empty()) return;
    this->sUtf8 = utf8;
    fromSupposedUtf8(this->sUtf8.data(), this->sUtf8.size());
}

UString &UString::operator=(std::nullptr_t) {
    this->clear();
    return *this;
}

void UString::clear() noexcept {
    this->sUnicode.clear();
    this->sUtf8.clear();
}

UString UString::join(std::span<const UString> strings, std::string_view delim) noexcept {
    if(strings.empty()) return {};

    UString delimStr(delim);
    UString result = strings[0];

    for(size_t i = 1; i < strings.size(); ++i) {
        result += delimStr;
        result += strings[i];
    }

    return result;
}

bool UString::isWhitespaceOnly() const noexcept {
    return std::ranges::all_of(this->sUnicode, [](char16_t c) { return std::iswspace(static_cast<wint_t>(c)) != 0; });
}

size_t UString::numCodepoints() const noexcept {
    return simdutf::count_utf16le(this->sUnicode.data(), this->sUnicode.size());
}

int UString::findChar(char16_t ch, int start, bool respectEscapeChars) const {
    int len = length();
    if(start < 0 || start >= len) return -1;

    bool escaped = false;
    for(int i = start; i < len; i++) {
        if(respectEscapeChars && !escaped && this->sUnicode[i] == ESCAPE_CHAR) {
            escaped = true;
        } else {
            if(!escaped && this->sUnicode[i] == ch) return i;
            escaped = false;
        }
    }

    return -1;
}

int UString::findChar(const UString &str, int start, bool respectEscapeChars) const {
    int len = length();
    int strLen = str.length();
    if(start < 0 || start >= len || strLen == 0) return -1;

    std::vector<bool> charMap(0x10000, false);
    std::vector<char16_t> extendedChars;

    for(int i = 0; i < strLen; i++) {
        char16_t ch = str.sUnicode[i];
        charMap[ch] = true;
    }

    bool escaped = false;
    for(int i = start; i < len; i++) {
        if(respectEscapeChars && !escaped && this->sUnicode[i] == ESCAPE_CHAR) {
            escaped = true;
        } else {
            char16_t ch = this->sUnicode[i];
            const bool found = charMap[ch];

            if(!escaped && found) return i;
            escaped = false;
        }
    }

    return -1;
}

int UString::find(const UString &str, int start) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || strLen == 0 || start > len - strLen) return -1;

    size_t pos = this->sUnicode.find(str.sUnicode, start);
    return (pos != std::u16string::npos) ? static_cast<int>(pos) : -1;
}

int UString::find(const UString &str, int start, int end) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || end > len || start >= end || strLen == 0) return -1;

    if(end < len) {
        auto tempSubstr = this->sUnicode.substr(start, end - start);
        size_t pos = tempSubstr.find(str.sUnicode);
        return (pos != std::u16string::npos) ? static_cast<int>(pos + start) : -1;
    }

    return find(str, start);
}

int UString::findLast(const UString &str, int start) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || strLen == 0 || start > len - strLen) return -1;

    size_t pos = this->sUnicode.rfind(str.sUnicode);
    if(pos != std::u16string::npos && std::cmp_greater_equal(pos, start)) return static_cast<int>(pos);

    return -1;
}

int UString::findLast(const UString &str, int start, int end) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || end > len || start >= end || strLen == 0) return -1;

    int lastPossibleMatch = std::min(end - strLen, len - strLen);
    for(int i = lastPossibleMatch; i >= start; i--) {
        if(std::equal(str.sUnicode.begin(), str.sUnicode.end(), this->sUnicode.begin() + i)) return i;
    }

    return -1;
}

int UString::findIgnoreCase(const UString &str, int start) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || strLen == 0 || start > len - strLen) return -1;

    auto toLower = [](auto c) { return std::towlower(static_cast<wint_t>(c)); };

    auto sourceView = this->sUnicode | std::views::drop(start) | std::views::transform(toLower);
    auto targetView = str.sUnicode | std::views::transform(toLower);

    auto result = std::ranges::search(sourceView, targetView);

    if(!result.empty()) return static_cast<int>(std::distance(sourceView.begin(), result.begin())) + start;

    return -1;
}

int UString::findIgnoreCase(const UString &str, int start, int end) const {
    int strLen = str.length();
    int len = length();
    if(start < 0 || end > len || start >= end || strLen == 0) return -1;

    auto toLower = [](auto c) { return std::towlower(static_cast<wint_t>(c)); };

    auto sourceView =
        this->sUnicode | std::views::drop(start) | std::views::take(end - start) | std::views::transform(toLower);
    auto targetView = str.sUnicode | std::views::transform(toLower);

    auto result = std::ranges::search(sourceView, targetView);

    if(!result.empty()) return static_cast<int>(std::distance(sourceView.begin(), result.begin())) + start;

    return -1;
}

void UString::collapseEscapes() {
    int len = length();
    if(len == 0) return;

    std::u16string result;
    result.reserve(len);

    bool escaped = false;
    for(char16_t ch : this->sUnicode) {
        if(!escaped && ch == ESCAPE_CHAR) {
            escaped = true;
        } else {
            result.push_back(ch);
            escaped = false;
        }
    }

    this->sUnicode = std::move(result);
    updateUtf8();
}

void UString::append(const UString &str) {
    if(str.length() == 0) return;
    size_t oldLength = this->sUnicode.length();
    this->sUnicode.append(str.sUnicode);
    updateUtf8(oldLength);
}

void UString::append(char16_t ch) {
    size_t oldLength = this->sUnicode.length();
    this->sUnicode.push_back(ch);
    updateUtf8(oldLength);
}

void UString::insert(int offset, const UString &str) {
    if(str.length() == 0) return;

    int len = length();
    offset = std::clamp(offset, 0, len);
    this->sUnicode.insert(offset, str.sUnicode);
    updateUtf8();
}

void UString::insert(int offset, char16_t ch) {
    int len = length();
    offset = std::clamp(offset, 0, len);
    this->sUnicode.insert(offset, 1, ch);
    updateUtf8();
}

void UString::erase(int offset, int count) {
    int len = length();
    if(len == 0 || count == 0 || offset >= len) return;

    offset = std::clamp(offset, 0, len);
    count = std::clamp(count, 0, len - offset);

    this->sUnicode.erase(offset, count);
    updateUtf8();
}

UString UString::trim() const {
    int len = length();
    if(len == 0) return {};

    auto isWhitespace = [](char16_t c) { return std::iswspace(static_cast<wint_t>(c)) != 0; };

    auto start = std::ranges::find_if_not(this->sUnicode, isWhitespace);
    if(start == this->sUnicode.end()) return {};

    auto rstart = std::ranges::find_if_not(std::ranges::reverse_view(this->sUnicode), isWhitespace);
    auto end = rstart.base();

    int startPos = static_cast<int>(std::distance(this->sUnicode.begin(), start));
    int length = static_cast<int>(std::distance(start, end));

    return substr(startPos, length);
}

void UString::lowerCase() {
    if(length() == 0) return;

    std::ranges::transform(this->sUnicode, this->sUnicode.begin(),
                           [](char16_t c) { return static_cast<char16_t>(std::towlower(static_cast<wint_t>(c))); });

    updateUtf8();
}

void UString::upperCase() {
    if(length() == 0) return;

    std::ranges::transform(this->sUnicode, this->sUnicode.begin(),
                           [](char16_t c) { return static_cast<char16_t>(std::towupper(static_cast<wint_t>(c))); });

    updateUtf8();
}

UString &UString::operator+=(const UString &ustr) {
    append(ustr);
    return *this;
}

UString UString::operator+(const UString &ustr) const {
    UString result(*this);
    result.append(ustr);
    return result;
}

UString &UString::operator+=(char16_t ch) {
    append(ch);
    return *this;
}

UString UString::operator+(char16_t ch) const {
    UString result(*this);
    result.append(ch);
    return result;
}

UString &UString::operator+=(char ch) {
    append(static_cast<char16_t>(ch));
    return *this;
}

UString UString::operator+(char ch) const {
    UString result(*this);
    result.append(static_cast<char16_t>(ch));
    return result;
}

bool UString::equalsIgnoreCase(const UString &ustr) const {
    if(length() != ustr.length()) return false;
    if(length() == 0 && ustr.length() == 0) return true;

    return std::ranges::equal(this->sUnicode, ustr.sUnicode, [](char16_t a, char16_t b) {
        return std::towlower(static_cast<wint_t>(a)) == std::towlower(static_cast<wint_t>(b));
    });
}

bool UString::lessThanIgnoreCase(const UString &ustr) const {
    auto it1 = this->sUnicode.begin();
    auto it2 = ustr.sUnicode.begin();

    while(it1 != this->sUnicode.end() && it2 != ustr.sUnicode.end()) {
        const auto c1 = std::towlower(static_cast<wint_t>(*it1));
        const auto c2 = std::towlower(static_cast<wint_t>(*it2));
        if(c1 != c2) return c1 < c2;
        ++it1;
        ++it2;
    }

    // if we've reached the end of one string but not the other,
    // the shorter string is lexicographically less
    return it1 == this->sUnicode.end() && it2 != ustr.sUnicode.end();
}

// only to be used in very specific scenarios
std::wstring UString::to_wstring() const noexcept {
#ifdef MCENGINE_PLATFORM_WINDOWS
    return std::wstring{reinterpret_cast<const wchar_t *>(this->sUnicode.data())};
#else
    std::wstring ret;
    size_t utf32Length = simdutf::utf32_length_from_utf16(this->sUnicode.data(), this->sUnicode.length());
    ret.resize_and_overwrite(utf32Length, [&](wchar_t *data, size_t /* size */) -> size_t {
        return simdutf::convert_utf16_to_utf32(this->sUnicode.data(), this->sUnicode.size(),
                                               reinterpret_cast<char32_t *>(data));
    });
    return ret;
#endif
};

#ifdef MCENGINE_PLATFORM_WINDOWS
// "deprecated"
std::wstring_view UString::wstringView() const noexcept {
    return static_cast<std::wstring_view>(reinterpret_cast<const std::wstring &>(this->sUnicode));
}
const wchar_t *UString::wchar_str() const noexcept { return reinterpret_cast<const wchar_t *>(this->sUnicode.data()); }
#endif

void UString::fromUtf32(const char32_t *utf32, size_t char32Length) {
    if(!utf32 || char32Length == 0) return;

    size_t utf16Length = simdutf::utf16_length_from_utf32(utf32, char32Length);
    this->sUnicode.resize_and_overwrite(utf16Length, [&](char16_t *data, size_t /*size*/) -> size_t {
        return simdutf::convert_utf32_to_utf16le(utf32, char32Length, data);
    });
}

void UString::updateUtf8(size_t startUtf16) {
    if(this->sUnicode.empty()) {
        this->sUtf8.clear();
        return;
    }

    if(startUtf16 == 0) {
        // full conversion
        size_t utf8Length = simdutf::utf8_length_from_utf16le(this->sUnicode.data(), this->sUnicode.size());

        this->sUtf8.resize_and_overwrite(utf8Length, [&](char *data, size_t /* size */) -> size_t {
            return simdutf::convert_utf16le_to_utf8(this->sUnicode.data(), this->sUnicode.size(), data);
        });
    } else {
        // partial conversion (append only the new portion)
        // assumes sUtf8 is already valid up to the position corresponding to startUtf16
        const char16_t *src = this->sUnicode.data() + startUtf16;
        const size_t srcLength = this->sUnicode.size() - startUtf16;

        size_t additionalUtf8Length = simdutf::utf8_length_from_utf16le(src, srcLength);
        size_t oldUtf8Length = this->sUtf8.size();

        this->sUtf8.resize_and_overwrite(
            oldUtf8Length + additionalUtf8Length, [&](char *data, size_t /* size */) -> size_t {
                size_t written = simdutf::convert_utf16le_to_utf8(src, srcLength, data + oldUtf8Length);
                return oldUtf8Length + written;
            });
    }
}

// this is only called from specific constructors, so assume that the parameters utf8 == this->sUtf8, char8Length == this->sUtf8.length()
void UString::fromSupposedUtf8(const char *utf8, size_t char8Length) {
    if(!utf8 || !char8Length) {
        this->sUnicode.clear();
        return;
    }

    // detect encoding with BOM support
    size_t bomPrefixBytes = 0;

    // check up to 4 bytes, since a UTF-32 BOM is 4 bytes
    simdutf::encoding_type detected = simdutf::BOM::check_bom(utf8, std::min<size_t>(4, char8Length));

    if(detected != simdutf::encoding_type::unspecified) {
        // remove BOM from conversion
        bomPrefixBytes = simdutf::BOM::bom_byte_size(detected);
        // sanity
        assert(bomPrefixBytes <= char8Length);
    } else {
        // if there was no BOM, autodetect encoding
        // only check the beginning (arbitrary 512 bytes) of the string, don't waste time checking the entire thing
        // we could be reading in an entire file, which might be KILOBYTES
        detected = simdutf::autodetect_encoding(utf8, std::min<size_t>(512, char8Length));
    }

    switch(detected) {
        case simdutf::encoding_type::unspecified:
            // fallthrough, assume UTF-8
        case simdutf::encoding_type::UTF8: {
            const char *src = &(utf8[bomPrefixBytes]);
            const size_t srcLength = char8Length - bomPrefixBytes;
            const size_t utf16Length = simdutf::utf16_length_from_utf8(src, srcLength);
            this->sUnicode.resize_and_overwrite(utf16Length, [&](char16_t *data, size_t /*size*/) -> size_t {
                return simdutf::convert_utf8_to_utf16le(src, srcLength, data);
            });
            break;
        }

        case simdutf::encoding_type::UTF16_LE: {
            // UTF-16LE
            const auto *src = reinterpret_cast<const char16_t *>(&(utf8[bomPrefixBytes]));
            const size_t srcLength = (char8Length - bomPrefixBytes) / 2;
            this->sUnicode.assign(src, srcLength);
            break;
        }

        case simdutf::encoding_type::UTF16_BE: {
            // UTF-16BE
            const auto *src = reinterpret_cast<const char16_t *>(&(utf8[bomPrefixBytes]));
            const size_t srcLength = (char8Length - bomPrefixBytes) / 2;
            this->sUnicode.resize(srcLength);
            // swap to UTF16_LE internal representation
            simdutf::change_endianness_utf16(src, srcLength, this->sUnicode.data());
            break;
        }

        case simdutf::encoding_type::UTF32_LE: {
            // UTF-32LE
            const auto *src = reinterpret_cast<const char32_t *>(&(utf8[bomPrefixBytes]));
            const size_t srcLength = (char8Length - bomPrefixBytes) / 4;
            const size_t utf16Length = simdutf::utf16_length_from_utf32(src, srcLength);
            this->sUnicode.resize_and_overwrite(utf16Length, [&](char16_t *data, size_t /*size*/) -> size_t {
                return simdutf::convert_utf32_to_utf16le(src, srcLength, data);
            });
            break;
        }

        case simdutf::encoding_type::UTF32_BE: {
            // UTF-32BE
            const auto *src = reinterpret_cast<const char32_t *>(&(utf8[bomPrefixBytes]));
            const size_t srcLength = (char8Length - bomPrefixBytes) / 4;
            const size_t utf16Length = simdutf::utf16_length_from_utf32(src, srcLength);
            this->sUnicode.resize_and_overwrite(utf16Length, [&](char16_t *data, size_t /*size*/) -> size_t {
                return simdutf::convert_utf32_to_utf16be(src, srcLength, data);
            });
            // convert from UTF-16BE to UTF-16LE
            simdutf::change_endianness_utf16(this->sUnicode.data(), this->sUnicode.size(), this->sUnicode.data());
            break;
        }

        case simdutf::encoding_type::Latin1:
            /* ... the function might return simdutf::encoding_type::UTF8,
            * simdutf::encoding_type::UTF16_LE, simdutf::encoding_type::UTF16_BE, or
            * simdutf::encoding_type::UTF32_LE.
            */
            std::unreachable();
            break;
    }

    const bool wasAlreadyUTF8 =
        detected == simdutf::encoding_type::UTF8 || detected == simdutf::encoding_type::unspecified;

    // re-convert our malformed (not UTF-8) representation to proper UTF-8
    if(!wasAlreadyUTF8 || bomPrefixBytes > 0) {
        // fast-path, just strip the BOM prefix if it was already UTF-8
        if(wasAlreadyUTF8) {
            this->sUtf8.erase(0, bomPrefixBytes);
        } else {
            // otherwise fully re-convert from our new unicode representation
            updateUtf8();
        }
    }
}
