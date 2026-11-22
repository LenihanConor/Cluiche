// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <Shared/WorldGeneration/BiomeClassify.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/SiteRules.h>
#include <Shared/WorldGeneration/SiteRules.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/WorldFiles.h>
#include <Shared/WorldGeneration/WorldJson.h>
#include <Shared/WorldGeneration/WorldLayerSource.h>
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>

#include <DiaAttribute/ParameterFile.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <vector>

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

const char* BiomePinName(uint32_t pin)
{
    switch (pin)
    {
    case BiomePin::None:     return "None";
    case BiomePin::Wetland:  return "Wetland";
    case BiomePin::Alpine:   return "Alpine";
    case BiomePin::Volcanic: return "Volcanic";
    case BiomePin::Water:    return "Water";
    }
    return "Unknown";
}

// The registry's zone table (an empty table when there is no registry).
const Draoi::HexWorld::StratumTable& ZonesOf(const Draoi::HexWorld::BiomeRegistry* biomes)
{
    static const Draoi::HexWorld::StratumTable kNoStrata;
    return biomes != nullptr ? biomes->Strata() : kNoStrata;
}

// Display name of a registry row ("Unknown" for no registry or a row past the table).
const char* BiomeLabel(const Draoi::HexWorld::BiomeRegistry* biomes, Draoi::HexWorld::BiomeIndex b)
{
    const char* name = biomes != nullptr ? biomes->Name(b) : "";
    return name[0] != '\0' ? name : "Unknown";
}

Json::Value ScoredBiomeJson(const Draoi::HexWorld::BiomeRegistry* biomes, Draoi::HexWorld::BiomeIndex b, float score)
{
    Json::Value v(Json::objectValue);
    v["id"] = static_cast<int>(b);
    v["name"] = BiomeLabel(biomes, b);
    v["score"] = ShortestFloat(score);
    return v;
}

// Site listing page size. A site row is ~90 bytes of JSON (a registered kind id can be 64 chars longer), so 80 rows plus
// the kind table stay well under the 16 KB sim reply.
constexpr int64_t kSitesMaxLimit = 80;

// Info for a stored kind id: built-in node kinds, then registered placers; false when unknown.
bool FindKindInfo(const SiteRegistry& registry, uint32_t kindCrc, SiteKindInfo& out)
{
    const NodeKindIds& ids = registry.NodeKinds();
    for (const Dia::Core::StringCRC& k : { ids.corridor, ids.river, ids.hunting, ids.resource })
    {
        if (SiteKindId(k) == kindCrc)
        {
            out = BuiltinNodeKindInfo(ids, k);
            return true;
        }
    }
    for (ISitePlacer* p : registry.Order())
    {
        if (p->Info().kindCrc == kindCrc)
        {
            out = p->Info();
            return true;
        }
    }
    return false;
}

Json::Value KindInfoJson(const SiteKindInfo& i)
{
    Json::Value k(Json::objectValue);
    k["kind"] = i.id;
    k["displayName"] = i.displayName;
    k["debugColour"] = static_cast<Json::UInt>(i.debugColour);
    k["debugShape"] = SiteShapeName(i.debugShape);
    k["variantLabel"] = i.variantLabel;
    return k;
}

std::string KindIdText(const SiteRegistry& registry, uint32_t kindCrc)
{
    SiteKindInfo info;
    if (FindKindInfo(registry, kindCrc, info))
    {
        return info.id;
    }
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", kindCrc);
    return text;
}

Json::Value SiteJson(const SiteRegistry& registry, const Site& s, int width)
{
    Json::Value e(Json::objectValue);
    e["kind"] = KindIdText(registry, s.kindCrc);
    e["cell"] = static_cast<Json::UInt>(s.cell);
    e["x"] = static_cast<int>(s.cell) % width;
    e["y"] = static_cast<int>(s.cell) / width;
    e["region"] = static_cast<Json::UInt>(s.region);
    e["variant"] = static_cast<Json::UInt>(s.variant);
    e["flags"] = static_cast<Json::UInt>(s.flags);
    return e;
}

} // namespace

const char* WorldGenStateName(WorldGenState state)
{
    switch (state)
    {
    case WorldGenState::Idle:     return "Idle";
    case WorldGenState::Complete: return "Complete";
    case WorldGenState::Failed:   return "Failed";
    case WorldGenState::Generating: return "Generating";
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
    : mGen(mRules, nullptr)
    , mDims(WorldSizeDimensions(WorldSize::Small))
{
    CaptureAccepted();
}

WorldGenSession::WorldGenSession(const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry* biomes)
    : mRules(rules)
    , mRulesLoaded(true)
    , mBiomes(biomes)
    , mGen(rules, biomes)
    , mDims(WorldSizeDimensions(WorldSize::Small))
{
    CaptureAccepted();
}

WorldGenSession::~WorldGenSession() = default;
WorldGenSession::WorldGenSession(WorldGenSession&&) noexcept = default;
WorldGenSession& WorldGenSession::operator=(WorldGenSession&&) noexcept = default;

bool WorldGenSession::LoadRules(const char* sidecarText, Dia::Attribute::ParamError& err)
{
    if (mRun != nullptr || mHasGenerated)
    {
        err.key.SetText("state");
        err.reason.SetText("rules can only be loaded before the first generation");
        return false;
    }
    auto file = std::make_unique<Dia::Attribute::ParameterFile>();
    if (sidecarText == nullptr || !file->Load(sidecarText, "terrain_rules.schema.json", nullptr, err))
    {
        if (sidecarText == nullptr)
        {
            err.key.SetText("sidecar");
            err.reason.SetText("no sidecar text");
        }
        return false;
    }
    TerrainRules rules;
    if (!TerrainRules::FromParameterFile(*file, rules))
    {
        err.key.SetText("binding");
        err.reason.SetText("sidecar does not match the bound TerrainRules members");
        return false;
    }
    mRules = rules;
    mRulesFile = std::move(file);
    mRulesLoaded = true;
    mGen = TerrainGenerator(mRules, mBiomes);
    mGen.SetAssignerRegistry(mAssigners);
    return true;
}

bool WorldGenSession::RulesRefusal(Json::Value& refusal) const
{
    if (!mBlockCode.empty())
    {
        refusal = Error(mBlockCode.c_str());
        refusal["message"] = "generation is blocked: " + mBlockCode;
        return true;
    }
    if (mRulesLoaded)
    {
        return false;
    }
    refusal = Error("rules_not_loaded");
    refusal["rulesLoaded"] = false;
    refusal["message"] = "the terrain rules sidecar is not loaded; generation is refused";
    return true;
}

bool WorldGenSession::BusyRefusal(Json::Value& refusal) const
{
    if (mRun == nullptr)
    {
        return false;
    }
    refusal = Error("busy");
    refusal["busy"] = true;
    refusal["generating"] = true;
    refusal["message"] = "a Regenerate is in progress; wait for it to finish or cancel it";
    return true;
}

Json::Value WorldGenSession::Set(const Json::Value& args)
{
    if (!args.isObject())
    {
        return Error("args must be a JSON object");
    }
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
    Json::Value lockRefusal;
    bool unlocking = false;
    if (!CheckLock(args, lockRefusal, unlocking))
    {
        return lockRefusal;
    }

    Json::Value applied(Json::arrayValue);
    Json::Value clamped(Json::arrayValue);
    Json::Value rejected(Json::arrayValue);
    std::string r_strataError;

    for (const std::string& key : args.getMemberNames())
    {
        const Json::Value& jv = args[key];

        if (key == "unlock")
        {
            continue;   // consumed by the lock gate; any shape is accepted and ignored (only `true` unlocks a locked world)
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

        if (key == "strata")
        {
            // Live tweak of the stratum table's sidecar parameters: { "depth.FirstShores.share": 0.3, ... } (shares, minimum depths,
            // attribute values). All-or-nothing; a value that changes nothing leaves the world as it is.
            std::vector<std::pair<std::string, double>> changes;
            bool shapeOk = jv.isObject() && mBiomesMutable != nullptr;
            if (shapeOk)
            {
                for (const std::string& k : jv.getMemberNames())
                {
                    const Json::Value& v = jv[k];
                    if (v.isBool()) { changes.emplace_back(k, v.asBool() ? 1.0 : 0.0); }
                    else if (v.isNumeric() && std::isfinite(v.asDouble())) { changes.emplace_back(k, v.asDouble()); }
                    else { shapeOk = false; }
                }
            }
            Draoi::HexWorld::StratumError strataError;
            bool changedStrata = false;
            if (!shapeOk || !mBiomesMutable->SetStrataParams(changes, strataError, &changedStrata))
            {
                rejected.append(key);
                r_strataError = strataError.field.empty() ? std::string("strata must be an object of numeric sidecar parameters") : strataError.reason;
                continue;
            }
            applied.append(key);
            if (changedStrata)
            {
                mGen.MarkStrataDirty();
                mDirty = true;
            }
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

    if (unlocking)
    {
        DropLock();   // the hand edits are discarded even if no slider value changed
    }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["applied"] = applied;
    r["clamped"] = clamped;
    r["rejected"] = rejected;
    if (!r_strataError.empty()) { r["strataError"] = r_strataError; }
    return r;
}

Json::Value WorldGenSession::SetSize(const Json::Value& args)
{
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
    if (!args.isObject() || !args.isMember("size") || !args["size"].isString())
    {
        return Error("\"size\" must be one of Small, Standard, Large");
    }
    WorldSize size = WorldSize::Small;
    if (!ParseSize(args["size"].asString(), size))
    {
        return Error("unknown size \"" + args["size"].asString() + "\" (expected Small, Standard or Large)");
    }
    Json::Value lockRefusal;
    bool unlocking = false;
    if (!CheckLock(args, lockRefusal, unlocking))
    {
        return lockRefusal;
    }
    const GridDimensions dims = WorldSizeDimensions(size);
    if (dims.width != mDims.width || dims.height != mDims.height)
    {
        mDims = dims;
        mDirty = true;
    }
    if (unlocking)
    {
        DropLock();
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
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
    // Validate everything before changing anything.
    uint64_t seed = mSeed;
    if (args.isObject() && args.isMember("seed") && !ParseSeed(args["seed"], seed))
    {
        return Error("\"seed\" must be a non-negative integer");
    }
    uint32_t maxRetry = mGenParams.maxRetryAttempts;
    if (args.isObject() && args.isMember("maxRetryAttempts"))
    {
        const Json::Value& v = args["maxRetryAttempts"];
        if (!v.isNumeric() || !v.isIntegral() || v.asDouble() < 0.0 || v.asDouble() > 4.0e9)
        {
            return Error("\"maxRetryAttempts\" must be a non-negative integer");
        }
        maxRetry = ClampMaxRetryAttempts(static_cast<uint32_t>(v.asDouble()));
    }
    float timeout = mGenParams.timeoutSeconds;
    if (args.isObject() && args.isMember("timeoutSeconds"))
    {
        const Json::Value& v = args["timeoutSeconds"];
        if (!v.isNumeric() || v.asDouble() < 0.0 || v.asDouble() != v.asDouble())
        {
            return Error("\"timeoutSeconds\" must be a non-negative number");
        }
        timeout = ClampTimeoutSeconds(static_cast<float>(std::min(v.asDouble(), static_cast<double>(kMaxTimeoutSeconds))));
    }
    Json::Value lockRefusal;
    bool unlocking = false;
    if (!CheckLock(args, lockRefusal, unlocking))
    {
        return lockRefusal;
    }
    if (unlocking)
    {
        DropLock();
    }
    mSeed = seed;
    mGenParams.maxRetryAttempts = maxRetry;
    mGenParams.timeoutSeconds = timeout;
    mDirty = true;
    mForceFull = true;
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["pending"] = true;
    r["maxRetryAttempts"] = static_cast<Json::UInt>(mGenParams.maxRetryAttempts);
    r["timeoutSeconds"] = ShortestFloat(mGenParams.timeoutSeconds);
    return r;
}

void WorldGenSession::CaptureAccepted()
{
    mAccepted.seed = mSeed;
    mAccepted.shape = mShape;
    mAccepted.dims = mDims;
    mAccepted.presetId = mPresetId;
    mAccepted.presetModified = mPresetModified;
    mAccepted.locked = mLocked;
    mAccepted.state = mState;
}

void WorldGenSession::RefreshStageInfo()
{
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
}

void WorldGenSession::ReleaseRun()
{
    mRun.reset();      // before the guard and terrain it points at
    mGuard.reset();
    mCandGen.reset();
    mCandTerrain = WorldStructure{};
}

bool WorldGenSession::Tick(Dia::Maths::RandomStream& wldStream)
{
    if (!mRulesLoaded || !mBlockCode.empty() || mBiomes == nullptr)
    {
        return false;   // nothing generates without rules or a biome registry (or while the host has blocked generation)
    }
    if (mRun != nullptr)
    {
        return AdvanceRun();   // one attempt per Tick
    }
    if (!mDirty)
    {
        return false;
    }
    if (mForceFull)
    {
        return StartRegenerate(wldStream);   // placement runs on Regenerate only (SD-S1-004)
    }

    // First generation and live preview: terrain and biomes only, regions and sites stay empty.
    mGen.Update(mSeed, mShape, mDims, wldStream);
    mGen.WriteTerrain(mWorld);   // also clears regions, sites and Cell::region

    mRegionsFresh = false;
    mLimits = LayoutLimits{};
    mLayout = LayoutReport{};
    mPlaceResult = PlaceResult::Ok();

    mImported = false;
    RefreshStageInfo();

    ++mRevision;
    mState = WorldGenState::Complete;
    mHasGenerated = true;
    mDirty = false;
    mForceFull = false;
    CaptureAccepted();
    return true;
}

bool WorldGenSession::StartRegenerate(Dia::Maths::RandomStream& wldStream)
{
    // The candidate terrain is generated next to the accepted world, which stays untouched until the run succeeds.
    mDirty = false;
    mForceFull = false;
    try
    {
        mCandGen = std::make_unique<TerrainGenerator>(mRules, mBiomes);
        mCandGen->SetAssignerRegistry(mAssigners);
        mCandGen->Update(mSeed, mShape, mDims, wldStream);
        mCandTerrain = WorldStructure{};
        mCandGen->WriteTerrain(mCandTerrain);   // terrain and biomes; regions and sites empty
    }
    catch (const std::exception& e)
    {
        GenerationReport report;
        report.outerSeed = mSeed;
        report.outcome = GenerationOutcome::Error;
        report.maxAttempts = ClampMaxRetryAttempts(mGenParams.maxRetryAttempts);
        char message[kFailureMessageSize];
        std::snprintf(message, sizeof(message), "FAILURE: World generation error: %s", e.what());
        report.SetFailureMessage(message);
        DIA_LOG_ERROR("HexWorld", "%s (terrain, seed %llu)", message, static_cast<unsigned long long>(mSeed));
        mCandGen.reset();
        mCandTerrain = WorldStructure{};
        return FailRegenerate(report);
    }

    mGuard = std::make_unique<GenerationGuard>(ClampTimeoutSeconds(mGenParams.timeoutSeconds), mClock);
    mRun = std::make_unique<GenerationRun>(mCandTerrain, Registry(), mRules, *mBiomes, mSeed, mGenParams, wldStream, mStepNames, mGuard.get());
    mState = WorldGenState::Generating;
    return AdvanceRun();
}

bool WorldGenSession::AdvanceRun()
{
    mRun->Step();
    if (!mRun->IsFinished())
    {
        mState = WorldGenState::Generating;
        return false;
    }
    if (mRun->HasAcceptedWorld())
    {
        return CommitRun();
    }
    const GenerationReport report = mRun->Report();
    mRun.reset();      // the candidate generator and terrain stay until FailRegenerate has decided what to keep
    mGuard.reset();
    const bool changed = FailRegenerate(report);
    mCandGen.reset();
    mCandTerrain = WorldStructure{};
    return changed;
}

bool WorldGenSession::CommitRun()
{
    mRun->SetWorldRevision(mRevision + 1);
    mReport = mRun->Report();
    mHasReport = true;
    ++mReportRevision;
    mLastRunKeptPrevious = false;

    mWorld = std::move(mRun->AcceptedWorld());
    mLimits = mRun->AcceptedLimits();
    mPlaceResult = mRun->LastPlaceReport().result;
    mGen = std::move(*mCandGen);
    mLayout = CheckLayout(mWorld, mRules, Registry().NodeKinds(), mLimits);
    mRegionsFresh = true;
    ReleaseRun();

    mImported = false;
    mLocked = false;
    RefreshStageInfo();
    ++mRevision;
    mState = WorldGenState::Complete;
    mHasGenerated = true;
    CaptureAccepted();
    return true;
}

bool WorldGenSession::FailRegenerate(const GenerationReport& report)
{
    mReport = report;
    mReport.worldRevision = 0;
    mHasReport = true;
    ++mReportRevision;
    mLastRunKeptPrevious = false;

    if (!mHasGenerated)
    {
        // Nothing to keep: the very first generation was a Regenerate. A terrain-level error leaves no world at all;
        // otherwise the candidate terrain stays with regions and sites cleared (the Retry-plan behaviour).
        if (mCandGen == nullptr)
        {
            mState = WorldGenState::Failed;
            return false;
        }
        mGen = std::move(*mCandGen);
        mWorld = std::move(mCandTerrain);
        mRegionsFresh = false;
        mLimits = LayoutLimits{};
        mLayout = LayoutReport{};
        mPlaceResult = PlaceResult::Ok();
        mImported = false;
        RefreshStageInfo();
        ++mRevision;
        mState = WorldGenState::Failed;
        mHasGenerated = true;
        CaptureAccepted();
        return true;
    }

    // The previous world survives untouched; only the pending request reverts to what produced it.
    mSeed = mAccepted.seed;
    mShape = mAccepted.shape;
    mDims = mAccepted.dims;
    mPresetId = mAccepted.presetId;
    mPresetModified = mAccepted.presetModified;
    mLocked = mAccepted.locked;
    mDirty = false;
    mForceFull = false;
    mState = report.outcome == GenerationOutcome::Cancelled ? mAccepted.state : WorldGenState::Failed;
    mLastRunKeptPrevious = true;
    mValidatedSinceFailure = false;
    return false;
}

Json::Value WorldGenSession::Cancel(const Json::Value& args)
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    if (mRun != nullptr)
    {
        mGuard->RequestCancel();
        mRun->Step();   // the guard reports Cancelled at once: nothing more runs
        const GenerationReport report = mRun->Report();
        mRun.reset();
        mGuard.reset();
        FailRegenerate(report);
        mCandGen.reset();
        mCandTerrain = WorldStructure{};
        r["cancelled"] = true;
        r["outcome"] = GenerationOutcomeName(report.outcome);
        r["attempts"] = static_cast<Json::UInt>(report.attempts.Size());
        r["message"] = report.failureMessage;
        return r;
    }
    if (mDirty && mForceFull)
    {
        // A Regenerate that has not started: drop it and revert the pending request.
        GenerationReport report;
        report.outerSeed = mSeed;
        report.outcome = GenerationOutcome::Cancelled;
        report.maxAttempts = ClampMaxRetryAttempts(mGenParams.maxRetryAttempts);
        report.SetFailureMessage("CANCEL: World generation cancelled by user");
        DIA_LOG_INFO("HexWorld", "%s (before it started, seed %llu)", report.failureMessage, static_cast<unsigned long long>(mSeed));
        if (mHasGenerated)
        {
            FailRegenerate(report);
        }
        else
        {
            mReport = report;
            mHasReport = true;
            ++mReportRevision;
            mLastRunKeptPrevious = false;
            mForceFull = false;   // the first generation still happens, terrain only
        }
        r["cancelled"] = true;
        r["outcome"] = GenerationOutcomeName(report.outcome);
        r["attempts"] = 0;
        r["message"] = report.failureMessage;
        return r;
    }
    r["cancelled"] = false;
    r["message"] = "no generation in progress";
    return r;
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
    effective["imported"] = mImported;
    if (mHasGenerated && !mImported)
    {
        const Effective& e = mGen.GetEffective();
        effective["continents"] = e.continents;
        effective["islands"] = e.islands;
        effective["islandShare"] = ShortestFloat(e.islandShare);
        effective["passes"] = e.passes;
        effective["lakes"] = e.lakes;
        effective["unresolvedPairs"] = e.unresolvedPairs;
    }
    else
    {
        effective["continents"] = static_cast<int>(mShape.continents);
        effective["islands"] = static_cast<int>(mShape.islands);
        effective["islandShare"] = 0.0;
        effective["passes"] = static_cast<int>(mShape.passes);
        effective["lakes"] = static_cast<int>(mShape.lakes);
        effective["unresolvedPairs"] = 0;
    }
    Json::Value corridorLimited(Json::arrayValue);
    for (uint32_t b = 0; b < mLimits.barrierCount && b < kMaxBarriers; ++b) { corridorLimited.append(mLimits.corridorLimited[b]); }
    effective["corridorLimited"] = corridorLimited;
    effective["nodeLimited"] = mLimits.nodeLimited;
    effective["regionLimitedBy"] = StratumName(ZonesOf(mBiomes), mLimits.regionLimitedBy);

    Json::Value r(Json::objectValue);
    r["seed"] = static_cast<Json::UInt64>(mSeed);
    r["params"] = params;
    r["preset"] = mPresetId;
    r["modified"] = mPresetModified;
    r["locked"] = mLocked;
    r["source"] = mImported ? "imported" : "generated";
    r["ranges"] = ranges;
    r["effective"] = effective;
    r["maxRetryAttempts"] = static_cast<Json::UInt>(mGenParams.maxRetryAttempts);
    return r;
}

Json::Value WorldGenSession::UnavailableLayers() const
{
    // The generator layers ImportLayers rebuilds from the cells; every other generator layer (intermediate fields,
    // climate) is not derivable from a file and stays empty for an imported world.
    static const char* const kDerivable[] = {
        "elevation", "latitude", "ridgeMask", "barrierMask", "waterSource", "terrainType", "stratum", "landmass",
        "terrainFlags", "flowDirection", "biomeIndex", "riverMask", "passMask", "volcanic" };

    Json::Value out(Json::arrayValue);
    const TerrainLayers& layers = mGen.GetLayers();
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const char* id = WorldLayerSource::At(i).id;
        bool unavailable = !mHasGenerated;
        if (!unavailable && std::strcmp(id, "regions") == 0)
        {
            unavailable = !mRegionsFresh;
        }
        else if (!unavailable && std::strcmp(id, "relief") != 0)
        {
            const int index = layers.FindLayer(id);
            if (index >= 0)
            {
                unavailable = layers.GetFloat(index) == nullptr && layers.GetUInt(index) == nullptr;
                if (!unavailable && mImported)
                {
                    bool derivable = false;
                    for (const char* d : kDerivable) { derivable = derivable || std::strcmp(d, id) == 0; }
                    unavailable = !derivable;
                }
            }
        }
        if (unavailable)
        {
            out.append(id);
        }
    }
    return out;
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
    r["locked"] = mLocked;
    r["source"] = mImported ? "imported" : "generated";
    r["lastMs"] = ShortestFloat(mLastMs);
    r["stagesRan"] = mStagesRan;
    r["regionsFresh"] = mRegionsFresh;
    r["generating"] = mRun != nullptr;
    r["unavailableLayers"] = UnavailableLayers();
    if (mRun != nullptr)
    {
        Json::Value progress(Json::objectValue);
        progress["attempts"] = static_cast<Json::UInt>(mRun->AttemptsRun());
        progress["maxAttempts"] = static_cast<Json::UInt>(mRun->Report().maxAttempts);
        progress["elapsedSeconds"] = ShortestFloat(static_cast<float>(mGuard->ElapsedSeconds()));
        progress["timeoutSeconds"] = ShortestFloat(mGuard->TimeoutSeconds());
        r["progress"] = progress;
    }

    // Layout check summary of the current regions and sites; null when there are none (not generated or live preview).
    Json::Value layout;
    if (mRegionsFresh)
    {
        layout = Json::Value(Json::objectValue);
        layout["ok"] = mLayout.ok && mPlaceResult.IsOk();
        layout["placeResult"] = mPlaceResult.IsOk() ? "Ok" : mPlaceResult.reason;
        Json::Value violations(Json::arrayValue);
        for (const LayoutViolation& v : mLayout.violations)
        {
            Json::Value e(Json::objectValue);
            e["code"] = LayoutCodeName(v.code);
            e["region"] = static_cast<Json::UInt>(v.region);
            e["count"] = static_cast<Json::UInt>(v.count);
            violations.append(e);
        }
        layout["violations"] = violations;
    }
    r["layout"] = layout;

    // One-line summary of the last Regenerate's retry run; null before the first.
    Json::Value generation;
    if (mHasReport)
    {
        generation = Json::Value(Json::objectValue);
        generation["outcome"] = GenerationOutcomeName(mReport.outcome);
        generation["message"] = mReport.failureMessage;
        generation["attempts"] = static_cast<Json::UInt>(mReport.attempts.Size());
        generation["maxAttempts"] = static_cast<Json::UInt>(mReport.maxAttempts);
        generation["worldRevision"] = static_cast<Json::UInt>(mReport.worldRevision);
        generation["reportRevision"] = static_cast<Json::UInt>(mReportRevision);
    }
    r["generation"] = generation;
    return r;
}

bool WorldGenSession::CheckLock(const Json::Value& args, Json::Value& refusal, bool& unlocking) const
{
    unlocking = false;
    if (!mLocked)
    {
        return true;
    }
    if (args.isObject() && args.isMember("unlock") && args["unlock"].isBool() && args["unlock"].asBool())
    {
        unlocking = true;
        return true;
    }
    refusal = Error("locked");
    refusal["locked"] = true;
    refusal["needsConfirm"] = true;
    refusal["message"] = "the world is a locked import; repeat the action with \"unlock\":true to discard its edits and regenerate from the recorded recipe";
    return false;
}

void WorldGenSession::DropLock()
{
    mLocked = false;
    mDirty = true;   // the imported world is replaced by a generation on the next tick
}

void WorldGenSession::SetWorldDirectories(const std::string& deployedDir, const std::string& outDir)
{
    mWorldsDeployedDir = deployedDir;
    mWorldsOutDir = outDir;
}

namespace {

// Replies are capped at 16 KB: at most 40 warnings, each at most 240 characters, then a count of the rest.
Json::Value WarningList(const std::vector<std::string>& warnings)
{
    constexpr size_t kMaxListed = 40;
    constexpr size_t kMaxLength = 240;
    Json::Value list(Json::arrayValue);
    for (size_t i = 0; i < warnings.size() && i < kMaxListed; ++i)
    {
        list.append(warnings[i].size() > kMaxLength ? warnings[i].substr(0, kMaxLength) + "..." : warnings[i]);
    }
    if (warnings.size() > kMaxListed)
    {
        list.append("... and " + std::to_string(warnings.size() - kMaxListed) + " more");
    }
    return list;
}

}

Json::Value WorldGenSession::ExportWorld(const Json::Value& args) const
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;   // the session's seed/shape describe the pending request, not the world still on show
    }
    if (!mHasGenerated)
    {
        return Error("no world generated yet");
    }
    std::string name = DefaultWorldName(mWorld.provenance.seed);
    if (args.isObject() && args.isMember("name"))
    {
        if (!args["name"].isString())
        {
            return Error("\"name\" must be a world name string");
        }
        name = args["name"].asString();
    }
    const bool overwrite = args.isObject() && args.isMember("overwrite") && args["overwrite"].isBool() && args["overwrite"].asBool();
    if (!IsValidWorldName(name))
    {
        Json::Value r = Error("invalid world name \"" + name + "\" (expected [A-Za-z0-9_-]{1,64})");
        r["status"] = WorldFileStatusName(WorldFileStatus::InvalidName);
        return r;
    }

    WorldJsonContext context;
    context.registry = &Registry();
    context.biomes = mBiomes;
    context.presetId = mPresetId;
    std::string text;
    JsonIssue issue;
    if (!ExportWorldText(mWorld, context, text, issue))
    {
        Json::Value r = Error("cannot export: " + issue.message);
        r["field"] = issue.field;
        return r;
    }
    const WorldFileResult written = WriteWorldFile(mWorldsOutDir, name, text, overwrite);
    if (!written.Ok())
    {
        Json::Value r = Error(written.message);
        r["status"] = WorldFileStatusName(written.status);
        return r;
    }

    std::vector<std::string> warnings;
    if (mWorld.regions.regions.Size() == 0)
    {
        warnings.push_back("the world has no regions or sites (it did not come from Regenerate); the file holds terrain and biomes only");
    }
    if (mDirty)
    {
        warnings.push_back("a regeneration is pending; the file holds the world as it is now");
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["name"] = name;
    r["path"] = written.path;
    r["bytes"] = static_cast<Json::UInt64>(written.bytes);
    r["revision"] = static_cast<Json::UInt>(mRevision);
    r["warnings"] = WarningList(warnings);
    return r;
}

Json::Value WorldGenSession::ImportWorld(const Json::Value& args)
{
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
    if (!args.isObject() || !args.isMember("name") || !args["name"].isString())
    {
        return Error("\"name\" must be a world name string");
    }
    const std::string name = args["name"].asString();
    if (!IsValidWorldName(name))
    {
        Json::Value r = Error("invalid world name \"" + name + "\" (expected [A-Za-z0-9_-]{1,64})");
        r["status"] = WorldFileStatusName(WorldFileStatus::InvalidName);
        return r;
    }

    std::string text;
    const WorldFileResult read = ReadWorldFile(mWorldsOutDir, mWorldsDeployedDir, name, text);
    if (!read.Ok())
    {
        Json::Value r = Error(read.message);
        r["status"] = WorldFileStatusName(read.status);
        return r;
    }

    WorldJsonContext context;
    context.registry = &Registry();
    context.biomes = mBiomes;
    context.rulesVersion = mRules.rulesVersion;
    WorldStructure imported;
    const WorldImportResult result = ImportWorldText(text, context, imported);
    if (!result.ok)
    {
        Json::Value r = Error("cannot import " + name + ": " + result.error.message);
        r["field"] = result.error.field;
        r["status"] = "Invalid";
        if (result.error.line > 0)
        {
            r["line"] = static_cast<Json::UInt>(result.error.line);
            r["column"] = static_cast<Json::UInt>(result.error.column);
        }
        return r;
    }

    // Everything validated: adopt the world and its recipe, rebuild the generator layers, open locked.
    mWorld = std::move(imported);
    mSeed = mWorld.provenance.seed;
    mShape = mWorld.provenance.shape;
    mDims = mWorld.dimensions;
    mPresetId = mWorld.provenance.presetId;
    mPresetModified = false;
    mGen.ImportLayers(mWorld);

    mPlaceResult = PlaceResult::Ok();
    mLimits = LayoutLimits{};
    mLimits.regionLimitedBy = mWorld.regions.limitedBy;
    mLimits.barrierCount = static_cast<uint8_t>(BarrierCountOf(ZonesOf(mBiomes)));
    mRegionsFresh = mWorld.regions.regions.Size() > 0;
    mLayout = mRegionsFresh ? CheckLayout(mWorld, mRules, Registry().NodeKinds(), mLimits) : LayoutReport{};

    mState = WorldGenState::Complete;
    ++mRevision;
    mHasGenerated = true;
    mDirty = false;
    mForceFull = false;
    mLastMs = 0.0f;
    mStagesRan = Json::Value(Json::arrayValue);
    mLocked = true;
    mImported = true;
    CaptureAccepted();

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["name"] = name;
    r["path"] = read.path;
    r["bytes"] = static_cast<Json::UInt64>(read.bytes);
    r["revision"] = static_cast<Json::UInt>(mRevision);
    r["locked"] = true;
    r["seed"] = static_cast<Json::UInt64>(mSeed);
    r["preset"] = mPresetId;
    r["dimensions"] = DimensionsJson(mDims);
    r["regionsFresh"] = mRegionsFresh;
    r["warnings"] = WarningList(result.warnings);
    return r;
}

void WorldGenSession::SetPresetDirectories(const std::string& deployedDir, const std::string& outDir)
{
    mDeployedDir = deployedDir;
    mOutDir = outDir;
}

Json::Value WorldGenSession::PresetLoad(const Json::Value& args)
{
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
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

    Json::Value lockRefusal;
    bool unlocking = false;
    if (!CheckLock(args, lockRefusal, unlocking))
    {
        return lockRefusal;
    }
    WorldShapeParams next = preset.params;
    next.Clamp();
    if (!(next == mShape))
    {
        mShape = next;
        mDirty = true;
    }
    if (unlocking)
    {
        DropLock();
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
        // Cell-derived layers (not generator buffers): "regions" = raw Cell::region ids (0 while not fresh).
        const bool regionLayer = index < 0 && name == "regions";
        if (index < 0 && !regionLayer)
        {
            return Error("unknown layer \"" + name + "\"");
        }
        const LayerInfo info = regionLayer ? LayerInfo{ "regions", LayerType::UInt } : TerrainLayers::GetLayerInfo(index);

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
        else if (regionLayer)
        {
            const auto* cells = mWorld.Cells();
            for (int64_t i = offset; i < offset + pageCount; ++i)
            {
                values.append(static_cast<Json::UInt>(cells[i].region));
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

    // Cells per biome: one entry per registry row, zero before the first generation.
    const int biomeRows = mBiomes != nullptr ? static_cast<int>(mBiomes->Count()) : 0;
    std::vector<int> biomeTotals(static_cast<size_t>(biomeRows), 0);
    for (size_t i = 0; i < count; ++i)
    {
        const int b = static_cast<int>(mWorld.Cells()[i].biome);
        if (b >= 0 && b < biomeRows)
        {
            ++biomeTotals[static_cast<size_t>(b)];
        }
    }
    Json::Value biomeCounts(Json::arrayValue);
    for (int b = 0; b < biomeRows; ++b)
    {
        Json::Value e(Json::objectValue);
        e["id"] = b;
        e["name"] = BiomeLabel(mBiomes, static_cast<Draoi::HexWorld::BiomeIndex>(b));
        e["count"] = biomeTotals[static_cast<size_t>(b)];
        biomeCounts.append(e);
    }
    r["biomeCounts"] = biomeCounts;
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

    const Draoi::HexWorld::Cell& c = mWorld.Cells()[index];
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["x"] = index % width;
    r["y"] = index / width;
    r["index"] = index;
    r["elevation"] = ShortestFloat(c.elevation);
    r["latitude"] = ShortestFloat(c.latitude);
    r["waterSource"] = WaterSourceName(c.waterSource);
    r["terrain"] = TerrainTypeName(c.terrain);
    r["stratum"] = StratumName(ZonesOf(mBiomes), c.stratum);
    r["landmass"] = static_cast<Json::UInt>(c.landmass);
    r["flags"] = static_cast<Json::UInt>(c.terrainFlags);
    r["zone"] = StratumName(ZonesOf(mBiomes), c.stratum);
    r["region"] = static_cast<Json::UInt>(c.region);
    // Absorbed: the cell sits in a (non-island) region of another depth zone, i.e. an undersize-merge or fragment cell
    // (regions-and-sites.md); false without regions.
    {
        bool absorbed = false;
        if (c.region != 0 && c.region <= mWorld.regions.regions.Size())
        {
            const RegionRecord& rec = mWorld.regions.regions[c.region - 1];
            absorbed = !rec.isIsland && c.stratum != rec.stratum;
        }
        r["absorbed"] = absorbed;
    }
    {
        const SiteRegistry& registry = mRegistry != nullptr ? *mRegistry : mEmptyRegistry;
        Json::Value cellSites(Json::arrayValue);
        const SitesAtCell here = mWorld.sites.At(static_cast<uint32_t>(index));
        for (unsigned i = 0; i < here.Size(); ++i)
        {
            cellSites.append(SiteJson(registry, here[i], width));
        }
        r["sites"] = cellSites;
    }
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

    // Biome classification (stages 12-14).
    auto floatAt = [&](const char* name) -> const float* { return layers.GetFloat(layers.FindLayer(name)); };
    auto uintAt = [&](const char* name) -> const uint32_t* { return layers.GetUInt(layers.FindLayer(name)); };
    const float* temperature = floatAt("temperature");
    const float* moisture = floatAt("moisture");
    const uint32_t* waterDistance = uintAt("waterDistance");
    const uint32_t* biomePin = uintAt("biomePin");
    const uint32_t* fixup = uintAt("fixupChanges");
    r["temperature"] = temperature != nullptr ? Json::Value(ShortestFloat(temperature[index])) : Json::Value();
    r["moisture"] = moisture != nullptr ? Json::Value(ShortestFloat(moisture[index])) : Json::Value();
    r["waterDistance"] = waterDistance != nullptr ? Json::Value(static_cast<Json::UInt>(waterDistance[index])) : Json::Value();

    Json::Value biome(Json::objectValue);
    biome["id"] = static_cast<int>(c.biome);
    biome["name"] = BiomeLabel(mBiomes, c.biome);
    r["biome"] = biome;

    const uint32_t pin = biomePin != nullptr ? biomePin[index] : BiomePin::None;
    Json::Value pinJson(Json::objectValue);
    pinJson["id"] = static_cast<Json::UInt>(pin);
    pinJson["name"] = BiomePinName(pin);
    r["biomePin"] = pinJson;

    // Winning and runner-up of the five climate biomes (ties: lowest enum value). Pinned cells never reach the
    // climate score, so they report the pinned biome with a null score and no runner-up.
    Json::Value ranking(Json::objectValue);
    Draoi::HexWorld::BiomeIndex climateWinner = c.biome;
    uint32_t climateRowCount = 0;
    if (mBiomes != nullptr) { mBiomes->ClimateBiomes(climateRowCount); }
    if (pin != BiomePin::None || temperature == nullptr || moisture == nullptr || climateRowCount == 0)
    {
        Json::Value winner(Json::objectValue);
        winner["id"] = static_cast<int>(c.biome);
        winner["name"] = BiomeLabel(mBiomes, c.biome);
        winner["score"] = Json::Value();
        ranking["winner"] = winner;
        ranking["runnerUp"] = Json::Value();
    }
    else
    {
        uint32_t                                climateCount = 0;
        const Draoi::HexWorld::BiomeIndex*                climate = mBiomes->ClimateBiomes(climateCount);
        int                                     best = -1;
        int                                     second = -1;
        std::vector<float>                      scores(climateCount, 0.0f);
        for (int k = 0; k < static_cast<int>(climateCount); ++k)
        {
            scores[k] = BiomeClimateScore(*mBiomes, climate[k], temperature[index], moisture[index]);
            if (best < 0 || scores[k] > scores[best])
            {
                second = best;
                best = k;
            }
            else if (second < 0 || scores[k] > scores[second])
            {
                second = k;
            }
        }
        climateWinner = climate[best];
        ranking["winner"] = ScoredBiomeJson(mBiomes, climateWinner, scores[best]);
        ranking["runnerUp"] = second >= 0 ? ScoredBiomeJson(mBiomes, climate[second], scores[second]) : Json::Value();
    }
    r["biomeRunnerUp"] = ranking;

    const uint32_t fix = fixup != nullptr ? fixup[index] : BiomeFixup::None;
    Json::Value fixJson(Json::objectValue);
    fixJson["id"] = static_cast<Json::UInt>(fix);
    std::string reason;
    if (fix == BiomeFixup::Zone)
    {
        reason = std::string("zone: ") + BiomeLabel(mBiomes, climateWinner) + " is not valid in " + StratumName(ZonesOf(mBiomes), c.stratum);
    }
    else if (fix == BiomeFixup::Forbidden)
    {
        reason = "forbidden neighbour fix-up";
    }
    fixJson["reason"] = reason;
    r["fixupChanges"] = fixJson;

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

Json::Value WorldGenSession::Regions() const
{
    Json::Value list(Json::arrayValue);
    for (unsigned i = 0; i < mWorld.regions.regions.Size(); ++i)
    {
        const RegionRecord& rec = mWorld.regions.regions[i];
        Json::Value e(Json::objectValue);
        e["id"] = static_cast<Json::UInt>(rec.id);
        e["zone"] = StratumName(ZonesOf(mBiomes), rec.stratum);
        e["landmass"] = static_cast<Json::UInt>(rec.landmass);
        e["isIsland"] = rec.isIsland;
        e["primaryBiome"] = BiomeLabel(mBiomes, rec.primaryBiome);
        e["purity"] = ShortestFloat(rec.purity);
        e["cellCount"] = static_cast<Json::UInt>(rec.cellCount);
        e["centreCell"] = static_cast<Json::UInt>(rec.centreCell);
        Json::Value neighbours(Json::arrayValue);
        for (unsigned n = 0; n < rec.neighbours.Size(); ++n)
        {
            neighbours.append(static_cast<Json::UInt>(rec.neighbours[n]));
        }
        e["neighbours"] = neighbours;
        list.append(e);
    }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["regions"] = list;
    r["limitedBy"] = StratumName(ZonesOf(mBiomes), mWorld.regions.limitedBy);
    r["absorbedCells"] = static_cast<Json::UInt>(mWorld.regions.absorbedCells);
    r["fragmentCells"] = static_cast<Json::UInt>(mWorld.regions.fragmentCells);
    r["mainlandRegionCount"] = static_cast<Json::UInt>(mWorld.regions.mainlandRegionCount);
    r["fresh"] = mRegionsFresh;
    return r;
}

Json::Value WorldGenSession::Sites(const Json::Value& args) const
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    const SiteRegistry& registry = mRegistry != nullptr ? *mRegistry : mEmptyRegistry;

    std::span<const Site> source = mWorld.sites.All();
    if (args.isObject() && args.isMember("kind"))
    {
        if (!args["kind"].isString())
        {
            return Error("\"kind\" must be a site kind id string");
        }
        const std::string kind = args["kind"].asString();
        const uint32_t crc = SiteKindId(Dia::Core::StringCRC(kind.c_str()));
        SiteKindInfo info;
        if (!FindKindInfo(registry, crc, info))
        {
            return Error("unknown site kind \"" + kind + "\"");
        }
        source = mWorld.sites.Of(crc);
    }

    int64_t offset = 0;
    int64_t limit = kSitesMaxLimit;
    std::string argError;
    if (args.isObject() &&
        (!ReadPagingArg(args, "offset", 0, offset, argError) || !ReadPagingArg(args, "limit", kSitesMaxLimit, limit, argError)))
    {
        return Error(argError);
    }
    const int64_t total = static_cast<int64_t>(source.size());
    limit = std::min(limit, kSitesMaxLimit);
    offset = std::min(offset, total);
    limit = std::min(limit, total - offset);

    const int width = std::max(mWorld.dimensions.width, 1);
    Json::Value page(Json::arrayValue);
    for (int64_t i = offset; i < offset + limit; ++i)
    {
        page.append(SiteJson(registry, source[static_cast<size_t>(i)], width));
    }

    Json::Value kinds(Json::arrayValue);
    for (const SiteKindInfo& k : ListSiteKinds(registry))
    {
        kinds.append(KindInfoJson(k));
    }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["total"] = static_cast<Json::Int64>(total);
    r["offset"] = static_cast<Json::Int64>(offset);
    r["count"] = static_cast<Json::Int64>(page.size());
    r["sites"] = page;
    r["kinds"] = kinds;
    r["fresh"] = mRegionsFresh;
    return r;
}

namespace
{
    // Attempt rows carry up to 4 diagnostics of 160 chars plus ids, so ~1 KB each; 12 rows plus the final-validation
    // summary stay under the 16 KB sim reply.
    constexpr int64_t kReportMaxLimit = 12;

    const char* PlaceCodeName(PlaceResult::Code c)
    {
        switch (c)
        {
        case PlaceResult::Code::Ok:           return "Ok";
        case PlaceResult::Code::Retryable:    return "Retryable";
        case PlaceResult::Code::TerrainCause: return "TerrainCause";
        }
        return "Unknown";
    }

    // Evidence arrays are paged per guarantee so a reply stays under the 16 KB sim cap (a proof path is ~150 cells).
    constexpr int64_t kEvidenceMaxPaths = 8;
    constexpr int64_t kEvidenceMaxCells = 128;
    constexpr int64_t kEvidenceMaxPairs = 64;
    constexpr unsigned kMaxListedDiagnostics = 4;

    struct EvidencePage
    {
        size_t offset = 0;
        size_t pathLimit = 2;
        size_t cellLimit = 32;
        size_t pairLimit = 16;
    };

    // The id text of a guarantee: the injected spec's entries, then the built-in layout guarantee, else 0xXXXXXXXX.
    std::string GuaranteeName(const GuaranteeSpec& spec, uint32_t crc)
    {
        for (const GuaranteeEntry& e : spec.guarantees)
        {
            if (Dia::Core::StringCRC(e.id.c_str()).Value() == crc) { return e.id; }
        }
        if (Dia::Core::StringCRC(WorldLayoutGuarantee::kName).Value() == crc) { return WorldLayoutGuarantee::kName; }
        char text[16];
        std::snprintf(text, sizeof(text), "0x%08X", crc);
        return text;
    }

    // `selector` is a guarantee id (CRC number) or name text.
    bool GuaranteeMatches(const GuaranteeSpec& spec, const Json::Value& selector, uint32_t crc)
    {
        if (selector.isUInt()) { return selector.asUInt() == crc; }
        const std::string text = selector.asString();
        return GuaranteeName(spec, crc) == text || Dia::Core::StringCRC(text.c_str()).Value() == crc;
    }

    // One page of `list` (offset, limit), reporting whether more remain.
    template <class T, class Fn>
    Json::Value PageOf(const EvidenceList<T>& list, size_t offset, size_t limit, bool& truncated, Fn toJson)
    {
        Json::Value page(Json::arrayValue);
        const size_t total = list.Size();
        const size_t begin = std::min(offset, total);
        const size_t end = std::min(begin + limit, total);
        for (size_t i = begin; i < end; ++i) { page.append(toJson(list[static_cast<unsigned>(i)])); }
        truncated = truncated || end < total;
        return page;
    }

    // Guarantee row: id, name, ok, evidence counts and diagnostics; with a page also the evidence arrays themselves.
    // The counts are the sizes of the full lists, so they equal the array sizes whenever nothing was truncated.
    Json::Value GuaranteeJson(const GuaranteeSpec& spec, const GuaranteeResult& g, const EvidencePage* page, bool& truncated)
    {
        Json::Value e(Json::objectValue);
        e["id"] = static_cast<Json::UInt>(g.idCrc);
        e["name"] = GuaranteeName(spec, g.idCrc);
        e["ok"] = g.satisfied;
        e["satisfied"] = g.satisfied;
        Json::Value counts(Json::objectValue);
        counts["proofPaths"] = static_cast<Json::UInt>(g.proofPaths.Size());
        counts["violatingCells"] = static_cast<Json::UInt>(g.violatingCells.Size());
        counts["violatingPairs"] = static_cast<Json::UInt>(g.violatingPairs.Size());
        e["counts"] = counts;
        Json::Value diags(Json::arrayValue);
        for (unsigned i = 0; i < g.diagnostics.Size() && i < kMaxListedDiagnostics; ++i) { diags.append(g.diagnostics[i].text); }
        e["diagnostics"] = diags;
        e["diagnosticCount"] = static_cast<Json::UInt>(g.diagnostics.Size() + g.droppedDiagnostics);
        if (page == nullptr)
        {
            return e;
        }
        e["proofPaths"] = PageOf(g.proofPaths, page->offset, page->pathLimit, truncated, [](const ProofPath& p)
        {
            Json::Value v(Json::objectValue);
            v["from"] = static_cast<Json::UInt>(p.from);
            v["to"] = static_cast<Json::UInt>(p.to);
            v["profile"] = static_cast<Json::UInt>(p.profileCrc);
            Json::Value cells(Json::arrayValue);
            for (uint32_t c : p.cells) { cells.append(static_cast<Json::UInt>(c)); }
            v["cells"] = cells;
            return v;
        });
        e["violatingCells"] = PageOf(g.violatingCells, page->offset, page->cellLimit, truncated,
                                     [](uint32_t c) { return Json::Value(static_cast<Json::UInt>(c)); });
        e["violatingPairs"] = PageOf(g.violatingPairs, page->offset, page->pairLimit, truncated, [](const CellPair& p)
        {
            Json::Value v(Json::arrayValue);
            v.append(static_cast<Json::UInt>(p.a));
            v.append(static_cast<Json::UInt>(p.b));
            return v;
        });
        return e;
    }

    Json::Value AttemptJson(const AttemptRecord& a)
    {
        Json::Value e(Json::objectValue);
        e["attempt"] = static_cast<Json::UInt>(a.attempt);
        e["subSeed"] = static_cast<Json::UInt64>(a.subSeed);
        e["passed"] = a.passed;
        e["terrainCause"] = a.terrainCause;
        e["placeResult"] = PlaceCodeName(a.placeCode);
        e["placeReason"] = a.placeReason;
        Json::Value failed(Json::arrayValue);
        for (uint32_t i = 0; i < a.failedGuaranteeCount; ++i)
        {
            Json::Value g(Json::objectValue);
            g["id"] = static_cast<Json::UInt>(a.failedGuarantees[i]);
            failed.append(g);
        }
        e["failedGuarantees"] = failed;
        Json::Value diags(Json::arrayValue);
        for (uint32_t i = 0; i < a.diagnosticCount; ++i) { diags.append(a.diagnostics[i].text); }
        e["diagnostics"] = diags;
        e["droppedDiagnostics"] = static_cast<Json::UInt>(a.droppedDiagnostics);
        e["elapsedMs"] = ShortestFloat(a.elapsedMs);
        return e;
    }
}

std::string WorldGenSession::GuaranteeLabel(uint32_t idCrc) const
{
    return GuaranteeName(mGuaranteeSpec, idCrc);
}

Json::Value WorldGenSession::Report(const Json::Value& args) const
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    int64_t offset = 0;
    int64_t limit = kReportMaxLimit;
    std::string argError;
    if (args.isObject() &&
        (!ReadPagingArg(args, "offset", 0, offset, argError) || !ReadPagingArg(args, "limit", kReportMaxLimit, limit, argError)))
    {
        return Error(argError);
    }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["hasReport"] = mHasReport;
    r["reportRevision"] = static_cast<Json::UInt>(mReportRevision);
    r["currentRevision"] = static_cast<Json::UInt>(mRevision);
    if (!mHasReport)
    {
        return r;
    }

    const int64_t total = static_cast<int64_t>(mReport.attempts.Size());
    limit = std::min(limit, kReportMaxLimit);
    offset = std::min(offset, total);
    limit = std::min(limit, total - offset);

    r["outcome"] = GenerationOutcomeName(mReport.outcome);
    r["message"] = mReport.failureMessage;
    r["outerSeed"] = static_cast<Json::UInt64>(mReport.outerSeed);
    r["maxAttempts"] = static_cast<Json::UInt>(mReport.maxAttempts);
    r["worldRevision"] = static_cast<Json::UInt>(mReport.worldRevision);
    // The evidence belongs to the world with this revision; once live preview or another run replaced it, it is stale.
    r["stale"] = mReport.worldRevision != 0 && mReport.worldRevision != mRevision;
    r["attemptCount"] = static_cast<Json::UInt>(total);
    Json::Value page(Json::arrayValue);
    for (int64_t i = offset; i < offset + limit; ++i) { page.append(AttemptJson(mReport.attempts[static_cast<unsigned>(i)])); }
    r["offset"] = static_cast<Json::Int64>(offset);
    r["count"] = static_cast<Json::Int64>(page.size());
    r["nextOffset"] = (offset + limit) >= total ? Json::Value() : Json::Value(static_cast<Json::Int64>(offset + limit));
    r["attempts"] = page;

    // Evidence: the fresh Validate result when it belongs to the current world, else the generation's own.
    const bool freshEvidence = mHasEvidence && mEvidenceRevision == mRevision;
    const ValidationResult& validation = freshEvidence ? mEvidence : mReport.finalValidation;

    EvidencePage evidence;
    std::string evidenceError;
    int64_t evidenceOffset = 0;
    int64_t evidenceLimit = 0;
    if (args.isObject() &&
        (!ReadPagingArg(args, "evidenceOffset", 0, evidenceOffset, evidenceError) ||
         !ReadPagingArg(args, "evidenceLimit", 0, evidenceLimit, evidenceError)))
    {
        return Error(evidenceError);
    }
    evidence.offset = static_cast<size_t>(evidenceOffset);
    if (evidenceLimit > 0)
    {
        evidence.pathLimit = static_cast<size_t>(std::min(evidenceLimit, kEvidenceMaxPaths));
        evidence.cellLimit = static_cast<size_t>(std::min(evidenceLimit, kEvidenceMaxCells));
        evidence.pairLimit = static_cast<size_t>(std::min(evidenceLimit, kEvidenceMaxPairs));
    }
    const bool filtered = args.isObject() && args.isMember("guarantee");
    if (filtered && !args["guarantee"].isString() && !args["guarantee"].isUInt())
    {
        return Error("\"guarantee\" must be a guarantee name or id");
    }

    Json::Value fin(Json::objectValue);
    fin["passed"] = validation.passed;
    fin["worldRevision"] = static_cast<Json::UInt>(freshEvidence ? mEvidenceRevision : mReport.worldRevision);
    fin["source"] = freshEvidence ? "validate" : "generation";
    Json::Value guarantees(Json::arrayValue);
    bool truncated = false;
    for (unsigned i = 0; i < validation.guarantees.Size(); ++i)
    {
        const GuaranteeResult& g = validation.guarantees[i];
        if (filtered && !GuaranteeMatches(mGuaranteeSpec, args["guarantee"], g.idCrc))
        {
            continue;
        }
        guarantees.append(GuaranteeJson(mGuaranteeSpec, g, &evidence, truncated));
    }
    fin["guarantees"] = guarantees;
    fin["evidenceOffset"] = static_cast<Json::UInt64>(evidence.offset);
    fin["truncated"] = truncated;
    r["finalValidation"] = fin;
    return r;
}

Json::Value WorldGenSession::Validate(const Json::Value& args)
{
    if (!args.isNull() && !args.isObject())
    {
        return Error("args must be a JSON object");
    }
    Json::Value busy;
    if (BusyRefusal(busy))
    {
        return busy;
    }
    Json::Value noRules;
    if (RulesRefusal(noRules))
    {
        return noRules;
    }
    if (!mHasGenerated)
    {
        return Error("no world generated yet");
    }

    // The set a generation validates with: the injected spec's entries in file order (a variant_cover entry only when
    // the world holds sites of its kind, so a world without them is not failed by it), the registry's guarantees, and
    // the layout guarantee when regions are fresh (live-preview worlds have none). One of each id.
    const TerrainTableNavigationProfile profile(mRules);
    const WorldLayoutGuarantee layout(mRules, mLimits, Registry().NodeKinds());
    std::vector<std::unique_ptr<IWorldGuarantee>> specRules;
    for (const GuaranteeEntry& entry : mGuaranteeSpec.guarantees)
    {
        std::unique_ptr<IWorldGuarantee> rule = MakeSiteRule(entry);
        if (entry.type == GuaranteeRuleType::VariantCover &&
            !static_cast<const VariantCoverGuarantee&>(*rule).AppliesTo(mWorld))
        {
            continue;
        }
        specRules.push_back(std::move(rule));
    }

    ValidationParams params;
    params.AddProfile(&profile);
    std::vector<uint32_t> present;
    auto add = [&](const IWorldGuarantee* g)
    {
        const uint32_t id = g->GetId().Value();
        if (std::find(present.begin(), present.end(), id) != present.end())
        {
            return;
        }
        if (params.AddGuarantee(g))
        {
            present.push_back(id);
        }
    };
    for (const std::unique_ptr<IWorldGuarantee>& rule : specRules)
    {
        add(rule.get());
    }
    for (const IWorldGuarantee* g : Registry().Guarantees())
    {
        add(g);
    }
    if (mRegionsFresh)
    {
        add(&layout);
    }

    mEvidence = ReachabilityValidator::Validate(mWorld, params);
    mEvidenceRevision = mRevision;
    mHasEvidence = true;
    mValidatedSinceFailure = true;

    Json::Value guarantees(Json::arrayValue);
    bool truncated = false;
    for (unsigned i = 0; i < mEvidence.guarantees.Size(); ++i)
    {
        guarantees.append(GuaranteeJson(mGuaranteeSpec, mEvidence.guarantees[i], nullptr, truncated));
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["ok"] = mEvidence.passed;
    r["revision"] = static_cast<Json::UInt>(mRevision);
    r["guarantees"] = guarantees;
    return r;
}

Json::Value WorldGenSession::Worlds() const
{
    std::map<std::string, const char*> byName;
    for (const std::string& n : ListWorldNames(mWorldsDeployedDir)) { byName[n] = "deployed"; }
    for (const std::string& n : ListWorldNames(mWorldsOutDir))      { byName[n] = "out"; }   // out wins

    Json::Value list(Json::arrayValue);
    for (const auto& entry : byName)
    {
        Json::Value e(Json::objectValue);
        e["name"] = entry.first;
        e["source"] = entry.second;
        list.append(e);
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["worlds"] = list;
    return r;
}

} // namespace WorldGeneration
} // namespace CoW
