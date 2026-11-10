// S-1 Biome Classification: Stage 12 (Temperature) and Stage 13 (Moisture).
//
// Stage 12: temperature = clamp(1 - kLatCool*latitude - kLapse*elevation + temperatureOffset, 0, 1).
// Stage 13: waterDistance (multi-source hex-step distance; ocean/lake start 0, river start kRiverPenalty),
//   proximity = exp(-waterDistance / kMoistScale), two column-ordered wind sweeps (westerly blows east, uses
//   upwind dirs 2,3; easterly blows west, uses dirs 0,5) blended by latitude band, then
//   moisture = clamp(kProximityMix*proximity + (1-kProximityMix)*air + kRainBonus*rainOut + moistureOffset, 0, 1).
// Free functions over raw arrays so tests can use hand-built grids. Null output pointers are skipped.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-classification.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

void ComputeStage12(const TerrainRules& rules, GridDimensions dims, float temperatureOffset,
                    const float* elevation, const float* latitude, float* temperatureOut);

struct Stage13Layers
{
    uint32_t* waterDistance = nullptr;
    float*    airMoisture = nullptr;
    float*    rainOut = nullptr;
    float*    moisture = nullptr;
};

// waterSource holds WaterSource values (0 None, 1 Ocean, 2 Lake, 3 River).
// Land with no water on the grid gets waterDistance 0xFFFFFFFF (proximity 0).
void ComputeStage13(const TerrainRules& rules, GridDimensions dims, float moistureOffset,
                    const float* elevation, const float* latitude, const uint32_t* waterSource,
                    Stage13Layers& out);

} // namespace WorldGeneration
} // namespace CoW
