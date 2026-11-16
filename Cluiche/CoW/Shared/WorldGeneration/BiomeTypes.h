// S-1 Biome Classification: biome vocabulary, zone validity and forbidden adjacency.
// The Biome enum itself lives in TerrainEnums.h so Cell can store it.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-classification.md
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>

namespace CoW {
namespace WorldGeneration {

// e.g. "Biome.Grassland"
Dia::Core::StringCRC BiomeId(Biome b);

// Display name, "Unknown" for Biome::Count / out of range.
const char* BiomeName(Biome b);

// Returns false (and leaves out unchanged) for ids that are not a biome id.
bool BiomeFromId(Dia::Core::StringCRC id, Biome& out);

// Which biomes may occupy which depth zone. Ocean/Lake only in DepthZone::None
// (see spec stage 14 zone table); land biomes never in DepthZone::None.
bool IsBiomeValidInZone(Biome b, DepthZone z);

// Symmetric forbidden adjacency pairs (spec stage 14).
bool AreBiomesForbidden(Biome a, Biome b);

} // namespace WorldGeneration
} // namespace CoW
