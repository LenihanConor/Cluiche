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
constexpr const char* kSidecarRelativePath = "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";

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
    v("kFirstShoresShare", &TerrainRules::kFirstShoresShare);
    v("kAncientShare", &TerrainRules::kAncientShare);
    v("kRiverCorridor", &TerrainRules::kRiverCorridor);
    v("kAncientMinDepth", &TerrainRules::kAncientMinDepth);
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
    v("kT0_Grass", &TerrainRules::kT0_Grass);
    v("kM0_Grass", &TerrainRules::kM0_Grass);
    v("kWT_Grass", &TerrainRules::kWT_Grass);
    v("kWM_Grass", &TerrainRules::kWM_Grass);
    v("kT0_Forest", &TerrainRules::kT0_Forest);
    v("kM0_Forest", &TerrainRules::kM0_Forest);
    v("kWT_Forest", &TerrainRules::kWT_Forest);
    v("kWM_Forest", &TerrainRules::kWM_Forest);
    v("kT0_Desert", &TerrainRules::kT0_Desert);
    v("kM0_Desert", &TerrainRules::kM0_Desert);
    v("kWT_Desert", &TerrainRules::kWT_Desert);
    v("kWM_Desert", &TerrainRules::kWM_Desert);
    v("kT0_Tundra", &TerrainRules::kT0_Tundra);
    v("kM0_Tundra", &TerrainRules::kM0_Tundra);
    v("kWT_Tundra", &TerrainRules::kWT_Tundra);
    v("kWM_Tundra", &TerrainRules::kWM_Tundra);
    v("kT0_Rainforest", &TerrainRules::kT0_Rainforest);
    v("kM0_Rainforest", &TerrainRules::kM0_Rainforest);
    v("kWT_Rainforest", &TerrainRules::kWT_Rainforest);
    v("kWM_Rainforest", &TerrainRules::kWM_Rainforest);
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

namespace {
template<class Binding>
void BindAll(Binding& binding)
{
    ForEachRuleMember([&](const char* key, auto m) { binding.Bind(StringCRC(key), m); });
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
        DIA_LOG_ERROR("CoW", "TerrainRules: sidecar does not match the bound members (%u unbound, %u without entry)",
                      report->unboundParameters.Size(), report->bindingsWithoutEntry.Size());
        return false;
    }
    TerrainRules r;
    binding.Snapshot(file.Values(), r);
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
