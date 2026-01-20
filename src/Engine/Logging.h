#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

#include "fmt/format.h"
#include "fmt/compile.h"

using fmt::literals::operator""_cf;

#include <string_view>
#include <cassert>
#include <source_location>

// main logging macro
#define debugLog(str__, ...) Logger::log(std::source_location::current(), str__ __VA_OPT__(, ) __VA_ARGS__)

// log only if condition is true
#define logIf(cond__, str__, ...) (static_cast<bool>(cond__) ? debugLog(str__ __VA_OPT__(, ) __VA_ARGS__) : void(0))

// log only if cvar__.getBool() == true
#define logIfCV(cvar__, str__, ...) logIf(cv::cvar__.getBool(), str__ __VA_OPT__(, ) __VA_ARGS__)

/*
// print the call stack immediately
// TODO: some portable way to do this
#define doBacktrace(...)                                                                                           \
    do {                                                                                                           \
        for(const auto &line : SString::split(fmt::format("{}", fmt::streamed(std::stacktrace::current())), "\n")) \
            Logger::logRaw(line);                                                                                  \
    } while(false);
#include <stacktrace>
#include "fmt/ostream.h"
*/

// main Logger API
namespace Logger {
class ConsoleBoxSink;
namespace _detail {

// copied from spdlog, don't want to include it here
// (since we are using spdlog header-only, to lower compile time, only including spdlog things in 1 translation unit)
namespace log_level {
enum level_enum : int { trace = 0, debug = 1, info = 2, warn = 3, err = 4, critical = 5, off = 6, n_levels };
}

void log_int(const char *filename, unsigned int line, const char *funcname, log_level::level_enum lvl,
             std::string_view str) noexcept;
void logRaw_int(log_level::level_enum lvl, std::string_view str) noexcept;
}  // namespace _detail

// Logger::init() is called immediately after main()
void init(bool create_console) noexcept;
void shutdown() noexcept;

// manual trigger for console commands
void flush() noexcept;

// is stdout a terminal (util func.)
[[nodiscard]] bool isaTTY() noexcept;

// _cf strings
template <typename S, typename... Args>
inline void log(std::source_location loc, S &&fmt, Args &&...args) noexcept
    requires(std::is_base_of_v<fmt::compiled_string, S> && sizeof...(Args) > 0)
{
    _detail::log_int(loc.file_name(), loc.line(), loc.function_name(), _detail::log_level::info,
                     fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...));
}

// fmt strings
template <typename... Args>
inline void log(std::source_location loc, const fmt::format_string<Args...> &fmt, Args &&...args) noexcept
    requires(sizeof...(Args) > 0)
{
    _detail::log_int(loc.file_name(), loc.line(), loc.function_name(), _detail::log_level::info,
                     fmt::format(fmt, std::forward<Args>(args)...));
}

inline void log(std::source_location loc, std::string_view str) noexcept {
    _detail::log_int(loc.file_name(), loc.line(), loc.function_name(), _detail::log_level::info, str);
}

// raw logging without any context

// _cf strings
template <typename S, typename... Args>
inline void logRaw(S &&fmt, Args &&...args) noexcept
    requires(std::is_base_of_v<fmt::compiled_string, S> && sizeof...(Args) > 0)
{
    _detail::logRaw_int(_detail::log_level::info, fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...));
}

// fmt strings
template <typename... Args>
inline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) noexcept
    requires(sizeof...(Args) > 0)
{
    _detail::logRaw_int(_detail::log_level::info, fmt::format(fmt, std::forward<Args>(args)...));
}

// standalone strings
inline void logRaw(std::string_view str) noexcept { _detail::logRaw_int(_detail::log_level::info, str); }

};  // namespace Logger
