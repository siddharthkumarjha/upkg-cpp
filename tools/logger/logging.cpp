#include "logging.hpp"

namespace upkg::log
{
    namespace detail
    {
        constinit std::mutex sink_mtx = {};
        constinit std::ostream *sink  = std::addressof(std::cerr);
    } // namespace detail

    void set_sink(std::ostream &oss)
    {
        std::lock_guard lk{detail::sink_mtx};
        detail::sink = std::addressof(oss);
    }
    void flush_sink()
    {
        std::lock_guard lk{detail::sink_mtx};
        detail::sink->flush();
    }
} // namespace upkg::log
