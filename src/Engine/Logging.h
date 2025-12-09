#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <string_view>
#include <cassert>

#if defined(_MSC_VER) && !defined(_DEBUG)
#define FUNC_TRIMMED Logger::trim_to_last_scope_internal(static_cast<const char *>(__FUNCTION__))
#define CAPTURED_FUNC Logger::trim_to_last_scope_internal(func)
#else
#define FUNC_TRIMMED static_cast<const char *>(__FUNCTION__)
#define CAPTURED_FUNC func
#endif

// main logging macro
#define debugLog(str__, ...) Logger::log(__FILE__, __LINE__, FUNC_TRIMMED, str__ __VA_OPT__(, ) __VA_ARGS__)

// explicitly capture func = __FUNCTION__ in lambda, then use this
#define debugLogLambda(str__, ...) Logger::log(__FILE__, __LINE__, CAPTURED_FUNC, str__ __VA_OPT__(, ) __VA_ARGS__)

// log only if condition is true
// (also this arcane-looking type check is just funny)
#define logIf(cond__, str__, ...)                                                                                \
    (void(0 * sizeof(char[!std::is_pointer_v<std::decay_t<decltype(cond__)>> ? 1 : -1])),                        \
     static_cast<bool>(cond__) ? Logger::log(__FILE__, __LINE__, FUNC_TRIMMED, str__ __VA_OPT__(, ) __VA_ARGS__) \
                               : void(0))

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
#define LOGGER_LEVEL_TRACE 0
#define LOGGER_LEVEL_DEBUG 1
#define LOGGER_LEVEL_INFO 2
#define LOGGER_LEVEL_WARN 3
#define LOGGER_LEVEL_ERROR 4
#define LOGGER_LEVEL_CRITICAL 5
#define LOGGER_LEVEL_OFF 6

namespace log_level {
enum level_enum : int {
    trace = LOGGER_LEVEL_TRACE,
    debug = LOGGER_LEVEL_DEBUG,
    info = LOGGER_LEVEL_INFO,
    warn = LOGGER_LEVEL_WARN,
    err = LOGGER_LEVEL_ERROR,
    critical = LOGGER_LEVEL_CRITICAL,
    off = LOGGER_LEVEL_OFF,
    n_levels
};
}

void log_int(const char *filename, int line, const char *funcname, log_level::level_enum lvl,
             std::string_view str) noexcept;
void logRaw_int(log_level::level_enum lvl, std::string_view str) noexcept;

extern bool g_initialized;
}  // namespace _detail

// Logger::init() is called immediately after main()
void init(bool create_console) noexcept;
void shutdown() noexcept;

// manual trigger for console commands
void flush() noexcept;

// is stdout a terminal (util func.)
[[nodiscard]] bool isaTTY() noexcept;

// logging with format strings
template <typename... Args>
inline void log(const char *filename, int line, const char *funcname, const fmt::format_string<Args...> &fmt,
                Args &&...args) noexcept
    requires(sizeof...(Args) > 0)
{
    // checking for wasInit for the unlikely case that we try to log something through here WHILE initializing/uninitializing
    if(likely(_detail::g_initialized))
        _detail::log_int(filename, line, funcname, _detail::log_level::info,
                         fmt::format(fmt, std::forward<Args>(args)...));
    else
        printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
}

// same but for logging strings/literals
inline void log(const char *filename, int line, const char *funcname, std::string_view str) noexcept {
    if(likely(_detail::g_initialized))
        _detail::log_int(filename, line, funcname, _detail::log_level::info, str);
    else
        printf("%.*s\n", static_cast<int>(str.length()), str.data());
}

// raw logging without any context
template <typename... Args>
inline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) noexcept
    requires(sizeof...(Args) > 0)
{
    if(likely(_detail::g_initialized))
        _detail::logRaw_int(_detail::log_level::info, fmt::format(fmt, std::forward<Args>(args)...));
    else
        printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
}

inline void logRaw(std::string_view str) noexcept {
    if(likely(_detail::g_initialized))
        _detail::logRaw_int(_detail::log_level::info, str);
    else
        printf("%.*s\n", static_cast<int>(str.length()), str.data());
}

// msvc always adds the full scope to __FUNCTION__, which we don't want for non-debug builds
#if defined(_MSC_VER) && !defined(_DEBUG)
forceinline const char *trim_to_last_scope_internal(std::string_view str) {
    auto pos = str.rfind("::");
    if(pos != std::string_view::npos) {
        return str.data() + pos + 2;  // +2 to skip "::"
    }
    return str.data();
}
#endif
};  // namespace Logger
