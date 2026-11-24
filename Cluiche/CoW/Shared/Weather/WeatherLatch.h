// Everything WeatherSimulation::Capture needs, latched by the stage on SimPU so Run reads only this (DFS-002).
#pragma once

#include <DraoiHexWorldWeather/WeatherParameters.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>

#include <stdint.h>

namespace CoW {
namespace Weather {

struct WeatherLatch
{
    Draoi::HexWorld::Weather::WeatherParameters      params;       // ParamBinding::Snapshot at Capture
    Draoi::HexWorld::Weather::Season                 season = Draoi::HexWorld::Weather::Season::Spring;
    uint32_t                                         weekOfYear = 0;   // WeekOf(day) % 52
    bool                                             hasBaseline = false;
    Draoi::HexWorld::Weather::SeasonalBaselineInput  baseline;     // per-cell baseline for `season`, clamped to [0, 1]
    Draoi::HexWorld::Weather::HydrologyInputs        hydro;        // present == false when no A5 provider
    Draoi::HexWorld::Weather::InfluenceSnapshot      influences;   // empty until Feature 7
};

} // namespace Weather
} // namespace CoW
