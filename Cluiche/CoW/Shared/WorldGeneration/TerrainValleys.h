// S-1 Terrain Generator: Stage 9 (Valleys).
//
// Carving: each kept river cell is lowered by carve = ValleyDepth * kCarveMax * size, where
//   size = 0.4 + 0.6 * clamp(log(acc / T) / log(kCarveFullAcc), 0, 1)  (kCarveFullAcc is the multiplier 8,
//   so size reaches 1 at acc = 8T). Land cells (not river, lake or ocean) one hex step from a river get
//   0.5 x the deepest adjacent river carve; cells at hex distance exactly two get 0.2 x the deepest carve
//   among river cells at distance two. valleyCarve stores that applied carve (0 for lake and ocean cells).
// New elevation e = max(kCoastElev, elevationRaw - valleyCarve) on land, 0 on ocean.
// Lake floors: a river cell's floor is the surface of the lake it ultimately drains into, else kCoastElev.
//   River cells are raised back to their floor.
// Downhill walk: river and lake cells in increasing flowAccumulation order (ties by index). For each, if the
//   downstream cell is a river cell: e(ds) = max(floor(ds), min(e(ds), e(c))). Lake cells act as sources at their
//   surface height, so a lake outlet is never above the lake surface. Only river cells are ever modified.
// Lake cells take their surface height. Every operation is monotone in the carve, so elevation never
// rises as ValleyDepth rises.
// Free functions over arrays so tests can use hand-built grids.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

struct Stage9Debug
{
    int   riverCells = 0;
    int   carvedCells = 0;      // cells with valleyCarve > 0
    float maxCarve = 0.0f;
    int   raisedByFloor = 0;    // river cells raised back to their lake/coast floor
    int   loweredByWalk = 0;    // river cells lowered by the downhill walk
};

struct Stage9Layers
{
    float* valleyCarve = nullptr;
    float* elevation = nullptr;
};

// Size factor for a river cell of accumulation acc given stage-6 threshold T.
float ValleyCarveSize(const TerrainRules& rules, float acc, float threshold);

// lakeSurface[id - 1] is the surface height of lake id (lakeCount entries). Ocean is elevationRaw <= 0.
// riverMask / lakeMask / lakeSurface / flow arrays may be null (none). Writes every cell.
void ComputeStage9(const TerrainRules& rules, GridDimensions dims, float valleyDepth, float threshold,
                   const float* elevationRaw, const uint32_t* flowDirection, const float* flowAccumulation,
                   const uint32_t* riverMask, const uint32_t* lakeMask, const float* lakeSurface, int lakeCount,
                   Stage9Layers out, Stage9Debug& dbg);

} // namespace WorldGeneration
} // namespace CoW
