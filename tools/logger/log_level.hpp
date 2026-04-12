#pragma once
#include <cstddef>
#include <string_view>

namespace upkg::log
{
    enum LogLevel : size_t
    {
        ERROR = 0,
        WARN,
        INFO,
        DEBUG,
        OFF
    };

    namespace detail
    {
        static inline constexpr std::string_view log_level_arr[] = {
            [LogLevel::ERROR] = "ERROR",
            [LogLevel::WARN]  = "WARN",
            [LogLevel::INFO]  = "INFO",
            [LogLevel::DEBUG] = "DEBUG",
            [LogLevel::OFF]   = "OFF",
        };
        static_assert(std::size(log_level_arr) == LogLevel::OFF + 1u, "update the array");

        auto consteval log_level_str(const LogLevel log_level) -> std::string_view
        {
            using namespace std::string_view_literals;

            if (log_level >= LogLevel::ERROR and log_level <= LogLevel::OFF)
            {
                return log_level_arr[static_cast<size_t>(log_level)];
            }
            return "UNKNOWN"sv;
        }
    } // namespace detail
} // namespace upkg::log
