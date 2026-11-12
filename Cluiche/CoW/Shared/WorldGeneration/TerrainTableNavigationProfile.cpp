// S-1 Reachability Validator: placeholder terrain-table profile. See TerrainTableNavigationProfile.h.
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>

#include <cmath>
#include <cstdio>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::IPathCostProvider;

namespace
{
    constexpr const char* kVersionKey = "version";
    constexpr const char* kTerrainKey = "terrain";

    void AddWarning(RuleWarnings& warnings, const char* kind, const char* name)
    {
        if (warnings.IsFull()) { return; }
        if (warnings.Size() == warnings.Capacity() - 1)
        {
            warnings.Add(StringCRC("warnings:truncated"));
            return;
        }
        char text[StringCRC::kStringLength];
        std::snprintf(text, sizeof(text), "%s:%s", kind, name);
        warnings.Add(StringCRC(text));
    }

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

TerrainNavigationTable TerrainNavigationTable::Defaults()
{
    TerrainNavigationTable t;
    auto set = [&](TerrainType type, bool passable, float cost) { t.entries[static_cast<int>(type)] = { passable, cost }; };
    set(TerrainType::DeepWater,    false, 1.0f);
    set(TerrainType::ShallowWater, false, 1.0f);
    set(TerrainType::Lake,         false, 1.0f);
    set(TerrainType::River,        true,  1.5f);
    set(TerrainType::Lowland,      true,  1.0f);
    set(TerrainType::Hills,        true,  1.5f);
    set(TerrainType::Highland,     true,  2.0f);
    set(TerrainType::Mountain,     true,  4.0f);
    set(TerrainType::Peak,         false, 1.0f);
    set(TerrainType::Cliff,        false, 1.0f);
    set(TerrainType::Pass,         true,  1.0f);
    set(TerrainType::Volcano,      false, 1.0f);
    return t;
}

bool TerrainNavigationTable::LoadFromJson(const Json::Value& v, RuleWarnings& warnings)
{
    if (!v.isObject())
    {
        AddWarning(warnings, "invalid", "root");
        return false;
    }
    if (!v.isMember(kTerrainKey) || !v[kTerrainKey].isObject())
    {
        AddWarning(warnings, "invalid", kTerrainKey);
        return false;
    }
    if (!v.isMember(kVersionKey) || !v[kVersionKey].isUInt())
    {
        AddWarning(warnings, v.isMember(kVersionKey) ? "invalid" : "missing", kVersionKey);
    }

    const Json::Value& terrain = v[kTerrainKey];
    for (int i = 0; i < kNavigationTerrainCount; ++i)
    {
        const char* name = TerrainTypeName(static_cast<TerrainType>(i));
        if (!terrain.isMember(name))
        {
            AddWarning(warnings, "missing", name);
            continue;
        }
        const Json::Value& e = terrain[name];
        if (!e.isObject() || !e.isMember("passable") || !e["passable"].isBool() || !e.isMember("cost") || !e["cost"].isNumeric())
        {
            AddWarning(warnings, "invalid", name);
            continue;
        }
        const double d = e["cost"].asDouble();
        float cost = static_cast<float>(d);
        if (!std::isfinite(d) || cost < kMinNavigationCost || cost > kMaxNavigationCost)
        {
            cost = !std::isfinite(d) || cost < kMinNavigationCost ? kMinNavigationCost : kMaxNavigationCost;
            AddWarning(warnings, "range", name);
        }
        entries[i] = { e["passable"].asBool(), cost };
    }

    for (const std::string& member : terrain.getMemberNames())
    {
        bool known = false;
        for (int i = 0; i < kNavigationTerrainCount && !known; ++i)
        {
            known = member == TerrainTypeName(static_cast<TerrainType>(i));
        }
        if (!known) { AddWarning(warnings, "unknown", member.c_str()); }
    }
    return true;
}

Json::Value TerrainNavigationTable::ToJson() const
{
    Json::Value root(Json::objectValue);
    root[kVersionKey] = 1;
    Json::Value terrain(Json::objectValue);
    for (int i = 0; i < kNavigationTerrainCount; ++i)
    {
        Json::Value e(Json::objectValue);
        e["passable"] = entries[i].passable;
        e["cost"] = static_cast<double>(entries[i].cost);
        terrain[TerrainTypeName(static_cast<TerrainType>(i))] = e;
    }
    root[kTerrainKey] = terrain;
    return root;
}

StringCRC TerrainTableNavigationProfile::GetId() const { return StringCRC("CoW.Nav.TerrainTable"); }

std::unique_ptr<IPathCostProvider> TerrainTableNavigationProfile::CreateCostProvider(const WorldStructure& world) const
{
    return std::make_unique<TableCosts>(world, mTable);
}

} // namespace WorldGeneration
} // namespace CoW
