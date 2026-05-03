#include "pfr.hpp"
#include "stream-concept.hpp"
#include <string_view>
#include <type_traits>

namespace upkg
{
    template <typename T> static inline auto debug_impl(std::ostream &oss, const T &data) -> std::ostream &
    {
        auto format_value = [&oss](const auto &field_value) -> std::ostream &
        {
            using field_t = std::decay_t<decltype(field_value)>;
            static_assert(is_streamable<field_t>, "field_t is not streamable");

            if constexpr (std::convertible_to<field_t, std::string_view>)
            {
                oss << "\"" << field_value << "\"";
            }
            else if constexpr (std::is_same_v<field_t, bool>)
            {
                oss << (field_value ? "true" : "false");
            }
            else if constexpr (is_streamable<field_t>)
            {
                oss << field_value;
            }

            return oss;
        };

        oss << '{';
        std::string_view sep = "";

        pfr::for_each_field_with_name(
            data,
            [&oss, &sep, &format_value](const std::string_view field_name, const auto &field_value) -> void
            {
                oss << sep << "<" << field_name << ": ";
                format_value(field_value) << ">";

                sep = ", ";
            });

        oss << '}';
        return oss;
    }
} // namespace upkg

#define IMPL_DEBUG(STRUCT)                                                                                             \
    friend inline auto operator<<(std::ostream &oss, const STRUCT &data)->std::ostream &                               \
    {                                                                                                                  \
        return upkg::debug_impl(oss, data);                                                                            \
    }
