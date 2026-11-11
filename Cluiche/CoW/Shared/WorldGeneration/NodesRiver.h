// S-1 Nodes: built-in river nodes (GDD Part 11). Plain S-1 code, adds Site records of kind
// BuiltinNodeKinds::River() to world.sites; never clears other sites. No RNG: fully deterministic.
//
// A river tree = the river cells (WaterSource::River, on land) that drain through Cell::downstream into one mouth
// cell. The mouth is the river cell whose downstream neighbour is ocean or Lake. River cells count as land for
// node purposes (the "never ocean or Lake" rule is about WaterSource). Trees are numbered 0.. by mouth cell index.
// Rivers that never reach ocean or a lake (no mouth) get no node.
//
// Per tree: a node on the mouth (step 0), then every kRiverNodeSpacing steps upstream along the longest path
// (the deepest cell by steps from the mouth, ties lowest cell index, then followed downstream to the mouth).
// Trees on a landmass other than world.mainLandmass get the mouth node only. Upstream cells are skipped when not
// claimable (main landmass, and with a region when the world has regions). The step grid is fixed, so a skipped
// cell leaves a gap of a multiple of kRiverNodeSpacing.
//
// Spacing (kNodeSpacing): mouth nodes are always placed (mouth coverage wins over spacing). An upstream node is
// dropped when it is closer than kNodeSpacing to an existing corridor node or to a river node already placed
// (other trees; same-tree nodes are kRiverNodeSpacing apart), EXCEPT on a RiverCrossesBarrier cell, where it
// coexists with a corridor node (spacing waived). Placement order: all mouths (ordinal order), then upstream nodes
// by tree ordinal then step.
//
// Site::variant = tree ordinal. Site::flags bit 0 = isMouth.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Nodes 2)
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

inline uint16_t MakeRiverFlags(bool isMouth) { return isMouth ? 1 : 0; }
inline bool     RiverIsMouth(uint16_t flags)  { return (flags & 1) != 0; }

struct RiverNodeResult
{
    uint16_t trees              = 0;   // river trees that reach ocean or a lake
    uint16_t mouthNodes         = 0;   // mouth nodes added
    uint16_t upstreamNodes      = 0;   // non-mouth nodes added
    uint16_t skippedSpacing     = 0;   // upstream nodes dropped by spacing
    uint16_t skippedUnclaimable = 0;   // upstream step cells skipped as not claimable
};

RiverNodeResult PlaceRiverNodes(WorldStructure& world, const TerrainRules& rules);

} // namespace WorldGeneration
} // namespace CoW
