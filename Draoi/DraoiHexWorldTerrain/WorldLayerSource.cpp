// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <DraoiHexWorldTerrain/WorldLayerSource.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Draoi {
namespace HexWorld {

namespace
{
    // Number of cells the readers can fill: 0 until a world has been generated.
    uint32_t GeneratedCells(const WorldGenSession& s, uint32_t count)
    {
        if (s.Revision() == 0)
            return 0;
        return (std::min)(count, static_cast<uint32_t>(s.World().CellCount()));
    }

    void ZeroTail(float* out, uint32_t from, uint32_t count)
    {
        if (from < count)
            std::fill(out + from, out + count, 0.0f);
    }

    void ReadTerrainLayer(const WorldGenSession& s, const char* name, float* out, uint32_t count)
    {
        uint32_t n = GeneratedCells(s, count);
        const TerrainLayers& layers = s.Generator().GetLayers();
        const int index = layers.FindLayer(name);
        if (const float* f = layers.GetFloat(index))
        {
            std::copy(f, f + n, out);
        }
        else if (const uint32_t* u = layers.GetUInt(index))
        {
            for (uint32_t i = 0; i < n; ++i)
                out[i] = static_cast<float>(u[i]);
        }
        else
        {
            n = 0;
        }
        ZeroTail(out, n, count);
    }

    template<class Fn>
    void ReadCellField(const WorldGenSession& s, float* out, uint32_t count, Fn field)
    {
        const uint32_t n = GeneratedCells(s, count);
        const Cell* cells = s.World().Cells();
        for (uint32_t i = 0; i < n; ++i)
            out[i] = static_cast<float>(field(cells[i]));
        ZeroTail(out, n, count);
    }

#define DIA_COW_TERRAIN_READER(fn, name) \
    void fn(const WorldGenSession& s, float* out, uint32_t count) { ReadTerrainLayer(s, name, out, count); }

    DIA_COW_TERRAIN_READER(ReadElevation,       "elevation")
    DIA_COW_TERRAIN_READER(ReadLatitude,        "latitude")
    DIA_COW_TERRAIN_READER(ReadCoastDistance,   "coastDistance")
    DIA_COW_TERRAIN_READER(ReadLandPotential,   "landPotential")
    DIA_COW_TERRAIN_READER(ReadIslandPotential, "islandPotential")
    DIA_COW_TERRAIN_READER(ReadRidgeMask,       "ridgeMask")
    DIA_COW_TERRAIN_READER(ReadBarrierMask,     "barrierMask")
    DIA_COW_TERRAIN_READER(ReadElevationRaw,    "elevationRaw")
    DIA_COW_TERRAIN_READER(ReadFilledElevation, "filledElevation")
    DIA_COW_TERRAIN_READER(ReadBasinDepth,      "basinDepth")
    DIA_COW_TERRAIN_READER(ReadFlowAccumulation, "flowAccumulation")
    DIA_COW_TERRAIN_READER(ReadValleyCarve,     "valleyCarve")
    DIA_COW_TERRAIN_READER(ReadSlope,           "slope")
    DIA_COW_TERRAIN_READER(ReadOceanDepth,      "oceanDepth")
    DIA_COW_TERRAIN_READER(ReadZoneCost,        "zoneCost")
    DIA_COW_TERRAIN_READER(ReadFlowDirection,   "flowDirection")
    DIA_COW_TERRAIN_READER(ReadRiverMask,       "riverMask")
    DIA_COW_TERRAIN_READER(ReadLakeMask,        "lakeMask")
    DIA_COW_TERRAIN_READER(ReadPassMask,        "passMask")
    DIA_COW_TERRAIN_READER(ReadVolcanic,        "volcanic")
    DIA_COW_TERRAIN_READER(ReadTemperature,     "temperature")
    DIA_COW_TERRAIN_READER(ReadMoisture,        "moisture")
    DIA_COW_TERRAIN_READER(ReadWaterDistance,   "waterDistance")
    DIA_COW_TERRAIN_READER(ReadAirMoisture,     "airMoisture")
    DIA_COW_TERRAIN_READER(ReadRainOut,         "rainOut")
    DIA_COW_TERRAIN_READER(ReadBiomeScore,      "biomeScore")
    DIA_COW_TERRAIN_READER(ReadBiomePin,        "biomePin")
    DIA_COW_TERRAIN_READER(ReadBiomeIndex,      "biomeIndex")
    DIA_COW_TERRAIN_READER(ReadFixupChanges,    "fixupChanges")
#undef DIA_COW_TERRAIN_READER

    void ReadTerrainType(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.terrain); });
    }
    void ReadWaterSource(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.waterSource); });
    }
    void ReadStratum(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.stratum); });
    }
    void ReadLandmass(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.landmass); });
    }

    void ReadRegions(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return RegionPaletteSlot(c.region); });
    }

    constexpr uint32_t kLakeIdValues = 13;   // none + lake ids 1..12 (shape.lakes max 12)

    // id ("worldgen.<field>") and schemaKey ("<for>/<field>") are built at first use (see Table()); rows store the snake_case field.
    struct Row { const char* id; const char* group; WorldLayerKind kind; uint32_t categories; WorldLayerReadFn read; bool defaultBase; };

    const Row kRows[] =
    {
        // Floats with ramps
        { "relief",           "Terrain",  WorldLayerKind::Float,    0, ReadElevation,        true  },
        { "elevation",        "Terrain",  WorldLayerKind::Float,    0, ReadElevation,        false },
        { "latitude",         "Terrain",  WorldLayerKind::Float,    0, ReadLatitude,         false },
        { "coast_distance",    "Landmass", WorldLayerKind::Float,    0, ReadCoastDistance,    false },
        { "land_potential",    "Landmass", WorldLayerKind::Float,    0, ReadLandPotential,    false },
        { "island_potential",  "Landmass", WorldLayerKind::Float,    0, ReadIslandPotential,  false },
        { "landmass",         "Landmass", WorldLayerKind::Float,    0, ReadLandmass,         false },
        { "ridge_mask",        "Mountains", WorldLayerKind::Float,   0, ReadRidgeMask,        false },
        { "barrier_mask",      "Mountains", WorldLayerKind::Float,   0, ReadBarrierMask,      false },
        { "elevation_raw",     "Terrain",  WorldLayerKind::Float,    0, ReadElevationRaw,     false },
        { "filled_elevation",  "Water",    WorldLayerKind::Float,    0, ReadFilledElevation,  false },
        { "basin_depth",       "Water",    WorldLayerKind::Float,    0, ReadBasinDepth,       false },
        { "flow_accumulation", "Water",    WorldLayerKind::Float,    0, ReadFlowAccumulation, false },
        { "valley_carve",      "Water",    WorldLayerKind::Float,    0, ReadValleyCarve,      false },
        { "slope",            "Terrain",  WorldLayerKind::Float,    0, ReadSlope,            false },
        { "ocean_depth",       "Water",    WorldLayerKind::Float,    0, ReadOceanDepth,       false },
        { "zone_cost",         "Zones",    WorldLayerKind::Float,    0, ReadZoneCost,         false },
        // Categories
        { "terrain_type",      "Terrain",  WorldLayerKind::Category, static_cast<uint32_t>(TerrainType::Count), ReadTerrainType, false },
        { "water_source",      "Water",    WorldLayerKind::Category, 4,  ReadWaterSource,     false },
        { "stratum",        "Zones",    WorldLayerKind::Category, 4,  ReadStratum,       false },
        { "flow_direction",    "Water",    WorldLayerKind::Category, 7,  ReadFlowDirection,   false },
        { "river_mask",        "Water",    WorldLayerKind::Category, 2,  ReadRiverMask,       false },
        { "lake_mask",         "Water",    WorldLayerKind::Category, kLakeIdValues, ReadLakeMask, false },
        { "pass_mask",         "Mountains", WorldLayerKind::Category, 2, ReadPassMask,        false },
        { "volcanic",         "Mountains", WorldLayerKind::Category, 2, ReadVolcanic,        false },
        // Climate and biomes (stages 12-14)
        { "temperature",      "Climate",  WorldLayerKind::Float,    0, ReadTemperature,      false },
        { "moisture",         "Climate",  WorldLayerKind::Float,    0, ReadMoisture,         false },
        { "water_distance",    "Climate",  WorldLayerKind::Float,    0, ReadWaterDistance,    false },
        { "air_moisture",      "Climate",  WorldLayerKind::Float,    0, ReadAirMoisture,      false },
        { "rain_out",          "Climate",  WorldLayerKind::Float,    0, ReadRainOut,          false },
        { "biome_score",       "Biomes",   WorldLayerKind::Float,    0, ReadBiomeScore,       false },
        { "biome_pin",         "Biomes",   WorldLayerKind::Category, 5, ReadBiomePin,         false },
        { "biome_index",       "Biomes",   WorldLayerKind::Category, kCategoriesFromBiomeRegistry, ReadBiomeIndex, false },
        { "fixup_changes",     "Biomes",   WorldLayerKind::Category, 3, ReadFixupChanges,     false },
        // Regions (placement pass; empty until a Regenerate)
        { "regions",          "Regions",  WorldLayerKind::Category, kRegionPaletteSlots, ReadRegions, false },
    };

    constexpr uint32_t kRowCount = static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0]));

    struct Table
    {
        WorldLayerDesc descs[kRowCount];
        char           keys[kRowCount][64];
        char           ids[kRowCount][64];
        Table()
        {
            for (uint32_t i = 0; i < kRowCount; ++i)
            {
                const Row& r = kRows[i];
                std::snprintf(ids[i], sizeof(ids[i]), "%s%s", kWorldLayerIdPrefix, r.id);
                std::snprintf(keys[i], sizeof(keys[i]), "%s/%s", kWorldLayerSchemaFor, r.id);
                descs[i] = { ids[i], keys[i], r.group, r.kind, r.categories, r.read, r.defaultBase };
            }
        }
    };

    const Table& GetTable()
    {
        static const Table table;
        return table;
    }
} // namespace

std::vector<ExtraStrataLayerDesc> WorldLayerSource::ExtraStrataLayers(const Draoi::HexWorld::BiomeRegistry* biomes)
{
    std::vector<ExtraStrataLayerDesc> out;
    if (biomes == nullptr)
        return out;
    const Draoi::HexWorld::StratumTable& table = biomes->Strata();
    for (uint32_t i = 0; i < table.SetCount(); ++i)
    {
        const Draoi::HexWorld::StratumSetSpec& set = table.Set(i);
        if (std::strcmp(set.id, Draoi::HexWorld::StratumTable::kDepthSetId) == 0)
            continue;
        out.push_back({ std::string(kExtraStrataIdPrefix) + set.id, set.id, set.count + 1u });
    }
    return out;
}

void WorldLayerSource::ReadExtraStrata(const WorldGenSession& s, const char* setId, float* out, uint32_t count)
{
    const uint32_t n = GeneratedCells(s, count);
    const ExtraStratumLayer* layer = s.World().FindExtraStrata(setId);
    uint32_t filled = 0;
    if (layer != nullptr && layer->cells.size() >= n)
    {
        for (; filled < n; ++filled)
            out[filled] = static_cast<float>(layer->cells[filled]);
    }
    ZeroTail(out, filled, count);
}

uint32_t WorldLayerSource::Count()
{
    return kRowCount;
}

const WorldLayerDesc& WorldLayerSource::At(uint32_t index)
{
    return GetTable().descs[index < kRowCount ? index : 0];
}

const WorldLayerDesc* WorldLayerSource::Find(const char* id)
{
    if (id == nullptr)
        return nullptr;
    const Table& t = GetTable();
    for (uint32_t i = 0; i < kRowCount; ++i)
        if (std::strcmp(t.descs[i].id, id) == 0)
            return &t.descs[i];
    return nullptr;
}

} // namespace HexWorld
} // namespace Draoi
