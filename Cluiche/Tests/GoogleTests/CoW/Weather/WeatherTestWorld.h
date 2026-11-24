// Test helper: copy a hand-built WeatherTerrain into a destination element by element (DynamicArray assignment
// truncates, so `dst = src` is not a copy).
#pragma once

#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace WeatherTestWorld
{
inline void CopyTerrain(const Draoi::HexWorld::Weather::WeatherTerrain& src, Draoi::HexWorld::Weather::WeatherTerrain& dst)
{
    dst = Draoi::HexWorld::Weather::WeatherTerrain();
    dst.width = src.width;
    dst.height = src.height;
    const uint32_t n = src.elevation.Size();
    dst.elevation.Reserve(n); dst.latitude.Reserve(n); dst.waterSource.Reserve(n); dst.biome.Reserve(n);
    for (uint32_t i = 0; i < n; ++i)
    {
        dst.elevation.Add(src.elevation[i]);
        dst.latitude.Add(src.latitude[i]);
        dst.waterSource.Add(src.waterSource[i]);
        dst.biome.Add(src.biome[i]);
    }
}
}
