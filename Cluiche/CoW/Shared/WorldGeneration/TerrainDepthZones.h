// S-1 Terrain Generator: Stage 8 (DepthZones).
//
// Depth zones come from the continent's shape: how far a cell is from the coast, counting barriers
// as costly to cross and river valleys as corridors through them.
//   zoneCostBase: multi-source Dijkstra over every land cell (lakes are land in the landmass layer),
//     sources = coast cells (land with coastDistance == 1) at cost 1 regardless of their own ridge or
//     barrier terms. Entering cell c costs 1 + kRidgeCost*ridge(c) + kBarrierCost*Barrier*barrierMask(c).
//     Landmasses are disconnected by ocean, so one run covers each independently.
//   zoneCost: same Dijkstra, but entering a river cell costs 1 + kRiverCorridor*(ridge and barrier terms).
//     Never above zoneCostBase.
//   Thresholds (main landmass base costs, sorted ascending, n cells):
//     t1 = value at index max(0, floor(kFirstShoresShare*n) - 1)   ("highest cost in the lowest 45%")
//     t2 = value at index n - ceil(kAncientShare*n)                 ("lowest cost in the highest 15%")
//     if t2 <= t1, t2 moves to the next distinct base cost above t1; none -> hasAncient = false
//     (then t2 is set equal to t1 and is unused). With no main landmass t1 = 1 (coast ring only).
//   Zones: First Shores where zoneCost <= t1. Ancient Interiors where zoneCost >= t2 and coastDistance >=
//     kAncientMinDepth, main landmass only (failing the depth test = Wild Reaches). Wild Reaches elsewhere.
//     Other landmasses use t1 only. Ocean: both costs 0, zone None.
// Free functions over arrays so tests can use hand-built grids.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

struct Stage8Debug
{
    float t1 = 0.0f;
    float t2 = 0.0f;
    bool  hasAncient = false;   // false when no distinct base cost above t1 exists (or kAncientShare == 0)
    int   mainCells = 0;        // land cells of the main landmass
};

struct Stage8Layers
{
    float*    zoneCostBase = nullptr;
    float*    zoneCost = nullptr;
    uint32_t* depthZone = nullptr;   // DepthZone values
};

// Dijkstra described above. riverMask may be null (no rivers). Writes every cell of out (ocean = 0).
// Land that no coast cell reaches (a grid with no ocean) is given cost 1.
void ComputeZoneCost(const TerrainRules& rules, GridDimensions dims, float barrier, const uint32_t* landmass,
                     const uint32_t* coastDistance, const float* ridgeMask, const float* barrierMask,
                     const uint32_t* riverMask, float* out);

void ComputeStage8(const TerrainRules& rules, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg);

} // namespace WorldGeneration
} // namespace CoW
