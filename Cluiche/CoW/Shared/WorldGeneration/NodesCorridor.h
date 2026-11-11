// S-1 Nodes: built-in corridor nodes (GDD Part 11). Plain S-1 code, adds Site records of kind
// BuiltinNodeKinds::Corridor() to world.sites; never clears other sites.
//
// Barrier 1 = First Shores / Wild Reaches boundary, barrier 2 = Wild Reaches / Ancient Interiors boundary.
// Candidates are land (non-ocean, non-Lake) cells of either zone with a hex neighbour in the other zone; each takes
// one source, highest priority first: Pass flag, RiverCrossesBarrier flag, coastal gap (no Ridge/Barrier flag within
// 2 steps and a Coast-flag cell within kCoastalGapReach).
//
// RNG draw order (the caller passes a stream ALREADY reseeded for step "CoW.Node.Corridor"; this does not reseed):
//   1. n_1 = 2 + RandomInt() % 3, then n_2 likewise (always drawn);
//   2. Fisher-Yates shuffle of barrier 1 candidates (cell-index order; i from n-1 down to 1, j = RandomInt() % (i+1)),
//      then barrier 2 (empty / single-element lists draw nothing).
// Candidates are then stable-partitioned (passes and rivers before coastal gaps) and accepted in order when at least
// kCorridorSpacing steps from every node of every accepted corridor of the same barrier. A pass is one node; river
// and coastal corridors add up to 2 direct-neighbour boundary cells of the same source (lowest index first), with
// spacing waived inside the corridor.
//
// Site::variant = corridor ordinal within the barrier (0-based).
// Site::flags   = bits 0-1 barrier (1|2), bits 2-3 source (CorridorSource), bits 4-5 width - 1.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Nodes, Corridors)
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

enum class CorridorSourceKind : uint8_t { Pass = 0, River = 1, Coastal = 2 };

inline uint16_t MakeCorridorFlags(int barrier, int source, int width)
{
    return static_cast<uint16_t>((barrier & 3) | ((source & 3) << 2) | (((width - 1) & 3) << 4));
}
inline int CorridorBarrier(uint16_t flags) { return flags & 3; }
inline int CorridorSource(uint16_t flags)  { return (flags >> 2) & 3; }
inline int CorridorWidth(uint16_t flags)   { return ((flags >> 4) & 3) + 1; }

struct CorridorResult
{
    bool     corridorLimited[2]      = {};   // accepted < 2 (geography, never a failure)
    uint8_t  accepted[2]             = {};   // corridors placed per barrier
    uint16_t candidates[2]           = {};
    uint16_t candidatesByType[2][3]  = {};   // [barrier][CorridorSourceKind]
};

CorridorResult PlaceCorridorNodes(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng);

} // namespace WorldGeneration
} // namespace CoW
