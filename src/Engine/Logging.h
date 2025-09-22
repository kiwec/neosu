#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

#include "Color.h"

#include "fmt/format.h"
#include "fmt/compile.h"
#include "fmt/color.h"

#include <source_location>
#include <string>
#include <optional>

// fmt::print seems to crash on windows with no console allocated (at least with mingw)
// just use printf to be safe in that case
#if defined(_WIN32) && !defined(_DEBUG)
#define FMT_PRINT(...) printf("%s", fmt::format(__VA_ARGS__).c_str())
#else
#define FMT_PRINT(...) fmt::print(__VA_ARGS__)
#endif

#ifdef _DEBUG
#define debugLog(...) Logger::log(std::source_location::current(), __FUNCTION__, __VA_ARGS__)
#else
#define debugLog(...) Logger::log(__FUNCTION__, __VA_ARGS__)
#endif

using fmt::literals::operator""_cf;

namespace fmt {
template <std::size_t N>
struct formatter<std::array<char, N>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) const {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::array<char, N> &array, FormatContext &ctx) const {
        return format_to(ctx.out(), "{:s}", array.data());
    }
};
}

namespace Logger {

[[nodiscard]] bool isaTTY();  // is stdout a terminal

namespace detail {
// logging stuff (implementation)
void logToConsole(std::optional<Color> color, const std::string &message);

static forceinline void trim_to_last_scope([[maybe_unused]] std::string &str) {
#ifdef _MSC_VER  // msvc always adds the full scope to __FUNCTION__, which we don't want for non-debug builds
    auto pos = str.rfind("::");
    if(pos != std::string::npos) {
        str.erase(1, pos + 1);
    }
#endif
}

static forceinline void logImpl(const std::string &message, Color color = rgb(255, 255, 255)) {
    if(color == rgb(255, 255, 255) || !Logger::isaTTY())
        FMT_PRINT("{}"_cf, message);
    else
        FMT_PRINT(fmt::fg(fmt::rgb(color.R(), color.G(), color.B())), "{}", message);
    logToConsole(color, message);
}

// reimplementing Environment::getFileNameFromFilePath to avoid transitive include
static forceinline std::string getFilenameFromPath(const std::string &path) noexcept {
    auto lastSlash = path.find_last_of("/\\");
    if(lastSlash == std::string::npos) return path;

    return path.substr(lastSlash + 1);
}

}  // namespace detail

// debug build shows full source location
#ifdef _DEBUG
template <typename... Args>
inline void log(const std::source_location &loc, const char *func, const fmt::format_string<Args...> &fmt,
                Args &&...args) {
    auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: "_cf, detail::getFilenameFromPath(loc.file_name()), loc.line(),
                                     loc.column(), func);

    auto message = fmt::format(fmt, std::forward<Args>(args)...);
    detail::logImpl(contextPrefix + message);
}

template <typename... Args>
inline void log(const std::source_location &loc, const char *func, Color color, const fmt::format_string<Args...> &fmt,
                Args &&...args) {
    auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: "_cf, detail::getFilenameFromPath(loc.file_name()), loc.line(),
                                     loc.column(), func);

    auto message = fmt::format(fmt, std::forward<Args>(args)...);
    detail::logImpl(contextPrefix + message, color);
}
#else
// release build only shows function name
template <typename... Args>
inline void log(const char *func, const fmt::format_string<Args...> &fmt, Args &&...args) {
    auto contextPrefix = fmt::format("[{}] "_cf, func);
    detail::trim_to_last_scope(contextPrefix);
    auto message = fmt::format(fmt, std::forward<Args>(args)...);
    detail::logImpl(contextPrefix + message);
}

template <typename... Args>
inline void log(const char *func, Color color, const fmt::format_string<Args...> &fmt, Args &&...args) {
    auto contextPrefix = fmt::format("[{}] "_cf, func);
    detail::trim_to_last_scope(contextPrefix);
    auto message = fmt::format(fmt, std::forward<Args>(args)...);
    detail::logImpl(contextPrefix + message, color);
}
#endif

template <typename... Args>
inline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) {
    auto message = fmt::format(fmt, std::forward<Args>(args)...);
    detail::logImpl(message);
}

}  // namespace Logger
