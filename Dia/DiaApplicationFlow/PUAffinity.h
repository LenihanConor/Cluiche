#pragma once
#include <cstdint>

namespace Dia::ApplicationFlow {

enum class PUAffinity : uint8_t
{
    kNone   = 0,
    kMain   = 1 << 0,
    kSim    = 1 << 1,
    kRender = 1 << 2,
    kAny    = 0xFF
};

inline constexpr PUAffinity operator|(PUAffinity a, PUAffinity b)
{
    return static_cast<PUAffinity>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr PUAffinity operator&(PUAffinity a, PUAffinity b)
{
    return static_cast<PUAffinity>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline constexpr bool HasAffinity(PUAffinity mask, PUAffinity flag)
{
    return (mask & flag) != PUAffinity::kNone;
}

} // namespace Dia::ApplicationFlow
