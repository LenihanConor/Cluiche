// A decoded weather-state record, the payload of a StateReplace queue entry (A4 WeatherModule, Gap-pass amendment 1).
// Filled by DecodeState (WeatherSaveFields.h) from a `cow.weather_state` export file. The StateReplace apply branch
// belongs to Feature 5/6; until then a drained StateReplace is logged and not applied.
#pragma once

#include <stdint.h>

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace CoW {
namespace Weather {

struct WeatherStateRecord
{
    uint32_t cellCount = 0;
    uint64_t week = 0;
    uint64_t paramsHash = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Draoi::HexWorld::Weather::WeatherFields                          fields;   // the ten arrays
    Dia::Core::Containers::DynamicArray<Draoi::HexWorld::BiomeIndex> biomes;   // remapped into the live registry
};

} // namespace Weather
} // namespace CoW
