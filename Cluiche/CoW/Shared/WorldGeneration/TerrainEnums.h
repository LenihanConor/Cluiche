// S-1 World Generation: cell enums shared by TerrainTypes.h, Regions.h and Sites.h.
// Split out of TerrainTypes.h so WorldStructure (TerrainTypes.h) can hold a RegionTable
// and SiteSet without a circular include.
#pragma once

#include <cstdint>

namespace CoW {
namespace World {

// Row index into the BiomeRegistry. Cell::biome and RegionRecord::primaryBiome store one.
using BiomeIndex = uint8_t;
constexpr BiomeIndex kNoBiome = 0xFF;

} // namespace World

namespace WorldGeneration {

enum class WaterSource : uint8_t { None = 0, Ocean, Lake, River };
enum class DepthZone   : uint8_t { None = 0, FirstShores, WildReaches, AncientInteriors }; // None = ocean
enum class TerrainType : uint8_t { DeepWater, ShallowWater, Lake, River, Lowland, Hills, Highland,
                                   Mountain, Peak, Cliff, Pass, Volcano, Count };
enum class BarrierForm : uint8_t { Plateau, VolcanicChain };
namespace TerrainFlag
{
    enum : uint8_t { Coast = 1, Ridge = 2, Barrier = 4, Pass = 8, Volcanic = 16, RiverCrossesBarrier = 32 };
}

// 0 = ocean / unassigned, 1..N = region (Regions.h)
using RegionId = uint8_t;
constexpr RegionId kNoRegion = 0;

} // namespace WorldGeneration
} // namespace CoW
