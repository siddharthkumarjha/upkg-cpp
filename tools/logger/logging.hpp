#pragma once

#include "fmt_source_loc.hpp"
#include "log_level.hpp"
#include "stream-concept.hpp"
#include <iostream>
#include <mutex>

namespace upkg::log
{
    void set_sink(std::ostream &oss);
    void flush_sink();

    namespace detail
    {
        extern constinit std::mutex sink_mtx;
        extern constinit std::ostream *sink;

        template <LogLevel Level, is_streamable... Args>
            requires(Level < LogLevel::OFF)
        struct log_impl
        {
            log_impl(Args &&...args, fmt_source_loc const location = std::source_location::current())
            {
                std::lock_guard lk{sink_mtx};
                auto &oss = *sink;

                oss << "[" << log_level_str(Level) << "] " << location << ' ';
                (oss << ... << std::forward<Args>(args));
                oss << '\n';
            }
        };
        template <LogLevel Level = LogLevel::OFF, is_streamable... Args>
        log_impl(Args &&...) -> log_impl<Level, Args...>;
    } // namespace detail

    template <is_streamable... Args> using error = detail::log_impl<LogLevel::ERROR, Args...>;
    template <is_streamable... Args> using warn  = detail::log_impl<LogLevel::WARN, Args...>;
    template <is_streamable... Args> using info  = detail::log_impl<LogLevel::INFO, Args...>;
    template <is_streamable... Args> using debug = detail::log_impl<LogLevel::DEBUG, Args...>;
} // namespace upkg::log
