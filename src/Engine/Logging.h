#pragma once
// Copyright (c) 2025, WH, All rights reserved.

// main logging macro
#define debugLog(...) Logger::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, __VA_ARGS__)

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
        s_logger->log(loc, spdlog::level::info, fmt, std::forward<Args>(args)...);
    }

    // raw logging without any context
    template <typename... Args>
    static forceinline void logRaw(const fmt::format_string<Args...> &fmt, Args &&...args) noexcept {
        s_raw_logger->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
    }

    // same as above but for non-format strings
    template <typename... Args>
    static forceinline void log(const spdlog::source_loc &loc, const std::string &logString) noexcept {
        s_logger->log(loc, spdlog::level::info, logString);
    }

    template <typename... Args>
    static forceinline void logRaw(const std::string &logString) noexcept {
        s_raw_logger->log(spdlog::level::info, logString);
    }

   private:
    // sink for engine console box output
    class ConsoleBoxSink;

    // Logger::init() is called immediately after main(), so these are guaranteed to be available until exiting
    static std::shared_ptr<spdlog::async_logger> s_logger;
    static std::shared_ptr<spdlog::async_logger> s_raw_logger;

    static bool wasInit;
};
