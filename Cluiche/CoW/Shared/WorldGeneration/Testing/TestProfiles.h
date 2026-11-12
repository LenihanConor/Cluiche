// S-1 Reachability Validator test doubles: navigation profiles. Header-only.
// A cell is blocked when its terrain is in the profile's blocked set. GetCost takes the edge's destination
// (offset col,row) like TerrainTableNavigationProfile.
#pragma once

#include <Shared/WorldGeneration/ValidationTypes.h>

#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

class WorldTerrainCosts : public Dia::Pathfinding::IPathCostProvider
{
public:
    WorldTerrainCosts(const WorldStructure& world, std::vector<TerrainType> blocked) : mWorld(world), mBlocked(std::move(blocked)) {}
    float GetCost(Dia::Pathfinding::CellCoord, Dia::Pathfinding::CellCoord to) const override
    {
        if (to.x < 0 || to.y < 0 || to.x >= mWorld.dimensions.width || to.y >= mWorld.dimensions.height) { return kImpassableCost; }
        const TerrainType t = mWorld.At(to.x, to.y).terrain;
        for (TerrainType b : mBlocked) { if (b == t) { return kImpassableCost; } }
        return 1.0f;
    }
private:
    const WorldStructure& mWorld;
    std::vector<TerrainType> mBlocked;
};

class BlockedTerrainProfile : public INavigationProfile
{
public:
    BlockedTerrainProfile(const char* id, std::vector<TerrainType> blocked) : mId(id), mBlocked(std::move(blocked)) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    std::unique_ptr<Dia::Pathfinding::IPathCostProvider> CreateCostProvider(const WorldStructure& world) const override
    {
        return std::make_unique<WorldTerrainCosts>(world, mBlocked);
    }
private:
    std::string mId;
    std::vector<TerrainType> mBlocked;
};

// Blocks DeepWater, ShallowWater and Lake.
class BlockWaterProfile : public BlockedTerrainProfile
{
public:
    explicit BlockWaterProfile(const char* id)
        : BlockedTerrainProfile(id, { TerrainType::DeepWater, TerrainType::ShallowWater, TerrainType::Lake }) {}
};

// Blocks nothing: water is crossable.
class AllowWaterProfile : public BlockedTerrainProfile
{
public:
    explicit AllowWaterProfile(const char* id) : BlockedTerrainProfile(id, {}) {}
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
