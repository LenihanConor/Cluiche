// S-1 Regions test support: hand-built worlds with rectangular land blocks. Header-only.
// Row-major, index = y * width + x. Default world is all ocean with mainLandmass = 1.
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

inline WorldStructure MakeBlankWorld(int w, int h)
{
    WorldStructure world;
    world.Resize({ w, h });
    world.mainLandmass = 1;
    return world;
}

inline int CellIndex(const WorldStructure& world, int x, int y) { return y * world.dimensions.width + x; }

// Sets one land cell (Lowland, no flags).
inline void SetLand(WorldStructure& world, int x, int y, DepthZone zone, Biome biome, uint8_t landmass = 1)
{
    Cell& c = world.At(x, y);
    c.landmass = landmass;
    c.depthZone = zone;
    c.biome = biome;
    c.waterSource = WaterSource::None;
    c.terrain = TerrainType::Lowland;
    c.elevation = 0.3f;
}

// Fills the half-open block [x0,x1) x [y0,y1).
inline void FillLand(WorldStructure& world, int x0, int y0, int x1, int y1, DepthZone zone, Biome biome, uint8_t landmass = 1)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x) { SetLand(world, x, y, zone, biome, landmass); }
    }
}

inline void AddFlag(WorldStructure& world, int x, int y, uint8_t flag) { world.At(x, y).terrainFlags |= flag; }

inline void SetBiome(WorldStructure& world, int x, int y, Biome b) { world.At(x, y).biome = b; }

inline void SetOcean(WorldStructure& world, int x, int y)
{
    world.At(x, y) = Cell{};
    world.At(x, y).waterSource = WaterSource::Ocean;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
