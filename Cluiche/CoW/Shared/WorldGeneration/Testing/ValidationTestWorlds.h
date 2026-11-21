// S-1 Reachability Validator test support: hand-built worlds for navigation and guarantee tests. Header-only.
// Builds on RegionTestWorlds.h (row-major, index = y * width + x). Land cells are Lowland, everything else is the
// default Cell (DeepWater, waterSource None), so BlockWaterProfile blocks it.
#pragma once

#include <Shared/WorldGeneration/Testing/RegionTestWorlds.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/Sites.h>
#include <Shared/WorldGeneration/ValidationTypes.h>

#include <DiaPathfinding/HexTopology.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

inline uint32_t Idx(const WorldStructure& world, int x, int y) { return static_cast<uint32_t>(y * world.dimensions.width + x); }

inline void SetTerrain(WorldStructure& world, int x, int y, TerrainType t) { world.At(x, y).terrain = t; }

// Adds a site (kind by id string) at (x,y). False if one of that kind is already there.
inline bool AddSiteAt(WorldStructure& world, const char* kindId, int x, int y, uint16_t variant = 0)
{
    Site s;
    s.kindCrc = Dia::Core::StringCRC(kindId).Value();
    s.cell = Idx(world, x, y);
    s.variant = variant;
    return world.sites.Add(s);
}

inline bool AddSiteAt(WorldStructure& world, const Dia::Core::StringCRC& kind, int x, int y, uint16_t variant = 0)
{
    Site s;
    s.kindCrc = kind.Value();
    s.cell = Idx(world, x, y);
    s.variant = variant;
    return world.sites.Add(s);
}

// All-land w x h world (one zone), no water.
inline WorldStructure MakeAllLandWorld(int w, int h, DepthZone zone = DepthZone::WildReaches)
{
    WorldStructure world = MakeBlankWorld(w, h);
    FillLand(world, 0, 0, w, h, zone, ::CoW::World::Testing::B("Biome.Grassland"));
    return world;
}

// Two land blocks split by a water channel of 'channel' columns starting at column x0: left block columns < x0,
// right block columns >= x0 + channel. The channel is DeepWater, so the grid is connected through it but a
// water-blocking profile is not.
inline WorldStructure MakeChannelWorld(int w, int h, int x0, int channel)
{
    WorldStructure world = MakeBlankWorld(w, h);
    FillLand(world, 0, 0, x0, h, DepthZone::WildReaches, ::CoW::World::Testing::B("Biome.Grassland"));
    FillLand(world, x0 + channel, 0, w, h, DepthZone::WildReaches, ::CoW::World::Testing::B("Biome.Grassland"));
    return world;
}

// Hex steps between two cells of the world.
inline uint32_t HexSteps(const WorldStructure& world, uint32_t a, uint32_t b)
{
    const Dia::Pathfinding::HexTopology topo(world.dimensions.width, world.dimensions.height);
    return Dia::Pathfinding::HexTopology::Distance(topo.ToCoord(a), topo.ToCoord(b));
}

// True if every consecutive pair of the path is a hex neighbour and the path runs from -> to.
inline bool IsValidWalk(const WorldStructure& world, const ProofPath& p)
{
    if (p.cells.IsEmpty() || p.cells[0] != p.from || p.cells[p.cells.Size() - 1] != p.to) { return false; }
    for (unsigned i = 1; i < p.cells.Size(); ++i)
    {
        if (HexSteps(world, p.cells[i - 1], p.cells[i]) != 1) { return false; }
    }
    return true;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
