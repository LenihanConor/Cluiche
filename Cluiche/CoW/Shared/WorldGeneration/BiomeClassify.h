// S-1 Biome Classification: Stage 14 (Biomes).
//
// 1 pins (in the registry's priority order; the shipped file has water 5/4, vent 3, alpine 2, wetland 1), 2 plateau interior, 3 climate score, 4 zone fix-up
// (vent cells keep the vent biome), 5 forbidden-neighbour fix-up. All ties: lowest cell index, then lowest registry
// row index. Free functions over raw arrays so tests can use hand-built grids.
//
// Vent definition: the terrain classifier already sets TerrainFlag::Volcanic on every non-ocean cell within
// kVentRadius hex steps of a vent (island vent and chain vents share the `volcanic` layer), and marks the vent
// itself terrain Volcano. So "within kVentRadius of a vent" == Volcanic flag OR terrain Volcano; no extra input.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-classification.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

// Forward declarations only: BiomeRegistry.h pulls windows.h, which this widely included header must not.
namespace CoW { namespace World { class BiomeRegistry; using BiomeIndex = uint8_t; } }

namespace CoW {
namespace WorldGeneration {

// biomePin values.
namespace BiomePin
{
    enum : uint32_t { None = 0, Wetland = 1, Alpine = 2, Volcanic = 3, Water = 4 };
}

// fixupChanges values.
namespace BiomeFixup
{
    enum : uint32_t { None = 0, Zone = 1, Forbidden = 2 };
}

struct Stage14Inputs
{
    GridDimensions  dims;
    const float*    temperature = nullptr;
    const float*    moisture = nullptr;
    const float*    elevation = nullptr;
    const uint32_t* terrainType = nullptr;    // TerrainType values
    const uint32_t* depthZone = nullptr;      // DepthZone values
    const uint32_t* terrainFlags = nullptr;   // TerrainFlag bits
    const float*    barrierMask = nullptr;    // optional
    const uint32_t* waterSource = nullptr;    // WaterSource values
    BarrierForm     barrierForm = BarrierForm::Plateau;
    float           barrier = 0.0f;           // slider 0..1
    const World::BiomeRegistry* biomes = nullptr;   // required: pins, climate rows, plateau rows, zones, forbidden pairs
};

struct Stage14Layers
{
    float*    biomeScore = nullptr;
    uint32_t* biomePin = nullptr;
    uint32_t* biomeIndex = nullptr;    // BiomeRegistry row index
    uint32_t* fixupChanges = nullptr;
};

struct Stage14Debug
{
    int unresolvedPairs = 0;   // forbidden neighbour pairs left, once per unordered pair
    int zoneChanges = 0;       // cells changed by step 4
    int forbiddenChanges = 0;  // cells changed by step 5
    int passes = 0;            // step 5 passes that changed something
};

// -((T-T0)/wT)^2 - ((M-M0)/wM)^2 from the registry row's climate envelope.
// A row without a climate envelope (or an unknown row) returns the lowest float.
float BiomeClimateScore(const World::BiomeRegistry& biomes, World::BiomeIndex b, float temperature, float moisture);

// River cell with an ocean neighbour.
bool IsRiverMouth(const Stage14Inputs& in, int cell);

// Spec stage 14 "Wetland-eligible".
bool IsWetlandEligible(const TerrainRules& rules, const Stage14Inputs& in, int cell);

// True for cells within kVentRadius of a vent (Volcanic flag or terrain Volcano).
bool IsVentCell(const Stage14Inputs& in, int cell);

void ComputeStage14(const TerrainRules& rules, const Stage14Inputs& in, Stage14Layers& out, Stage14Debug& dbg);

} // namespace WorldGeneration
} // namespace CoW
