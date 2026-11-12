// S-1 Reachability Validator: the placeholder navigation profile.
// An authored table maps each TerrainType to passable/cost. The profile's bound cost provider returns
// kImpassableCost for an edge entering a blocked terrain (or leaving the map) and the table cost otherwise.
// It is NOT B2's body-plan matrix: the terrain types are S-1's, and B2 later replaces or supplements it by injecting
// its own INavigationProfile implementations.
//
// File: Cluiche/Assets/CoW/Global/WorldGen/terrain_navigation.json
//   {"version":1,"terrain":{"<TerrainType name>":{"passable":bool,"cost":float}, ...}}
// Cost is clamped to [kMinNavigationCost, kMaxNavigationCost]. Warnings (StringCRC "<kind>:<name>"): invalid:root,
// invalid:terrain, unknown:<name>, invalid:<name>, range:<name>, missing:<name>.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Placeholder Profile)
#pragma once

#include "TerrainRules.h"
#include "ValidationTypes.h"

#include <DiaCore/Json/external/json/json.h>

namespace CoW {
namespace WorldGeneration {

constexpr float kMinNavigationCost = 0.1f;
constexpr float kMaxNavigationCost = 100.0f;
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

    static TerrainNavigationTable Defaults();
    const TerrainNavigationEntry& Get(TerrainType t) const { return entries[static_cast<int>(t)]; }

    // False (table unchanged) only when the root is not an object or "terrain" is not an object. Otherwise per-entry
    // problems are warnings and the entry keeps its current value.
    bool LoadFromJson(const Json::Value& v, RuleWarnings& warnings);
    Json::Value ToJson() const;

    bool operator==(const TerrainNavigationTable&) const = default;
};

class TerrainTableNavigationProfile : public INavigationProfile
{
public:
    TerrainTableNavigationProfile() : mTable(TerrainNavigationTable::Defaults()) {}
    explicit TerrainTableNavigationProfile(const TerrainNavigationTable& table) : mTable(table) {}

    Dia::Core::StringCRC GetId() const override;
    std::unique_ptr<Dia::Pathfinding::IPathCostProvider> CreateCostProvider(const WorldStructure& world) const override;

    const TerrainNavigationTable& Table() const { return mTable; }

private:
    TerrainNavigationTable mTable;
};

} // namespace WorldGeneration
} // namespace CoW
