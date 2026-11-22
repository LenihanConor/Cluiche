// S-1 Reachability Validator: placeholder terrain-table profile. See TerrainTableNavigationProfile.h.
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::IPathCostProvider;

namespace
{
    class TableCosts final : public IPathCostProvider
    {
    public:
        TableCosts(const WorldStructure& world, const TerrainNavigationTable& table) : mWorld(world), mTable(table) {}
        float GetCost(CellCoord /*from*/, CellCoord to) const override
        {
            const GridDimensions d = mWorld.dimensions;
            if (to.x < 0 || to.y < 0 || to.x >= d.width || to.y >= d.height) { return kImpassableCost; }
            const TerrainNavigationEntry& e = mTable.Get(mWorld.At(to.x, to.y).terrain);
            return e.passable ? e.cost : kImpassableCost;
        }
    private:
        const WorldStructure& mWorld;
        TerrainNavigationTable mTable;
    };
}

TerrainNavigationTable TerrainNavigationTable::FromRules(const TerrainRules& rules)
{
    TerrainNavigationTable t;
    for (int i = 0; i < kNavigationTerrainCount; ++i)
    {
        t.entries[i] = { rules.terrainPassable[i], rules.terrainCost[i] };
    }
    return t;
}

StringCRC TerrainTableNavigationProfile::GetId() const { return StringCRC("HexWorld.Nav.TerrainTable"); }

std::unique_ptr<IPathCostProvider> TerrainTableNavigationProfile::CreateCostProvider(const WorldStructure& world) const
{
    return std::make_unique<TableCosts>(world, mTable);
}

} // namespace WorldGeneration
} // namespace CoW
