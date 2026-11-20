// Biome registry for mapping biome identifiers to properties.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md
#pragma once

#include <cstdint>

namespace CoW {
namespace World {

using BiomeIndex = uint8_t;
constexpr BiomeIndex kNoBiome = 0xFF;

class BiomeRegistry
{
};

} // namespace World
} // namespace CoW
