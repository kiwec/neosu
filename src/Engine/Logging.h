#pragma once
// Copyright (c) 2025, WH, All rights reserved.

// main logging macro
#if defined(_MSC_VER) && !defined(_DEBUG)
#define debugLog(...)                                                                                         \
    Logger::log(spdlog::source_loc{__FILE__, __LINE__, Logger::trim_to_last_scope_internal(SPDLOG_FUNCTION)}, \
                __VA_ARGS__)
#else
#define debugLog(...) Logger::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, __VA_ARGS__)
#endif

#include "BaseEnvironment.h"

// TODO: handle log level switching at runtime
// we currently want all logging to be output, so set it to the most verbose level
// otherwise, the SPDLOG_ macros below SPD_LOG_LEVEL_INFO will just do (void)0;
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "spdlog/common.h"
#include "spdlog/async_logger.h"

#include <string>
#include <cassert>

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
    static inline void flush() noexcept {
        assert(wasInit);
        s_logger->flush();
        s_raw_logger->flush();
    }

    // is stdout a terminal (util func.)
    [[nodiscard]] static bool isaTTY() noexcept;

    // logging with context
    template <typename... Args>
    static forceinline void log(const spdlog::source_loc &loc, const fmt::format_string<Args...> &fmt,
                                Args &&...args) noexcept {
        // checking for wasInit for the unlikely case that we try to log something through here WHILE initializing/uninitializing
        if(likely(wasInit))
            s_logger->log(loc, spdlog::level::info, fmt, std::forward<Args>(args)...);
        else
            printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
    }

    // raw logging without any context
    template <typename... Args>
    static forceinline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) noexcept {
        if(likely(wasInit))
            s_raw_logger->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        else
            printf("%s\n", fmt::format(fmt, std::forward<Args>(args)...).c_str());
    }

    // same as above but for non-format strings
    template <typename... Args>
    static forceinline void log(const spdlog::source_loc &loc, const std::string &logString) noexcept {
        if(likely(wasInit))
            s_logger->log(loc, spdlog::level::info, logString);
        else
            printf("%s\n", logString.c_str());
    }

    template <typename... Args>
    static forceinline void logRaw(const std::string &logString) noexcept {
        if(likely(wasInit))
            s_raw_logger->log(spdlog::level::info, logString);
        else
            printf("%s\n", logString.c_str());
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

    static bool wasInit;
};
