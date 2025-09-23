// Copyright (c) 2025, WH, All rights reserved.
#include "Logging.h"

#include "Engine.h"
#include "ConsoleBox.h"
#include "UString.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <io.h>
#else
#include <unistd.h>
#endif

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/pattern_formatter.h"

namespace {  // static namespace
static constexpr const auto DEFAULT_LOGGER_NAME{"main"};
static constexpr const auto RAW_LOGGER_NAME{"raw"};
}  // namespace

// static member definitions
std::shared_ptr<spdlog::async_logger> Logger::s_logger;
std::shared_ptr<spdlog::async_logger> Logger::s_raw_logger;

bool Logger::wasInit{false};

// implementation of ConsoleBoxSink
class Logger::ConsoleBoxSink : public spdlog::sinks::base_sink<std::mutex> {
   private:
    // circular buffer for batching console messages
    // size is a tradeoff for efficiency vs console update latency
    static constexpr size_t CONSOLE_BUFFER_SIZE{64};

    std::array<UString, CONSOLE_BUFFER_SIZE> message_buffer_{};
    size_t buffer_head_{0};   // next write position
    size_t buffer_count_{0};  // current number of messages

    // separate formatters for different logger types
    std::unique_ptr<spdlog::pattern_formatter> main_formatter_{nullptr};
    std::unique_ptr<spdlog::pattern_formatter> raw_formatter_{nullptr};

    // ConsoleBox::log is thread-safe, batched console updates for better performance
    // TODO: implement color
    inline void flush_buffer_to_console() noexcept {
        if(buffer_count_ == 0) return;

        std::shared_ptr<ConsoleBox> cbox{likely(!!engine) ? engine->getConsoleBox() : nullptr};
        if(unlikely(!cbox)) {
            // should only be possible briefly on startup/shutdown
            return;
        }

        // print messages in order (handling wrap-around)
        size_t read_pos{(buffer_head_ + CONSOLE_BUFFER_SIZE - buffer_count_) % CONSOLE_BUFFER_SIZE};
        {
            // hold the lock outside the loop, so we don't continuously acquire and release it for each log call
            std::scoped_lock lock{cbox->logMutex};
            for(size_t i = 0; i < buffer_count_; ++i) {
                cbox->log(message_buffer_[read_pos]);
                read_pos = (read_pos + 1) % CONSOLE_BUFFER_SIZE;
            }
        }
        buffer_count_ = 0;
    }

   public:
    ConsoleBoxSink() noexcept {
        // create separate formatters for different logger types
        // set patterns based on build type
        // also, don't auto-append newlines, each console log is already on a new line
#ifdef _DEBUG
        // debug pattern: [filename:line] [function]: message
        main_formatter_ =
            std::make_unique<spdlog::pattern_formatter>("[%s:%#] [%!]: %v", spdlog::pattern_time_type::local, "");
#else
        // release pattern: [function] message
        main_formatter_ = std::make_unique<spdlog::pattern_formatter>("[%!] %v", spdlog::pattern_time_type::local, "");
#endif
        // raw formatter always uses plain pattern
        raw_formatter_ = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "");
    }

   protected:
    inline void sink_it_(const spdlog::details::log_msg& msg) noexcept override {
        spdlog::memory_buf_t formatted;

        // choose formatter based on logger name
        static_assert(RAW_LOGGER_NAME[0] == 'r');
        if(msg.logger_name[0] == RAW_LOGGER_NAME[0]) {  // raw
            raw_formatter_->format(msg, formatted);
        } else {  // cooked
            main_formatter_->format(msg, formatted);
        }

        // the formatter doesn't append newlines, but the engine console doesn't like having them, so doing this just in case
        auto end_pos{static_cast<int>(formatted.size())};
        while(end_pos > 0 && (formatted[end_pos - 1] == '\r' || formatted[end_pos - 1] == '\n')) {
            --end_pos;
        }

        // store as UString in the circular buffer
        message_buffer_[buffer_head_] = UString{formatted.data(), end_pos};
        buffer_head_ = (buffer_head_ + 1) % CONSOLE_BUFFER_SIZE;

        if(buffer_count_ < CONSOLE_BUFFER_SIZE) {
            ++buffer_count_;
        }

        // flush when buffer is full to avoid losing messages
        if(buffer_count_ == CONSOLE_BUFFER_SIZE) {
            flush_buffer_to_console();
        }
    }
    inline void flush_() noexcept override { flush_buffer_to_console(); }
};

// to be called in main(), for one-time setup/teardown
void Logger::init() noexcept {
    if(wasInit) return;
    wasInit = true;

    // disable output buffering for external dependencies that might use printf/fprintf
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // initialize async thread pool before creating any async loggers
    // queue size: 8192 slots (each ~256 bytes), 1 background thread
    // use overrun_oldest policy for non-blocking behavior
    spdlog::init_thread_pool(8192, 1);

    // create sinks using spdlog's standard colored stdout sink + custom console sink
    auto stdout_sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    auto console_sink{std::make_shared<ConsoleBoxSink>()};

    // set patterns for stdout sinks only (console sink handles its own formatting)
#ifdef _DEBUG
    // debug pattern: [filename:line] [function]: message
    stdout_sink->set_pattern("[%s:%#] [%!]: %v");
#else
    // release pattern: [function] message
    stdout_sink->set_pattern("[%!] %v");
#endif

    // create unformatted stdout sink
    auto raw_stdout_sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};

    // raw logger uses plain pattern (just the message)
    raw_stdout_sink->set_pattern("%v");

    // create main async logger with stdout + shared console sink
    s_logger = std::make_shared<spdlog::async_logger>(
        DEFAULT_LOGGER_NAME, spdlog::sinks_init_list{std::move(stdout_sink), console_sink}, spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    // create raw async logger with separate stdout + shared console sink
    s_raw_logger = std::make_shared<spdlog::async_logger>(
        RAW_LOGGER_NAME, spdlog::sinks_init_list{std::move(raw_stdout_sink), console_sink}, spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    // set to trace level so we print out all messages
    // TODO: add custom log level support (ConVar callback + build type?)
    s_logger->set_level(spdlog::level::trace);
    s_raw_logger->set_level(spdlog::level::trace);

    // for async loggers, flush operations are queued to the background thread
    // disable automatic flushing based on level, let periodic flusher handle it
    s_logger->flush_on(spdlog::level::off);
    s_raw_logger->flush_on(spdlog::level::off);

    // register both loggers so they both get periodic flushing
    spdlog::register_logger(s_logger);
    spdlog::register_logger(s_raw_logger);

    // flush every 500ms
    // console commands will trigger a flush for responsiveness, though
    spdlog::flush_every(std::chrono::milliseconds(500));

    // make the s_logger default (doesn't really matter right now since we're handling it manually, i think, but still)
    spdlog::set_default_logger(s_logger);
};

// spdlog::shutdown() explodes if its called at program exit (by global atexit handler), so we need to manually shut it down
void Logger::shutdown() noexcept {
    if(!wasInit) return;

    s_raw_logger.reset();
    s_logger.reset();

    // spdlog docs recommend calling this on exit
    // for async loggers, this waits for the background thread to finish processing queued messages
    spdlog::shutdown();

    wasInit = false;
}

// extra util function
bool Logger::isaTTY() noexcept {
    static const bool tty_status{isatty(fileno(stdout)) != 0};
    return tty_status;
}
