// S-1 Terrain Generator: Stage 6 (Rivers) and Stage 7 (Lakes).
//
// Stage 6. T = kFlowMax * (kFlowMin / kFlowMax)^Rivers; land cells with flowAccumulation >= T are
// candidates (Rivers == 0 forces an empty mask). Candidates group into trees by the ocean cell their
// flow path first reaches (land that no ocean reaches is ignored). A tree is kept whole only if its
// highest source (candidate with no candidate upstream) has filledElevation >= kSourceMinElev and its
// longest source-to-mouth run is >= kMinRiverLength cells. Both tests are monotone in T on the filled
// surface, so a lower threshold never drops a kept tree (river sets nest as Rivers rises).
//
// Stage 7. The first min(Lakes, eligible) basins by rank become lakes; a lake covers every basin cell,
// its surface is the basin's epsilon-free spill height, its outflow is the flow path from the spill
// cell to the sea. lakeMask holds the lake id (1.. in rank order), 0 = no lake.
// Both are free functions over arrays so tests can use hand-built grids.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainDrainage.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct RiverTree
{
    int   mouth = -1;             // ocean cell the tree drains into
    int   cellCount = 0;          // candidate cells in the tree
    float highestSource = 0.0f;   // max filledElevation over the tree's sources
    int   longestRun = 0;         // longest source-to-mouth chain, in candidate cells
    bool  kept = false;
};

struct Stage6Debug
{
    float                  threshold = 0.0f;   // T (0 when Rivers == 0)
    int                    candidates = 0;
    int                    riverCells = 0;
    std::vector<RiverTree> trees;              // ordered by mouth cell index
};

struct Lake
{
    int              basinIndex = -1;    // index into Stage5Debug::basins
    std::vector<int> cells;              // the basin's cells, ascending
    float            surfaceHeight = 0.0f;
    int              spillCell = -1;
    std::vector<int> outflowPath;        // spill cell, then downhill along flowDirection, to the first ocean cell
};

struct Stage7Debug
{
    std::vector<Lake> lakes;             // lake id = index + 1, rank order
};

// riverMask is written 0/1 for every cell. oceanMask non-zero = ocean.
void ComputeStage6(const TerrainRules& rules, GridDimensions dims, float rivers, const float* filledElevation,
                   const uint32_t* flowDirection, const float* flowAccumulation, const uint8_t* oceanMask,
                   uint32_t* riverMask, Stage6Debug& out);

// lakeMask is written for every cell. Returns the effective lake count (min(lakes, eligible)).
int ComputeStage7(GridDimensions dims, int lakes, const Stage5Debug& drainage, const uint32_t* flowDirection,
                  const uint8_t* oceanMask, uint32_t* lakeMask, Stage7Debug& out);

} // namespace WorldGeneration
} // namespace CoW
