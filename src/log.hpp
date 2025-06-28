#pragma once

#ifndef __LOG_HPP
#define __LOG_HPP

#include <string>
#include <vector>
#include <memory>

#ifndef WITHOUT_SPDLOG
//#   define SPDLOG_NO_SOURCE_LOC
//#   define SPDLOG_NO_THREAD_ID
//#   define SPDLOG_NO_ATOMIC_LEVELS
//#   define SPDLOG_DISABLE_DEFAULT_LOGGER
#   define SPDLOG_HEADER_ONLY
#   define FMT_UNICODE 0
#   ifdef _MSC_VER
#	    pragma warning( push )
#	    pragma warning( disable : 4459 ) // declaration of '...' hides global declaration
#   endif // #ifdef _MSC_VER
#   include <spdlog/spdlog.h>
#   include <spdlog/fmt/bundled/printf.h>
#   include <spdlog/fmt/bundled/ostream.h>
#   include <spdlog/sinks/stdout_sinks.h>
#   include <spdlog/sinks/stdout_color_sinks.h>
#   include <spdlog/sinks/basic_file_sink.h>
#   ifdef _MSC_VER
#	    pragma warning( pop )
#   endif // #ifdef _MSC_VER
#else
#   include <cstdio>
#   if __has_include(<format>)
#       include <format>
        namespace fmt = std;
#   endif
#endif // #ifndef WITHOUT_SPDLOG

namespace applog {

#ifndef WITHOUT_SPDLOG

using logger = std::shared_ptr<spdlog::logger>;

// default log id
constexpr char const* _log_id_ = "app";
// default log file
constexpr char const* _log_file_ = "app.log";

struct log {
    struct config {
        // log level
        int level{ -1 };
        // log name
        char const* id{ _log_id_ };
        // log file
        char const* file{ _log_file_ };
        // log to console
        bool std_out{ true };
        // color
        bool no_color{ false };
        // log to file
        bool file_out{ true };
        // multi-threaded
        bool threaded{ true };
        // file truncate
        bool truncate{ true };
        // flush period in sec
        unsigned int flush_every{ 1 };
    };

    using config_t = config;

    static logger& get() {
        static logger instance;
        if (instance == nullptr) {
            instance = make();
            g_instance = instance;
        }
        return instance;
    }

    static config& cfg() {
        static config conf;
        return conf;
    }

    static logger make() {
        auto &config = cfg();
        std::vector<spdlog::sink_ptr> sinks;
        if (config.std_out) {
            if (config.no_color) {
                if (config.threaded)
                    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
                else sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_sink_st>());
            } else {
                if (config.threaded)
                    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
                else sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_st>());
            }
        }
        if (config.file_out) {
            if (config.threaded)
                sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.file, config.truncate));
            else sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_st>(config.file, config.truncate));
        }
        auto result = std::make_shared<spdlog::logger>(config.id, begin(sinks), end(sinks));
        //if (config.std_out)
        //    result->set_level((config.level < 0) ? spdlog::level::debug : (spdlog::level::level_enum) config.level);
        //if (config.file_out)
        //    result->set_level((config.level < 0) ? spdlog::level::warn : (spdlog::level::level_enum) config.level);
        spdlog::register_logger(result);
        //spdlog::set_default_logger(result);
        spdlog::flush_every(std::chrono::seconds(config.flush_every));
        return result;
    }

    static inline logger g_instance{ nullptr };
};

inline logger& log() {
    return log::get();
}

// compile time log levels
// define SPDLOG_ACTIVE_LEVEL to desired level, to one of those (before including spdlog.h):
// SPDLOG_LEVEL_TRACE,
// SPDLOG_LEVEL_DEBUG,
// SPDLOG_LEVEL_INFO,
// SPDLOG_LEVEL_WARN,
// SPDLOG_LEVEL_ERROR,
// SPDLOG_LEVEL_CRITICAL,
// SPDLOG_LEVEL_OFF
// https://stackoverflow.com/questions/45621996/how-to-enable-disable-spdlog-logging-in-code
// https://stackoverflow.com/questions/68762824/spdlog-logger-call-and-va-args-in

#endif // #ifndef WITHOUT_SPDLOG

} // namespace applog

#ifdef LOG_DISABLED
#   define LOG_LEVEL_FMT_IMPL( ... )	            (void)0
#   define LOG_LEVEL_FMT( ... )			            (void)0
#   define LOG_LEVEL_PRINTF( ... )		            (void)0
#else
#   ifdef WITHOUT_SPDLOG
#       ifdef __cpp_lib_format
#           define LOG_LEVEL_FMT_IMPL( ... )	    std::cout << std::format( __VA_ARGS__ ) << std::endl;
#           define LOG_LEVEL_FMT( level, ... )		LOG_LEVEL_FMT_IMPL( _##level, __VA_ARGS__ )
#           define LOG_LEVEL_PRINTF( level, ... )	LOG_LEVEL_FMT_IMPL( _##level, "{}", std::sprintf( __VA_ARGS__ ) )
#       else
#           define LOG_LEVEL_FMT_IMPL( ... )        (void)0
#           define LOG_LEVEL_FMT( ... )			    (void)0
#           define LOG_LEVEL_PRINTF( ... )		    (void)0
#       endif // #ifdef __cpp_lib_format
#   else
#       define LOG_LEVEL_FMT_IMPL( level, ... )	    if ( ::applog::log::g_instance ) SPDLOG_LOGGER##level( applog::log(), __VA_ARGS__ ); else (void)0
#       define LOG_LEVEL_FMT( level, ... )			LOG_LEVEL_FMT_IMPL( _##level, __VA_ARGS__ )
#       define LOG_LEVEL_PRINTF( level, ... )		LOG_LEVEL_FMT_IMPL( _##level, "{}", fmt::sprintf( __VA_ARGS__ ) )
#   endif // #ifdef WITHOUT_SPDLOG
#endif // #ifdef LOG_DISABLED

#define LOG_TRACE_PRINTF(...)				        LOG_LEVEL_PRINTF( TRACE, __VA_ARGS__ )
#define LOG_DEBUG_PRINTF(...)				        LOG_LEVEL_PRINTF( DEBUG, __VA_ARGS__ )
#define LOG_INFO_PRINTF(...)				        LOG_LEVEL_PRINTF( INFO, __VA_ARGS__ )
#define LOG_WARNING_PRINTF(...)				        LOG_LEVEL_PRINTF( WARN, __VA_ARGS__ )
#define LOG_ERROR_PRINTF(...)				        LOG_LEVEL_PRINTF( ERROR, __VA_ARGS__ )
#define LOG_CRITICAL_PRINTF(...)			        LOG_LEVEL_PRINTF( CRITICAL, __VA_ARGS__ )

#define LOG_TRACE_FMT(...)					        LOG_LEVEL_FMT( TRACE, __VA_ARGS__ )
#define LOG_DEBUG_FMT(...)					        LOG_LEVEL_FMT( DEBUG, __VA_ARGS__ )
#define LOG_INFO_FMT(...)					        LOG_LEVEL_FMT( INFO, __VA_ARGS__ )
#define LOG_WARNING_FMT(...)				        LOG_LEVEL_FMT( WARN, __VA_ARGS__ )
#define LOG_ERROR_FMT(...)					        LOG_LEVEL_FMT( ERROR, __VA_ARGS__ )
#define LOG_CRITICAL_FMT(...)				        LOG_LEVEL_FMT( CRITICAL, __VA_ARGS__ )

#define LOG_TRACE							        LOG_TRACE_PRINTF
#define LOG_DEBUG							        LOG_DEBUG_PRINTF
#define LOG_INFO							        LOG_INFO_PRINTF
#define LOG_WARNING							        LOG_WARNING_PRINTF
#define LOG_ERROR							        LOG_ERROR_PRINTF
#define LOG_CRITICAL						        LOG_CRITICAL_PRINTF

#endif //__LOG_HPP