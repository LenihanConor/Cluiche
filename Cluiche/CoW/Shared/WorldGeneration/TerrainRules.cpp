// S-1 Terrain Generator: TerrainRules binding to the parameter sidecar, adapter loader and serialiser.
#include <Shared/WorldGeneration/TerrainRules.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

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
constexpr const char* kSidecarRelativePath = "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";

// The single member list: sidecar key and bound member, in Rule order.
template<class Visitor>
void ForEachRuleMember(Visitor&& v)
{
    v("kCoastElev", &TerrainRuleValues::kCoastElev);
    v("kBorderWidth", &TerrainRuleValues::kBorderWidth);
    v("kEdgeFade", &TerrainRuleValues::kEdgeFade);
    v("kShelfScale", &TerrainRuleValues::kShelfScale);
    v("kMinLandmass", &TerrainRuleValues::kMinLandmass);
    v("kMinIslandShare", &TerrainRuleValues::kMinIslandShare);
    v("kMaxIslandShare", &TerrainRuleValues::kMaxIslandShare);
    v("kIslandGapMin", &TerrainRuleValues::kIslandGapMin);
    v("kIslandGapMax", &TerrainRuleValues::kIslandGapMax);
    v("kIslandSpacing", &TerrainRuleValues::kIslandSpacing);
    v("kWarpAmp", &TerrainRuleValues::kWarpAmp);
    v("kCoastAmp", &TerrainRuleValues::kCoastAmp);
    v("kRidgeCellMin", &TerrainRuleValues::kRidgeCellMin);
    v("kRidgeWidthMin", &TerrainRuleValues::kRidgeWidthMin);
    v("kRidgeWidthMax", &TerrainRuleValues::kRidgeWidthMax);
    v("kSpineReach", &TerrainRuleValues::kSpineReach);
    v("kRingReach", &TerrainRuleValues::kRingReach);
    v("kMinPiece", &TerrainRuleValues::kMinPiece);
    v("kPassFloor", &TerrainRuleValues::kPassFloor);
    v("kBarrierQuantile", &TerrainRuleValues::kBarrierQuantile);
    v("kBarrierWobble", &TerrainRuleValues::kBarrierWobble);
    v("kBarrierHalfWidth", &TerrainRuleValues::kBarrierHalfWidth);
    v("kPlateauInner", &TerrainRuleValues::kPlateauInner);
    v("kVentSpacing", &TerrainRuleValues::kVentSpacing);
    v("kLowlandRise", &TerrainRuleValues::kLowlandRise);
    v("kInlandScale", &TerrainRuleValues::kInlandScale);
    v("kMountainHeight", &TerrainRuleValues::kMountainHeight);
    v("kBarrierHeight", &TerrainRuleValues::kBarrierHeight);
    v("kVentHeight", &TerrainRuleValues::kVentHeight);
    v("kVentRadius", &TerrainRuleValues::kVentRadius);
    v("kRuggedAmp", &TerrainRuleValues::kRuggedAmp);
    v("kMinBasinDepth", &TerrainRuleValues::kMinBasinDepth);
    v("kHighlandRain", &TerrainRuleValues::kHighlandRain);
    v("kFlowMax", &TerrainRuleValues::kFlowMax);
    v("kFlowMin", &TerrainRuleValues::kFlowMin);
    v("kSourceMinElev", &TerrainRuleValues::kSourceMinElev);
    v("kMinRiverLength", &TerrainRuleValues::kMinRiverLength);
    v("kRidgeCost", &TerrainRuleValues::kRidgeCost);
    v("kBarrierCost", &TerrainRuleValues::kBarrierCost);
    v("kFirstShoresShare", &TerrainRuleValues::kFirstShoresShare);
    v("kAncientShare", &TerrainRuleValues::kAncientShare);
    v("kRiverCorridor", &TerrainRuleValues::kRiverCorridor);
    v("kAncientMinDepth", &TerrainRuleValues::kAncientMinDepth);
    v("kCarveMax", &TerrainRuleValues::kCarveMax);
    v("kCarveFullAcc", &TerrainRuleValues::kCarveFullAcc);
    v("kShelfWidth", &TerrainRuleValues::kShelfWidth);
    v("kCliffSlope", &TerrainRuleValues::kCliffSlope);
    v("kSteepSlope", &TerrainRuleValues::kSteepSlope);
    v("kPeakElev", &TerrainRuleValues::kPeakElev);
    v("kMountainElev", &TerrainRuleValues::kMountainElev);
    v("kHighlandElev", &TerrainRuleValues::kHighlandElev);
    v("kHillElev", &TerrainRuleValues::kHillElev);
    v("kHillSlope", &TerrainRuleValues::kHillSlope);
    v("kLatCool", &TerrainRuleValues::kLatCool);
    v("kLapse", &TerrainRuleValues::kLapse);
    v("kRiverPenalty", &TerrainRuleValues::kRiverPenalty);
    v("kMoistScale", &TerrainRuleValues::kMoistScale);
    v("kBandBlend", &TerrainRuleValues::kBandBlend);
    v("kAirDecay", &TerrainRuleValues::kAirDecay);
    v("kOrographic", &TerrainRuleValues::kOrographic);
    v("kProximityMix", &TerrainRuleValues::kProximityMix);
    v("kRainBonus", &TerrainRuleValues::kRainBonus);
    v("kWetlandMaxElev", &TerrainRuleValues::kWetlandMaxElev);
    v("kWetlandMoist", &TerrainRuleValues::kWetlandMoist);
    v("kPlateauWet", &TerrainRuleValues::kPlateauWet);
    v("kMaxFixupPasses", &TerrainRuleValues::kMaxFixupPasses);
    v("kT0_Grass", &TerrainRuleValues::kT0_Grass);
    v("kM0_Grass", &TerrainRuleValues::kM0_Grass);
    v("kWT_Grass", &TerrainRuleValues::kWT_Grass);
    v("kWM_Grass", &TerrainRuleValues::kWM_Grass);
    v("kT0_Forest", &TerrainRuleValues::kT0_Forest);
    v("kM0_Forest", &TerrainRuleValues::kM0_Forest);
    v("kWT_Forest", &TerrainRuleValues::kWT_Forest);
    v("kWM_Forest", &TerrainRuleValues::kWM_Forest);
    v("kT0_Desert", &TerrainRuleValues::kT0_Desert);
    v("kM0_Desert", &TerrainRuleValues::kM0_Desert);
    v("kWT_Desert", &TerrainRuleValues::kWT_Desert);
    v("kWM_Desert", &TerrainRuleValues::kWM_Desert);
    v("kT0_Tundra", &TerrainRuleValues::kT0_Tundra);
    v("kM0_Tundra", &TerrainRuleValues::kM0_Tundra);
    v("kWT_Tundra", &TerrainRuleValues::kWT_Tundra);
    v("kWM_Tundra", &TerrainRuleValues::kWM_Tundra);
    v("kT0_Rainforest", &TerrainRuleValues::kT0_Rainforest);
    v("kM0_Rainforest", &TerrainRuleValues::kM0_Rainforest);
    v("kWT_Rainforest", &TerrainRuleValues::kWT_Rainforest);
    v("kWM_Rainforest", &TerrainRuleValues::kWM_Rainforest);
    v("kMinRegionCells", &TerrainRuleValues::kMinRegionCells);
    v("kSeedBiomeBonus", &TerrainRuleValues::kSeedBiomeBonus);
    v("kBiomeMismatch", &TerrainRuleValues::kBiomeMismatch);
    v("kRegionRidgeCost", &TerrainRuleValues::kRegionRidgeCost);
    v("kMaxRegionShare", &TerrainRuleValues::kMaxRegionShare);
    v("kMaxRegionSlack", &TerrainRuleValues::kMaxRegionSlack);
    v("kMaxAbsorbedShare", &TerrainRuleValues::kMaxAbsorbedShare);
    v("kMaxFragmentShare", &TerrainRuleValues::kMaxFragmentShare);
    v("kNodeSpacing", &TerrainRuleValues::kNodeSpacing);
    v("kCorridorSpacing", &TerrainRuleValues::kCorridorSpacing);
    v("kCoastalGapReach", &TerrainRuleValues::kCoastalGapReach);
    v("kRiverNodeSpacing", &TerrainRuleValues::kRiverNodeSpacing);
    v("kCellsPerNode", &TerrainRuleValues::kCellsPerNode);
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

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string FindSidecarText()
{
    std::error_code ec;
    const std::filesystem::path starts[] = { std::filesystem::current_path(ec), std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path() };
    for (std::filesystem::path dir : starts)
    {
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / kSidecarRelativePath;
            if (std::filesystem::exists(candidate, ec))
            {
                return ReadFile(candidate);
            }
            if (dir == dir.parent_path())
            {
                break;
            }
            dir = dir.parent_path();
        }
    }
    return {};
}

// Interim value source (until plan task 26 wires the catalogue alias): the sidecar text and a loaded file.
struct SidecarHolder
{
    std::string   text;
    ParameterFile file;
    bool          ok = false;

    SidecarHolder()
    {
        text = FindSidecarText();
        if (text.empty())
        {
            DIA_LOG_ERROR("CoW", "TerrainRules: sidecar %s not found; no compiled fallback exists", kSidecarRelativePath);
            return;
        }
        ParamError err;
        ok = file.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err);
        if (!ok)
        {
            DIA_LOG_ERROR("CoW", "TerrainRules: sidecar load failed at key '%s': %s", err.key.AsCStr(), err.reason.AsCStr());
        }
    }
};

SidecarHolder& Sidecar()
{
    static SidecarHolder holder;
    return holder;
}

} // namespace

const Dia::Attribute::ParameterFile* TerrainRules::SidecarFile()
{
    SidecarHolder& s = Sidecar();
    return s.ok ? &s.file : nullptr;
}

const RuleDef& GetRuleDef(uint32_t index)
{
    static RuleDef table[kRuleCount];
    static const bool built = [] {
        const ParameterFile* file = TerrainRules::SidecarFile();
        uint32_t i = 0;
        ForEachRuleMember([&](const char* key, float TerrainRuleValues::*) {
            const Dia::Attribute::AttributeDefinition* def = file != nullptr ? file->Schema().FindAttribute(StringCRC(key)) : nullptr;
            table[i] = def != nullptr ? RuleDef{ def->fullKey.AsCStr(), def->default_value, def->minimum_value, def->maximum_value }
                                      : RuleDef{ key, 0.0f, 0.0f, 0.0f };
            ++i;
        });
        return true;
    }();
    (void)built;
    return table[index];
}
const RuleDef& GetRuleDef(Rule r) { return GetRuleDef(static_cast<uint32_t>(r)); }

namespace {
template<class Binding>
void BindAll(Binding& binding)
{
    ForEachRuleMember([&](const char* key, float TerrainRuleValues::* m) { binding.Bind(StringCRC(key), m); });
}
} // namespace

bool TerrainRules::ResolveBindings(const Dia::Attribute::AttributeSchema& schema, BindingReport& report)
{
    ParamBinding<TerrainRuleValues> binding;
    BindAll(binding);
    return binding.Resolve(schema, report);
}

bool TerrainRules::FromParameterFile(const ParameterFile& file, TerrainRules& out)
{
    ParamBinding<TerrainRuleValues> binding;
    BindAll(binding);

    auto report = std::make_unique<BindingReport>();   // large: keep it off the stack
    if (!binding.Resolve(file.Schema(), *report))
    {
        DIA_LOG_ERROR("CoW", "TerrainRules: sidecar does not match the bound members (%u unbound, %u without entry)",
                      report->unboundParameters.Size(), report->bindingsWithoutEntry.Size());
        return false;
    }
    TerrainRules r;
    binding.Snapshot(file.Values(), static_cast<TerrainRuleValues&>(r));
    out = r;
    return true;
}

TerrainRules TerrainRules::Defaults()
{
    static const TerrainRules defaults = [] {
        TerrainRules r;
        const ParameterFile* file = SidecarFile();
        if (file == nullptr || !FromParameterFile(*file, r))
        {
            r = TerrainRules();
            r.degraded = true;
        }
        return r;
    }();
    return defaults;
}

bool TerrainRules::LoadFromJson(const Json::Value& v, RuleWarnings& warnings)
{
    *this = Defaults();
    if (!v.isObject())
    {
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

    SidecarHolder& s = Sidecar();
    if (!s.ok)
    {
        degraded = true;
        return true;
    }
    ParameterFile file;
    ParamError err;
    if (!file.Load(s.text.c_str(), "terrain_rules.schema.json", nullptr, err))
    {
        degraded = true;
        return true;
    }

    // Build a pack out of the usable numeric entries; the file clamps. Classification warnings keep the old texts.
    Json::Value pack = file.ToJson(Dia::Attribute::JsonShape::Diff);   // envelope only (a fresh file is all defaults)
    const Dia::Attribute::AttributeSchema& schema = file.Schema();
    for (unsigned int i = 0; i < schema.GetAttributeCount(); ++i)
    {
        const Dia::Attribute::AttributeDefinition& def = schema.GetAttributeByIndex(i);
        const char* key = def.fullKey.AsCStr();
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

Json::Value TerrainRules::ToJson() const
{
    Json::Value root(Json::objectValue);
    root[kRulesVersionKey] = Json::Value(rulesVersion);
    ForEachRuleMember([&](const char* key, float TerrainRuleValues::* m) {
        // Short decimal so 0.02f is written as 0.02, not 0.019999999552965164.
        char text[32];
        std::snprintf(text, sizeof(text), "%.7g", static_cast<double>(this->*m));
        root[key] = Json::Value(std::strtod(text, nullptr));
    });
    return root;
}

} // namespace WorldGeneration
} // namespace CoW
