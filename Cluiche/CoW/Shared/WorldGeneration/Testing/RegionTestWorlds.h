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


// ---- Corridor-node test worlds ---------------------------------------------------------------------
// All-land world: rows < row1 FirstShores, rows < row2 WildReaches, the rest AncientInteriors (row2 <= 0 or
// >= h means no third zone). Barrier 1 boundary = rows row1-1 and row1; barrier 2 = rows row2-1 and row2.
inline WorldStructure MakeZonedBarrierWorld(int w, int h, int row1, int row2)
{
    WorldStructure world = MakeBlankWorld(w, h);
    for (int y = 0; y < h; ++y)
    {
        const DepthZone z = y < row1 ? DepthZone::FirstShores : (y < row2 ? DepthZone::WildReaches : DepthZone::AncientInteriors);
        FillLand(world, 0, y, w, y + 1, z, Biome::GrasslandSteppe);
    }
    return world;
}

inline void MarkPass(WorldStructure& world, int x, int y)
{
    world.At(x, y).terrain = TerrainType::Pass;
    world.At(x, y).terrainFlags |= TerrainFlag::Pass;
}

inline void MarkRiverCut(WorldStructure& world, int x, int y)
{
    world.At(x, y).waterSource = WaterSource::River;
    world.At(x, y).terrain = TerrainType::River;
    world.At(x, y).terrainFlags |= TerrainFlag::RiverCrossesBarrier;
}

inline void MarkCoast(WorldStructure& world, int x, int y) { world.At(x, y).terrainFlags |= TerrainFlag::Coast; }
inline void MarkRidge(WorldStructure& world, int x, int y) { world.At(x, y).terrainFlags |= TerrainFlag::Ridge; }

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
