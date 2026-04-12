#pragma once

#include <cstdint>
#include <ostream>
#include <source_location>
#include <string_view>

namespace upkg::log
{
    struct fmt_source_loc
    {
        std::string_view base_file_name;
        std::string_view fn_name;
        std::uint_least32_t line_num;

        consteval fmt_source_loc(std::source_location loc = std::source_location::current())
            : base_file_name{file_base_name(loc.file_name())}, fn_name{fn_base_name(loc.function_name())},
              line_num{loc.line()}
        {
        }

        friend auto operator<<(std::ostream &oss, const fmt_source_loc &loc) -> std::ostream &
        {
            oss << "[" << loc.base_file_name << ':' << loc.line_num << ':' << loc.fn_name << "]";
            return oss;
        }

    private:
        consteval static inline std::string_view file_base_name(std::string_view const file_name)
        {
            const size_t pos = file_name.find_last_of('/');
            return pos == std::string_view::npos ? file_name : file_name.substr(pos + 1);
        }

        consteval static inline size_t rfind_balanced(std::string_view str, const char rstart_tok, const char rstop_tok)
        {
            int64_t depth_balance_cnt = 0;
            size_t idx                = str.size();
            for (; idx > 0; --idx)
            {
                const char c = str[idx - 1];
                if (rstop_tok == c)
                {
                    ++depth_balance_cnt;
                }
                else if (rstart_tok == c)
                {
                    --depth_balance_cnt;
                    if (depth_balance_cnt == 0)
                    {
                        return (idx - 1);
                    }
                }
            }
            return std::string_view::npos;
        }

        consteval static inline std::string_view fn_base_name(std::string_view fn_name)
        {
            // trim compiler template deductions
            size_t pos = rfind_balanced(fn_name, '[', ']');
            fn_name    = (pos == std::string_view::npos) ? fn_name : fn_name.substr(0, pos);

            // trim fn params
            pos     = rfind_balanced(fn_name, '(', ')');
            fn_name = (pos == std::string_view::npos) ? fn_name : fn_name.substr(0, pos);

            // trim fn return types
            pos     = fn_name.rfind(' ');
            fn_name = (pos == std::string_view::npos) ? fn_name : fn_name.substr(pos + 1);

            // trim long namespaces
            pos     = fn_name.rfind("::");
            fn_name = (pos == std::string_view::npos) ? fn_name : fn_name.substr(pos + 2);

            return fn_name;
        }
    };
} // namespace upkg::log
