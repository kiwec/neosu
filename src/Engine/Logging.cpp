// Copyright (c) 2025, WH, All rights reserved.
#include "Logging.h"

#include "Engine.h"
#include "ConsoleBox.h"
#include "Thread.h"
#include "Environment.h"
#include "UString.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <io.h>
#else
#include <unistd.h>
#endif

// TODO: handle log level switching at runtime
// we currently want all logging to be output, so set it to the most verbose level
// otherwise, the SPDLOG_ macros below SPD_LOG_LEVEL_INFO will just do (void)0;
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "spdlog/common.h"
#include "spdlog/async_logger.h"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/pattern_formatter.h"

#define DEFAULT_LOGGER_NAME "main"
#define RAW_LOGGER_NAME "raw"

#ifdef _DEBUG
// debug pattern: [filename:line] [function]: message
#define FANCY_LOG_PATTERN "[%s:%#] [%!]: %v"
// for file output, add timestamp (and thread, for debug) info
#define FILE_LOG_PATTERN_PREF "[%T.%e] [th:%t]"
#define RELEASE_IDENTIFIER "dev"
#else
// release pattern: [function] message
#define FANCY_LOG_PATTERN "[%!] %v"
// add HH:MM:SS timestamp
#define FILE_LOG_PATTERN_PREF "[%T]"
#define RELEASE_IDENTIFIER "rel"
#endif

// same as release build stdout logging, don't clutter engine logs with source info and stuff
#define ENGINE_LOG_PATTERN "[%!] %v"

// e.g. ./logs/
#define LOGFILE_LOCATION MCENGINE_DATA_DIR "logs/"
// e.g. ./logs/neosu-linux-x64-dev-40.03.log
#define LOGFILE_NAME LOGFILE_LOCATION PACKAGE_NAME "-" OS_NAME "-" RELEASE_IDENTIFIER "-" PACKAGE_VERSION ".log"

namespace Logger {
namespace {  // static
std::shared_ptr<spdlog::async_logger> g_logger;
std::shared_ptr<spdlog::async_logger> g_raw_logger;

#ifdef MCENGINE_PLATFORM_WINDOWS
bool s_created_console{false};
#endif

// workaround for really odd internal template decisions in spdlog...
struct custom_spdmtx : public Sync::mutex {
    using mutex_t = Sync::mutex;
    static mutex_t &mutex() {
        static mutex_t s_mutex;
        return s_mutex;
    }
};

}  // namespace

namespace _detail {
// global var defs
bool g_initialized{false};

void log_int(const char *filename, int line, const char *funcname, log_level::level_enum lvl,
             std::string_view str) noexcept {
    return g_logger->log(spdlog::source_loc{filename, line, funcname}, (spdlog::level::level_enum)lvl, str);
}

void logRaw_int(log_level::level_enum lvl, std::string_view str) noexcept {
    return g_raw_logger->log((spdlog::level::level_enum)lvl, str);
}

}  // namespace _detail
using namespace _detail;

// implementation of ConsoleBoxSink
class ConsoleBoxSink final : public spdlog::sinks::base_sink<custom_spdmtx> {
   private:
    // circular buffer for batching console messages
    // size is a tradeoff for efficiency vs console update latency
    static constexpr size_t CONSOLE_BUFFER_SIZE{64};

    std::array<UString, CONSOLE_BUFFER_SIZE> message_buffer_{};
    size_t buffer_head_{0};   // next write position
    size_t buffer_count_{0};  // current number of messages

    // separate formatters for different logger types
    // the base class has a "formatter_" member which we can use as the main formatter
    std::unique_ptr<spdlog::pattern_formatter> raw_formatter_{nullptr};

    // ConsoleBox::log is thread-safe, batched console updates for better performance
    // TODO: implement color
    inline void flush_buffer_to_console() noexcept {
        if(buffer_count_ == 0) return;

        std::shared_ptr<ConsoleBox> cbox{Engine::getConsoleBox()};
        if(unlikely(!cbox)) {
            // should only be possible briefly on startup/shutdown
            return;
        }

        // print messages in order (handling wrap-around)
        size_t read_pos{(buffer_head_ + CONSOLE_BUFFER_SIZE - buffer_count_) % CONSOLE_BUFFER_SIZE};
        {
            // hold the lock outside the loop, so we don't continuously acquire and release it for each log call
            Sync::unique_lock lock(cbox->logMutex);
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
        // also, don't auto-append newlines, each console log is already on a new line
        base_sink::formatter_ =
            std::make_unique<spdlog::pattern_formatter>(ENGINE_LOG_PATTERN, spdlog::pattern_time_type::local, "");

        // raw formatter always uses plain pattern
        raw_formatter_ = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "");
    }

   protected:
    inline void sink_it_(const spdlog::details::log_msg &msg) noexcept override {
        spdlog::memory_buf_t formatted;

        // choose formatter based on logger name
        static_assert(RAW_LOGGER_NAME[0] == 'r');
        if(msg.logger_name[0] == RAW_LOGGER_NAME[0]) {  // raw
            raw_formatter_->format(msg, formatted);
        } else {  // cooked
            base_sink::formatter_->format(msg, formatted);
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

namespace {  // static
// with basic_file_sink, it seems that multiple different sinks to the same file aren't properly synchronized (on Linux at least)
// so the pattern of using 2 loggers and 2 sinks doesn't quite work
// not a huge issue, we can just do a similar thing to ConsoleBoxSink to use a different formatter based on the logger name,
// and register it with both loggers

// annoyingly, though, basic_file_sink is marked final, so it can't be overridden (we just have to reimplement it entirely :/)
class DualPatternFileSink final : public spdlog::sinks::base_sink<custom_spdmtx> {
   private:
    spdlog::details::file_helper file_helper_;

    // separate formatters for different logger types (same as consolebox)
    std::unique_ptr<spdlog::pattern_formatter> raw_formatter_{nullptr};

   public:
    explicit DualPatternFileSink(const spdlog::filename_t &filename, bool truncate = false,
                                 const spdlog::file_event_handlers &event_handlers = {}) noexcept
        : file_helper_{event_handlers} {
        // do both the prefix and the fancy log pattern
        base_sink::formatter_ =
            std::make_unique<spdlog::pattern_formatter>(FILE_LOG_PATTERN_PREF " " FANCY_LOG_PATTERN);
        // plain after the prefix
        raw_formatter_ = std::make_unique<spdlog::pattern_formatter>(FILE_LOG_PATTERN_PREF " %v");

        file_helper_.open(filename, truncate);
    }

    // the rest of this implementation is basically copied from spdlog's basic_file_sink

    [[nodiscard]] inline const spdlog::filename_t &filename() const noexcept { return file_helper_.filename(); }

    inline void truncate() {
        Sync::lock_guard lock(base_sink::mutex_);
        file_helper_.reopen(true);
    }

   protected:
    inline void sink_it_(const spdlog::details::log_msg &msg) noexcept override {
        spdlog::memory_buf_t formatted;

        static_assert(RAW_LOGGER_NAME[0] == 'r');
        if(msg.logger_name[0] == RAW_LOGGER_NAME[0]) {  // raw
            raw_formatter_->format(msg, formatted);
        } else {  // cooked
            base_sink::formatter_->format(msg, formatted);
        }

        file_helper_.write(formatted);
    }

    inline void flush_() override { file_helper_.flush(); }
};

}  // namespace

// to be called in main(), for one-time setup/teardown
void init(bool create_console) noexcept {
    if(g_initialized) return;

#ifdef MCENGINE_PLATFORM_WINDOWS
    // when the spdlog::sinks::wincolor_stdout_sink is created, it checks GetStdHandle(STD_OUTPUT_HANDLE) at initialization
    // so, create a console, such that GetStdHandle(STD_OUTPUT_HANDLE) returns a handle to it
    // this might be desirable on release builds, which are linked against the "windows" subsystem
    // (which don't create a console when opening the app)
    if(create_console && !isaTTY() /* don't create console if we're already in one */) {
        // allocate a new console window
        if((s_created_console = AllocConsole())) {
            // redirect stdout/stderr to the new console
            // using freopen is the simplest approach that works with both C and C++ streams
            FILE *fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);

            SetConsoleTitleW(L"" PACKAGE_NAME L" " PACKAGE_VERSION L" console output");

            SetConsoleOutputCP(65001 /*CP_UTF8*/);
        }
    }
#else
    (void)create_console;  // it's not as big of a commotion on platforms outside of windows
#endif

    // make console output visible immediately
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // initialize async thread pool before creating any async loggers
    // queue size: 8192 slots (each ~256 bytes), 1 background thread
    // use overrun_oldest policy for non-blocking behavior
    spdlog::init_thread_pool(8192, 1, []() -> void { McThread::set_current_thread_name(ULITERAL("spd_logger")); });

    // engine console sink handles its own formatting
    auto engine_console_sink{std::make_shared<ConsoleBoxSink>()};

    // default debugLog sink
#ifdef MCENGINE_PLATFORM_WINDOWS
    using mt_stdout_sink_t = spdlog::sinks::wincolor_stdout_sink<custom_spdmtx>;
#else
    using mt_stdout_sink_t = spdlog::sinks::ansicolor_stdout_sink<custom_spdmtx>;
#endif
    auto stdout_sink{std::make_shared<mt_stdout_sink_t>()};
    stdout_sink->set_pattern(FANCY_LOG_PATTERN);

    // unformatted stdout sink
    auto raw_stdout_sink{std::make_shared<mt_stdout_sink_t>()};
    raw_stdout_sink->set_pattern("%v");  // just the message

    // prepare sink lists for loggers
    std::vector<spdlog::sink_ptr> main_sinks{std::move(stdout_sink), engine_console_sink};
    std::vector<spdlog::sink_ptr> raw_sinks{std::move(raw_stdout_sink), engine_console_sink};

    // if Environment::createDirectory failed, it means we definitely won't be able to write to anything in there
    // (returns true if it already exists)
    const bool log_to_file{Environment::createDirectory(LOGFILE_LOCATION)};

    // add file sinks if directory is writable
    if(log_to_file) {
        // similar to the ConsoleBoxSink, the logger source determines the output pattern
        auto file_sink{std::make_shared<DualPatternFileSink>(LOGFILE_NAME, true /* overwrite */)};

        main_sinks.push_back(file_sink);
        raw_sinks.push_back(file_sink);
    }

    // create main async logger with stdout + console + optional file sink
    g_logger =
        std::make_shared<spdlog::async_logger>(DEFAULT_LOGGER_NAME, main_sinks.begin(), main_sinks.end(),
                                               spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);

    // create raw async logger with separate stdout + console + optional file sink
    g_raw_logger =
        std::make_shared<spdlog::async_logger>(RAW_LOGGER_NAME, raw_sinks.begin(), raw_sinks.end(),
                                               spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);

    // set to trace level so we print out all messages
    // TODO: add custom log level support (ConVar callback + build type?)
    g_logger->set_level(spdlog::level::trace);
    g_raw_logger->set_level(spdlog::level::trace);

    // for async loggers, flush operations are queued to the background thread
    // disable automatic flushing based on level, let periodic flusher handle it
    g_logger->flush_on(spdlog::level::off);
    g_raw_logger->flush_on(spdlog::level::off);

    // register both loggers so they both get periodic flushing
    spdlog::register_logger(g_logger);
    spdlog::register_logger(g_raw_logger);

    // flush every 500ms
    // console commands will trigger a flush for responsiveness, though
    spdlog::flush_every(std::chrono::milliseconds(500));

    // make the s_logger default (doesn't really matter right now since we're handling it manually, i think, but still)
    spdlog::set_default_logger(g_logger);

    g_initialized = true;
};

// spdlog::shutdown() explodes if its called at program exit (by global atexit handler), so we need to manually shut it down
void shutdown() noexcept {
    if(!g_initialized) return;
    flush();
    g_initialized = false;

    g_raw_logger.reset();
    g_logger.reset();

    // spdlog docs recommend calling this on exit
    // for async loggers, this waits for the background thread to finish processing queued messages
    spdlog::shutdown();

#ifdef MCENGINE_PLATFORM_WINDOWS
    if(s_created_console) {
        FreeConsole();
        s_created_console = false;
    }
#endif
}

// manual trigger for console commands
void flush() noexcept {
    if(likely(g_initialized)) {
        g_logger->flush();
        g_raw_logger->flush();
    } else {
        fflush(stdout);
        fflush(stderr);
    }
}

// extra util functions
bool isaTTY() noexcept {
    static const bool tty_status{isatty(fileno(stdout)) != 0};
    return tty_status;
}
}  // namespace Logger
