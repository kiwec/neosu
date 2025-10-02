#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

// TODO: handle log level switching at runtime
// we currently want all logging to be output, so set it to the most verbose level
// otherwise, the SPDLOG_ macros below SPD_LOG_LEVEL_INFO will just do (void)0;
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "spdlog/common.h"
#include "spdlog/async_logger.h"

#include <string_view>
#include <cassert>

#if defined(_MSC_VER) && !defined(_DEBUG)
#define FUNC_TRIMMED Logger::trim_to_last_scope_internal(SPDLOG_FUNCTION)
#define CAPTURED_FUNC Logger::trim_to_last_scope_internal(func)
#else
#define FUNC_TRIMMED SPDLOG_FUNCTION
#define CAPTURED_FUNC func
#endif

// main logging macro
#define debugLog(str__, ...) \
    Logger::log(spdlog::source_loc{__FILE__, __LINE__, FUNC_TRIMMED}, str__ __VA_OPT__(, ) __VA_ARGS__)

// explicitly capture func = __FUNCTION__ in lambda, then use this
#define debugLogLambda(str__, ...) \
    Logger::log(spdlog::source_loc{__FILE__, __LINE__, CAPTURED_FUNC}, str__ __VA_OPT__(, ) __VA_ARGS__)

/*
// print the call stack immediately
// TODO: some portable way to do this
#define doBacktrace(...)                                                                         \
    do {                                                                                         \
        std::vector<std::string> lines{                                                          \
            SString::split(fmt::format("{}", fmt::streamed(std::stacktrace::current())), "\n")}; \
        for(const auto &line : lines) Logger::logRaw(line);                                      \
    } while(false);
#include <stacktrace>
#include "fmt/ostream.h"
*/

// main logger class
class Logger final {
    NOCOPY_NOMOVE(Logger)
   public:
    // entirely static
    Logger() = delete;
    ~Logger() = delete;

    static void init() noexcept;
    static void shutdown() noexcept;

    // manual trigger for console commands
    static void flush() noexcept;

    // is stdout a terminal (util func.)
    [[nodiscard]] static bool isaTTY() noexcept;

    // logging with format strings
    template <typename... Args>
    static inline void log(const spdlog::source_loc &loc, const fmt::format_string<Args...> &fmt,
                           Args &&...args) noexcept
        requires(sizeof...(Args) > 0)
    {
        // checking for wasInit for the unlikely case that we try to log something through here WHILE initializing/uninitializing
        if(likely(s_initialized))
            s_logger->log(loc, spdlog::level::info, fmt, std::forward<Args>(args)...);
        else
            printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
    }

    // same but for logging strings/literals
    static inline void log(const spdlog::source_loc &loc, std::string_view str) noexcept {
        if(likely(s_initialized))
            s_logger->log(loc, spdlog::level::info, str);
        else
            printf("%.*s\n", static_cast<int>(str.length()), str.data());
    }

    // raw logging without any context
    template <typename... Args>
    static inline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) noexcept
        requires(sizeof...(Args) > 0)
    {
        if(likely(s_initialized))
            s_raw_logger->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        else
            printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
    }

    static inline void logRaw(std::string_view str) noexcept {
        if(likely(s_initialized))
            s_raw_logger->log(spdlog::level::info, str);
        else
            printf("%.*s\n", static_cast<int>(str.length()), str.data());
    }

// msvc always adds the full scope to __FUNCTION__, which we don't want for non-debug builds
#if defined(_MSC_VER) && !defined(_DEBUG)
    static forceinline const char *trim_to_last_scope_internal(std::string_view str) {
        auto pos = str.rfind("::");
        if(pos != std::string_view::npos) {
            return str.data() + pos + 2;  // +2 to skip "::"
        }
        return str.data();
    }
#endif

   private:
    // sink for engine console box output
    class ConsoleBoxSink;

    // Logger::init() is called immediately after main(), so these are guaranteed to be available until exiting
    static std::shared_ptr<spdlog::async_logger> s_logger;
    static std::shared_ptr<spdlog::async_logger> s_raw_logger;

    static bool s_initialized;
};
