// S-1 World Generation: JSON export of a WorldStructure. See WorldJson.h.
#include <Shared/WorldGeneration/WorldJson.h>

#include <Shared/WorldGeneration/BiomeTypes.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

namespace
{
    // Enum value names the file uses, computed once.
    struct BiomeNames
    {
        std::string names[kBiomeCount];
        BiomeNames()
        {
            for (int i = 0; i < kBiomeCount; ++i)
            {
                names[i] = BiomeId(static_cast<Biome>(i)).AsChar();
            }
        }
    };

    const BiomeNames& GetBiomeNames()
    {
        static const BiomeNames sNames;
        return sNames;
    }

    template <typename Enum, typename NameFn>
    bool ParseEnum(const std::string& text, int count, NameFn nameOf, Enum& out)
    {
        for (int i = 0; i < count; ++i)
        {
            if (text == nameOf(static_cast<Enum>(i)))
            {
                out = static_cast<Enum>(i);
                return true;
            }
        }
        return false;
    }

    bool IsKnownBiome(Biome b) { return static_cast<int>(b) >= 0 && static_cast<int>(b) < kBiomeCount; }
    bool IsKnownTerrain(TerrainType t) { return static_cast<int>(t) >= 0 && static_cast<int>(t) < static_cast<int>(TerrainType::Count); }
    bool IsKnownWater(WaterSource w) { return static_cast<int>(w) >= 0 && static_cast<int>(w) <= static_cast<int>(WaterSource::River); }
    bool IsKnownZone(DepthZone z) { return static_cast<int>(z) >= 0 && static_cast<int>(z) <= static_cast<int>(DepthZone::AncientInteriors); }

    // Appends printf-style text of any length.
    void Appendf(std::string& out, const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        va_list copy;
        va_copy(copy, args);
        const int n = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (n >= 0 && n < static_cast<int>(sizeof(buffer)))
        {
            out.append(buffer, static_cast<size_t>(n));
        }
        else if (n >= 0)
        {
            std::string big(static_cast<size_t>(n) + 1, '\0');
            std::vsnprintf(big.data(), big.size(), format, copy);
            out.append(big.data(), static_cast<size_t>(n));
        }
        va_end(copy);
    }

    // JSON string literal with the escapes JSON requires.
    void AppendString(std::string& out, const std::string& s)
    {
        out.push_back('"');
        for (unsigned char c : s)
        {
            switch (c)
            {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20)
                {
                    Appendf(out, "\\u%04x", static_cast<unsigned>(c));
                }
                else
                {
                    out.push_back(static_cast<char>(c));
                }
            }
        }
        out.push_back('"');
    }

    // 9 significant digits always read back to the same float.
    void AppendFloat(std::string& out, float v) { Appendf(out, "%.9g", static_cast<double>(v)); }

    bool Fail(JsonIssue& error, std::string field, std::string message)
    {
        error.field = std::move(field);
        error.message = std::move(message);
        return false;
    }

    bool SameBuiltin(uint32_t crc, const Dia::Core::StringCRC& kind) { return crc == SiteKindId(kind); }

    void AppendParams(std::string& out, const WorldShapeParams& shape)
    {
        WorldShapePreset preset;
        preset.params = shape;
        const Json::Value params = PresetToJson(preset)["params"];
        const std::vector<std::string> keys = params.getMemberNames();   // jsoncpp keeps members sorted: stable order
        for (size_t i = 0; i < keys.size(); ++i)
        {
            const Json::Value& v = params[keys[i]];
            out += "      ";
            AppendString(out, keys[i]);
            out += ": ";
            if (v.isString())
            {
                AppendString(out, v.asString());
            }
            else if (v.type() == Json::realValue)
            {
                Appendf(out, "%.9g", v.asDouble());
            }
            else
            {
                Appendf(out, "%llu", static_cast<unsigned long long>(v.asUInt64()));
            }
            out += (i + 1 < keys.size()) ? ",\n" : "\n";
        }
    }
}

const char* WorldJsonBiomeName(Biome b)
{
    return IsKnownBiome(b) ? GetBiomeNames().names[static_cast<int>(b)].c_str() : "Unknown";
}

bool WorldJsonParseBiome(const std::string& text, Biome& out)
{
    return ParseEnum<Biome>(text, kBiomeCount, [](Biome b) { return WorldJsonBiomeName(b); }, out);
}

bool WorldJsonParseTerrain(const std::string& text, TerrainType& out)
{
    return ParseEnum<TerrainType>(text, static_cast<int>(TerrainType::Count), [](TerrainType t) { return TerrainTypeName(t); }, out);
}

bool WorldJsonParseWaterSource(const std::string& text, WaterSource& out)
{
    return ParseEnum<WaterSource>(text, 4, [](WaterSource w) { return WaterSourceName(w); }, out);
}

bool WorldJsonParseDepthZone(const std::string& text, DepthZone& out)
{
    return ParseEnum<DepthZone>(text, 4, [](DepthZone z) { return DepthZoneName(z); }, out);
}

std::string WorldJsonSiteKindKey(uint32_t kindCrc, const SiteRegistry* registry, const SiteSet& sites)
{
    for (const Dia::Core::StringCRC& builtin : { BuiltinNodeKinds::Corridor(), BuiltinNodeKinds::River(),
                                                 BuiltinNodeKinds::Hunting(), BuiltinNodeKinds::Resource() })
    {
        if (SameBuiltin(kindCrc, builtin))
        {
            return builtin.AsChar();
        }
    }
    if (registry != nullptr)
    {
        for (const SiteKindInfo& info : ListSiteKinds(*registry))
        {
            if (info.kindCrc == kindCrc && info.id[0] != '\0')
            {
                return info.id;
            }
        }
    }
    if (const char* remembered = sites.KindName(kindCrc))
    {
        return remembered;
    }
    char fallback[16];
    std::snprintf(fallback, sizeof(fallback), "crc:%08x", kindCrc);
    return fallback;
}

bool ExportWorldText(const WorldStructure& world, const WorldJsonContext& context, std::string& text, JsonIssue& error)
{
    text.clear();
    error = JsonIssue{};

    const int width = world.dimensions.width;
    const int height = world.dimensions.height;
    const int n = world.CellCount();
    if (width <= 0 || height <= 0 || n != width * height)
    {
        return Fail(error, "world.dimensions", "dimensions do not match the cell count");
    }
    const Cell* cells = world.Cells();

    std::string out;
    out.reserve(static_cast<size_t>(n) * 190u + 4096u);

    const std::string presetId = !context.presetId.empty() ? context.presetId : std::string(world.provenance.presetId);
    out += "{\n";
    out += "  \"format\": \"cow.world\",\n";
    Appendf(out, "  \"version\": %u,\n", kWorldJsonVersion);
    out += "  \"provenance\": {\n";
    Appendf(out, "    \"seed\": %llu,\n", static_cast<unsigned long long>(world.provenance.seed));
    Appendf(out, "    \"generator_version\": %u,\n", world.provenance.generatorVersion);
    Appendf(out, "    \"rules_version\": %u,\n", world.provenance.rulesVersion);
    out += "    \"preset_id\": ";
    AppendString(out, presetId);
    out += ",\n";
    out += "    \"params\": {\n";
    AppendParams(out, world.provenance.shape);
    out += "    }\n";
    out += "  },\n";

    out += "  \"world\": {\n";
    out += "    \"topology\": \"hex_flat_odd_q\",\n";
    Appendf(out, "    \"dimensions\": { \"width\": %d, \"height\": %d },\n", width, height);
    Appendf(out, "    \"main_landmass\": %u,\n", static_cast<unsigned>(world.mainLandmass));

    out += "    \"cells\": [\n";
    for (int i = 0; i < n; ++i)
    {
        const Cell& c = cells[i];
        char field[64];
        std::snprintf(field, sizeof(field), "world.cells[%d]", i);
        if (!std::isfinite(c.elevation)) { return Fail(error, std::string(field) + ".elevation", "elevation is not finite"); }
        if (!std::isfinite(c.latitude))  { return Fail(error, std::string(field) + ".latitude", "latitude is not finite"); }
        if (!IsKnownBiome(c.biome))      { return Fail(error, std::string(field) + ".biome", "biome value is out of range"); }
        if (!IsKnownTerrain(c.terrain))  { return Fail(error, std::string(field) + ".terrain", "terrain value is out of range"); }
        if (!IsKnownWater(c.waterSource)) { return Fail(error, std::string(field) + ".water_source", "water source value is out of range"); }
        if (!IsKnownZone(c.depthZone))   { return Fail(error, std::string(field) + ".depth_zone", "depth zone value is out of range"); }

        out += "      { \"biome\": \"";
        out += WorldJsonBiomeName(c.biome);
        out += "\", \"terrain\": \"";
        out += TerrainTypeName(c.terrain);
        Appendf(out, "\", \"region\": %u, \"elevation\": ", static_cast<unsigned>(c.region));
        AppendFloat(out, c.elevation);
        out += ", \"latitude\": ";
        AppendFloat(out, c.latitude);
        out += ", \"water_source\": \"";
        out += WaterSourceName(c.waterSource);
        out += "\", \"depth_zone\": \"";
        out += DepthZoneName(c.depthZone);
        Appendf(out, "\", \"landmass\": %u, \"terrain_flags\": %u, \"downstream\": %u }",
                static_cast<unsigned>(c.landmass), static_cast<unsigned>(c.terrainFlags), static_cast<unsigned>(c.downstream));
        out += (i + 1 < n) ? ",\n" : "\n";
    }
    out += "    ],\n";

    const unsigned regionCount = world.regions.regions.Size();
    out += "    \"regions\": [\n";
    for (unsigned r = 0; r < regionCount; ++r)
    {
        const RegionRecord& rec = world.regions.regions[r];
        char field[64];
        std::snprintf(field, sizeof(field), "world.regions[%u]", r);
        if (!std::isfinite(rec.purity))   { return Fail(error, std::string(field) + ".purity", "purity is not finite"); }
        if (!IsKnownBiome(rec.primaryBiome)) { return Fail(error, std::string(field) + ".primary_biome", "biome value is out of range"); }
        if (!IsKnownZone(rec.zone))       { return Fail(error, std::string(field) + ".zone", "zone value is out of range"); }

        Appendf(out, "      { \"id\": %u, \"zone\": \"%s\", \"landmass\": %u, \"is_island\": %s, \"primary_biome\": \"%s\", \"purity\": ",
                static_cast<unsigned>(rec.id), DepthZoneName(rec.zone), static_cast<unsigned>(rec.landmass),
                rec.isIsland ? "true" : "false", WorldJsonBiomeName(rec.primaryBiome));
        AppendFloat(out, rec.purity);
        Appendf(out, ", \"cell_count\": %u, \"centre_cell\": %u, \"neighbours\": [", static_cast<unsigned>(rec.cellCount),
                static_cast<unsigned>(rec.centreCell));
        for (unsigned k = 0; k < rec.neighbours.Size(); ++k)
        {
            Appendf(out, k == 0 ? "%u" : ", %u", static_cast<unsigned>(rec.neighbours[k]));
        }
        out += "] }";
        out += (r + 1 < regionCount) ? ",\n" : "\n";
    }
    out += "    ],\n";

    Appendf(out, "    \"region_table\": { \"absorbed_cells\": %u, \"fragment_cells\": %u, \"limited_by\": \"%s\" },\n",
            static_cast<unsigned>(world.regions.absorbedCells), static_cast<unsigned>(world.regions.fragmentCells),
            DepthZoneName(world.regions.limitedBy));

    // Sites grouped by kind, kinds sorted by their key text so the order does not depend on CRC values.
    struct KindGroup { std::string key; uint32_t crc; };
    std::vector<KindGroup> groups;
    uint32_t lastCrc = 0;
    bool haveLast = false;
    for (const Site& s : world.sites.All())
    {
        if (!haveLast || s.kindCrc != lastCrc)
        {
            groups.push_back({ WorldJsonSiteKindKey(s.kindCrc, context.registry, world.sites), s.kindCrc });
            lastCrc = s.kindCrc;
            haveLast = true;
        }
        if (s.cell >= static_cast<uint32_t>(n))
        {
            char field[96];
            std::snprintf(field, sizeof(field), "world.sites cell %u", s.cell);
            return Fail(error, field, "site cell is outside the grid");
        }
    }
    std::sort(groups.begin(), groups.end(), [](const KindGroup& a, const KindGroup& b) { return a.key < b.key; });

    if (groups.empty())
    {
        out += "    \"sites\": {}\n";
    }
    else
    {
        out += "    \"sites\": {\n";
        for (size_t g = 0; g < groups.size(); ++g)
        {
            out += "      ";
            AppendString(out, groups[g].key);
            out += ": [\n";
            const std::span<const Site> list = world.sites.Of(groups[g].crc);
            for (size_t k = 0; k < list.size(); ++k)
            {
                Appendf(out, "        { \"cell\": %u, \"region\": %u, \"variant\": %u, \"flags\": %u }%s\n",
                        list[k].cell, static_cast<unsigned>(list[k].region), static_cast<unsigned>(list[k].variant),
                        static_cast<unsigned>(list[k].flags), (k + 1 < list.size()) ? "," : "");
            }
            out += (g + 1 < groups.size()) ? "      ],\n" : "      ]\n";
        }
        out += "    }\n";
    }
    out += "  }\n";
    out += "}\n";

    if (out.size() > kMaxWorldJsonBytes)   // import refuses larger files, so writing one would be a trap
    {
        return Fail(error, "world", "the exported text would be larger than " + std::to_string(kMaxWorldJsonBytes) + " bytes");
    }
    text = std::move(out);
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
