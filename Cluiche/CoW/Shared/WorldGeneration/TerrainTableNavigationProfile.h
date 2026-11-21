// S-1 Reachability Validator: the placeholder navigation profile.
// An authored table maps each TerrainType to passable/cost. The profile's bound cost provider returns
// kImpassableCost for an edge entering a blocked terrain (or leaving the map) and the table cost otherwise.
// It is NOT B2's body-plan matrix: the terrain types are S-1's, and B2 later replaces or supplements it by injecting
// its own INavigationProfile implementations.
//
// The table is read once from the bound TerrainRules (sidecar maps terrain.passable and terrain.cost); there is no
// separate navigation file. Cost is whatever the sidecar range allows.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Placeholder Profile)
#pragma once

#include "TerrainRules.h"
#include "ValidationTypes.h"

namespace CoW {
namespace WorldGeneration {

constexpr int   kNavigationTerrainCount = static_cast<int>(TerrainType::Count);

struct TerrainNavigationEntry
{
    bool  passable = true;
    float cost     = 1.0f;
    bool operator==(const TerrainNavigationEntry&) const = default;
};

struct TerrainNavigationTable
{
    TerrainNavigationEntry entries[kNavigationTerrainCount];

    // Copies passable/cost once from the bound rules (built once per world, not per query).
    static TerrainNavigationTable FromRules(const TerrainRules& rules);
    const TerrainNavigationEntry& Get(TerrainType t) const { return entries[static_cast<int>(t)]; }

    bool operator==(const TerrainNavigationTable&) const = default;
};

class TerrainTableNavigationProfile : public INavigationProfile
{
public:
    explicit TerrainTableNavigationProfile(const TerrainRules& rules) : mTable(TerrainNavigationTable::FromRules(rules)) {}
    explicit TerrainTableNavigationProfile(const TerrainNavigationTable& table) : mTable(table) {}

    Dia::Core::StringCRC GetId() const override;
    std::unique_ptr<Dia::Pathfinding::IPathCostProvider> CreateCostProvider(const WorldStructure& world) const override;

    const TerrainNavigationTable& Table() const { return mTable; }

private:
    TerrainNavigationTable mTable;
};

} // namespace WorldGeneration
} // namespace CoW
