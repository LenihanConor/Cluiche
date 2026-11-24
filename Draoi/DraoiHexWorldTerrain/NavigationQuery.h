// S-1 Reachability Validator: reachability under injected navigation profiles (any-profile semantics).
// Built per Validate() call: one all-passable Dia HexPathGrid(width, height) plus one world-bound IPathCostProvider per
// profile. Passability lives in the providers (kImpassableCost). The search is a deterministic Dijkstra over the grid:
// heap key (cost, cellIndex), early exit when the first target is popped, no path-length cap (Dia FindPath's
// PathResult holds at most 256 cells). Profiles are tried in injection order; the first success wins.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Navigation)
#pragma once

#include "ValidationTypes.h"

#include <DiaPathfinding/HexPathGrid.h>
#include <DiaGeometry2D/Grid/HexTopology.h>

#include <memory>
#include <span>
#include <vector>

namespace Draoi {
namespace HexWorld {

class NavigationQuery final : public INavigationQuery
{
public:
    // Null profiles are skipped. The world and the profiles must outlive the query (providers are bound to the world).
    NavigationQuery(const WorldStructure& world, std::span<const INavigationProfile* const> profiles);
    NavigationQuery(const WorldStructure& world, const ValidationParams& params);

    bool IsReachable(uint32_t from, uint32_t to) const override;
    bool FindProofPath(uint32_t from, uint32_t to, ProofPath& out) const override;
    bool FindProofPathToAny(uint32_t from, std::span<const uint32_t> targets, ProofPath& out) const override;

private:
    bool Search(size_t profileIndex, uint32_t from, std::span<const uint32_t> targets, ProofPath& out) const;

    const WorldStructure&                                              mWorld;
    bool                                                               mHasCells;
    Dia::Pathfinding::HexPathGrid                                      mGrid;
    Dia::Geometry2D::HexTopology                                      mTopology;
    std::vector<std::unique_ptr<Dia::Pathfinding::IPathCostProvider>> mCosts;
    std::vector<uint32_t>                                              mProfileCrc;
};

} // namespace HexWorld
} // namespace Draoi
