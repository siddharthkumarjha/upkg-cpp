#pragma once

#include <concepts>
#include <ostream>

template <typename Tp>
concept is_streamable =
    requires { requires std::same_as<decltype(std::declval<std::ostream &>() << std::declval<Tp>()), std::ostream &>; };
