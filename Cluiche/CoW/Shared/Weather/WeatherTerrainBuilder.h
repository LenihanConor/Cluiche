// Copies the S-1 world into the weather simulation's terrain input (elevation, latitude, water source, biome per cell).
// Plain function over plain data: no module, no retained pointer.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-module.md (world ingest)
#pragma once

#include <DraoiHexWorld/WorldStructure.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace CoW {
namespace Weather {

// Fills `out` (replacing its contents) from `world`. False, with `out` left empty, when the world has no cells.
bool BuildWeatherTerrain(const Draoi::HexWorld::WorldStructure& world, Draoi::HexWorld::Weather::WeatherTerrain& out);

} // namespace Weather
} // namespace CoW
