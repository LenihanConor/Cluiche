// S-1 World Generation: JSON import of a WorldStructure. See WorldJson.h.
#include <Shared/WorldGeneration/WorldJson.h>

#include <Shared/WorldGeneration/RegionRepair.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <memory>

namespace CoW {
namespace WorldGeneration {

namespace
{
    // Thrown-free early exit: every check returns false after filling `issue`.
    struct Importer
    {
        const WorldJsonContext& context;
        JsonIssue               issue;
        std::vector<std::string> warnings;
        size_t                  suppressed = 0;

        explicit Importer(const WorldJsonContext& c) : context(c) {}

        bool Fail(const std::string& field, const std::string& message)
        {
            issue.field = field;
            issue.message = message;
            return false;
        }

        void Warn(const std::string& text)
        {
            if (warnings.size() < kMaxImportWarnings) { warnings.push_back(text); }
            else { ++suppressed; }
        }

        void WarnUnknownFields(const Json::Value& obj, const std::string& path, std::initializer_list<const char*> known)
        {
            for (const std::string& name : obj.getMemberNames())
            {
                bool isKnown = false;
                for (const char* k : known) { isKnown = isKnown || name == k; }
                if (!isKnown) { Warn(path + "." + name + ": unknown field ignored"); }
            }
        }

        // ---- typed readers (all return false with the issue filled) -----------------------------------------------
        bool RequireMember(const Json::Value& obj, const char* key, const std::string& path)
        {
            if (!obj.isMember(key)) { return Fail(path + "." + key, "required field is missing"); }
            return true;
        }

        bool ReadInt(const Json::Value& obj, const char* key, const std::string& path, int64_t lo, int64_t hi, int64_t& out)
        {
            if (!RequireMember(obj, key, path)) { return false; }
            const Json::Value& v = obj[key];
            const std::string field = path + "." + key;
            if (!v.isNumeric() || v.isBool() || !v.isIntegral()) { return Fail(field, "must be an integer"); }
            if (!v.isUInt64() && !v.isInt64()) { return Fail(field, "must be an integer"); }
            const std::string range = std::to_string(lo) + ".." + std::to_string(hi);
            int64_t value = 0;
            if (v.isUInt64())   // every non-negative integer is a UInt64 in jsoncpp
            {
                const uint64_t u = v.asUInt64();
                if (u > static_cast<uint64_t>(hi)) { return Fail(field, "value " + std::to_string(u) + " is out of range " + range); }
                value = static_cast<int64_t>(u);
            }
            else
            {
                value = v.asInt64();   // negative
            }
            if (value < lo) { return Fail(field, "value " + std::to_string(value) + " is out of range " + range); }
            out = value;
            return true;
        }

        bool ReadUnit(const Json::Value& obj, const char* key, const std::string& path, float& out)
        {
            if (!RequireMember(obj, key, path)) { return false; }
            const Json::Value& v = obj[key];
            const std::string field = path + "." + key;
            if (!v.isNumeric() || v.isBool()) { return Fail(field, "must be a number"); }
            const double d = v.asDouble();
            if (!std::isfinite(d)) { return Fail(field, "must be finite"); }
            if (d < 0.0 || d > 1.0) { return Fail(field, "value is outside [0, 1]"); }
            out = static_cast<float>(d);
            return true;
        }

        bool ReadString(const Json::Value& obj, const char* key, const std::string& path, std::string& out)
        {
            if (!RequireMember(obj, key, path)) { return false; }
            if (!obj[key].isString()) { return Fail(path + "." + key, "must be a string"); }
            out = obj[key].asString();
            return true;
        }

        bool ReadObject(const Json::Value& obj, const char* key, const std::string& path, const Json::Value*& out)
        {
            if (!RequireMember(obj, key, path)) { return false; }
            if (!obj[key].isObject()) { return Fail(path + "." + key, "must be an object"); }
            out = &obj[key];
            return true;
        }

        bool ReadArray(const Json::Value& obj, const char* key, const std::string& path, const Json::Value*& out)
        {
            if (!RequireMember(obj, key, path)) { return false; }
            if (!obj[key].isArray()) { return Fail(path + "." + key, "must be an array"); }
            out = &obj[key];
            return true;
        }

        template <typename Enum, typename ParseFn>
        bool ReadEnum(const Json::Value& obj, const char* key, const std::string& path, ParseFn parse, const char* what, Enum& out)
        {
            std::string text;
            if (!ReadString(obj, key, path, text)) { return false; }
            if (!parse(text, out)) { return Fail(path + "." + key, std::string("unknown ") + what + " \"" + text + "\""); }
            return true;
        }
    };

    std::string Index(const std::string& path, size_t i) { return path + "[" + std::to_string(i) + "]"; }

    bool ParseCrcKey(const std::string& key, uint32_t& crc)
    {
        if (key.size() != 12 || key.compare(0, 4, "crc:") != 0) { return false; }
        uint32_t value = 0;
        for (size_t i = 4; i < 12; ++i)
        {
            const char c = key[i];
            uint32_t digit;
            if (c >= '0' && c <= '9') { digit = static_cast<uint32_t>(c - '0'); }
            else if (c >= 'a' && c <= 'f') { digit = static_cast<uint32_t>(c - 'a' + 10); }
            else { return false; }
            value = (value << 4) | digit;
        }
        crc = value;
        return true;
    }

    bool IsBuiltinKind(uint32_t crc)
    {
        return crc == SiteKindId(BuiltinNodeKinds::Corridor()) || crc == SiteKindId(BuiltinNodeKinds::River()) ||
               crc == SiteKindId(BuiltinNodeKinds::Hunting()) || crc == SiteKindId(BuiltinNodeKinds::Resource());
    }

    bool IsRegisteredKind(const SiteRegistry* registry, uint32_t crc)
    {
        if (IsBuiltinKind(crc)) { return true; }
        if (registry == nullptr) { return false; }
        for (const SiteKindInfo& info : ListSiteKinds(*registry))
        {
            if (info.kindCrc == crc) { return true; }
        }
        return false;
    }

    // jsoncpp's failIfExtra lets some trailing garbage through, so find the end of the root value ourselves
    // (string- and escape-aware, iterative) and require only whitespace after it.
    bool OnlyWhitespaceAfterRoot(const std::string& text)
    {
        size_t i = 0;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) { ++i; }
        if (i >= text.size() || text[i] != '{') { return true; }   // not an object: the parser reports it
        int depth = 0;
        bool inString = false, escaped = false;
        for (; i < text.size(); ++i)
        {
            const char c = text[i];
            if (inString)
            {
                if (escaped) { escaped = false; }
                else if (c == '\\') { escaped = true; }
                else if (c == '"') { inString = false; }
                continue;
            }
            if (c == '"') { inString = true; }
            else if (c == '{' || c == '[') { ++depth; }
            else if (c == '}' || c == ']')
            {
                if (--depth == 0) { ++i; break; }
            }
        }
        for (; i < text.size(); ++i)
        {
            const char c = text[i];
            if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) { return false; }
        }
        return true;
    }

    bool ParseText(const std::string& text, Json::Value& root, std::string& errors)
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);   // no comments, no NaN/Inf, no trailing garbage
        builder["rejectDupKeys"] = true;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        try
        {
            if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors)) { return false; }
        }
        catch (const std::exception& e)   // e.g. "Exceeded stackLimit" on absurd nesting
        {
            errors = e.what();
            return false;
        }
        if (!OnlyWhitespaceAfterRoot(text))
        {
            errors = "unexpected text after the end of the JSON document";
            return false;
        }
        return true;
    }

    bool ImportProvenance(Importer& im, const Json::Value& root, WorldProvenance& prov)
    {
        prov = WorldProvenance{};
        if (!root.isMember("provenance"))
        {
            im.Warn("provenance: missing; the world has no recipe (seed 0, default shape)");
            prov.generatorVersion = 0;
            prov.rulesVersion = 0;
            return true;
        }
        const Json::Value* p = nullptr;
        if (!im.ReadObject(root, "provenance", "", p)) { im.issue.field = "provenance"; return false; }
        const std::string path = "provenance";
        im.WarnUnknownFields(*p, path, { "seed", "generator_version", "rules_version", "preset_id", "params" });

        int64_t seedSigned = 0;
        if (!p->isMember("seed")) { return im.Fail(path + ".seed", "required field is missing"); }
        {
            const Json::Value& v = (*p)["seed"];
            if (!v.isNumeric() || v.isBool() || !v.isIntegral() || !(v.isUInt64())) { return im.Fail(path + ".seed", "must be a non-negative integer"); }
            prov.seed = v.asUInt64();
        }
        (void)seedSigned;
        int64_t gen = 0, rules = 0;
        if (!im.ReadInt(*p, "generator_version", path, 0, 0xFFFFFFFFll, gen)) { return false; }
        if (!im.ReadInt(*p, "rules_version", path, 0, 0xFFFFFFFFll, rules)) { return false; }
        prov.generatorVersion = static_cast<uint32_t>(gen);
        prov.rulesVersion = static_cast<uint32_t>(rules);

        std::string preset;
        if (!im.ReadString(*p, "preset_id", path, preset)) { return false; }
        if (preset.size() >= sizeof(prov.presetId)) { return im.Fail(path + ".preset_id", "longer than 32 characters"); }
        std::memcpy(prov.presetId, preset.c_str(), preset.size());

        const Json::Value* params = nullptr;
        if (!im.ReadObject(*p, "params", path, params)) { return false; }
        Json::Value presetRoot(Json::objectValue);
        presetRoot["format"] = kWorldShapePresetFormat;
        presetRoot["version"] = static_cast<Json::UInt>(kWorldShapePresetVersion);
        presetRoot["id"] = "provenance";
        presetRoot["displayName"] = "provenance";
        presetRoot["params"] = *params;
        WorldShapePreset parsed;
        RuleWarnings ruleWarnings;
        const PresetResult r = ParsePreset(presetRoot, "", parsed, ruleWarnings);
        if (!r.Ok()) { return im.Fail(path + ".params", r.message); }
        for (unsigned i = 0; i < ruleWarnings.Size(); ++i)
        {
            im.Warn(path + ".params: " + ruleWarnings[i].AsChar());
        }
        prov.shape = parsed.params;

        if (gen != static_cast<int64_t>(kTerrainGeneratorVersion))
        {
            im.Warn("provenance.generator_version: file " + std::to_string(gen) + ", this build " + std::to_string(kTerrainGeneratorVersion) +
                    "; regenerating may not reproduce the world");
        }
        if (im.context.rulesVersion != 0 && rules != static_cast<int64_t>(im.context.rulesVersion))
        {
            im.Warn("provenance.rules_version: file " + std::to_string(rules) + ", this build " + std::to_string(im.context.rulesVersion) +
                    "; regenerating may not reproduce the world");
        }
        return true;
    }
}

static WorldImportResult ImportWorldTextImpl(const std::string& text, const WorldJsonContext& context, WorldStructure& out)
{
    WorldImportResult result;
    Importer im(context);

    auto finish = [&](bool ok) -> WorldImportResult
    {
        result.ok = ok;
        if (!ok) { result.error = im.issue; }
        else
        {
            result.warnings = std::move(im.warnings);
            if (im.suppressed > 0) { result.warnings.push_back("... and " + std::to_string(im.suppressed) + " more warnings"); }
        }
        return result;
    };

    if (text.empty()) { im.Fail("", "file is empty"); return finish(false); }
    if (text.size() > kMaxWorldJsonBytes) { im.Fail("", "file is larger than " + std::to_string(kMaxWorldJsonBytes) + " bytes"); return finish(false); }

    Json::Value root;
    std::string parseErrors;
    if (!ParseText(text, root, parseErrors))
    {
        while (!parseErrors.empty() && (parseErrors.back() == '\n' || parseErrors.back() == '\r')) { parseErrors.pop_back(); }
        im.Fail("", "JSON parse error: " + parseErrors);
        return finish(false);
    }
    if (!root.isObject()) { im.Fail("", "the document must be a JSON object"); return finish(false); }

    // Envelope.
    if (!root.isMember("format") || !root["format"].isString()) { im.Fail("format", "required string field is missing"); return finish(false); }
    if (root["format"].asString() != kWorldJsonFormat)
    {
        im.Fail("format", "Unsupported format \"" + root["format"].asString() + "\" (expected \"" + kWorldJsonFormat + "\")");
        return finish(false);
    }
    if (!root.isMember("version") || !root["version"].isNumeric() || root["version"].isBool() || !root["version"].isIntegral())
    {
        im.Fail("version", "required integer field is missing");
        return finish(false);
    }
    {
        const Json::Value& v = root["version"];
        if (!(v.isInt64() && v.asInt64() == static_cast<int64_t>(kWorldJsonVersion)))
        {
            const std::string shown = v.isInt64() ? std::to_string(v.asInt64()) : (v.isUInt64() ? std::to_string(v.asUInt64()) : std::string("?"));
            im.Fail("version", "Unsupported cow.world version: " + shown);
            return finish(false);
        }
    }
    im.WarnUnknownFields(root, "", { "format", "version", "provenance", "world" });

    WorldStructure world;   // scratch: the caller's world is untouched unless everything validates

    if (!ImportProvenance(im, root, world.provenance)) { return finish(false); }

    const Json::Value* w = nullptr;
    if (!im.ReadObject(root, "world", "", w)) { im.issue.field = "world"; return finish(false); }
    im.WarnUnknownFields(*w, "world", { "topology", "dimensions", "main_landmass", "cells", "regions", "region_table", "sites" });

    std::string topology;
    if (!im.ReadString(*w, "topology", "world", topology)) { return finish(false); }
    if (topology != kWorldJsonTopology)
    {
        im.Fail("world.topology", "unsupported topology \"" + topology + "\" (only \"" + kWorldJsonTopology + "\")");
        return finish(false);
    }

    const Json::Value* dims = nullptr;
    if (!im.ReadObject(*w, "dimensions", "world", dims)) { return finish(false); }
    int64_t width = 0, height = 0;
    if (!im.ReadInt(*dims, "width", "world.dimensions", 1, kMaxWorldJsonDimension, width)) { return finish(false); }
    if (!im.ReadInt(*dims, "height", "world.dimensions", 1, kMaxWorldJsonDimension, height)) { return finish(false); }
    if (width * height > kMaxWorldJsonCells)
    {
        im.Fail("world.dimensions", "grid has " + std::to_string(width * height) + " cells; the limit is " + std::to_string(kMaxWorldJsonCells));
        return finish(false);
    }
    int64_t mainLandmass = 0;
    if (!im.ReadInt(*w, "main_landmass", "world", 0, 255, mainLandmass)) { return finish(false); }

    const Json::Value* cellsJson = nullptr;
    if (!im.ReadArray(*w, "cells", "world", cellsJson)) { return finish(false); }
    const size_t expectedCells = static_cast<size_t>(width * height);
    if (cellsJson->size() != expectedCells)
    {
        im.Fail("world.cells", "Cell count mismatch: expected " + std::to_string(expectedCells) + " (" + std::to_string(width) + " x " +
                std::to_string(height) + "), got " + std::to_string(cellsJson->size()));
        return finish(false);
    }

    // Regions first (cells validate their region ids against the count).
    const Json::Value* regionsJson = nullptr;
    if (!im.ReadArray(*w, "regions", "world", regionsJson)) { return finish(false); }
    if (regionsJson->size() > RegionTable::kMaxRegions)
    {
        im.Fail("world.regions", "more than " + std::to_string(RegionTable::kMaxRegions) + " regions");
        return finish(false);
    }
    const unsigned regionCount = regionsJson->size();

    world.Resize(GridDimensions{ static_cast<int>(width), static_cast<int>(height) });
    world.mainLandmass = static_cast<uint8_t>(mainLandmass);   // Resize keeps provenance (it only clears cells, regions, sites)

    // Cells.
    bool anyWater = false;
    Cell* cells = world.Cells();
    for (size_t i = 0; i < expectedCells; ++i)
    {
        const std::string path = Index("world.cells", i);
        const Json::Value& cj = (*cellsJson)[static_cast<Json::ArrayIndex>(i)];
        if (!cj.isObject()) { im.Fail(path, "must be an object"); return finish(false); }
        if (i < 8) { im.WarnUnknownFields(cj, path, { "biome", "terrain", "region", "elevation", "latitude", "water_source", "depth_zone", "landmass", "terrain_flags", "downstream" }); }

        Cell c;
        std::memset(&c, 0, sizeof(Cell));
        int64_t region = 0, landmass = 0, flags = 0, downstream = 0;
        if (!im.ReadEnum<Biome>(cj, "biome", path, WorldJsonParseBiome, "biome", c.biome)) { return finish(false); }
        if (!im.ReadEnum<TerrainType>(cj, "terrain", path, WorldJsonParseTerrain, "terrain", c.terrain)) { return finish(false); }
        if (!im.ReadInt(cj, "region", path, 0, 255, region)) { return finish(false); }
        if (region > static_cast<int64_t>(regionCount))
        {
            im.Fail(path + ".region", "region " + std::to_string(region) + " does not exist (the world has " + std::to_string(regionCount) + " regions)");
            return finish(false);
        }
        if (!im.ReadUnit(cj, "elevation", path, c.elevation)) { return finish(false); }
        if (!im.ReadUnit(cj, "latitude", path, c.latitude)) { return finish(false); }
        if (!im.ReadEnum<WaterSource>(cj, "water_source", path, WorldJsonParseWaterSource, "water source", c.waterSource)) { return finish(false); }
        if (!im.ReadEnum<DepthZone>(cj, "depth_zone", path, WorldJsonParseDepthZone, "depth zone", c.depthZone)) { return finish(false); }
        if (!im.ReadInt(cj, "landmass", path, 0, 255, landmass)) { return finish(false); }
        if (!im.ReadInt(cj, "terrain_flags", path, 0, 63, flags)) { return finish(false); }
        if (!im.ReadInt(cj, "downstream", path, 0, 7, downstream)) { return finish(false); }
        if (downstream == 6) { im.Fail(path + ".downstream", "must be a direction 0..5 or 7 (none)"); return finish(false); }
        c.region = static_cast<RegionId>(region);
        c.landmass = static_cast<uint8_t>(landmass);
        c.terrainFlags = static_cast<uint8_t>(flags);
        c.downstream = static_cast<uint8_t>(downstream);
        anyWater = anyWater || c.waterSource != WaterSource::None;
        std::memcpy(&cells[i], &c, sizeof(Cell));
    }
    if (!anyWater) { im.Warn("world: no water cell; weather (A4) cannot run on this world"); }

    // Regions: identity from the file, summaries recomputed below.
    struct FileSummary { int64_t cellCount, centreCell; Biome primary; float purity; std::vector<int64_t> neighbours; };
    std::vector<FileSummary> fileSummaries;
    for (unsigned r = 0; r < regionCount; ++r)
    {
        const std::string path = Index("world.regions", r);
        const Json::Value& rj = (*regionsJson)[static_cast<Json::ArrayIndex>(r)];
        if (!rj.isObject()) { im.Fail(path, "must be an object"); return finish(false); }
        im.WarnUnknownFields(rj, path, { "id", "zone", "landmass", "is_island", "primary_biome", "purity", "cell_count", "centre_cell", "neighbours" });

        RegionRecord rec;
        int64_t id = 0, landmass = 0;
        FileSummary fs{};
        if (!im.ReadInt(rj, "id", path, 1, RegionTable::kMaxRegions, id)) { return finish(false); }
        if (id != static_cast<int64_t>(r) + 1)
        {
            im.Fail(path + ".id", "region ids must run 1, 2, 3, ... in order (expected " + std::to_string(r + 1) + ", got " + std::to_string(id) + ")");
            return finish(false);
        }
        if (!im.ReadEnum<DepthZone>(rj, "zone", path, WorldJsonParseDepthZone, "depth zone", rec.zone)) { return finish(false); }
        if (!im.ReadInt(rj, "landmass", path, 0, 255, landmass)) { return finish(false); }
        if (!rj.isMember("is_island")) { im.Fail(path + ".is_island", "required field is missing"); return finish(false); }
        if (!rj["is_island"].isBool()) { im.Fail(path + ".is_island", "must be true or false"); return finish(false); }
        if (!im.ReadEnum<Biome>(rj, "primary_biome", path, WorldJsonParseBiome, "biome", fs.primary)) { return finish(false); }
        if (!im.ReadUnit(rj, "purity", path, fs.purity)) { return finish(false); }
        if (!im.ReadInt(rj, "cell_count", path, 0, 65535, fs.cellCount)) { return finish(false); }
        if (!im.ReadInt(rj, "centre_cell", path, 0, static_cast<int64_t>(expectedCells) - 1, fs.centreCell)) { return finish(false); }
        const Json::Value* neighbours = nullptr;
        if (!im.ReadArray(rj, "neighbours", path, neighbours)) { return finish(false); }
        for (Json::ArrayIndex k = 0; k < neighbours->size(); ++k)
        {
            const Json::Value& nj = (*neighbours)[k];
            const std::string npath = Index(path + ".neighbours", k);
            if (!nj.isNumeric() || nj.isBool() || !nj.isIntegral() || !nj.isInt64()) { im.Fail(npath, "must be a region id"); return finish(false); }
            const int64_t nb = nj.asInt64();
            if (nb < 1 || nb > static_cast<int64_t>(regionCount)) { im.Fail(npath, "region " + std::to_string(nb) + " does not exist"); return finish(false); }
            fs.neighbours.push_back(nb);
        }
        rec.id = static_cast<RegionId>(id);
        rec.landmass = static_cast<uint8_t>(landmass);
        rec.isIsland = rj["is_island"].asBool();
        world.regions.AddRegion(rec);
        fileSummaries.push_back(std::move(fs));
    }

    // Region table fields that cannot be derived from cells.
    if (!w->isMember("region_table"))
    {
        if (regionCount > 0) { im.Warn("world.region_table: missing; absorbed/fragment cells 0, not terrain-limited"); }
    }
    else
    {
        const Json::Value* t = nullptr;
        if (!im.ReadObject(*w, "region_table", "world", t)) { return finish(false); }
        im.WarnUnknownFields(*t, "world.region_table", { "absorbed_cells", "fragment_cells", "limited_by" });
        int64_t absorbed = 0, fragment = 0;
        if (!im.ReadInt(*t, "absorbed_cells", "world.region_table", 0, 65535, absorbed)) { return finish(false); }
        if (!im.ReadInt(*t, "fragment_cells", "world.region_table", 0, 65535, fragment)) { return finish(false); }
        if (!im.ReadEnum<DepthZone>(*t, "limited_by", "world.region_table", WorldJsonParseDepthZone, "depth zone", world.regions.limitedBy)) { return finish(false); }
        world.regions.absorbedCells = static_cast<uint16_t>(absorbed);
        world.regions.fragmentCells = static_cast<uint16_t>(fragment);
    }

    // Derived summaries: recompute with the generator's arithmetic; the recomputed value wins.
    RecomputeRegionRecords(world);
    for (unsigned r = 0; r < regionCount; ++r)
    {
        const std::string path = Index("world.regions", r);
        const RegionRecord& rec = world.regions.regions[r];
        const FileSummary& fs = fileSummaries[r];
        if (rec.cellCount == 0) { im.Warn(path + ": region has no cells"); }
        if (fs.cellCount != static_cast<int64_t>(rec.cellCount))
        {
            im.Warn(path + ".cell_count: file " + std::to_string(fs.cellCount) + ", recomputed " + std::to_string(rec.cellCount));
        }
        if (fs.primary != rec.primaryBiome)
        {
            im.Warn(path + ".primary_biome: file " + WorldJsonBiomeName(fs.primary) + ", recomputed " + WorldJsonBiomeName(rec.primaryBiome));
        }
        if (fs.purity != rec.purity)
        {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), ".purity: file %.9g, recomputed %.9g", static_cast<double>(fs.purity), static_cast<double>(rec.purity));
            im.Warn(path + buffer);
        }
        if (fs.centreCell != static_cast<int64_t>(rec.centreCell))
        {
            im.Warn(path + ".centre_cell: file " + std::to_string(fs.centreCell) + ", recomputed " + std::to_string(rec.centreCell));
        }
        bool sameNeighbours = fs.neighbours.size() == rec.neighbours.Size();
        for (size_t k = 0; sameNeighbours && k < fs.neighbours.size(); ++k)
        {
            sameNeighbours = fs.neighbours[k] == static_cast<int64_t>(rec.neighbours[static_cast<unsigned>(k)]);
        }
        if (!sameNeighbours) { im.Warn(path + ".neighbours: file list differs from the cells; recomputed list used"); }
    }

    // Sites.
    const Json::Value* sitesJson = nullptr;
    if (!im.ReadObject(*w, "sites", "world", sitesJson)) { return finish(false); }
    for (const std::string& key : sitesJson->getMemberNames())
    {
        const std::string kpath = "world.sites." + key;
        if (key.empty()) { im.Fail("world.sites", "site kind names must not be empty"); return finish(false); }
        if (key.size() > 63) { im.Fail(kpath.substr(0, 40) + "...", "site kind name is longer than 63 characters"); return finish(false); }
        uint32_t crc = 0;
        bool named = true;
        if (ParseCrcKey(key, crc)) { named = false; }
        else { crc = Dia::Core::StringCRC(key.c_str()).Value(); }

        const Json::Value& list = (*sitesJson)[key];
        if (!list.isArray()) { im.Fail(kpath, "must be an array of sites"); return finish(false); }
        if (!list.empty() && !named) { /* crc: keys carry no name */ }
        if (!IsRegisteredKind(context.registry, crc))
        {
            im.Warn(kpath + ": kind is not registered in this build; its sites are kept unchanged");
        }
        if (named && !world.sites.RememberKindName(crc, key))
        {
            im.Fail(kpath, "site kind name cannot be stored");
            return finish(false);
        }
        for (Json::ArrayIndex k = 0; k < list.size(); ++k)
        {
            const std::string spath = Index(kpath, k);
            const Json::Value& sj = list[k];
            if (!sj.isObject()) { im.Fail(spath, "must be an object"); return finish(false); }
            im.WarnUnknownFields(sj, spath, { "cell", "region", "variant", "flags" });
            int64_t cell = 0, region = 0, variant = 0, siteFlags = 0;
            if (!im.ReadInt(sj, "cell", spath, 0, static_cast<int64_t>(expectedCells) - 1, cell)) { return finish(false); }
            if (!im.ReadInt(sj, "region", spath, 0, 255, region)) { return finish(false); }
            if (region > static_cast<int64_t>(regionCount))
            {
                im.Fail(spath + ".region", "region " + std::to_string(region) + " does not exist (the world has " + std::to_string(regionCount) + " regions)");
                return finish(false);
            }
            if (!im.ReadInt(sj, "variant", spath, 0, 65535, variant)) { return finish(false); }
            if (!im.ReadInt(sj, "flags", spath, 0, 65535, siteFlags)) { return finish(false); }
            Site site;
            site.kindCrc = crc;
            site.cell = static_cast<uint32_t>(cell);
            site.region = static_cast<RegionId>(region);
            site.variant = static_cast<uint16_t>(variant);
            site.flags = static_cast<uint16_t>(siteFlags);
            if (!world.sites.Add(site))
            {
                im.Fail(spath + ".cell", "a site of kind " + key + " already exists at cell " + std::to_string(cell));
                return finish(false);
            }
            if (cells[cell].region != site.region)
            {
                im.Warn(spath + ".region: site says region " + std::to_string(region) + " but its cell is in region " + std::to_string(cells[cell].region));
            }
        }
    }

    out = std::move(world);
    return finish(true);
}

WorldImportResult ImportWorldText(const std::string& text, const WorldJsonContext& context, WorldStructure& out)
{
    // Last line of defence: hostile input must produce an error, never an exception out of the loader.
    try
    {
        return ImportWorldTextImpl(text, context, out);
    }
    catch (const std::exception& e)
    {
        WorldImportResult result;
        result.ok = false;
        result.error.field = "";
        result.error.message = std::string("internal error while reading the world: ") + e.what();
        return result;
    }
}

} // namespace WorldGeneration
} // namespace CoW
