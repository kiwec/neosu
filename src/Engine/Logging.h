#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

#include "fmt/format.h"
#include "fmt/compile.h"

#include <string_view>
#include <cassert>

#define LOGGER_FUNC __FUNCTION__

#if defined(_MSC_VER) && !defined(_DEBUG)
#define LOGGER_FUNC_TRIMMED Logger::trim_to_last_scope_internal(static_cast<const char *>(LOGGER_FUNC))
#define LOGGER_FUNC_CAPTURED Logger::trim_to_last_scope_internal(func)
#else
#define LOGGER_FUNC_TRIMMED static_cast<const char *>(LOGGER_FUNC)
#define LOGGER_FUNC_CAPTURED func
#endif

// main logging macro
#define debugLog(str__, ...) Logger::log(__FILE__, __LINE__, LOGGER_FUNC_TRIMMED, str__ __VA_OPT__(, ) __VA_ARGS__)

// explicitly capture func = LOGGER_FUNC in lambda, then use this
#define debugLogLambda(str__, ...) \
    Logger::log(__FILE__, __LINE__, LOGGER_FUNC_CAPTURED, str__ __VA_OPT__(, ) __VA_ARGS__)

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
template <typename S, typename... Args>
inline void log(const char *filename, int line, const char *funcname, S &&fmt, Args &&...args) noexcept
    requires(std::is_base_of_v<fmt::compiled_string, S> && sizeof...(Args) > 0)
{
    // checking for wasInit for the unlikely case that we try to log something through here WHILE initializing/uninitializing
    if(likely(_detail::g_initialized))
        _detail::log_int(filename, line, funcname, _detail::log_level::info,
                         fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...));
    else
        printf("%s\n", fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...).c_str());
}

// not sure what template coaxing needs to be done to avoid duplicating these, but the above works for 
// compile time format strings (_cf suffixed), but the below doesn't
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
template <typename S, typename... Args>
inline void logRaw(S &&fmt, Args &&...args) noexcept
    requires(std::is_base_of_v<fmt::compiled_string, S> && sizeof...(Args) > 0)
{
    if(likely(_detail::g_initialized))
        _detail::logRaw_int(_detail::log_level::info, fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...));
    else
        printf("%s\n", fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...).c_str());
}

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

// msvc always adds the full scope to LOGGER_FUNC, which we don't want for non-debug builds
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
