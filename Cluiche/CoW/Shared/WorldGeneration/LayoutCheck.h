// S-1 Regions, Nodes and Site Hook: layout check of a placed world (region partition + built-in nodes).
// Becomes the body of WorldLayoutGuarantee (Feature 2, not built). Reads only; never modifies the world.
//
// Checks (each violation carries evidence: up to kMaxEvidenceCells cell indices, a region id where it applies,
// and a count):
//   UncoveredLand / OceanHasRegion      every land cell has a region, no ocean cell has one
//   RegionDisconnected                  a main-landmass region is in several pieces (cells = those outside the largest)
//   RegionTwoZones                      more than half of a main-landmass region lies outside its zone (cells = those)
//   RegionTooSmall                      a main-landmass region has fewer than kMinRegionCells cells (count = cells)
//   RegionCountRange                    mainland region total outside 10..16 and not terrain-limited (count = total)
//   AbsorbedOverCap                     RegionTable::absorbedCells > kMaxAbsorbedShare * land cells (count = absorbed)
//   NodeOnWater                         a built-in node on a non-land, ocean or Lake cell
//   NodeSpacing                         built-in nodes closer than kNodeSpacing (count = pairs). Exempt pairs: two
//                                       nodes of the same corridor, and any pair involving a river mouth node or a
//                                       RiverCrossesBarrier cell (the node steps waive spacing there)
//   CorridorCount                       a barrier with more than 4, or fewer than 2 unless corridor-limited (region = barrier)
//   RegionNodeCount                     a region with built-in nodes outside 6..15 (skipped when node-limited)
//   TotalNodeCount                      built-in nodes outside 80..150 (skipped when node-limited)
//   FragmentOverCap                     RegionTable::fragmentCells > kMaxFragmentShare * land cells (count = fragment cells)
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Layout guarantee)
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct LayoutLimits
{
    DepthZone regionLimitedBy = DepthZone::None;   // None unless terrain-limited (RegionTable::limitedBy)
    bool      corridorLimited[2] = {};             // per barrier: fewer than 2 candidates
    bool      nodeLimited = false;                 // quotas could not reach 80..150, or a slot found no cell
};

enum class LayoutCode : uint8_t
{
    UncoveredLand, OceanHasRegion, RegionDisconnected, RegionTwoZones, RegionTooSmall, RegionCountRange,
    AbsorbedOverCap, NodeOnWater, NodeSpacing, CorridorCount, RegionNodeCount, TotalNodeCount, FragmentOverCap
};

const char* LayoutCodeName(LayoutCode code);

constexpr unsigned kMaxEvidenceCells = 16;

struct LayoutViolation
{
    LayoutCode code = LayoutCode::UncoveredLand;
    RegionId   region = kNoRegion;     // region id (CorridorCount: barrier 1 or 2); 0 when not region-specific
    uint32_t   count = 0;              // meaning depends on the code (see above)
    uint8_t    cellCount = 0;          // valid entries in cells
    uint32_t   cells[kMaxEvidenceCells] = {};
};

struct LayoutReport
{
    bool                         ok = true;
    std::vector<LayoutViolation> violations;
};

LayoutReport CheckLayout(const WorldStructure& world, const TerrainRules& rules, const LayoutLimits& limits = {});

} // namespace WorldGeneration
} // namespace CoW
