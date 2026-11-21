// Core cell vocabulary: the enums and ids a world cell stores and sims read off it.
// Split out of the former TerrainEnums.h so BiomeRegistry and the world types have no
// include dependency on the generator. Namespaces are unchanged (pure move).
#pragma once

#include <cstdint>

namespace Draoi {
namespace HexWorld {

// Row index into the BiomeRegistry. Cell::biome and RegionRecord::primaryBiome store one.
using BiomeIndex = uint8_t;
constexpr BiomeIndex kNoBiome = 0xFF;



// Stratum: a plain index into the StratumTable. 0 = none/ocean, 1..N = stratum k (strata k and k+1 are separated by
// barrier k). The names live in data (World/StratumTable.h), not in code.
using StratumIndex = uint8_t;
constexpr StratumIndex kNoStratum = 0;

enum class WaterSource : uint8_t { None = 0, Ocean, Lake, River };
enum class TerrainType : uint8_t { DeepWater, ShallowWater, Lake, River, Lowland, Hills, Highland,
                                   Mountain, Peak, Cliff, Pass, Volcano, Count };
namespace TerrainFlag
{
    enum : uint8_t { Coast = 1, Ridge = 2, Barrier = 4, Pass = 8, Volcanic = 16, RiverCrossesBarrier = 32 };
}

// 0 = ocean / unassigned, 1..N = region (Regions.h)
using RegionId = uint8_t;
constexpr RegionId kNoRegion = 0;

} // namespace HexWorld
} // namespace Draoi
