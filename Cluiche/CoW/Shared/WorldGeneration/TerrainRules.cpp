// S-1 Terrain Generator: TerrainRules binding to the parameter sidecar, adapter loader and serialiser.
#include <Shared/WorldGeneration/TerrainRules.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

namespace {

using Dia::Attribute::BindingReport;
using Dia::Attribute::ParameterFile;
using Dia::Attribute::ParamBinding;
using Dia::Attribute::ParamError;
using Dia::Attribute::ParamWarnings;
using Dia::Core::StringCRC;

constexpr const char* kRulesVersionKey = "rulesVersion";

// The single member list: sidecar key and bound member, in Rule order.
template<class Visitor>
void ForEachRuleMember(Visitor&& v)
{
    v("kCoastElev", &TerrainRules::kCoastElev);
    v("kBorderWidth", &TerrainRules::kBorderWidth);
    v("kEdgeFade", &TerrainRules::kEdgeFade);
    v("kShelfScale", &TerrainRules::kShelfScale);
    v("kMinLandmass", &TerrainRules::kMinLandmass);
    v("kMinIslandShare", &TerrainRules::kMinIslandShare);
    v("kMaxIslandShare", &TerrainRules::kMaxIslandShare);
    v("kIslandGapMin", &TerrainRules::kIslandGapMin);
    v("kIslandGapMax", &TerrainRules::kIslandGapMax);
    v("kIslandSpacing", &TerrainRules::kIslandSpacing);
    v("kWarpAmp", &TerrainRules::kWarpAmp);
    v("kCoastAmp", &TerrainRules::kCoastAmp);
    v("kRidgeCellMin", &TerrainRules::kRidgeCellMin);
    v("kRidgeWidthMin", &TerrainRules::kRidgeWidthMin);
    v("kRidgeWidthMax", &TerrainRules::kRidgeWidthMax);
    v("kSpineReach", &TerrainRules::kSpineReach);
    v("kRingReach", &TerrainRules::kRingReach);
    v("kMinPiece", &TerrainRules::kMinPiece);
    v("kPassFloor", &TerrainRules::kPassFloor);
    v("kBarrierQuantile", &TerrainRules::kBarrierQuantile);
    v("kBarrierWobble", &TerrainRules::kBarrierWobble);
    v("kBarrierHalfWidth", &TerrainRules::kBarrierHalfWidth);
    v("kPlateauInner", &TerrainRules::kPlateauInner);
    v("kVentSpacing", &TerrainRules::kVentSpacing);
    v("kLowlandRise", &TerrainRules::kLowlandRise);
    v("kInlandScale", &TerrainRules::kInlandScale);
    v("kMountainHeight", &TerrainRules::kMountainHeight);
    v("kBarrierHeight", &TerrainRules::kBarrierHeight);
    v("kVentHeight", &TerrainRules::kVentHeight);
    v("kVentRadius", &TerrainRules::kVentRadius);
    v("kRuggedAmp", &TerrainRules::kRuggedAmp);
    v("kMinBasinDepth", &TerrainRules::kMinBasinDepth);
    v("kHighlandRain", &TerrainRules::kHighlandRain);
    v("kFlowMax", &TerrainRules::kFlowMax);
    v("kFlowMin", &TerrainRules::kFlowMin);
    v("kSourceMinElev", &TerrainRules::kSourceMinElev);
    v("kMinRiverLength", &TerrainRules::kMinRiverLength);
    v("kRidgeCost", &TerrainRules::kRidgeCost);
    v("kBarrierCost", &TerrainRules::kBarrierCost);
    v("kRiverCorridor", &TerrainRules::kRiverCorridor);
    v("kCarveMax", &TerrainRules::kCarveMax);
    v("kCarveFullAcc", &TerrainRules::kCarveFullAcc);
    v("kShelfWidth", &TerrainRules::kShelfWidth);
    v("kCliffSlope", &TerrainRules::kCliffSlope);
    v("kSteepSlope", &TerrainRules::kSteepSlope);
    v("kPeakElev", &TerrainRules::kPeakElev);
    v("kMountainElev", &TerrainRules::kMountainElev);
    v("kHighlandElev", &TerrainRules::kHighlandElev);
    v("kHillElev", &TerrainRules::kHillElev);
    v("kHillSlope", &TerrainRules::kHillSlope);
    v("kLatCool", &TerrainRules::kLatCool);
    v("kLapse", &TerrainRules::kLapse);
    v("kRiverPenalty", &TerrainRules::kRiverPenalty);
    v("kMoistScale", &TerrainRules::kMoistScale);
    v("kBandBlend", &TerrainRules::kBandBlend);
    v("kAirDecay", &TerrainRules::kAirDecay);
    v("kOrographic", &TerrainRules::kOrographic);
    v("kProximityMix", &TerrainRules::kProximityMix);
    v("kRainBonus", &TerrainRules::kRainBonus);
    v("kWetlandMaxElev", &TerrainRules::kWetlandMaxElev);
    v("kWetlandMoist", &TerrainRules::kWetlandMoist);
    v("kPlateauWet", &TerrainRules::kPlateauWet);
    v("kMaxFixupPasses", &TerrainRules::kMaxFixupPasses);
    v("kMinRegionCells", &TerrainRules::kMinRegionCells);
    v("kSeedBiomeBonus", &TerrainRules::kSeedBiomeBonus);
    v("kBiomeMismatch", &TerrainRules::kBiomeMismatch);
    v("kRegionRidgeCost", &TerrainRules::kRegionRidgeCost);
    v("kMaxRegionShare", &TerrainRules::kMaxRegionShare);
    v("kMaxRegionSlack", &TerrainRules::kMaxRegionSlack);
    v("kMaxAbsorbedShare", &TerrainRules::kMaxAbsorbedShare);
    v("kMaxFragmentShare", &TerrainRules::kMaxFragmentShare);
    v("kNodeSpacing", &TerrainRules::kNodeSpacing);
    v("kCorridorSpacing", &TerrainRules::kCorridorSpacing);
    v("kCoastalGapReach", &TerrainRules::kCoastalGapReach);
    v("kRiverNodeSpacing", &TerrainRules::kRiverNodeSpacing);
    v("kCellsPerNode", &TerrainRules::kCellsPerNode);
}

void AddWarning(RuleWarnings& warnings, const char* prefix, const char* key)
{
    if (warnings.IsFull())
    {
        return;
    }
    if (warnings.Size() == warnings.Capacity() - 1)
    {
        warnings.Add(Dia::Core::StringCRC("warnings:truncated"));
        return;
    }
    char text[Dia::Core::StringCRC::kStringLength];
    std::snprintf(text, sizeof(text), "%s:%s", prefix, key);
    warnings.Add(Dia::Core::StringCRC(text));
}

} // namespace

namespace {
template<class Binding>
void BindAll(Binding& binding)
{
    // The sidecar stores every rule as a float; the 12 int32_t members opt in to the float->int conversion.
    ForEachRuleMember([&](const char* key, auto m) {
        if constexpr (std::is_same_v<decltype(m), int32_t TerrainRules::*>)
            binding.BindIntFromFloat(StringCRC(key), m);
        else
            binding.Bind(StringCRC(key), m);
    });
    binding.BindMap(StringCRC("terrain.passable"), &TerrainRules::terrainPassable);
    binding.BindMap(StringCRC("terrain.cost"), &TerrainRules::terrainCost);
    binding.BindMap(StringCRC("terrain.soil_depth_mult"), &TerrainRules::terrainSoilDepthMult);
    binding.BindMap(StringCRC("terrain.orographic_factor"), &TerrainRules::terrainOrographicFactor);
    binding.BindMap(StringCRC("terrain.runoff"), &TerrainRules::terrainRunoff);
}
} // namespace

bool TerrainRules::ResolveBindings(const Dia::Attribute::AttributeSchema& schema, BindingReport& report)
{
    ParamBinding<TerrainRules> binding;
    BindAll(binding);
    return binding.Resolve(schema, report);
}

bool TerrainRules::FromParameterFile(const ParameterFile& file, TerrainRules& out)
{
    ParamBinding<TerrainRules> binding;
    BindAll(binding);

    auto report = std::make_unique<BindingReport>();   // large: keep it off the stack
    if (!binding.Resolve(file.Schema(), *report))
    {
        DIA_LOG_ERROR("HexWorld", "TerrainRules: sidecar does not match the bound members (%u unbound, %u without entry)",
                      report->unboundParameters.Size(), report->bindingsWithoutEntry.Size());
        return false;
    }
    TerrainRules r;
    binding.Snapshot(file.Values(), r);
    out = r;
    return true;
}

bool TerrainRules::LoadFromJson(const Json::Value& v, RuleWarnings& warnings, const char* sidecarText)
{
    ParameterFile file;
    ParamError err;
    TerrainRules base;
    if (sidecarText == nullptr || !file.Load(sidecarText, "terrain_rules.schema.json", nullptr, err) || !FromParameterFile(file, base))
    {
        // No compiled fallback: without a loadable sidecar there are no defaults to start from.
        DIA_LOG_ERROR("HexWorld", "TerrainRules::LoadFromJson: sidecar not usable (key '%s')", err.key.AsCStr());
        *this = TerrainRules();
        degraded = true;
        AddWarning(warnings, "invalid", "sidecar");
        return false;
    }
    *this = base;
    if (!v.isObject())
    {
        DIA_LOG_WARNING("HexWorld", "TerrainRules::LoadFromJson: rules root is not a JSON object; keeping sidecar defaults");
        degraded = true;
        AddWarning(warnings, "invalid", "root");
        return false;
    }

    if (v.isMember(kRulesVersionKey) && v[kRulesVersionKey].isUInt())
    {
        rulesVersion = v[kRulesVersionKey].asUInt();
    }
    else
    {
        AddWarning(warnings, v.isMember(kRulesVersionKey) ? "invalid" : "missing", kRulesVersionKey);
    }

    // Build a pack out of the usable numeric entries; the file clamps. Classification warnings keep the old texts.
    Json::Value pack = file.ToJson(Dia::Attribute::JsonShape::Diff);   // envelope only (a fresh file is all defaults)
    const Dia::Attribute::AttributeSchema& schema = file.Schema();
    for (unsigned int i = 0; i < schema.GetAttributeCount(); ++i)
    {
        const Dia::Attribute::AttributeDefinition& def = schema.GetAttributeByIndex(i);
        const char* key = def.fullKey.AsCStr();
        if (std::strncmp(key, "terrain.", 8) == 0)
        {
            continue;   // terrain maps are pack-only: not part of the flat rules JSON
        }
        if (!v.isMember(key))
        {
            AddWarning(warnings, "missing", key);
            continue;
        }
        const Json::Value& jv = v[key];
        if (!jv.isNumeric())
        {
            AddWarning(warnings, "invalid", key);
            degraded = true;
            continue;
        }
        const double d = jv.asDouble();
        if (!std::isfinite(d))
        {
            AddWarning(warnings, "range", key);
            degraded = true;
            continue;
        }
        const float f = static_cast<float>(d);
        if (f < def.minimum_value || f > def.maximum_value)
        {
            AddWarning(warnings, "range", key);
            degraded = true;
        }
        pack[key] = Json::Value(d);
    }
    for (const std::string& name : v.getMemberNames())
    {
        if (name != kRulesVersionKey && schema.FindAttribute(StringCRC(name.c_str())) == nullptr)
        {
            AddWarning(warnings, "unknown", name.c_str());
        }
    }

    ParamWarnings packWarnings;
    if (!file.ApplyPack(pack, packWarnings, err))
    {
        DIA_LOG_WARNING("HexWorld", "TerrainRules::LoadFromJson: rules pack rejected (key '%s': %s); keeping sidecar defaults",
                        err.key.AsCStr(), err.reason.AsCStr());
        degraded = true;
        AddWarning(warnings, "invalid", "pack");
        return true;
    }
    const uint32_t version = rulesVersion;
    const bool wasDegraded = degraded;
    TerrainRules loaded;
    if (FromParameterFile(file, loaded))
    {
        *this = loaded;
    }
    rulesVersion = version;
    degraded = wasDegraded;
    return true;
}

float TerrainRules::ValueAt(uint32_t index) const
{
    struct MemberRef { uint32_t offset; bool isInt; };
    static MemberRef table[kRuleCount];
    static const bool built = [] {
        const TerrainRules probe;
        uint32_t i = 0;
        ForEachRuleMember([&](const char*, auto m) {
            table[i].offset = static_cast<uint32_t>(reinterpret_cast<const char*>(&(probe.*m)) - reinterpret_cast<const char*>(&probe));
            table[i].isInt = std::is_same_v<decltype(probe.*m), const int32_t&>;
            ++i;
        });
        return i == kRuleCount;
    }();
    (void)built;
    const char* base = reinterpret_cast<const char*>(this) + table[index].offset;
    if (table[index].isInt)
    {
        int32_t v;
        std::memcpy(&v, base, sizeof(v));
        return static_cast<float>(v);
    }
    float f;
    std::memcpy(&f, base, sizeof(f));
    return f;
}

Json::Value TerrainRules::ToJson() const
{
    Json::Value root(Json::objectValue);
    root[kRulesVersionKey] = Json::Value(rulesVersion);
    ForEachRuleMember([&](const char* key, auto m) {
        // Short decimal so 0.02f is written as 0.02, not 0.019999999552965164.
        char text[32];
        std::snprintf(text, sizeof(text), "%.7g", static_cast<double>(this->*m));
        root[key] = Json::Value(std::strtod(text, nullptr));
    });
    return root;
}

} // namespace WorldGeneration
} // namespace CoW
