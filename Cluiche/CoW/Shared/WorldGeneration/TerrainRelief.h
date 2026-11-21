// S-1 Terrain Generator: Stage 3 (Barrier) and Stage 4 (Elevation).
// Stage 3 builds the second barrier, a closed band on the main landmass at a fixed depth inland
// (kBarrierQuantile of the coast distance), as a plateau rim or a volcanic chain with vents.
// Stage 4 sums the elevation terms (lowland rise, mountains, barrier, vents, ruggedness) into
// elevationRaw and writes the ocean shelf depth.
//
// Volcanic layer seam: bit 1 (kVolcanicIslandBit) belongs to stage 1 (island vent), bit 2
// (kVolcanicChainBit) to stage 3. Stage 3 clears and rewrites only its own bit, so a re-run never
// touches the island vent. Stage 4 treats any non-zero value as a vent.
//
// Map space for the noise is the same as stages 0-1 (centres / maximum centre Y, 1 tall).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct SeedFields;

constexpr uint32_t kVolcanicIslandBit = 1u;
constexpr uint32_t kVolcanicChainBit  = 2u;

// Noise frequencies in map space (guesses; the spec gives none).
constexpr float kBarrierWobbleFrequency = 6.0f;
constexpr float kVentFrequency          = 8.0f;
constexpr float kRidgedFrequency        = 10.0f;
constexpr float kRuggedFrequency        = 14.0f;

struct Stage3Debug
{
    int                mainCells = 0;       // main-landmass cells
    float              bStar = 0.0f;        // quantile of coastDistance over the main landmass
    std::vector<float> band;                // per cell, 0 off the main landmass
    std::vector<int>   chainVents;          // accepted chain vent cells, in acceptance order
};

struct Stage3Layers
{
    float*    barrierMask = nullptr;   // size width*height
    uint32_t* volcanic = nullptr;      // read-modify-write: only kVolcanicChainBit is touched
};

struct Stage4Layers
{
    float* elevationRaw = nullptr;
    float* oceanDepth = nullptr;
};

// landmass / coastDistance are the stage 1 layers (size width*height); mainLandmass is the id of the
// main landmass (0 = none).
void ComputeStage3(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const uint32_t* landmass, const uint32_t* coastDistance,
                   int mainLandmass, Stage3Layers& layers, Stage3Debug& out);

// ridgeMask is stage 2's layer; barrierMask / volcanic are stage 3's.
void ComputeStage4(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const uint32_t* landmass, const uint32_t* coastDistance,
                   const float* ridgeMask, const float* barrierMask, const uint32_t* volcanic,
                   Stage4Layers& layers);

} // namespace WorldGeneration
} // namespace CoW
