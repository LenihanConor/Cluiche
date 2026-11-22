// S-1 Reachability Validator: WorldLayoutGuarantee, the layout check of Regions and Sites as an IWorldGuarantee.
// Wraps CheckLayout (LayoutCheck.h): satisfied == report.ok. Each violation becomes one diagnostic
// ("<LayoutCodeName>: count N[, region R]"), its evidence cells go to violatingCells, and a violationCount measurement
// (tag = LayoutCode, value = count, a = first evidence cell) is recorded. Needs no navigation.
// Terrain cause (SD-S1-008): when the world is terrain-limited (limits.regionLimitedBy != None), RegionCountRange and
// AbsorbedOverCap diagnostics end with " (terrain cause: terrain-limited world)"; the caller reads that to end
// generation without a retry. The limits come from RegionsAndSites::PlaceReport, so a guarantee is built per attempt.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Amendment 2026-10-18)
#pragma once

#include "LayoutCheck.h"
#include "TerrainRules.h"
#include "ValidationTypes.h"

namespace CoW {
namespace WorldGeneration {

class WorldLayoutGuarantee final : public IWorldGuarantee
{
public:
    WorldLayoutGuarantee(const TerrainRules& rules, const LayoutLimits& limits, const Draoi::HexWorld::NodeKindIds& nodeKinds)
        : mRules(rules), mLimits(limits), mNodeKinds(nodeKinds) {}

    static constexpr const char* kName = "HexWorld.Guarantee.WorldLayout";   // built in, not part of a GuaranteeSpec
    Dia::Core::StringCRC GetId() const override;
    GuaranteeResult Check(const ValidationContext& context) const override;

private:
    TerrainRules mRules;
    LayoutLimits mLimits;
    Draoi::HexWorld::NodeKindIds mNodeKinds;
};

} // namespace WorldGeneration
} // namespace CoW
