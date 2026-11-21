// S-1 World Generation: cell enums shared by TerrainTypes.h, Regions.h and Sites.h.
// The cell vocabulary (BiomeIndex, DepthZone, TerrainType, ...) lives in World/CellVocabulary.h;
// this header re-exports it and adds the generator-only options.
#pragma once

#include <Shared/World/CellVocabulary.h>

namespace CoW {
namespace WorldGeneration {

enum class BarrierForm : uint8_t { Plateau, VolcanicChain };

} // namespace WorldGeneration
} // namespace CoW
