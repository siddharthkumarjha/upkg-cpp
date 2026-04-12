#pragma once

#include "stream-concept.hpp"
#include <array>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <span>
#include <vector>

namespace std
{
    template <typename T, std::size_t Extent = std::dynamic_extent>
        requires(is_streamable<T>)
    static inline auto operator<<(std::ostream &oss, const std::span<T, Extent> &range) -> std::ostream &
    {
        auto flags           = oss.flags();
        auto fill            = oss.fill();

        std::string_view sep = "";
        oss << "[";
        for (const auto &data : range)
        {
            oss << sep;
            sep = ", ";

            if constexpr (std::is_same_v<T, uint8_t> or std::is_same_v<T, int8_t>)
            {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<uint16_t>(static_cast<uint8_t>(data));
            }
            else
            {
                oss << data;
            }
        }
        oss << "]";

        oss.flags(flags);
        oss.fill(fill);

        return oss;
    }

    template <typename T>
        requires(is_streamable<T>)
    static inline auto operator<<(std::ostream &oss, const std::vector<T> &vec) -> std::ostream &
    {
        return oss << std::span{vec};
    }

    template <typename T, std::size_t Nm>
        requires(is_streamable<T>)
    static inline auto operator<<(std::ostream &oss, const std::array<T, Nm> &arr) -> std::ostream &
    {
        return oss << std::span{arr};
    }
} // namespace std
