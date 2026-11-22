// S-1 Terrain Generator: Stage 8 (Strata).
//
// Depth strata come from the continent's shape: how far a cell is from the coast, counting barriers
// as costly to cross and river valleys as corridors through them.
//   zoneCostBase: multi-source Dijkstra over every land cell (lakes are land in the landmass layer),
//     sources = coast cells (land with coastDistance == 1) at cost 1 regardless of their own ridge or
//     barrier terms. Entering cell c costs 1 + kRidgeCost*ridge(c) + kBarrierCost*Barrier*barrierMask(c).
//     Landmasses are disconnected by ocean, so one run covers each independently.
//   zoneCost: same Dijkstra, but entering a river cell costs 1 + kRiverCorridor*(ridge and barrier terms).
//     Never above zoneCostBase.
//   Banding into N strata (the set's strata in order; N = 1..kMaxStrata). Main-landmass base costs, sorted
//   ascending, m cells; C(k) is the cumulative share of strata 1..k:
//     end cut k (k = 1..N-2) = value at index max(0, floor(C(k)*m) - 1)   ("highest cost in the lowest C(k)")
//       stratum k is every cost <= end cut k not already taken. A cut that is not above the previous one moves to
//       the next distinct base cost above it; none exists -> this and every later cut collapse onto the previous
//       value and those strata are empty. A stratum with share 0 (k >= 2) is empty without moving the cut.
//     last cut (N >= 2) = value at index m - ceil(share(N)*m)   ("lowest cost in the highest share(N)")
//       with the same fix-up against the previous end cut (against the lowest cost when N = 2); none ->
//       hasAncient = false and the last stratum is empty (its cut is set equal to the previous one and unused).
//     With no main landmass every cut is 1 (coast ring only) and the last stratum is empty.
//   Cell stratum: the first k with zoneCost <= end cut k; otherwise the last stratum when zoneCost >= the last cut,
//     else stratum N-1. Then each stratum's rules apply, falling to the previous stratum on failure and stopping at
//     stratum 1 (which has no rule): a main_landmass_only stratum is unavailable off the main landmass, and a stratum
//     with min_coast_depth d needs coastDistance >= d. Ocean: both costs 0, stratum None.
//   For a three-stratum set this is the shallowest stratum where zoneCost <= t1, the deepest where zoneCost >= t2
//     on the main landmass at coast distance >= its min_coast_depth, the middle one elsewhere (t1 = end cut 1, t2 = the
//     last cut). The depth value comes from the stratum table, not from TerrainRules.
// Free functions over arrays so tests can use hand-built grids.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <DraoiHexWorldTerrain/TerrainRules.h>
#include <DraoiHexWorldTerrain/TerrainTypes.h>

#include <DraoiHexWorld/StratumTable.h>
#include <DraoiHexWorld/WorldStructure.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {

struct Stage8Debug
{
    static constexpr uint32_t kMaxCuts = Draoi::HexWorld::StratumTable::kMaxStrata;
    // For three strata t1 is the first end cut and t2 the last cut; with two, t1 is the lowest cost; with one, both are.
    float t1 = 0.0f;
    float t2 = 0.0f;
    bool  hasAncient = false;   // false when the last stratum is empty (no distinct base cost above the previous cut, or share 0)
    int   mainCells = 0;        // land cells of the main landmass
    uint32_t cutCount = 0;      // N-1 for N >= 2, else 0
    float cuts[kMaxCuts] = {};  // cuts[k-1] = end cut k (k <= N-2); cuts[N-2] = the last cut
};

struct Stage8Layers
{
    float*    zoneCostBase = nullptr;
    float*    zoneCost = nullptr;
    uint32_t* stratum = nullptr;   // StratumIndex values
};

// Dijkstra described above. riverMask may be null (no rivers). Writes every cell of out (ocean = 0).
// Land that no coast cell reaches (a grid with no ocean) is given cost 1.
void ComputeZoneCost(const TerrainRules& rules, GridDimensions dims, float barrier, const uint32_t* landmass,
                     const uint32_t* coastDistance, const float* ridgeMask, const float* barrierMask,
                     const uint32_t* riverMask, float* out);

// Both Dijkstra runs: zoneCostBase (no rivers) and zoneCost (river corridors). Writes every cell of both (ocean = 0).
void ComputeStrataCosts(const TerrainRules& rules, GridDimensions dims, float barrier, const uint32_t* landmass,
                        const uint32_t* coastDistance, const float* ridgeMask, const float* barrierMask,
                        const uint32_t* riverMask, float* zoneCostBase, float* zoneCost);

// The banding above over the costs already computed. Writes every cell of stratum (ocean and unassigned = 0).
// A set with no strata leaves every cell 0.
void BandStrata(const Draoi::HexWorld::StratumSetSpec& set, GridDimensions dims, int mainLandmass, const uint32_t* landmass,
                const uint32_t* coastDistance, const float* zoneCostBase, const float* zoneCost, StratumIndex* stratum,
                Stage8Debug& dbg);

// Costs then banding for any stratum set (N strata). Output strata go to out.stratum (StratumIndex values).
void ComputeStage8(const TerrainRules& rules, const Draoi::HexWorld::StratumSetSpec& set, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg);

// The same for the table's depth set (an empty table assigns nothing).
void ComputeStage8(const TerrainRules& rules, const Draoi::HexWorld::StratumTable& strata, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg);

} // namespace HexWorld
} // namespace Draoi
