// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <Shared/WorldGeneration/WorldLayerSource.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

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
#undef DIA_COW_TERRAIN_READER

    void ReadTerrainType(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.terrain); });
    }
    void ReadWaterSource(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.waterSource); });
    }
    void ReadDepthZone(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.depthZone); });
    }
    void ReadLandmass(const WorldGenSession& s, float* out, uint32_t count)
    {
        ReadCellField(s, out, count, [](const Cell& c) { return static_cast<uint32_t>(c.landmass); });
    }

    constexpr uint32_t kLakeIdValues = 13;   // none + lake ids 1..12 (shape.lakes max 12)

    // schemaKey is built at first use (see Table()); the table stores the id and group only.
    struct Row { const char* id; const char* group; WorldLayerKind kind; uint32_t categories; WorldLayerReadFn read; bool defaultBase; };

    const Row kRows[] =
    {
        // Floats with ramps
        { "relief",           "Terrain",  WorldLayerKind::Float,    0, ReadElevation,        true  },
        { "elevation",        "Terrain",  WorldLayerKind::Float,    0, ReadElevation,        false },
        { "latitude",         "Terrain",  WorldLayerKind::Float,    0, ReadLatitude,         false },
        { "coastDistance",    "Landmass", WorldLayerKind::Float,    0, ReadCoastDistance,    false },
        { "landPotential",    "Landmass", WorldLayerKind::Float,    0, ReadLandPotential,    false },
        { "islandPotential",  "Landmass", WorldLayerKind::Float,    0, ReadIslandPotential,  false },
        { "landmass",         "Landmass", WorldLayerKind::Float,    0, ReadLandmass,         false },
        { "ridgeMask",        "Mountains", WorldLayerKind::Float,   0, ReadRidgeMask,        false },
        { "barrierMask",      "Mountains", WorldLayerKind::Float,   0, ReadBarrierMask,      false },
        { "elevationRaw",     "Terrain",  WorldLayerKind::Float,    0, ReadElevationRaw,     false },
        { "filledElevation",  "Water",    WorldLayerKind::Float,    0, ReadFilledElevation,  false },
        { "basinDepth",       "Water",    WorldLayerKind::Float,    0, ReadBasinDepth,       false },
        { "flowAccumulation", "Water",    WorldLayerKind::Float,    0, ReadFlowAccumulation, false },
        { "valleyCarve",      "Water",    WorldLayerKind::Float,    0, ReadValleyCarve,      false },
        { "slope",            "Terrain",  WorldLayerKind::Float,    0, ReadSlope,            false },
        { "oceanDepth",       "Water",    WorldLayerKind::Float,    0, ReadOceanDepth,       false },
        { "zoneCost",         "Zones",    WorldLayerKind::Float,    0, ReadZoneCost,         false },
        // Categories
        { "terrainType",      "Terrain",  WorldLayerKind::Category, static_cast<uint32_t>(TerrainType::Count), ReadTerrainType, false },
        { "waterSource",      "Water",    WorldLayerKind::Category, 4,  ReadWaterSource,     false },
        { "depthZone",        "Zones",    WorldLayerKind::Category, 4,  ReadDepthZone,       false },
        { "flowDirection",    "Water",    WorldLayerKind::Category, 7,  ReadFlowDirection,   false },
        { "riverMask",        "Water",    WorldLayerKind::Category, 2,  ReadRiverMask,       false },
        { "lakeMask",         "Water",    WorldLayerKind::Category, kLakeIdValues, ReadLakeMask, false },
        { "passMask",         "Mountains", WorldLayerKind::Category, 2, ReadPassMask,        false },
        { "volcanic",         "Mountains", WorldLayerKind::Category, 2, ReadVolcanic,        false },
    };

    constexpr uint32_t kRowCount = static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0]));

    struct Table
    {
        WorldLayerDesc descs[kRowCount];
        char           keys[kRowCount][64];
        Table()
        {
            for (uint32_t i = 0; i < kRowCount; ++i)
            {
                const Row& r = kRows[i];
                std::snprintf(keys[i], sizeof(keys[i]), "%s/%s", kWorldLayerSchemaFor, r.id);
                descs[i] = { r.id, keys[i], r.group, r.kind, r.categories, r.read, r.defaultBase };
            }
        }
    };

    const Table& GetTable()
    {
        static const Table table;
        return table;
    }
} // namespace

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

} // namespace WorldGeneration
} // namespace CoW
