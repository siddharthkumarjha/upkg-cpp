#include "logging.hpp"

namespace upkg::log
{
    namespace detail
    {
        constinit std::ostream &sink  = std::cerr;
    } // namespace detail
} // namespace upkg::log
