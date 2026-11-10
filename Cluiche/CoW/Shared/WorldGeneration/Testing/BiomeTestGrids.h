// S-1 Biome Classification test support: hand-built grids for climate stage tests (AC7/AC8/AC9).
// Header-only. Row-major, index = y * width + x.
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

struct ClimateGrid
{
    GridDimensions        dims;
    std::vector<float>    elevation;
    std::vector<float>    latitude;
    std::vector<uint32_t> waterSource;   // WaterSource values

    ClimateGrid(int w, int h, float lat = 0.0f)
        : dims{w, h},
          elevation(static_cast<size_t>(w) * h, 0.1f),
          latitude(static_cast<size_t>(w) * h, lat),
          waterSource(static_cast<size_t>(w) * h, 0u)
    {
    }

    int Index(int x, int y) const { return y * dims.width + x; }

    void SetWater(int x, int y, WaterSource s)
    {
        waterSource[Index(x, y)] = static_cast<uint32_t>(s);
        elevation[Index(x, y)] = (s == WaterSource::Ocean) ? 0.0f : elevation[Index(x, y)];
    }
};

// North-south ridge at column ridgeX of the given height over flat land (elevation 0.1).
// oceanWest / oceanEast fill column 0 / the last column with ocean. Constant latitude.
inline ClimateGrid MakeRidgeGrid(int w, int h, int ridgeX, float ridgeHeight, float latitude, bool oceanWest,
                                 bool oceanEast)
{
    ClimateGrid g(w, h, latitude);
    for (int y = 0; y < h; ++y)
    {
        g.elevation[g.Index(ridgeX, y)] = ridgeHeight;
        if (oceanWest)
        {
            g.SetWater(0, y, WaterSource::Ocean);
        }
        if (oceanEast)
        {
            g.SetWater(w - 1, y, WaterSource::Ocean);
        }
    }
    return g;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
