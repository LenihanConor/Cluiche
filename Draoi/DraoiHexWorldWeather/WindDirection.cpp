// A4 weather wind helpers: hex direction snap, direction unit vectors, prevailing direction by latitude.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md ("Wind snap and the prevailing-direction tie")
#include <DraoiHexWorldWeather/WindDirection.h>

#include <math.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = 2.0 * kPi;
    constexpr double kStep = kPi / 3.0;       // 60 deg
    constexpr double kOffset = kPi / 6.0;     // 30 deg
}

uint8_t SnapWindDirection(float angleRadians, uint32_t column)
{
    double angle = fmod(static_cast<double>(angleRadians), kTwoPi);
    if (angle < 0.0) { angle += kTwoPi; }
    if (angle >= kTwoPi) { angle = 0.0; }

    // Nearest half-step (multiple of 60 deg), 0..6.
    const int halfStep = static_cast<int>(floor(angle / kStep + 0.5));
    if (fabs(angle - halfStep * kStep) <= static_cast<double>(kWindTieEpsilon))
    {
        const bool odd = (column & 1u) != 0u;
        if (halfStep == 0 || halfStep == 6) { return odd ? 0 : 5; }  // east
        if (halfStep == 3) { return odd ? 2 : 3; }                   // west
        return static_cast<uint8_t>(halfStep - 1);                   // lower index of the two neighbours
    }

    int d = static_cast<int>(floor((angle - kOffset) / kStep + 0.5)) % 6;
    if (d < 0) { d += 6; }
    return static_cast<uint8_t>(d);
}

float PrevailingDirection(float effectiveLatitude, const WesterlyBand& band)
{
    const float pi = static_cast<float>(kPi);
    if (effectiveLatitude < band.tradeEnd) { return 0.0f; }
    if (effectiveLatitude < band.westerlyStart)
    {
        const float t = (effectiveLatitude - band.tradeEnd) / (band.westerlyStart - band.tradeEnd);
        return pi * t;
    }
    if (effectiveLatitude < band.westerlyEnd) { return pi; }
    if (effectiveLatitude < band.polarStart)
    {
        const float t = (effectiveLatitude - band.westerlyEnd) / (band.polarStart - band.westerlyEnd);
        return pi + pi * t;
    }
    return 0.0f;
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
