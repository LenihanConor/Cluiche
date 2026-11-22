// S-1 Nodes: Hunting and Resource fill (GDD Part 11). Plain S-1 code, adds Site records of kinds
// BuiltinNodeKinds::Hunting() / Resource() to world.sites; never clears other sites.
//
// Quotas: quota(region) = clamp(round(cellCount / kCellsPerNode), 6, 15) (round = floor(x + 0.5)). When the sum is
// above 150 or below 80 the quotas are scaled by target/sum (target = the nearest bound, 150 or 80), rounded and
// clamped to 6..15; if the sum is still outside 80..150 regions are nudged one at a time by +-1 within the clamps
// (largest cellCount first, ties lowest id, repeating the sweep) until it is in range or no region can move.
// A sum still outside the range then is "node-limited" (never a failure).
//
// Fill count of a region = quota - the built-in nodes (any of the four built-in kinds) already in it.
// Only the four built-in kinds count for quotas and spacing; other registered kinds (game-specific layers) are ignored.
//
// RNG draw order (the caller passes a stream ALREADY reseeded for the fill step (StepSeedNames::nodeFill); this does not reseed):
// regions in ascending id, slots in order; per slot:
//   1. kind: RandomUnit() < BiomeFoodShare(region primary biome) ? Hunting : Resource  (always drawn);
//   2. cell: RandomInt() % totalWeight over eligible cells in ascending index (NOT drawn if no cell is eligible,
//      in which case the slot is dropped).
// Eligible cell: in the region, land and not ocean/Lake, weight CellWeight(kind, cell) > 0, and at least kNodeSpacing
// (hex steps) from every existing built-in node (corridor, river, earlier fill).
//
// Weight = yield(kind, cell biome) * TerrainFactor(kind, cell terrain). Yield: Food for Hunting, Materials for
// Resource, from the GDD table (High 3, Medium 2, Low 1), Food/Materials:
//   Grassland 3/1, TemperateForest 2/3, WetlandDelta 2/1, DesertBasin 1/2, AlpineMountain 1/3 (the spec's "Alpine"),
//   TundraBoreal 1/2, Rainforest 3/2, VolcanicAshland 1/3, Ocean/Lake 0/0 (never eligible).
// TerrainFactor: Hunting Lowland/Hills/Highland 3, Mountain/Cliff 1; Resource Hills/Highland/Mountain/Cliff 3,
// Lowland 1; Peak, Volcano, River, Lake, Pass and water terrain 0 for both. A WaterSource::River cell is excluded too.
//
// Site::variant = 0, Site::flags = 0, Site::region = Cell::region.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Nodes 3)
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <span>
#include <vector>

namespace CoW {
namespace WorldGeneration {

enum class FillKind : uint8_t { Hunting, Resource };

int   BiomeFood(Draoi::HexWorld::BiomeIndex b);        // 0 for water biomes
int   BiomeMaterials(Draoi::HexWorld::BiomeIndex b);   // 0 for water biomes
float BiomeFoodShare(Draoi::HexWorld::BiomeIndex b);   // Food / (Food + Materials); 0 when both are 0
inline bool DrawIsHunting(float draw, float foodShare) { return draw < foodShare; }

int TerrainFactor(FillKind kind, TerrainType t);
// yield * terrain factor; 0 when the cell can never hold a fill node of this kind.
int CellWeight(FillKind kind, const Cell& cell);

struct QuotaResult
{
    std::vector<uint16_t> quota;       // per region, index = id - 1
    uint32_t              total = 0;
    bool                  nodeLimited = false;
};

QuotaResult ComputeNodeQuotas(std::span<const uint16_t> cellCounts, int cellsPerNode);

struct FillResult
{
    bool                  nodeLimited = false;   // quota sum could not reach 80..150, or any slot dropped
    bool                  quotaLimited = false;  // the quota sum could not reach 80..150
    uint32_t              quotaTotal = 0;
    uint16_t              placedHunting = 0;
    uint16_t              placedResource = 0;
    uint16_t              dropped = 0;           // slots with no eligible cell
    std::vector<uint16_t> quota;                 // per region, index = id - 1
    std::vector<uint16_t> existing;              // built-in nodes already in the region on entry
    std::vector<uint16_t> placed;                // fill nodes added per region
};

FillResult PlaceFillNodes(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng);

} // namespace WorldGeneration
} // namespace CoW
