// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>

namespace CoW {
namespace WorldGeneration {

namespace {

enum class Kind : uint8_t { Float, UInt8, Form };

// One slider row, in ShapeParam order. Bounds mirror WorldShapeParams::Clamp(); orientationDeg is [0, 360).
struct ParamRow
{
    const char* key;
    ShapeParam  param;
    Kind        kind;
    float       lo;
    float       hi;
    float WorldShapeParams::*   fieldF;
    uint8_t WorldShapeParams::* fieldU;
};

const ParamRow kRows[] =
{
    { "landFraction",      ShapeParam::LandFraction,      Kind::Float, 0.20f, 0.65f,  &WorldShapeParams::landFraction,      nullptr },
    { "continents",        ShapeParam::Continents,        Kind::UInt8, 1.0f,  3.0f,   nullptr, &WorldShapeParams::continents },
    { "islands",           ShapeParam::Islands,           Kind::UInt8, 0.0f,  6.0f,   nullptr, &WorldShapeParams::islands },
    { "continentIslands",  ShapeParam::ContinentIslands,  Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::continentIslands,  nullptr },
    { "peninsulas",        ShapeParam::Peninsulas,        Kind::UInt8, 0.0f,  3.0f,   nullptr, &WorldShapeParams::peninsulas },
    { "coastline",         ShapeParam::Coastline,         Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::coastline,         nullptr },
    { "mountainousness",   ShapeParam::Mountainousness,   Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::mountainousness,   nullptr },
    { "spineToRing",       ShapeParam::SpineToRing,       Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::spineToRing,       nullptr },
    { "orientationDeg",    ShapeParam::Orientation,       Kind::Float, 0.0f,  360.0f, &WorldShapeParams::orientationDeg,    nullptr },
    { "passes",            ShapeParam::Passes,            Kind::UInt8, 0.0f,  4.0f,   nullptr, &WorldShapeParams::passes },
    { "barrier",           ShapeParam::Barrier,           Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::barrier,           nullptr },
    { "barrierForm",       ShapeParam::BarrierForm,       Kind::Form,  0.0f,  1.0f,   nullptr, nullptr },
    { "ruggedness",        ShapeParam::Ruggedness,        Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::ruggedness,        nullptr },
    { "rivers",            ShapeParam::Rivers,            Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::rivers,            nullptr },
    { "valleyDepth",       ShapeParam::ValleyDepth,       Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::valleyDepth,       nullptr },
    { "lakes",             ShapeParam::Lakes,             Kind::UInt8, 0.0f,  12.0f,  nullptr, &WorldShapeParams::lakes },
    { "latitudeNorthDeg",  ShapeParam::LatitudeNorth,     Kind::Float, -90.0f, 90.0f, &WorldShapeParams::latitudeNorthDeg,  nullptr },
    { "latitudeSouthDeg",  ShapeParam::LatitudeSouth,     Kind::Float, -90.0f, 90.0f, &WorldShapeParams::latitudeSouthDeg,  nullptr },
    { "temperatureOffset", ShapeParam::TemperatureOffset, Kind::Float, -0.3f, 0.3f,   &WorldShapeParams::temperatureOffset, nullptr },
    { "moistureOffset",    ShapeParam::MoistureOffset,    Kind::Float, -0.3f, 0.3f,   &WorldShapeParams::moistureOffset,    nullptr },
};

const ParamRow* FindRow(const std::string& key)
{
    for (const ParamRow& r : kRows)
    {
        if (key == r.key)
        {
            return &r;
        }
    }
    return nullptr;
}

// Shortest %.{1..9}g text that reads back as the same float, as a double for jsoncpp.
double ShortestFloat(float f)
{
    char text[32];
    for (int digits = 1; digits <= 9; ++digits)
    {
        std::snprintf(text, sizeof(text), "%.*g", digits, static_cast<double>(f));
        if (static_cast<float>(std::strtod(text, nullptr)) == f)
        {
            break;
        }
    }
    return std::strtod(text, nullptr);
}

// FNV-1a 64 over raw cell bytes (4 bytes per cell, little-endian host), as 16 lowercase hex digits.
std::string LayerHash(const void* data, size_t bytes)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < bytes; ++i)
    {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(h));
    return text;
}

constexpr int64_t kLayersDefaultCount = 500;
constexpr int64_t kLayersMaxCount = 1000;
// The sim reply writer prints doubles with 17 significant digits (~21 chars each), so a 1000-value float page
// would overflow the 16 KB reply; float pages are capped lower.
constexpr int64_t kLayersMaxFloatCount = 500;

// Optional non-negative integer argument; false (with outError set) when present but invalid.
bool ReadPagingArg(const Json::Value& args, const char* key, int64_t defaultValue, int64_t& out, std::string& outError)
{
    out = defaultValue;
    if (!args.isMember(key))
    {
        return true;
    }
    const Json::Value& v = args[key];
    if (!v.isNumeric() || !v.isIntegral() || v.asDouble() < 0.0 || v.asDouble() > 9.0e15)
    {
        outError = std::string("\"") + key + "\" must be a non-negative integer";
        return false;
    }
    out = static_cast<int64_t>(v.asDouble());
    return true;
}

Json::Value Error(const std::string& message)
{
    Json::Value r(Json::objectValue);
    r["success"] = false;
    r["error"] = message;
    return r;
}

bool ParseSeed(const Json::Value& v, uint64_t& out)
{
    if (!v.isIntegral())
    {
        return false;
    }
    if (v.isInt64() && v.asInt64() < 0)
    {
        return false;
    }
    if (!v.isUInt64())
    {
        return false;
    }
    out = v.asUInt64();
    return true;
}

bool ParseSize(const std::string& name, WorldSize& out)
{
    for (WorldSize s : { WorldSize::Small, WorldSize::Standard, WorldSize::Large })
    {
        if (name == WorldSizeName(s))
        {
            out = s;
            return true;
        }
    }
    return false;
}

Json::Value DimensionsJson(GridDimensions d)
{
    Json::Value v(Json::objectValue);
    v["width"] = d.width;
    v["height"] = d.height;
    return v;
}

Json::Value ParamValueJson(const ParamRow& r, const WorldShapeParams& p)
{
    switch (r.kind)
    {
    case Kind::Float: return Json::Value(ShortestFloat(p.*(r.fieldF)));
    case Kind::UInt8: return Json::Value(static_cast<Json::UInt>(p.*(r.fieldU)));
    case Kind::Form:  return Json::Value(BarrierFormName(p.barrierForm));
    }
    return Json::Value();
}

} // namespace

const char* WorldGenStateName(WorldGenState state)
{
    switch (state)
    {
    case WorldGenState::Idle:     return "Idle";
    case WorldGenState::Complete: return "Complete";
    case WorldGenState::Failed:   return "Failed";
    }
    return "Unknown";
}

const char* WorldSizeName(WorldSize size)
{
    switch (size)
    {
    case WorldSize::Small:    return "Small";
    case WorldSize::Standard: return "Standard";
    case WorldSize::Large:    return "Large";
    }
    return "Unknown";
}

GridDimensions WorldSizeDimensions(WorldSize size)
{
    switch (size)
    {
    case WorldSize::Small:    return GridDimensions{ 66, 42 };
    case WorldSize::Standard: return GridDimensions{ 80, 52 };
    case WorldSize::Large:    return GridDimensions{ 104, 64 };
    }
    return GridDimensions{ 66, 42 };
}

WorldGenSession::WorldGenSession()
    : WorldGenSession(TerrainRules::Defaults())
{
}

WorldGenSession::WorldGenSession(const TerrainRules& rules)
    : mRules(rules)
    , mGen(rules)
    , mDims(WorldSizeDimensions(WorldSize::Small))
{
}

Json::Value WorldGenSession::Set(const Json::Value& args)
{
    if (!args.isObject())
    {
        return Error("args must be a JSON object");
    }

    Json::Value applied(Json::arrayValue);
    Json::Value clamped(Json::arrayValue);
    Json::Value rejected(Json::arrayValue);

    for (const std::string& key : args.getMemberNames())
    {
        const Json::Value& jv = args[key];

        if (key == "unlock")
        {
            continue;   // accepted and ignored in this slice
        }
        if (key == "seed")
        {
            uint64_t seed = 0;
            if (!ParseSeed(jv, seed))
            {
                rejected.append(key);
                continue;
            }
            if (seed != mSeed)
            {
                mSeed = seed;
                mDirty = true;
            }
            applied.append(key);
            continue;
        }

        const ParamRow* row = FindRow(key);
        if (row == nullptr)
        {
            rejected.append(key);
            continue;
        }

        WorldShapeParams next = mShape;
        bool wasClamped = false;

        if (row->kind == Kind::Form)
        {
            const std::string form = jv.isString() ? jv.asString() : std::string();
            if (form == "Plateau")
            {
                next.barrierForm = BarrierForm::Plateau;
            }
            else if (form == "VolcanicChain")
            {
                next.barrierForm = BarrierForm::VolcanicChain;
            }
            else
            {
                rejected.append(key);
                continue;
            }
        }
        else
        {
            if (!jv.isNumeric())
            {
                rejected.append(key);
                continue;
            }
            const double d = jv.asDouble();
            if (!std::isfinite(d))
            {
                rejected.append(key);
                continue;
            }
            if (row->kind == Kind::Float)
            {
                const float requested = static_cast<float>(d);
                next.*(row->fieldF) = requested;
                next.Clamp();
                wasClamped = next.*(row->fieldF) != requested;
            }
            else
            {
                const double rounded = std::round(d);
                next.*(row->fieldU) = static_cast<uint8_t>(std::min(std::max(rounded, 0.0), 255.0));
                next.Clamp();
                wasClamped = static_cast<double>(next.*(row->fieldU)) != rounded;
            }
        }

        applied.append(key);
        if (wasClamped)
        {
            clamped.append(key);
        }
        if (!(next == mShape))
        {
            mShape = next;
            mDirty = true;
            if (!mPresetId.empty())
            {
                mPresetId.clear();
                mPresetModified = true;
            }
        }
    }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["applied"] = applied;
    r["clamped"] = clamped;
    r["rejected"] = rejected;
    return r;
}

Json::Value WorldGenSession::SetSize(const Json::Value& args)
{
    if (!args.isObject() || !args.isMember("size") || !args["size"].isString())
    {
        return Error("\"size\" must be one of Small, Standard, Large");
    }
    WorldSize size = WorldSize::Small;
    if (!ParseSize(args["size"].asString(), size))
    {
        return Error("unknown size \"" + args["size"].asString() + "\" (expected Small, Standard or Large)");
    }
    const GridDimensions dims = WorldSizeDimensions(size);
    if (dims.width != mDims.width || dims.height != mDims.height)
    {
        mDims = dims;
        mDirty = true;
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["dimensions"] = DimensionsJson(mDims);
    return r;
}

Json::Value WorldGenSession::Regenerate(const Json::Value& args)
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    if (args.isObject() && args.isMember("seed"))
    {
        uint64_t seed = 0;
        if (!ParseSeed(args["seed"], seed))
        {
            return Error("\"seed\" must be a non-negative integer");
        }
        mSeed = seed;
    }
    mDirty = true;
    mForceFull = true;
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["pending"] = true;
    return r;
}

bool WorldGenSession::Tick(Dia::Maths::RandomStream& wldStream)
{
    if (!mDirty)
    {
        return false;
    }
    if (mForceFull)
    {
        mGen.Invalidate();
    }
    mGen.Update(mSeed, mShape, mDims, wldStream);
    mGen.WriteTerrain(mWorld);

    mLastMs = mGen.GetLastTotalMs();
    mStagesRan = Json::Value(Json::arrayValue);
    const StageTiming* timings = mGen.GetTimings();
    for (int i = 0; i < kImplementedStageCount; ++i)
    {
        if (timings[i].ranLastUpdate)
        {
            mStagesRan.append(TerrainStageName(static_cast<TerrainStage>(i)));
        }
    }

    ++mRevision;
    mState = WorldGenState::Complete;
    mHasGenerated = true;
    mDirty = false;
    mForceFull = false;
    return true;
}

Json::Value WorldGenSession::Params() const
{
    const WorldShapeParams defaults;

    Json::Value params(Json::objectValue);
    Json::Value ranges(Json::objectValue);
    for (const ParamRow& r : kRows)
    {
        params[r.key] = ParamValueJson(r, mShape);

        Json::Value range(Json::objectValue);
        if (r.kind == Kind::Form)
        {
            range["min"] = 0;
            range["max"] = 1;
            range["default"] = 0;
            Json::Value values(Json::arrayValue);
            values.append(BarrierFormName(BarrierForm::Plateau));
            values.append(BarrierFormName(BarrierForm::VolcanicChain));
            range["values"] = values;
        }
        else
        {
            range["min"] = ShortestFloat(r.lo);
            range["max"] = ShortestFloat(r.hi);
            range["default"] = ParamValueJson(r, defaults);
        }
        range["firstStage"] = TerrainStageName(FirstStageAffected(r.param));
        ranges[r.key] = range;
    }

    Json::Value effective(Json::objectValue);
    if (mHasGenerated)
    {
        const Effective& e = mGen.GetEffective();
        effective["continents"] = e.continents;
        effective["islands"] = e.islands;
        effective["islandShare"] = ShortestFloat(e.islandShare);
        effective["passes"] = e.passes;
        effective["lakes"] = e.lakes;
    }
    else
    {
        effective["continents"] = static_cast<int>(mShape.continents);
        effective["islands"] = static_cast<int>(mShape.islands);
        effective["islandShare"] = 0.0;
        effective["passes"] = static_cast<int>(mShape.passes);
        effective["lakes"] = static_cast<int>(mShape.lakes);
    }

    Json::Value r(Json::objectValue);
    r["seed"] = static_cast<Json::UInt64>(mSeed);
    r["params"] = params;
    r["preset"] = mPresetId;
    r["modified"] = mPresetModified;
    r["locked"] = false;
    r["ranges"] = ranges;
    r["effective"] = effective;
    return r;
}

Json::Value WorldGenSession::Status() const
{
    const char* sizeName = "Custom";
    for (WorldSize s : { WorldSize::Small, WorldSize::Standard, WorldSize::Large })
    {
        const GridDimensions d = WorldSizeDimensions(s);
        if (d.width == mDims.width && d.height == mDims.height)
        {
            sizeName = WorldSizeName(s);
        }
    }

    Json::Value r(Json::objectValue);
    r["state"] = WorldGenStateName(mState);
    r["revision"] = static_cast<Json::UInt>(mRevision);
    r["seed"] = static_cast<Json::UInt64>(mSeed);
    r["size"] = sizeName;
    r["dimensions"] = DimensionsJson(mDims);
    r["locked"] = false;
    r["lastMs"] = ShortestFloat(mLastMs);
    r["stagesRan"] = mStagesRan;
    return r;
}

void WorldGenSession::SetPresetDirectories(const std::string& deployedDir, const std::string& outDir)
{
    mDeployedDir = deployedDir;
    mOutDir = outDir;
}

Json::Value WorldGenSession::PresetLoad(const Json::Value& args)
{
    if (!args.isObject() || !args.isMember("id") || !args["id"].isString())
    {
        return Error("\"id\" must be a preset id string");
    }
    const std::string id = args["id"].asString();
    if (!IsValidPresetId(id))
    {
        return Error("invalid preset id \"" + id + "\" (expected [a-z0-9_]{1,32})");
    }

    // The out/ copy wins over the deployed one; an invalid out/ copy is reported, not skipped.
    std::string directory;
    std::error_code ec;
    if (!mOutDir.empty() && std::filesystem::exists(PresetFilePath(mOutDir, id), ec))
    {
        directory = mOutDir;
    }
    else if (!mDeployedDir.empty())
    {
        directory = mDeployedDir;
    }
    else
    {
        return Error("unknown preset \"" + id + "\"");
    }

    WorldShapePreset preset;
    RuleWarnings warnings;
    const PresetResult result = LoadPreset(directory, id, preset, warnings);
    if (!result.Ok())
    {
        return Error(result.message.empty() ? std::string(PresetStatusName(result.status)) : result.message);
    }

    WorldShapeParams next = preset.params;
    next.Clamp();
    if (!(next == mShape))
    {
        mShape = next;
        mDirty = true;
    }
    mPresetId = id;
    mPresetModified = false;

    Json::Value warningList(Json::arrayValue);
    for (unsigned int i = 0; i < warnings.Size(); ++i)
    {
        warningList.append(warnings[i].AsChar());
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["id"] = id;
    r["warnings"] = warningList;
    return r;
}

Json::Value WorldGenSession::PresetSave(const Json::Value& args)
{
    if (!args.isObject() || !args.isMember("id") || !args["id"].isString())
    {
        return Error("\"id\" must be a preset id string");
    }
    if (mOutDir.empty())
    {
        return Error("preset output directory is not configured");
    }

    WorldShapePreset preset;
    preset.id = args["id"].asString();
    preset.displayName = args.isMember("displayName") && args["displayName"].isString() ? args["displayName"].asString() : preset.id;
    if (args.isMember("description") && args["description"].isString())
    {
        preset.description = args["description"].asString();
    }
    preset.params = mShape;
    const bool overwrite = args.isMember("overwrite") && args["overwrite"].isBool() && args["overwrite"].asBool();

    const PresetResult result = SavePreset(mOutDir, preset, overwrite);
    if (!result.Ok())
    {
        Json::Value r = Error(result.message.empty() ? std::string(PresetStatusName(result.status)) : result.message);
        r["status"] = PresetStatusName(result.status);
        return r;
    }

    mPresetId = preset.id;
    mPresetModified = false;

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["writtenTo"] = result.path.empty() ? PresetFilePath(mOutDir, preset.id) : result.path;
    r["isSource"] = false;
    return r;
}

Json::Value WorldGenSession::Presets() const
{
    std::map<std::string, PresetInfo> byId;
    if (!mDeployedDir.empty())
    {
        for (const PresetInfo& p : ListPresets(mDeployedDir))
        {
            byId[p.id] = p;
        }
    }
    if (!mOutDir.empty())
    {
        for (const PresetInfo& p : ListPresets(mOutDir))
        {
            byId[p.id] = p;   // out/ wins
        }
    }

    Json::Value list(Json::arrayValue);
    for (const auto& entry : byId)
    {
        const PresetInfo& p = entry.second;
        Json::Value e(Json::objectValue);
        e["id"] = p.id;
        e["displayName"] = p.displayName;
        e["description"] = p.description;
        e["version"] = static_cast<Json::UInt>(p.version);
        e["sourcePath"] = p.sourcePath;
        list.append(e);
    }
    return list;
}

Json::Value WorldGenSession::Layers(const Json::Value& args) const
{
    const TerrainLayers& layers = mGen.GetLayers();
    // Size of the GENERATED grid: SetSize only changes pending dimensions (mDims) until the next Tick, and the layer
    // buffers still have the old size, so paging/min-max/hash must not use mDims here.
    const size_t count = mHasGenerated ? static_cast<size_t>(mWorld.CellCount()) : 0u;

    if (args.isObject() && args.isMember("layer"))
    {
        if (!mHasGenerated)
        {
            return Error("no world generated yet");
        }
        if (!args["layer"].isString())
        {
            return Error("\"layer\" must be a layer name string");
        }
        const std::string name = args["layer"].asString();
        const int index = layers.FindLayer(name.c_str());
        if (index < 0)
        {
            return Error("unknown layer \"" + name + "\"");
        }
        const LayerInfo& info = TerrainLayers::GetLayerInfo(index);

        int64_t offset = 0;
        int64_t pageCount = 0;
        std::string argError;
        if (!ReadPagingArg(args, "offset", 0, offset, argError) ||
            !ReadPagingArg(args, "count", kLayersDefaultCount, pageCount, argError))
        {
            return Error(argError);
        }
        const int64_t total = static_cast<int64_t>(count);
        if (offset > total)
        {
            return Error("\"offset\" " + std::to_string(offset) + " is beyond the layer size " + std::to_string(total));
        }
        pageCount = std::min(pageCount, info.type == LayerType::Float ? kLayersMaxFloatCount : kLayersMaxCount);
        pageCount = std::min(pageCount, total - offset);

        Json::Value values(Json::arrayValue);
        if (info.type == LayerType::Float)
        {
            const float* data = layers.GetFloat(index);
            for (int64_t i = offset; data != nullptr && i < offset + pageCount; ++i)
            {
                values.append(ShortestFloat(data[i]));
            }
        }
        else
        {
            const uint32_t* data = layers.GetUInt(index);
            for (int64_t i = offset; data != nullptr && i < offset + pageCount; ++i)
            {
                values.append(static_cast<Json::UInt>(data[i]));
            }
        }

        const int64_t returned = static_cast<int64_t>(values.size());
        const int64_t next = offset + returned;
        Json::Value r(Json::objectValue);
        r["success"] = true;
        r["name"] = info.name;
        r["type"] = info.type == LayerType::Float ? "float" : "uint";
        r["revision"] = static_cast<Json::UInt>(mRevision);
        r["offset"] = static_cast<Json::Int64>(offset);
        r["count"] = static_cast<Json::Int64>(returned);
        r["total"] = static_cast<Json::Int64>(total);
        // Null once the end is reached (including offset == total); a count-0 request mid-layer echoes the offset.
        r["nextOffset"] = next >= total ? Json::Value() : Json::Value(static_cast<Json::Int64>(next));
        r["values"] = values;
        return r;
    }

    const StageTiming* timings = mGen.GetTimings();

    Json::Value stages(Json::arrayValue);
    for (int s = 0; s < kImplementedStageCount; ++s)
    {
        Json::Value stage(Json::objectValue);
        stage["id"] = s;
        stage["name"] = TerrainStageName(static_cast<TerrainStage>(s));
        stage["ms"] = ShortestFloat(timings[s].milliseconds);
        stage["ranLastUpdate"] = timings[s].ranLastUpdate;

        Json::Value stageLayers(Json::arrayValue);
        for (int i = 0; mHasGenerated && i < TerrainLayers::GetLayerCount(); ++i)
        {
            const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
            if (static_cast<int>(info.stage) != s)
            {
                continue;
            }
            double lo = 0.0;
            double hi = 0.0;
            if (info.type == LayerType::Float)
            {
                const float* data = layers.GetFloat(i);
                if (data != nullptr && count > 0)
                {
                    const auto mm = std::minmax_element(data, data + count);
                    lo = ShortestFloat(*mm.first);
                    hi = ShortestFloat(*mm.second);
                }
            }
            else
            {
                const uint32_t* data = layers.GetUInt(i);
                if (data != nullptr && count > 0)
                {
                    const auto mm = std::minmax_element(data, data + count);
                    lo = static_cast<double>(*mm.first);
                    hi = static_cast<double>(*mm.second);
                }
            }
            Json::Value l(Json::objectValue);
            l["name"] = info.name;
            l["type"] = info.type == LayerType::Float ? "float" : "uint";
            l["min"] = lo;
            l["max"] = hi;
            // Hash is omitted before the first generation (this loop only runs once mHasGenerated).
            const void* raw = info.type == LayerType::Float ? static_cast<const void*>(layers.GetFloat(i))
                                                            : static_cast<const void*>(layers.GetUInt(i));
            if (raw != nullptr)
            {
                l["hash"] = LayerHash(raw, count * sizeof(uint32_t));
            }
            stageLayers.append(l);
        }
        stage["layers"] = stageLayers;
        stages.append(stage);
    }

    Json::Value r(Json::objectValue);
    r["rulesVersion"] = static_cast<Json::UInt>(mRules.rulesVersion);
    r["revision"] = static_cast<Json::UInt>(mRevision);
    r["totalMs"] = ShortestFloat(mLastMs);
    r["stages"] = stages;
    return r;
}

Json::Value WorldGenSession::Cell(const Json::Value& args) const
{
    if (!mHasGenerated)
    {
        return Error("no world generated yet");
    }
    if (!args.isObject())
    {
        return Error("args must be a JSON object");
    }

    const int width = mWorld.dimensions.width;
    const int height = mWorld.dimensions.height;
    const int count = mWorld.CellCount();

    int index = 0;
    if (args.isMember("index"))
    {
        const Json::Value& v = args["index"];
        if (!v.isIntegral() || !v.isInt64() || v.asInt64() < 0 || v.asInt64() >= count)
        {
            return Error("\"index\" must be an integer in [0, " + std::to_string(count) + ")");
        }
        index = static_cast<int>(v.asInt64());
    }
    else
    {
        if (!args.isMember("x") || !args.isMember("y") || !args["x"].isIntegral() || !args["y"].isIntegral())
        {
            return Error("expected integer \"x\" and \"y\", or integer \"index\"");
        }
        const Json::Int64 x = args["x"].isInt64() ? args["x"].asInt64() : -1;
        const Json::Int64 y = args["y"].isInt64() ? args["y"].asInt64() : -1;
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return Error("cell out of range (grid is " + std::to_string(width) + "x" + std::to_string(height) + ")");
        }
        index = static_cast<int>(y) * width + static_cast<int>(x);
    }

    const WorldGeneration::Cell& c = mWorld.Cells()[index];
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["x"] = index % width;
    r["y"] = index / width;
    r["index"] = index;
    r["elevation"] = ShortestFloat(c.elevation);
    r["latitude"] = ShortestFloat(c.latitude);
    r["waterSource"] = WaterSourceName(c.waterSource);
    r["terrain"] = TerrainTypeName(c.terrain);
    r["depthZone"] = DepthZoneName(c.depthZone);
    r["landmass"] = static_cast<Json::UInt>(c.landmass);
    r["flags"] = static_cast<Json::UInt>(c.terrainFlags);
    r["downstream"] = c.downstream == kNoDirection ? Json::Value() : Json::Value(static_cast<Json::UInt>(c.downstream));

    const TerrainLayers& layers = mGen.GetLayers();
    Json::Value layerValues(Json::objectValue);
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
        if (info.type == LayerType::Float)
        {
            const float* data = layers.GetFloat(i);
            layerValues[info.name] = data != nullptr ? Json::Value(ShortestFloat(data[index])) : Json::Value();
        }
        else
        {
            const uint32_t* data = layers.GetUInt(i);
            layerValues[info.name] = data != nullptr ? Json::Value(static_cast<Json::UInt>(data[index])) : Json::Value();
        }
    }
    r["layers"] = layerValues;

    const Dia::Pathfinding::HexTopology topology(width, height);
    Json::Value neighbours(Json::arrayValue);
    for (uint32_t dir = 0; dir < 6; ++dir)
    {
        const uint32_t n = topology.NeighbourAt(static_cast<uint32_t>(index), dir);
        if (n == Dia::Pathfinding::kNoCell)
        {
            neighbours.append("edge");
            continue;
        }
        Json::Value e(Json::objectValue);
        e["dir"] = dir;
        e["x"] = static_cast<int>(n) % width;
        e["y"] = static_cast<int>(n) / width;
        neighbours.append(e);
    }
    r["neighbours"] = neighbours;
    return r;
}

} // namespace WorldGeneration
} // namespace CoW
