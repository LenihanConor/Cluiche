// S-1 Regions: region partition, first half (targets, zone components, seeds, growth).
// Repair, records and renumbering are the second half (not here): this result carries provisional ids.
//
// RNG draw order (one passed stream, nothing else draws):
//   1. desired region count n_Z, one draw per zone in zone order: First Shores, Wild Reaches, Ancient Interiors
//      (always drawn, even when the zone has no land, so the budget is fixed);
//   2. first seed of each component with at least kMinRegionCells cells, components in ascending
//      min-cell-index order (one draw each; tiny components draw nothing).
// All ties: lowest cell index, then lowest region id.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Region partition)
#pragma once

#include <Shared/WorldGeneration/Regions.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <vector>

namespace CoW { namespace World { class BiomeRegistry; } }

namespace CoW {
namespace WorldGeneration {

// Zone slot 0..2 <-> DepthZone FirstShores..AncientInteriors.
constexpr int kRegionZoneCount = 3;
constexpr uint8_t kZoneCountMin[kRegionZoneCount] = { 4, 4, 2 };
constexpr uint8_t kZoneCountMax[kRegionZoneCount] = { 6, 6, 4 };
constexpr int kTotalRegionsMin = 10;
constexpr int kTotalRegionsMax = 16;

inline int ZoneSlot(DepthZone z) { return static_cast<int>(z) - 1; }   // None -> -1
inline DepthZone ZoneFromSlot(int s) { return static_cast<DepthZone>(s + 1); }

// A connected set of main-landmass land cells (non-ocean) of one depth zone.
struct RegionComponent
{
    DepthZone             zone     = DepthZone::None;
    uint8_t               landmass = 0;
    std::vector<uint32_t> cells;            // ascending cell index
    bool                  tiny     = false; // fewer than kMinRegionCells cells: gets no seed (second half handles it)
};

struct RegionTargets
{
    uint8_t   count[kRegionZoneCount]     = {};   // final region count per zone
    uint16_t  capacity[kRegionZoneCount]  = {};   // floor(cells_Z / kMinRegionCells), 0 without a non-tiny component
    uint32_t  zoneCells[kRegionZoneCount] = {};
    uint8_t   total                       = 0;
    DepthZone limitedBy                   = DepthZone::None;   // None unless terrain-limited
};

struct RegionSeed
{
    uint32_t  cell      = 0;                    // island regions: lowest cell index of the landmass
    Biome     biome     = Biome::Ocean;         // seed biome (drives the growth cost)
    DepthZone zone      = DepthZone::None;      // island regions: majority zone
    uint8_t   landmass  = 0;
    bool      isIsland  = false;
    int32_t   component = -1;                   // index into RegionGrowth::components, -1 for islands
};

struct RegionGrowth
{
    RegionTargets                targets;
    std::vector<RegionComponent> components;
    std::vector<uint32_t>        tinyComponents;     // indices into components
    std::vector<uint32_t>        unzonedCells;       // main-landmass non-ocean cells with DepthZone::None
    std::vector<RegionId>        regionOf;           // per cell; 0 = ocean, tiny/unzoned (second half repairs)
    std::vector<RegionSeed>      seeds;              // index = provisional region id - 1
    std::vector<uint16_t>        capClaims;          // per region: cells claimed before the leftover phase
    uint32_t                     leftoverCells = 0;  // cells claimed in the leftover phase
    uint8_t                      mainlandRegionCount = 0;
    bool                         overflow = false;   // more than RegionTable::kMaxRegions regions needed; extras not created
};

// Land = non-ocean cell with a landmass id (lakes and rivers are ordinary land here).
bool IsRegionLand(const Cell& c);

// Connected components of main-landmass land cells per zone, ascending min cell index.
std::vector<RegionComponent> FindZoneComponents(const WorldStructure& world, const TerrainRules& rules);

// Spec step 2 rebalance in place. total < 10: add one at a time to the zone with the most unused capacity
// (capacity - count) among zones below their range maximum with unused capacity, ties by zone order.
// total > 16: remove one at a time from the zone with the least spare capacity (capacity - count) among zones
// above their range minimum, ties by zone order. If the total still cannot reach 10, limitedBy = the zone with
// the smallest capacity relative to its range minimum (capacity / min, ties by zone order); else None.
void RebalanceRegionCounts(uint8_t count[kRegionZoneCount], const uint16_t capacity[kRegionZoneCount], DepthZone& limitedBy);

// Draws n_Z per zone (zone order), clamps to capacity, rebalances.
RegionTargets ComputeRegionTargets(const WorldStructure& world, const TerrainRules& rules,
                                   const std::vector<RegionComponent>& components, Dia::Maths::RandomStream& rng);
RegionTargets ComputeRegionTargets(const WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng);

// Seeds per non-tiny component: share = max(1, floor(quota)) with quota = n_Z * compCells / (cells of the zone's
// non-tiny components); the remaining n_Z - sum go to the largest fractional quotas (largest remainder, ties by
// component order). If the zone has more non-tiny components than n_Z the sum exceeds n_Z (each still gets 1).
// With minRegionCells > 0 a component's share is capped at max(1, floor(cells / minRegionCells)) (it cannot hold more
// regions of that size) and the surplus goes to components with room; if none has room the zone gets fewer regions.
// Result is indexed like `components`; tiny components get 0.
std::vector<int> AllocateSeedShares(const std::vector<RegionComponent>& components, const RegionTargets& targets,
                                    int minRegionCells = 0);

// Farthest-point sampling inside one component. First seed: cells[rng.RandomInt() % size]; each next seed
// maximises minHexDistance(seeds) + kSeedBiomeBonus * [biome differs from every seed's biome], ties lowest cell.
std::vector<uint32_t> PlaceSeeds(const WorldStructure& world, const TerrainRules& rules, const RegionComponent& comp,
                                 int seedCount, Dia::Maths::RandomStream& rng, const World::BiomeRegistry& biomes);

// Multi-source Dijkstra growth for every non-tiny component (seeds[i] belongs out.components[i]; empty = skipped),
// then one provisional region per island landmass. Needs out.components; fills the rest of `out`.
void GrowRegions(const WorldStructure& world, const TerrainRules& rules, const std::vector<std::vector<uint32_t>>& seeds,
                 RegionGrowth& out);

// Targets + components + shares + seeds + growth in one call.
RegionGrowth BuildProvisionalRegions(const WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng,
                                     const World::BiomeRegistry& biomes);

} // namespace WorldGeneration
} // namespace CoW
