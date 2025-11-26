// Copyright (c) 2009, 2D Boy & PG & 2025, WH, All rights reserved.
#pragma once
#include <algorithm>
#include <cassert>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "BaseEnvironment.h"  // for Env::cfg (consteval)

#include "fmt/format.h"
#include "fmt/printf.h"
#include "fmt/compile.h"

using fmt::literals::operator""_cf;
using std::string_view_literals::operator""sv;

#ifndef MCENGINE_PLATFORM_WINDOWS
#define delete_if_not_windows = delete
#else
#define delete_if_not_windows
#endif

template <typename T>
class AlignedAllocator {
   public:
    using value_type = T;
    static constexpr std::align_val_t alignment{alignof(char32_t)};

    AlignedAllocator() noexcept = default;
    template <typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U> & /**/) noexcept {}

    [[nodiscard]] T *allocate(std::size_t n) noexcept {
        if(n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            fubar_abort();
        }
        const std::size_t bytes = n * sizeof(T);
        return static_cast<T *>(::operator new(bytes, alignment));
    }

    void deallocate(T *p, std::size_t /**/) noexcept { ::operator delete(p, alignment); }

    template <typename U>
    constexpr bool operator==(const AlignedAllocator<U> & /**/) const noexcept {
        return true;
    }
};

class UString {
   public:
    template <typename... Args>
    [[nodiscard]] static UString format(std::string_view fmt, Args &&...args) noexcept;

    template <typename Range>
        requires std::ranges::range<Range> && std::convertible_to<std::ranges::range_value_t<Range>, UString>
    [[nodiscard]] static UString join(const Range &range, std::string_view delim = " ") noexcept;

    [[nodiscard]] static UString join(std::span<const UString> strings, std::string_view delim = " ") noexcept;

   public:
    // constructors
    constexpr UString() noexcept = default;
    UString(std::nullptr_t) = delete;
    UString(const char16_t *str) noexcept;
    UString(const char16_t *str, int length) noexcept;
    UString(std::u16string_view str) noexcept;
    UString(const wchar_t *str) noexcept;
    UString(const wchar_t *str, int length) noexcept;
    UString(const char *utf8) noexcept;
    UString(const char *utf8, int length) noexcept;
    UString(std::string_view utf8) noexcept;
    UString(const std::string &utf8) noexcept;
    inline constexpr UString(std::string_view utf8, std::u16string_view unicode) noexcept
        : sUnicode(unicode), sUtf8(utf8) {}
#define ULITERAL(str__) \
    UString { str__##sv, u##str__##sv }  // is C++ even powerful enough to do this without macros

    // member functions
    UString(const UString &ustr) noexcept = default;
    UString(UString &&ustr) noexcept = default;
    UString &operator=(const UString &ustr) noexcept = default;
    UString &operator=(UString &&ustr) noexcept = default;
    UString &operator=(std::nullptr_t) noexcept;
    ~UString() noexcept = default;

    // basic operations
    void clear() noexcept;

    // getters
    [[nodiscard]] constexpr int length() const noexcept { return static_cast<int>(this->sUnicode.length()); }
    [[nodiscard]] constexpr int lengthUtf8() const noexcept { return static_cast<int>(this->sUtf8.length()); }
    [[nodiscard]] size_t numCodepoints() const noexcept;
    [[nodiscard]] constexpr std::string_view utf8View() const noexcept { return this->sUtf8; }
    [[nodiscard]] constexpr const char *toUtf8() const noexcept { return this->sUtf8.c_str(); }
    [[nodiscard]] constexpr std::u16string_view u16View() const noexcept { return this->sUnicode; }
    [[nodiscard]] constexpr const char16_t *u16_str() const noexcept { return this->sUnicode.c_str(); }

    [[nodiscard]] std::wstring to_wstring() const noexcept;
    [[nodiscard]] std::wstring_view wstringView() const noexcept delete_if_not_windows;
    [[nodiscard]] const wchar_t *wchar_str() const noexcept delete_if_not_windows;

    // platform-specific string access (for filesystem operations, etc.)
    [[nodiscard]] constexpr const auto *plat_str() const noexcept {
        // this crazy casting->.data() thing is required for MSVC, otherwise its not constexpr
        if constexpr(Env::cfg(OS::WINDOWS))
            return static_cast<std::wstring_view>(reinterpret_cast<const std::wstring &>(this->sUnicode)).data();
        else
            return this->sUtf8.c_str();
    }

    // state queries
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return this->sUnicode.empty(); }
    [[nodiscard]] bool isWhitespaceOnly() const noexcept;

    // string tests
    [[nodiscard]] constexpr bool endsWith(char ch) const noexcept {
        return !this->sUnicode.empty() && this->sUtf8.back() == ch;
    }
    [[nodiscard]] constexpr bool endsWith(char16_t ch) const noexcept {
        return !this->sUnicode.empty() && this->sUnicode.back() == ch;
    }
    [[nodiscard]] constexpr bool endsWith(const UString &suffix) const noexcept {
        if(this->sUnicode.empty()) return false;
        int suffixLen = suffix.length();
        int thisLen = length();
        return suffixLen <= thisLen &&
               std::equal(suffix.sUnicode.begin(), suffix.sUnicode.end(), this->sUnicode.end() - suffixLen);
    }
    [[nodiscard]] constexpr bool startsWith(char ch) const noexcept {
        return !this->sUnicode.empty() && this->sUtf8.front() == ch;
    }
    [[nodiscard]] constexpr bool startsWith(char16_t ch) const noexcept {
        return !this->sUnicode.empty() && this->sUnicode.front() == ch;
    }
    [[nodiscard]] constexpr bool startsWith(const UString &prefix) const noexcept {
        if(this->sUnicode.empty()) return false;
        int prefixLen = prefix.length();
        int thisLen = length();
        return prefixLen <= thisLen &&
               std::equal(prefix.sUnicode.begin(), prefix.sUnicode.end(), this->sUnicode.begin());
    }

    // search functions
    [[nodiscard]] int find(char16_t ch, std::optional<int> startOpt = std::nullopt,
                           std::optional<int> endOpt = std::nullopt, bool respectEscapeChars = false) const noexcept;
    [[nodiscard]] int findFirstOf(const UString &str, int start = 0, bool respectEscapeChars = false) const noexcept;
    [[nodiscard]] int find(const UString &str, std::optional<int> startOpt = std::nullopt,
                           std::optional<int> endOpt = std::nullopt) const noexcept;
    [[nodiscard]] int findLast(const UString &str, std::optional<int> startOpt = std::nullopt,
                               std::optional<int> endOpt = std::nullopt) const noexcept;
    [[nodiscard]] int findIgnoreCase(const UString &str, std::optional<int> startOpt = std::nullopt,
                                     std::optional<int> endOpt = std::nullopt) const noexcept;

    // iterators for range-based for loops
    [[nodiscard]] constexpr auto begin() noexcept { return this->sUnicode.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return this->sUnicode.end(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return this->sUnicode.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return this->sUnicode.end(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept { return this->sUnicode.cbegin(); }
    [[nodiscard]] constexpr auto cend() const noexcept { return this->sUnicode.cend(); }

    // modifiers
    void collapseEscapes() noexcept;
    void append(const UString &str) noexcept;
    void append(char16_t ch) noexcept;
    void insert(int offset, const UString &str) noexcept;
    void insert(int offset, char16_t ch) noexcept;

    void erase(int offset, int count) noexcept;

    inline constexpr const char16_t &front() noexcept {
        assert(!this->isEmpty());
        return operator[](0);
    }
    inline constexpr const char16_t &back() noexcept {
        assert(!this->isEmpty());
        return operator[](this->length() - 1);
    }
    inline void pop_back() noexcept {
        if(!this->isEmpty()) this->erase(this->length() - 1, 1);
    }
    inline void pop_front() noexcept {
        if(!this->isEmpty()) this->erase(0, 1);
    }

    // actions (non-modifying)
    template <typename T = UString>
    [[nodiscard]] constexpr T substr(int offset, int charCount = -1) const noexcept {
        int len = length();
        offset = std::clamp<int>(offset, 0, len);

        if(charCount < 0) charCount = len - offset;
        charCount = std::clamp<int>(charCount, 0, len - offset);

        UString result;
        result.sUnicode = this->sUnicode.substr(offset, charCount);
        result.updateUtf8();

        if constexpr(std::is_same_v<T, UString>)
            return result;
        else
            return result.to<T>();
    }

    template <typename T = UString>
    [[nodiscard]] std::vector<T> split(const UString &delim) const noexcept {
        std::vector<T> results;
        int delimLen = delim.length();
        int thisLen = length();
        if(delimLen < 1 || thisLen < 1) return results;

        int start = 0;
        int end = 0;

        while((end = find(delim, start)) != -1) {
            results.push_back(substr<T>(start, end - start));
            start = end + delimLen;
        }
        results.push_back(substr<T>(start));

        return results;
    }

    [[nodiscard]] UString trim() const noexcept;

    // type conversions
    template <typename T>
    [[nodiscard]] constexpr T to() const noexcept {
        if(this->sUtf8.empty()) return T{};

        if constexpr(std::is_same_v<T, UString>)
            return *this;
        else if constexpr(std::is_same_v<T, std::string>)
            return std::string{this->sUtf8};
        else if constexpr(std::is_same_v<T, std::string_view>)
            return std::string_view{this->sUtf8};
        else if constexpr(std::is_same_v<T, std::u16string>)
            return std::u16string{this->sUnicode};
        else if constexpr(std::is_same_v<T, std::u16string_view>)
            return std::u16string_view{this->sUnicode};
        else if constexpr(std::is_same_v<T, float>)
            return std::strtof(this->sUtf8.c_str(), nullptr);
        else if constexpr(std::is_same_v<T, double>)
            return std::strtod(this->sUtf8.c_str(), nullptr);
        else if constexpr(std::is_same_v<T, long double>)
            return std::strtold(this->sUtf8.c_str(), nullptr);
        else if constexpr(std::is_same_v<T, int>)
            return static_cast<int>((std::strtol)(this->sUtf8.c_str(), nullptr, 0));
        else if constexpr(std::is_same_v<T, bool>)
            return !!static_cast<int>((std::strtol)(this->sUtf8.c_str(), nullptr, 0));
        else if constexpr(std::is_same_v<T, long>)
            return (std::strtol)(this->sUtf8.c_str(), nullptr, 0);
        else if constexpr(std::is_same_v<T, long long>)
            return (std::strtoll)(this->sUtf8.c_str(), nullptr, 0);
        else if constexpr(std::is_same_v<T, unsigned int>)
            return static_cast<unsigned int>((std::strtoul)(this->sUtf8.c_str(), nullptr, 0));
        else if constexpr(std::is_same_v<T, unsigned long>)
            return (std::strtoul)(this->sUtf8.c_str(), nullptr, 0);
        else if constexpr(std::is_same_v<T, unsigned long long>)
            return (std::strtoull)(this->sUtf8.c_str(), nullptr, 0);
        else
            static_assert(Env::always_false_v<T>, "unsupported type");
    }

    // conversion shortcuts
    [[nodiscard]] constexpr float toFloat() const noexcept { return to<float>(); }
    [[nodiscard]] constexpr double toDouble() const noexcept { return to<double>(); }
    [[nodiscard]] constexpr long double toLongDouble() const noexcept { return to<long double>(); }
    [[nodiscard]] constexpr int toInt() const noexcept { return to<int>(); }
    [[nodiscard]] constexpr bool toBool() const noexcept { return to<bool>(); }
    [[nodiscard]] constexpr long toLong() const noexcept { return to<long>(); }
    [[nodiscard]] constexpr long long toLongLong() const noexcept { return to<long long>(); }
    [[nodiscard]] constexpr unsigned int toUnsignedInt() const noexcept { return to<unsigned int>(); }
    [[nodiscard]] constexpr unsigned long toUnsignedLong() const noexcept { return to<unsigned long>(); }
    [[nodiscard]] constexpr unsigned long long toUnsignedLongLong() const noexcept { return to<unsigned long long>(); }

    // case conversion
    void lowerCase() noexcept;
    void upperCase() noexcept;

    // operators
    [[nodiscard]] constexpr const char16_t &operator[](int index) const noexcept {
        int len = length();
        return this->sUnicode[std::clamp(index, 0, len - 1)];
    }

    bool operator==(const char *utf8) const noexcept { return this->sUtf8 == utf8; }
    auto operator<=>(const char *utf8) const noexcept { return std::operator<=>(this->sUtf8, utf8); }
    bool operator==(const UString &ustr) const noexcept { return this->sUnicode == ustr.sUnicode; };
    auto operator<=>(const UString &ustr) const noexcept { return std::operator<=>(this->sUnicode, ustr.sUnicode); };

    UString &operator+=(const UString &ustr) noexcept;
    [[nodiscard]] UString operator+(const UString &ustr) const noexcept;
    UString &operator+=(char16_t ch) noexcept;
    [[nodiscard]] UString operator+(char16_t ch) const noexcept;
    UString &operator+=(char ch) noexcept;
    [[nodiscard]] UString operator+(char ch) const noexcept;

    [[nodiscard]] bool equalsIgnoreCase(const UString &ustr) const noexcept;
    [[nodiscard]] bool lessThanIgnoreCase(const UString &ustr) const noexcept;

    friend struct std::hash<UString>;

   private:
    using alignedUTF8String = std::basic_string<char, std::char_traits<char>, AlignedAllocator<char>>;

    // deduplication helper
    [[nodiscard]] int findCharSimd(char16_t ch, int start, int end) const noexcept;

    // constructor helpers
    void fromUtf32(const char32_t *utf32, size_t length) noexcept;
    void fromSupposedUtf8(const char *utf8, size_t length) noexcept;

    // for updating utf8 representation when unicode representation changes
    void updateUtf8(size_t startUtf16 = 0) noexcept;

    std::u16string sUnicode;
    alignedUTF8String sUtf8;
};

namespace std {
template <>
struct hash<UString> {
    size_t operator()(const UString &str) const noexcept { return hash<std::u16string>()(str.sUnicode); }
};
}  // namespace std

// for printf-style formatting (legacy McEngine style, should convert over to fmt::format because it's nicer)
template <typename... Args>
UString UString::format(std::string_view fmt, Args &&...args) noexcept {
    return UString(fmt::sprintf(fmt, std::forward<Args>(args)...));
}

// forward decls to avoid including simdutf here
namespace simdutf {
extern size_t utf8_length_from_utf16le(const char16_t *input, size_t length) noexcept;
extern size_t convert_utf16le_to_utf8(const char16_t *input, size_t length, char *utf8_output) noexcept;
}  // namespace simdutf

// need a specialization for fmt, so that UStrings can be passed directly without needing .toUtf8() (for fmt::format)
namespace fmt {
template <>
struct formatter<UString> : formatter<string_view> {
    template <typename FormatContext>
    auto format(const UString &str, FormatContext &ctx) const noexcept {
        return formatter<string_view>::format(str.utf8View(), ctx);
    }
};

// u16string_view support
template <>
struct formatter<std::u16string_view> : formatter<string_view> {
    template <typename FormatContext>
    auto format(std::u16string_view str, FormatContext &ctx) const noexcept {
        size_t utf8_length = simdutf::utf8_length_from_utf16le(str.data(), str.size());
        std::string result;
        result.resize_and_overwrite(utf8_length, [&](char *data, size_t /* size */) -> size_t {
            return simdutf::convert_utf16le_to_utf8(str.data(), str.size(), data);
        });
        return formatter<string_view>::format(result, ctx);
    }
};
}  // namespace fmt

template <typename Range>
    requires std::ranges::range<Range> && std::convertible_to<std::ranges::range_value_t<Range>, UString>
UString UString::join(const Range &range, std::string_view delim) noexcept {
    if(std::ranges::empty(range)) return {};

    UString delimStr(delim);
    auto it = std::ranges::begin(range);
    UString result = *it;
    ++it;

    for(; it != std::ranges::end(range); ++it) {
        result += delimStr;
        result += *it;
    }

    return result;
}

#undef delete_if_not_windows
