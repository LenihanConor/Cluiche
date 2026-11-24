// A4 weather wind helpers: hex direction snap, direction unit vectors, prevailing direction by latitude.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md ("Wind snap and the prevailing-direction tie")
#pragma once

#include <stdint.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

// Unit vector in y-down world space.
struct WindVector
{
    float x;
    float y;
};

// Direction d (0..5) points at on-screen counter-clockwise angle 30deg + 60deg * d, so with y down the
// vector is (cos(a), -sin(a)). 0 = NE, 1 = N, 2 = NW, 3 = SW, 4 = S, 5 = SE.
constexpr WindVector kDirectionVectors[6] = {
    {  0.8660254037844386f, -0.5f },  // d0, 30 deg
    {  0.0f,                -1.0f },  // d1, 90 deg
    { -0.8660254037844386f, -0.5f },  // d2, 150 deg
    { -0.8660254037844386f,  0.5f },  // d3, 210 deg
    {  0.0f,                 1.0f },  // d4, 270 deg
    {  0.8660254037844386f,  0.5f },  // d5, 330 deg
};

// Half-width of the tie window around each half-step (multiples of 60deg), inclusive:
// |angle - tie| <= kWindTieEpsilon.
constexpr float kWindTieEpsilon = 1e-4f;

// Latitude bands for the prevailing wind (GDD A4_TechnicalSpec s5.1). Latitudes are 0 (equator) to 1 (pole).
struct WesterlyBand
{
    float tradeEnd = 0.25f;       // below: trade winds (0 rad, east)
    float westerlyStart = 0.35f;  // [tradeEnd, westerlyStart): lerp 0 -> pi
    float westerlyEnd = 0.55f;    // [westerlyStart, westerlyEnd): pi (west)
    float polarStart = 0.65f;     // [westerlyEnd, polarStart): lerp pi -> 2pi; at or above: 0 (polar easterlies)
};

// Snaps an angle (radians, any value, on-screen ccw) to a hex direction 0..5:
// d = round((angle - 30deg) / 60deg) mod 6. Angles within kWindTieEpsilon of a half-step are ties.
// East tie (0): even column 5, odd column 0. West tie (pi): even column 3, odd column 2.
// Any other tie: lower index.
uint8_t SnapWindDirection(float angleRadians, uint32_t column);

// Prevailing wind angle in radians for an already-shifted effective latitude. No clamping is applied.
float PrevailingDirection(float effectiveLatitude, const WesterlyBand& band);

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
