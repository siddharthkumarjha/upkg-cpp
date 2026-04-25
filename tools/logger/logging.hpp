#pragma once

#include "fmt_source_loc.hpp"
#include "log_level.hpp"
#include "stream-concept.hpp"
#include <iostream>

namespace upkg::log
{
    namespace detail
    {
        extern constinit std::ostream &sink;

        template <LogLevel Level, is_streamable... Args>
            requires(Level < LogLevel::OFF)
        struct log_impl
        {
            log_impl(Args &&...args, fmt_source_loc const location = std::source_location::current())
            {
                sink << "[" << log_level_str(Level) << "] " << location << ' ';
                (sink << ... << std::forward<Args>(args));
                sink << '\n';
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
