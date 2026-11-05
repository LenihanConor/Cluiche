// S-1 Terrain Generator: Stage 1 (Landmass), steps 1-9 of the spec.
// Part A: continent blobs + peninsula lobes -> potential M (landPotential), land budget L, island
// budget K. Part B: reference mainland M_ref (top L, cleaned), island placement and island
// potential I, mainland + clean-up, landmass labelling / main landmass, coast distance.
//
// The stage is a pure function of (seed fields, sliders, rules, grid). Hex distances, "next to"
// and BFS use the six hex neighbours; the blob distance is the Euclidean distance between cell
// centres in cell units (adjacent cells are 1 apart).
//
// Volcanic layer seam: stage 1 owns only the island vent bit (the first group's centre cell).
// Stage 3 (Barrier) later ORs its chain vents into the same layer and must not clear stage 1's bit.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct SeedFields;

struct Stage1Debug
{
    int  cellCount = 0;            // N
    int  eligibleCount = 0;        // cells with finite M (not in the border exclusion)
    int  landTargetRequested = 0;  // round(Land * N)
    int  landTarget = 0;           // L = requested clamped to eligibleCount
    bool landClamped = false;      // requested > eligibleCount
    int  islandBudget = 0;         // K (0 when Islands == 0)
    std::vector<int> rank;         // eligible cell indices, M descending, ties lowest index first

    // Part B results (masks are size cellCount, 1 = member). Island cells are exempt from clean-up.
    std::vector<uint8_t> refMainland;   // M_ref: top L cells after clean-up (step 5)
    std::vector<uint8_t> mainland;      // top (L - placed island cells) after clean-up (step 7)
    std::vector<uint8_t> islandMask;    // placed island cells
    std::vector<int>     islandGroup;   // group id (0-based, acceptance order) per island cell, else -1
    int islandCapacity = 0;             // eligible island cells (before the budget cap)
    int islandCellsPlaced = 0;
    int landCells = 0;                  // mainland + island cells
    int protectedPeak = -1;             // highest-M mainland cell (never removed by clean-up)

    // 1 for the n highest-M cells (n clamped to [0, eligibleCount]), else 0. Size cellCount.
    std::vector<uint8_t> TopMask(int n) const;

    // M_ref after clean-up (never depends on ContinentIslands).
    const std::vector<uint8_t>& ReferenceMainland() const { return refMainland; }
};

// What the generator actually produced for the requested sliders.
struct Effective
{
    int   islands = 0;          // island groups accepted (<= requested)
    float islandShare = 0.0f;   // island cells / land cells actually reached
    int   continents = 1;       // requested continents clamped to [1, 3]
    int   passes = 0;           // placeholder until the Barrier stage
    int   lakes = 0;            // placeholder until the Lakes stage
    int   eligibleBasins = 0;   // stage 5: eligible basins (the Lakes stage caps lakes at this)
    int   mainLandmass = 0;     // landmass id holding the mainland's highest-M cell (0 = none)
    std::vector<int> islandCentres;   // centre cell per accepted group, in acceptance order
};

// Layer outputs of stage 1 (each sized width*height; null pointers are skipped).
struct Stage1Layers
{
    float*    landPotential = nullptr;
    float*    islandPotential = nullptr;
    uint32_t* landmass = nullptr;
    uint32_t* coastDistance = nullptr;
    uint32_t* volcanic = nullptr;
};

// Steps 1-9.
void ComputeStage1(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, Stage1Layers& layers, Stage1Debug& out, Effective& effective);

// Step 7 clean-up on a 0/1 mask: components smaller than minLandmass become ocean (the component
// holding protectedCell, if >= 0, is kept), then ocean components not connected to the map-edge
// ocean become land.
void CleanUpLand(std::vector<uint8_t>& land, GridDimensions dims, int minLandmass, int protectedCell);

// Step 8: land components numbered 1.. by smallest cell index; ocean = 0. Returns the count.
int LabelLandmasses(const std::vector<uint8_t>& land, GridDimensions dims, uint32_t* out);

// Step 9: land cells get steps to the nearest ocean cell (coast = 1), ocean cells steps to the
// nearest land cell. A map with no ocean (or no land) leaves the unreachable side at 0.
void ComputeCoastDistance(const std::vector<uint8_t>& land, GridDimensions dims, uint32_t* out);

} // namespace WorldGeneration
} // namespace CoW
