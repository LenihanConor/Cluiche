// S-1 Nodes: built-in corridor nodes (GDD Part 11). Plain S-1 code, adds Site records of kind
// BuiltinNodeKinds::Corridor() to world.sites; never clears other sites.
//
// Barrier k separates stratum k from stratum k+1 of the ORDERED depth set (CoW: 1 = First Shores / Wild Reaches,
// 2 = Wild Reaches / Ancient Interiors); a set of N strata has N-1 barriers, none for N = 1 or an unordered set.
// Candidates are land (non-ocean, non-Lake) cells of either stratum with a hex neighbour in the other; each takes
// one source, highest priority first: Pass flag, RiverCrossesBarrier flag, coastal gap (no Ridge/Barrier flag within
// 2 steps and a Coast-flag cell within kCoastalGapReach).
//
// RNG draw order (the caller passes a stream ALREADY reseeded for step "CoW.Node.Corridor"; this does not reseed):
//   1. n_1 = 2 + RandomInt() % 3, then n_2 ... n_(N-1) likewise (always drawn);
//   2. Fisher-Yates shuffle of barrier 1 candidates (cell-index order; i from n-1 down to 1, j = RandomInt() % (i+1)),
//      then barrier 2, ... (empty / single-element lists draw nothing).
// Candidates are then stable-partitioned (passes and rivers before coastal gaps) and accepted in order when at least
// kCorridorSpacing steps from every node of every accepted corridor of the same barrier. A pass is one node; river
// and coastal corridors add up to 2 direct-neighbour boundary cells of the same source (lowest index first), with
// spacing waived inside the corridor.
//
// Site::variant = corridor ordinal within the barrier (0-based).
// Site::flags   = bits 0-1 barrier (low two bits), bits 2-3 source (CorridorSource), bits 4-5 width - 1,
//                 bits 6-8 barrier >> 2 (zero for barriers 1..3, so three-stratum flags are unchanged).
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Nodes, Corridors)
#pragma once

#include <DraoiHexWorld/StratumTable.h>
#include <Shared/WorldGeneration/HexWorldAccess.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

enum class CorridorSourceKind : uint8_t { Pass = 0, River = 1, Coastal = 2 };

// A set of N strata has N-1 barriers.
constexpr uint32_t kMaxBarriers = Draoi::HexWorld::StratumTable::kMaxStrata - 1;

// Barriers of the strata table's depth set: Count()-1 when the set is ordered, else 0.
inline uint32_t BarrierCountOf(const Draoi::HexWorld::StratumTable& strata)
{
    const Draoi::HexWorld::StratumSetSpec* depth = strata.Depth();
    if (depth == nullptr || !depth->ordered || depth->count == 0) { return 0; }
    return depth->count - 1u < kMaxBarriers ? depth->count - 1u : kMaxBarriers;
}

inline uint16_t MakeCorridorFlags(int barrier, int source, int width)
{
    return static_cast<uint16_t>((barrier & 3) | ((source & 3) << 2) | (((width - 1) & 3) << 4) | (((barrier >> 2) & 7) << 6));
}
inline int CorridorBarrier(uint16_t flags) { return (flags & 3) | (((flags >> 6) & 7) << 2); }
inline int CorridorSource(uint16_t flags)  { return (flags >> 2) & 3; }
inline int CorridorWidth(uint16_t flags)   { return ((flags >> 4) & 3) + 1; }

struct CorridorResult
{
    // Indexed by barrier - 1; entries beyond the set's barrier count stay zero / false.
    bool     corridorLimited[kMaxBarriers]      = {};   // accepted < 2 (geography, never a failure)
    uint8_t  accepted[kMaxBarriers]             = {};   // corridors placed per barrier
    uint16_t candidates[kMaxBarriers]           = {};
    uint16_t candidatesByType[kMaxBarriers][3]  = {};   // [barrier][CorridorSourceKind]
};

// Barriers = those of the strata table's depth set (Count()-1 when ordered, else 0).
CorridorResult PlaceCorridorNodes(WorldStructure& world, const TerrainRules& rules, const Draoi::HexWorld::StratumTable& strata,
                                  Dia::Maths::RandomStream& rng);

} // namespace WorldGeneration
} // namespace CoW
