#pragma once

#include "stream-concept.hpp"
#include <concepts>
#include <iomanip>
#include <optional>
#include <ostream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace std
{
    template <typename T>
        requires(is_streamable<T>)
    static inline auto operator<<(std::ostream &oss, const std::optional<T> &opt) -> std::ostream &
    {
        if (opt.has_value())
            oss << opt.value();
        else
            oss << "`nullopt`";

        return oss;
    }

    template <template <typename...> class Tuple, is_streamable... Elem>
        requires // clang-format off
        ( 
            std::same_as<Tuple<Elem...>, std::tuple<Elem...>> or
            std::same_as<Tuple<Elem...>, std::pair<Elem...>>
        ) // clang-format on
    static inline auto operator<<(std::ostream &oss, const Tuple<Elem...> &tuple) -> std::ostream &
    {
        std::string_view sep = "";
        auto print_elem      = [&](const auto &elem)
        {
            oss << sep;
            sep = ", ";

            using Tp = std::decay_t<decltype(elem)>;
            if constexpr (std::convertible_to<Tp, std::string_view>)
            {
                oss << std::quoted(elem);
            }
            else
            {
                oss << elem;
            }
        };

        oss << "<";
        std::apply(
            [&](const auto &...elems)
            {
                (print_elem(elems), ...);
            },
            tuple);
        oss << ">";
        return oss;
    }
} // namespace std
