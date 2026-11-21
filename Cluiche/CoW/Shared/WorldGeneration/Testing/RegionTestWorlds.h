// S-1 Regions test support: hand-built worlds with rectangular land blocks. Header-only.
// Row-major, index = y * width + x. Default world is all ocean with mainLandmass = 1.
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

#include <algorithm>
#include <cstdint>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>
#include <Shared/WorldGeneration/HexWorldAccess.h>

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
inline void SetLand(WorldStructure& world, int x, int y, StratumIndex zone, ::Draoi::HexWorld::BiomeIndex biome, uint8_t landmass = 1)
{
    Cell& c = world.At(x, y);
    c.landmass = landmass;
    c.stratum = zone;
    c.biome = biome;
    c.waterSource = WaterSource::None;
    c.terrain = TerrainType::Lowland;
    c.elevation = 0.3f;
}

// Fills the half-open block [x0,x1) x [y0,y1).
inline void FillLand(WorldStructure& world, int x0, int y0, int x1, int y1, StratumIndex zone, ::Draoi::HexWorld::BiomeIndex biome, uint8_t landmass = 1)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x) { SetLand(world, x, y, zone, biome, landmass); }
    }
}

inline void AddFlag(WorldStructure& world, int x, int y, uint8_t flag) { world.At(x, y).terrainFlags |= flag; }

inline void SetBiome(WorldStructure& world, int x, int y, ::Draoi::HexWorld::BiomeIndex b) { world.At(x, y).biome = b; }

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
        const StratumIndex z = y < row1 ? kStratumFirst : (y < row2 ? kStratumWild : kStratumAncient);
        FillLand(world, 0, y, w, y + 1, z, ::Draoi::HexWorld::Testing::B("Biome.Grassland"));
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


// ---- River / fill node test worlds -----------------------------------------------------------------
// Sets Cell::region for every land cell in [x0,x1) x [y0,y1).
inline void AssignRegion(WorldStructure& world, int x0, int y0, int x1, int y1, RegionId id)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            Cell& c = world.At(x, y);
            if (c.landmass != 0 && c.waterSource != WaterSource::Ocean) { c.region = id; }
        }
    }
}

// Rebuilds world.regions from Cell::region: cellCount, majority non-Lake biome (ties lowest enum), landmass and zone
// of the lowest cell, isIsland, centre = lowest cell. Regions 1..max id; ids with no cells get an empty record.
inline void BuildRegionRecords(WorldStructure& world)
{
    world.regions = RegionTable{};
    int maxId = 0;
    for (int i = 0; i < world.CellCount(); ++i) { maxId = std::max<int>(maxId, world.Cells()[i].region); }
    for (int id = 1; id <= maxId; ++id)
    {
        RegionRecord rec;
        rec.id = static_cast<RegionId>(id);
        int hist[256] = {};   // BiomeIndex is a uint8_t
        bool first = true;
        int count = 0;
        for (int i = 0; i < world.CellCount(); ++i)
        {
            const Cell& c = world.Cells()[i];
            if (c.region != id) { continue; }
            ++count;
            ++hist[static_cast<int>(c.biome)];
            if (first)
            {
                first = false;
                rec.landmass = c.landmass;
                rec.stratum = c.stratum;
                rec.centreCell = static_cast<uint32_t>(i);
            }
        }
        const int lakeIdx = static_cast<int>(::Draoi::HexWorld::Testing::B("Biome.Lake"));
        int best = lakeIdx;
        for (int b = 0; b < 256; ++b)
        {
            if (b == lakeIdx || hist[b] == 0) { continue; }
            if (best == lakeIdx || hist[b] > hist[best]) { best = b; }
        }
        rec.primaryBiome = static_cast<::Draoi::HexWorld::BiomeIndex>(best);
        rec.cellCount = static_cast<uint16_t>(count);
        rec.isIsland = rec.landmass != world.mainLandmass;
        rec.purity = count ? static_cast<float>(hist[best]) / static_cast<float>(count) : 0.0f;
        world.regions.AddRegion(rec);
    }
}

// All-land world (one region of the given biome per block of blockW x blockH cells, ids row-major) with records built.
inline WorldStructure MakeRegionBlocksWorld(int blocksX, int blocksY, int blockW, int blockH, ::Draoi::HexWorld::BiomeIndex biome)
{
    WorldStructure world = MakeBlankWorld(blocksX * blockW, blocksY * blockH);
    FillLand(world, 0, 0, world.dimensions.width, world.dimensions.height, kStratumFirst, biome);
    for (int by = 0; by < blocksY; ++by)
    {
        for (int bx = 0; bx < blocksX; ++bx)
        {
            AssignRegion(world, bx * blockW, by * blockH, (bx + 1) * blockW, (by + 1) * blockH,
                         static_cast<RegionId>(by * blocksX + bx + 1));
        }
    }
    BuildRegionRecords(world);
    return world;
}

// Makes (x,y) a river cell (terrain River, WaterSource::River, land). Flow with FlowRiver.
inline void SetRiver(WorldStructure& world, int x, int y)
{
    Cell& c = world.At(x, y);
    c.waterSource = WaterSource::River;
    c.terrain = TerrainType::River;
}

inline void SetLake(WorldStructure& world, int x, int y)
{
    Cell& c = world.At(x, y);
    c.waterSource = WaterSource::Lake;
    c.terrain = TerrainType::Lake;
    c.biome = ::Draoi::HexWorld::Testing::B("Biome.Lake");
}

// Sets river cell `from`'s downstream direction so it flows into the adjacent cell `to` (odd-q, directions NE N NW
// SW S SE). Returns false when the cells are not adjacent.
inline bool FlowRiver(WorldStructure& world, int fx, int fy, int tx, int ty)
{
    static const int dx[6]     = { 1, 0, -1, -1, 0, 1 };
    static const int dyEven[6] = { -1, -1, -1, 0, 1, 0 };
    static const int dyOdd[6]  = { 0, -1, 0, 1, 1, 1 };
    for (int d = 0; d < 6; ++d)
    {
        if (fx + dx[d] == tx && fy + ((fx & 1) ? dyOdd[d] : dyEven[d]) == ty)
        {
            world.At(fx, fy).downstream = static_cast<uint8_t>(d);
            return true;
        }
    }
    return false;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
