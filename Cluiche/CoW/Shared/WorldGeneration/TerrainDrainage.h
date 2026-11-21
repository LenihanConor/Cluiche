// S-1 Terrain Generator: Stage 5 (Drainage).
// Priority-flood fill of elevationRaw from every ocean cell, basin detection and ranking, steepest-
// descent flow direction on the filled surface and highland-weighted flow accumulation.
//
// Fill. Each cell carries two values:
//   level  = epsilon-free fill level, max(raw, level(parent)): the classic priority-flood surface;
//   filled = max(raw, filled(parent) + kFillEpsilon): the published filledElevation.
// The flood pops cells in strict lexicographic order of (level, filled, cell index). Popping by
// level first makes level exact (a basin fills exactly to its pour point); filled breaks ties on
// equal levels so a flat floods outward from its outlet (shortest eps-paths); the cell index makes
// the order a strict total order, so the result does not depend on the heap implementation.
// filled(cell) >= filled(parent) + eps > filled(parent) for every flooded land cell, so every land
// cell has a strictly descending path (its flood parent chain) to the ocean on filledElevation.
//
// Basins. basinDepth = level - raw (values below kBasinDepthFloor become 0). The spec writes
// filledElevation - elevationRaw; that difference includes the eps ramp, which on a flat or a
// gentle slope (rise < eps per cell) grows by 1e-5 per cell and passes 1e-4 after ~10 cells, so a
// long flat would read as a spurious basin. level carries no eps, so flats and monotone slopes
// have depth exactly 0, and spill height / volume / max depth are epsilon-free.
//
// Flow direction: largest drop on filledElevation, ties by the lowest direction (strict '>'
// scanning d = 0..5). Encoding 0..5 = HexTopology directions (NE, N, NW, SW, S, SE), kFlowSink = 6
// for ocean cells and for land that no ocean reaches (a grid without ocean: degenerate, filled = raw).
//
// Flow accumulation: land cells in descending filledElevation (ties by ascending cell index) add
// their total to their downstream land cell; ocean cells keep 0 (water arriving at the sea is the
// sum of the accumulation of the land cells draining straight into it).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

constexpr float    kFillEpsilon     = 1e-5f;
constexpr float    kBasinDepthFloor = 1e-4f;
constexpr uint32_t kFlowSink        = 6u;

struct Basin
{
    int              firstCell = -1;      // smallest cell index in the basin
    float            volume = 0.0f;       // sum of basinDepth, accumulated in ascending cell order
    float            maxDepth = 0.0f;
    float            spillHeight = 0.0f;  // epsilon-free fill level of the basin
    int              spillCell = -1;      // pour point: where the basin's lowest filled cell drains to
    bool             eligible = false;    // maxDepth >= kMinBasinDepth && cells >= 2
    uint32_t         rank = 0;            // 1-based among eligible basins, 0 = not eligible
    std::vector<int> cells;               // ascending cell index
};

struct Stage5Debug
{
    std::vector<Basin> basins;     // every basin (depth > 0 component), ordered by firstCell
    std::vector<int>   ranked;     // indices into basins of the eligible ones, in rank order
    std::vector<float> fillLevel;  // per cell epsilon-free fill level
    int                eligibleBasins = 0;
};

struct Stage5Layers
{
    float*    filledElevation = nullptr;
    float*    basinDepth = nullptr;
    uint32_t* basinRank = nullptr;
    uint32_t* flowDirection = nullptr;
    float*    flowAccumulation = nullptr;
};

// elevationRaw and oceanMask (non-zero = ocean) are width*height. All five layers must be non-null
// for anything to be written. Uses kMinBasinDepth and kHighlandRain from the rules.
void ComputeStage5(const TerrainRules& rules, GridDimensions dims, const float* elevationRaw,
                   const uint8_t* oceanMask, Stage5Layers& layers, Stage5Debug& out);

} // namespace WorldGeneration
} // namespace CoW
