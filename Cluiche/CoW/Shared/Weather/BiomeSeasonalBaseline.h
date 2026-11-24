// A1 -> A4 seam: the per-biome seasonal baseline temperature the weather sim seeds and relaxes toward. Standalone on
// purpose (A1's ecology implements it without including the weather module).
#pragma once

#include <DraoiHexWorldWeather/WeatherTypes.h>

#include <stdint.h>

namespace CoW {
namespace Weather {

// Called only inside the stage's Capture on SimPU, never from a worker. Values are expected in [0, 1]; the stage clamps.
class IBiomeSeasonalBaseline
{
public:
    virtual ~IBiomeSeasonalBaseline() {}
    virtual float GetBaseline(uint32_t biome, Draoi::HexWorld::Weather::Season season) const = 0;
};

} // namespace Weather
} // namespace CoW
