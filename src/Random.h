#pragma once

#include <cstdint>
#include <random>

namespace rng
{

inline std::mt19937& engine()
{
    static std::mt19937 instance {std::random_device {}()};
    return instance;
}

inline int32_t randomInt(int32_t min, int32_t max)
{
    return std::uniform_int_distribution {min, max}(engine());
}

} // namespace rng
