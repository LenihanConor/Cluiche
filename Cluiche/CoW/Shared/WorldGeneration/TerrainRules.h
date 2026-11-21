// S-1 Terrain Generator: rule constants shared by all generator stages.
// Pure logic, no Dia module dependencies beyond StringCRC/DynamicArrayC/jsoncpp/DiaAttribute.
// Names, ranges and defaults live in the sidecar Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json
// (a Dia parameter file). TerrainRules is a plain struct of members, one per sidecar parameter and named as its
// key, filled by ParamBinding<TerrainRules>::Snapshot from a ParameterFile. Integer-valued rules are int32_t
// members (rounded half away from zero at Snapshot); everything else is float. Read them directly: rules.kFoo.
// To add a rule: add the sidecar entry, the member, and its ForEachRuleMember line (coverage test fails otherwise).
// There is no compiled default set: the session loads the sidecar (WorldGenSession::LoadRules).
// Spec: docs/specs/applications/dia/systems/diaattribute/parameter-files.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <Shared/WorldGeneration/TerrainEnums.h>

#include <cstdint>

namespace Dia::Attribute { class ParameterFile; class AttributeSchema; struct BindingReport; }

namespace CoW {
namespace WorldGeneration {

// Number of sidecar parameters / rule members.
constexpr uint32_t kRuleCount = 79;

// Warnings are short texts such as "missing:kCoastElev", "range:kEdgeFade",
// "unknown:kFoo", "invalid:kBar" (StringCRC keeps up to 63 readable chars via AsChar()).
constexpr unsigned int kMaxRuleWarnings = 128;
using RuleWarnings = Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxRuleWarnings>;

constexpr uint32_t kDefaultRulesVersion = 4;

// Standard layout (ParamBinding requirement): the 79 rule members, the five terrain maps, then the two plain fields.
struct TerrainRules
{
    float kCoastElev = 0.0f;
    float kBorderWidth = 0.0f;
    float kEdgeFade = 0.0f;
    float kShelfScale = 0.0f;
    int32_t kMinLandmass = 0;
    float kMinIslandShare = 0.0f;
    float kMaxIslandShare = 0.0f;
    float kIslandGapMin = 0.0f;
    float kIslandGapMax = 0.0f;
    float kIslandSpacing = 0.0f;
    float kWarpAmp = 0.0f;
    float kCoastAmp = 0.0f;
    float kRidgeCellMin = 0.0f;
    float kRidgeWidthMin = 0.0f;
    float kRidgeWidthMax = 0.0f;
    float kSpineReach = 0.0f;
    float kRingReach = 0.0f;
    float kMinPiece = 0.0f;
    float kPassFloor = 0.0f;
    float kBarrierQuantile = 0.0f;
    float kBarrierWobble = 0.0f;
    float kBarrierHalfWidth = 0.0f;
    float kPlateauInner = 0.0f;
    int32_t kVentSpacing = 0;
    float kLowlandRise = 0.0f;
    float kInlandScale = 0.0f;
    float kMountainHeight = 0.0f;
    float kBarrierHeight = 0.0f;
    float kVentHeight = 0.0f;
    float kVentRadius = 0.0f;
    float kRuggedAmp = 0.0f;
    float kMinBasinDepth = 0.0f;
    float kHighlandRain = 0.0f;
    float kFlowMax = 0.0f;
    float kFlowMin = 0.0f;
    float kSourceMinElev = 0.0f;
    float kMinRiverLength = 0.0f;
    float kRidgeCost = 0.0f;
    float kBarrierCost = 0.0f;
    float kFirstShoresShare = 0.0f;
    float kAncientShare = 0.0f;
    float kRiverCorridor = 0.0f;
    int32_t kAncientMinDepth = 0;
    float kCarveMax = 0.0f;
    float kCarveFullAcc = 0.0f;
    int32_t kShelfWidth = 0;
    float kCliffSlope = 0.0f;
    float kSteepSlope = 0.0f;
    float kPeakElev = 0.0f;
    float kMountainElev = 0.0f;
    float kHighlandElev = 0.0f;
    float kHillElev = 0.0f;
    float kHillSlope = 0.0f;
    float kLatCool = 0.0f;
    float kLapse = 0.0f;
    float kRiverPenalty = 0.0f;
    float kMoistScale = 0.0f;
    float kBandBlend = 0.0f;
    float kAirDecay = 0.0f;
    float kOrographic = 0.0f;
    float kProximityMix = 0.0f;
    float kRainBonus = 0.0f;
    float kWetlandMaxElev = 0.0f;
    float kWetlandMoist = 0.0f;
    float kPlateauWet = 0.0f;
    int32_t kMaxFixupPasses = 0;
    int32_t kMinRegionCells = 0;
    float kSeedBiomeBonus = 0.0f;
    float kBiomeMismatch = 0.0f;
    float kRegionRidgeCost = 0.0f;
    float kMaxRegionShare = 0.0f;
    int32_t kMaxRegionSlack = 0;
    float kMaxAbsorbedShare = 0.0f;
    float kMaxFragmentShare = 0.0f;
    int32_t kNodeSpacing = 0;
    int32_t kCorridorSpacing = 0;
    int32_t kCoastalGapReach = 0;
    int32_t kRiverNodeSpacing = 0;
    int32_t kCellsPerNode = 0;

    // Terrain profile maps (sidecar map entries keyed by the terrain_type enum, in TerrainType order).
    // Pack-overridable only; the flat rules JSON does not carry them (LoadFromJson skips "terrain." columns).
    static constexpr uint32_t kTerrainTypeCount = static_cast<uint32_t>(TerrainType::Count);
    bool  terrainPassable[kTerrainTypeCount] = {};
    float terrainCost[kTerrainTypeCount] = {};
    float terrainSoilDepthMult[kTerrainTypeCount] = {};     // PROVISIONAL (A5 consumes; S-2.6 tunes)
    float terrainOrographicFactor[kTerrainTypeCount] = {};  // PROVISIONAL
    float terrainRunoff[kTerrainTypeCount] = {};            // PROVISIONAL

    // Plain code constant, not a sidecar entry (user decision 2026-11-20).
    uint32_t rulesVersion = kDefaultRulesVersion;
    bool     degraded = false;   // set when any value was out of range or invalid

    // Used by WorldGenSession::LoadRules (plan task 26): fills `out` (rulesVersion default, degraded false)
    // from a loaded ParameterFile via ParamBinding::Snapshot. Returns false (and logs) if the binding does
    // not cover the file's schema exactly; `out` is then left unchanged.
    static bool FromParameterFile(const Dia::Attribute::ParameterFile& file, TerrainRules& out);

    // Resolves the member bindings against a schema (coverage check, plan AC 15): true when no sidecar
    // parameter is unbound and no binding lacks an entry.
    static bool ResolveBindings(const Dia::Attribute::AttributeSchema& schema, Dia::Attribute::BindingReport& report);

    // Value of the member at sidecar schema index `index` (int members as float). Generic consumers only.
    float ValueAt(uint32_t index) const;

    // Rounds half away from zero (the conversion integer-valued rules use).
    static int RoundToInt(float v) { return static_cast<int>(v + (v >= 0.0f ? 0.5f : -0.5f)); }

    // Starts from the defaults of the sidecar given as text (no compiled fallback: if it does not load, `*this` is
    // zeroed with degraded set and false is returned), then applies the file. Missing key -> default + warning;
    // out-of-range -> clamped + warning + degraded; unknown key -> ignored + warning.
    // Returns false (defaults kept, degraded set) only if v is not a JSON object.
    // Warnings beyond kMaxRuleWarnings are dropped (the last slot becomes "warnings:truncated").
    // Implemented as an adapter over ParameterFile::ApplyPack.
    bool LoadFromJson(const Json::Value& v, RuleWarnings& warnings, const char* sidecarText);

    // Flat key -> value object including "rulesVersion".
    Json::Value ToJson() const;
};

} // namespace WorldGeneration
} // namespace CoW
