//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once

//
// Global registry functions
//
#include <lquantlog/lquantlog.h>
#include <lquantlog/details/registry.h>
#include <lquantlog/sinks/file_sinks.h>
#include <lquantlog/sinks/stdout_sinks.h>
#ifdef SPDLOG_ENABLE_SYSLOG
#include <lquantlog/sinks/syslog_sink.h>
#endif

#ifdef _WIN32
#include <lquantlog/sinks/wincolor_sink.h>
#else
#include <lquantlog/sinks/ansicolor_sink.h>
#endif


#ifdef __ANDROID__
#include <lquantlog/sinks/android_sink.h>
#endif

#include <chrono>
#include <functional>
#include <memory>
#include <string>

inline void lquantlog::register_logger(std::shared_ptr<logger> logger)
{
    return details::registry::instance().register_logger(logger);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::get(const std::string& name)
{
    return details::registry::instance().get(name);
}

inline void lquantlog::drop(const std::string &name)
{
    details::registry::instance().drop(name);
}

// Create multi/single threaded simple file logger
inline std::shared_ptr<lquantlog::logger> lquantlog::basic_logger_mt(const std::string& logger_name, const filename_t& filename, bool truncate)
{
    return create<lquantlog::sinks::simple_file_sink_mt>(logger_name, filename, truncate);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::basic_logger_st(const std::string& logger_name, const filename_t& filename, bool truncate)
{
    return create<lquantlog::sinks::simple_file_sink_st>(logger_name, filename, truncate);
}

// Create multi/single threaded rotating file logger
inline std::shared_ptr<lquantlog::logger> lquantlog::rotating_logger_mt(const std::string& logger_name, const filename_t& filename, size_t max_file_size, size_t max_files)
{
    return create<lquantlog::sinks::rotating_file_sink_mt>(logger_name, filename, SPDLOG_FILENAME_T("txt"), max_file_size, max_files);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::rotating_logger_st(const std::string& logger_name, const filename_t& filename, size_t max_file_size, size_t max_files)
{
    return create<lquantlog::sinks::rotating_file_sink_st>(logger_name, filename, SPDLOG_FILENAME_T("txt"), max_file_size, max_files);
}

// Create file logger which creates new file at midnight):
inline std::shared_ptr<lquantlog::logger> lquantlog::daily_logger_mt(const std::string& logger_name, const filename_t& filename, int hour, int minute)
{
    return create<lquantlog::sinks::daily_file_sink_mt>(logger_name, filename, SPDLOG_FILENAME_T("txt"), hour, minute);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::daily_logger_st(const std::string& logger_name, const filename_t& filename, int hour, int minute)
{
    return create<lquantlog::sinks::daily_file_sink_st>(logger_name, filename, SPDLOG_FILENAME_T("txt"), hour, minute);
}


//
// stdout/stderr loggers
//
inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_logger_mt(const std::string& logger_name)
{
    return lquantlog::details::registry::instance().create(logger_name, lquantlog::sinks::stdout_sink_mt::instance());
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_logger_st(const std::string& logger_name)
{
    return lquantlog::details::registry::instance().create(logger_name, lquantlog::sinks::stdout_sink_st::instance());
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_logger_mt(const std::string& logger_name)
{
    return lquantlog::details::registry::instance().create(logger_name, lquantlog::sinks::stderr_sink_mt::instance());
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_logger_st(const std::string& logger_name)
{
    return lquantlog::details::registry::instance().create(logger_name, lquantlog::sinks::stderr_sink_st::instance());
}

//
// stdout/stderr color loggers
//
#ifdef _WIN32
inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_color_mt(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::wincolor_stdout_sink_mt>();
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_color_st(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::wincolor_stdout_sink_st>();
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_color_mt(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::wincolor_stderr_sink_mt>();
    return lquantlog::details::registry::instance().create(logger_name, sink);
}


inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_color_st(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::wincolor_stderr_sink_st>();
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

#else //ansi terminal colors

inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_color_mt(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::ansicolor_sink>(lquantlog::sinks::stdout_sink_mt::instance());
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stdout_color_st(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::ansicolor_sink>(lquantlog::sinks::stdout_sink_st::instance());
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_color_mt(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::ansicolor_sink>(lquantlog::sinks::stderr_sink_mt::instance());
    return lquantlog::details::registry::instance().create(logger_name, sink);
}

inline std::shared_ptr<lquantlog::logger> lquantlog::stderr_color_st(const std::string& logger_name)
{
    auto sink = std::make_shared<lquantlog::sinks::ansicolor_sink>(lquantlog::sinks::stderr_sink_st::instance());
    return lquantlog::details::registry::instance().create(logger_name, sink);
}
#endif

#ifdef SPDLOG_ENABLE_SYSLOG
// Create syslog logger
inline std::shared_ptr<lquantlog::logger> lquantlog::syslog_logger(const std::string& logger_name, const std::string& syslog_ident, int syslog_option)
{
    return create<lquantlog::sinks::syslog_sink>(logger_name, syslog_ident, syslog_option);
}
#endif

#ifdef __ANDROID__
inline std::shared_ptr<lquantlog::logger> lquantlog::android_logger(const std::string& logger_name, const std::string& tag)
{
    return create<lquantlog::sinks::android_sink>(logger_name, tag);
}
#endif

// Create and register a logger a single sink
inline std::shared_ptr<lquantlog::logger> lquantlog::create(const std::string& logger_name, const lquantlog::sink_ptr& sink)
{
    return details::registry::instance().create(logger_name, sink);
}

//Create logger with multiple sinks

inline std::shared_ptr<lquantlog::logger> lquantlog::create(const std::string& logger_name, lquantlog::sinks_init_list sinks)
{
    return details::registry::instance().create(logger_name, sinks);
}


template <typename Sink, typename... Args>
inline std::shared_ptr<lquantlog::logger> lquantlog::create(const std::string& logger_name, Args... args)
{
    sink_ptr sink = std::make_shared<Sink>(args...);
    return details::registry::instance().create(logger_name, { sink });
}


template<class It>
inline std::shared_ptr<lquantlog::logger> lquantlog::create(const std::string& logger_name, const It& sinks_begin, const It& sinks_end)
{
    return details::registry::instance().create(logger_name, sinks_begin, sinks_end);
}

inline void lquantlog::set_formatter(lquantlog::formatter_ptr f)
{
    details::registry::instance().formatter(f);
}

inline void lquantlog::set_pattern(const std::string& format_string)
{
    return details::registry::instance().set_pattern(format_string);
}

inline void lquantlog::set_level(level::level_enum log_level)
{
    return details::registry::instance().set_level(log_level);
}

inline void lquantlog::set_error_handler(log_err_handler handler)
{
    return details::registry::instance().set_error_handler(handler);
}


inline void lquantlog::set_async_mode(size_t queue_size, uint32 work_core, const async_overflow_policy overflow_policy, const std::function<void()>& worker_warmup_cb, const std::chrono::milliseconds& flush_interval_ms, const std::function<void()>& worker_teardown_cb)
{
    details::registry::instance().set_async_mode(queue_size, work_core, overflow_policy, worker_warmup_cb, flush_interval_ms, worker_teardown_cb);
}

inline void lquantlog::set_sync_mode()
{
    details::registry::instance().set_sync_mode();
}

inline void lquantlog::apply_all(std::function<void(std::shared_ptr<logger>)> fun)
{
    details::registry::instance().apply_all(fun);
}

inline void lquantlog::drop_all()
{
    details::registry::instance().drop_all();
}
