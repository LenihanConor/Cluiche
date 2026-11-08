// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    : mGen(rules)
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

} // namespace WorldGeneration
} // namespace CoW
