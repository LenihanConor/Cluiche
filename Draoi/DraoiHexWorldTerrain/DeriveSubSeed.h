// S-1 Regions, Nodes and Site Hook: deterministic sub-seed derivation.
#pragma once

#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {

// splitmix64 finaliser of (parent ^ key * golden-ratio). Deterministic, well spread.
uint64_t DeriveSubSeed(uint64_t parent, uint64_t key);
// Same mixer, keyed by step.Value().
uint64_t DeriveSubSeed(uint64_t parent, Dia::Core::StringCRC step);

} // namespace HexWorld
} // namespace Draoi
