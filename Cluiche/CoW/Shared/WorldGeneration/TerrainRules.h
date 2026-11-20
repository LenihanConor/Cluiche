// S-1 Terrain Generator: rule constants shared by all generator stages.
// Pure logic, no Dia module dependencies beyond StringCRC/DynamicArrayC/jsoncpp/DiaAttribute.
// Names, ranges and defaults live in the sidecar Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json
// (a Dia parameter file). TerrainRuleValues is a plain struct of members filled by
// ParamBinding<TerrainRuleValues>::Snapshot from a ParameterFile; the Rule enum / Get / GetInt / kFoo()
// accessors below are a THIN SHIM kept for call sites (plan task 25, stage 1).
// To add a rule: add the sidecar entry, the Rule enumerator, the member, and its ForEachRuleMember line.
// Spec: docs/specs/applications/dia/systems/diaattribute/parameter-files.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdint>

namespace Dia::Attribute { class ParameterFile; class AttributeSchema; struct BindingReport; }

namespace CoW {
namespace WorldGeneration {

// Declaration order == member order in TerrainRuleValues (Get(Rule) indexes the contiguous floats).
enum class Rule : uint32_t
{
    kCoastElev,
    kBorderWidth,
    kEdgeFade,
    kShelfScale,
    kMinLandmass,
    kMinIslandShare,
    kMaxIslandShare,
    kIslandGapMin,
    kIslandGapMax,
    kIslandSpacing,
    kWarpAmp,
    kCoastAmp,
    kRidgeCellMin,
    kRidgeWidthMin,
    kRidgeWidthMax,
    kSpineReach,
    kRingReach,
    kMinPiece,
    kPassFloor,
    kBarrierQuantile,
    kBarrierWobble,
    kBarrierHalfWidth,
    kPlateauInner,
    kVentSpacing,
    kLowlandRise,
    kInlandScale,
    kMountainHeight,
    kBarrierHeight,
    kVentHeight,
    kVentRadius,
    kRuggedAmp,
    kMinBasinDepth,
    kHighlandRain,
    kFlowMax,
    kFlowMin,
    kSourceMinElev,
    kMinRiverLength,
    kRidgeCost,
    kBarrierCost,
    kFirstShoresShare,
    kAncientShare,
    kRiverCorridor,
    kAncientMinDepth,
    kCarveMax,
    kCarveFullAcc,
    kShelfWidth,
    kCliffSlope,
    kSteepSlope,
    kPeakElev,
    kMountainElev,
    kHighlandElev,
    kHillElev,
    kHillSlope,
    kLatCool,
    kLapse,
    kRiverPenalty,
    kMoistScale,
    kBandBlend,
    kAirDecay,
    kOrographic,
    kProximityMix,
    kRainBonus,
    kWetlandMaxElev,
    kWetlandMoist,
    kPlateauWet,
    kMaxFixupPasses,
    kT0_Grass,
    kM0_Grass,
    kWT_Grass,
    kWM_Grass,
    kT0_Forest,
    kM0_Forest,
    kWT_Forest,
    kWM_Forest,
    kT0_Desert,
    kM0_Desert,
    kWT_Desert,
    kWM_Desert,
    kT0_Tundra,
    kM0_Tundra,
    kWT_Tundra,
    kWM_Tundra,
    kT0_Rainforest,
    kM0_Rainforest,
    kWT_Rainforest,
    kWM_Rainforest,
    kMinRegionCells,
    kSeedBiomeBonus,
    kBiomeMismatch,
    kRegionRidgeCost,
    kMaxRegionShare,
    kMaxRegionSlack,
    kMaxAbsorbedShare,
    kMaxFragmentShare,
    kNodeSpacing,
    kCorridorSpacing,
    kCoastalGapReach,
    kRiverNodeSpacing,
    kCellsPerNode,
    Count
};

constexpr uint32_t kRuleCount = static_cast<uint32_t>(Rule::Count);

struct RuleDef
{
    const char* key;
    float       defaultValue;
    float       minValue;
    float       maxValue;
};

// Warnings are short texts such as "missing:kCoastElev", "range:kEdgeFade",
// "unknown:kFoo", "invalid:kBar" (StringCRC keeps up to 63 readable chars via AsChar()).
constexpr unsigned int kMaxRuleWarnings = 128;
using RuleWarnings = Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxRuleWarnings>;

// Rule metadata (kRuleCount rows, indexed by Rule), read from the sidecar schema.
const RuleDef& GetRuleDef(Rule r);
const RuleDef& GetRuleDef(uint32_t index);

constexpr uint32_t kDefaultRulesVersion = 4;

// The bound members: standard layout, one float per sidecar parameter, named as the sidecar key.
// The `values` array aliases the named members (stage 1 shim, so existing `rules.values[Rule::kX]` writes keep working).
#pragma warning(push)
#pragma warning(disable : 4201)   // nameless struct/union
struct TerrainRuleValues
{
    union
    {
        struct
        {
            float kCoastElev;
            float kBorderWidth;
            float kEdgeFade;
            float kShelfScale;
            float kMinLandmass;
            float kMinIslandShare;
            float kMaxIslandShare;
            float kIslandGapMin;
            float kIslandGapMax;
            float kIslandSpacing;
            float kWarpAmp;
            float kCoastAmp;
            float kRidgeCellMin;
            float kRidgeWidthMin;
            float kRidgeWidthMax;
            float kSpineReach;
            float kRingReach;
            float kMinPiece;
            float kPassFloor;
            float kBarrierQuantile;
            float kBarrierWobble;
            float kBarrierHalfWidth;
            float kPlateauInner;
            float kVentSpacing;
            float kLowlandRise;
            float kInlandScale;
            float kMountainHeight;
            float kBarrierHeight;
            float kVentHeight;
            float kVentRadius;
            float kRuggedAmp;
            float kMinBasinDepth;
            float kHighlandRain;
            float kFlowMax;
            float kFlowMin;
            float kSourceMinElev;
            float kMinRiverLength;
            float kRidgeCost;
            float kBarrierCost;
            float kFirstShoresShare;
            float kAncientShare;
            float kRiverCorridor;
            float kAncientMinDepth;
            float kCarveMax;
            float kCarveFullAcc;
            float kShelfWidth;
            float kCliffSlope;
            float kSteepSlope;
            float kPeakElev;
            float kMountainElev;
            float kHighlandElev;
            float kHillElev;
            float kHillSlope;
            float kLatCool;
            float kLapse;
            float kRiverPenalty;
            float kMoistScale;
            float kBandBlend;
            float kAirDecay;
            float kOrographic;
            float kProximityMix;
            float kRainBonus;
            float kWetlandMaxElev;
            float kWetlandMoist;
            float kPlateauWet;
            float kMaxFixupPasses;
            float kT0_Grass;
            float kM0_Grass;
            float kWT_Grass;
            float kWM_Grass;
            float kT0_Forest;
            float kM0_Forest;
            float kWT_Forest;
            float kWM_Forest;
            float kT0_Desert;
            float kM0_Desert;
            float kWT_Desert;
            float kWM_Desert;
            float kT0_Tundra;
            float kM0_Tundra;
            float kWT_Tundra;
            float kWM_Tundra;
            float kT0_Rainforest;
            float kM0_Rainforest;
            float kWT_Rainforest;
            float kWM_Rainforest;
            float kMinRegionCells;
            float kSeedBiomeBonus;
            float kBiomeMismatch;
            float kRegionRidgeCost;
            float kMaxRegionShare;
            float kMaxRegionSlack;
            float kMaxAbsorbedShare;
            float kMaxFragmentShare;
            float kNodeSpacing;
            float kCorridorSpacing;
            float kCoastalGapReach;
            float kRiverNodeSpacing;
            float kCellsPerNode;
        };
        float values[kRuleCount];
    };

    TerrainRuleValues() { for (float& v : values) { v = 0.0f; } }
};
#pragma warning(pop)
static_assert(sizeof(TerrainRuleValues) == kRuleCount * sizeof(float), "TerrainRuleValues must be exactly kRuleCount floats");

struct TerrainRules : TerrainRuleValues
{
    uint32_t rulesVersion = kDefaultRulesVersion;   // code constant, not a sidecar entry (user decision 2026-11-20)
    bool     degraded = false;   // set when any value was out of range or invalid

    // The sidecar defaults (loaded once from the sidecar file; see SidecarFile). If the sidecar cannot be
    // loaded there is no fallback: an error is logged and the result is all zeros with degraded set.
    static TerrainRules Defaults();

    // Seam for the module/session wiring (plan task 26): fills `out` (rulesVersion default, degraded false)
    // from a loaded ParameterFile via ParamBinding::Snapshot. Returns false (and logs) if the binding does
    // not cover the file's schema exactly; `out` is then left unchanged.
    static bool FromParameterFile(const Dia::Attribute::ParameterFile& file, TerrainRules& out);

    // Resolves the member bindings against a schema (coverage check, plan AC 15): true when no sidecar
    // parameter is unbound and no binding lacks an entry.
    static bool ResolveBindings(const Dia::Attribute::AttributeSchema& schema, Dia::Attribute::BindingReport& report);

    // The interim value source until task 26: the sidecar read from disk (searched upward from the working
    // directory and from this source file), loaded once. nullptr if it could not be found or loaded.
    static const Dia::Attribute::ParameterFile* SidecarFile();

    float Get(Rule r) const { return reinterpret_cast<const float*>(static_cast<const TerrainRuleValues*>(this))[static_cast<uint32_t>(r)]; }
    // Rounds half away from zero (the conversion every integer-valued rule has always used).
    static int RoundToInt(float v) { return static_cast<int>(v + (v >= 0.0f ? 0.5f : -0.5f)); }
    int   GetInt(Rule r) const { return RoundToInt(Get(r)); }

    float kCoastElev() const { return TerrainRuleValues::kCoastElev; }
    float kBorderWidth() const { return TerrainRuleValues::kBorderWidth; }
    float kEdgeFade() const { return TerrainRuleValues::kEdgeFade; }
    float kShelfScale() const { return TerrainRuleValues::kShelfScale; }
    float kMinLandmass() const { return TerrainRuleValues::kMinLandmass; }
    float kMinIslandShare() const { return TerrainRuleValues::kMinIslandShare; }
    float kMaxIslandShare() const { return TerrainRuleValues::kMaxIslandShare; }
    float kIslandGapMin() const { return TerrainRuleValues::kIslandGapMin; }
    float kIslandGapMax() const { return TerrainRuleValues::kIslandGapMax; }
    float kIslandSpacing() const { return TerrainRuleValues::kIslandSpacing; }
    float kWarpAmp() const { return TerrainRuleValues::kWarpAmp; }
    float kCoastAmp() const { return TerrainRuleValues::kCoastAmp; }
    float kRidgeCellMin() const { return TerrainRuleValues::kRidgeCellMin; }
    float kRidgeWidthMin() const { return TerrainRuleValues::kRidgeWidthMin; }
    float kRidgeWidthMax() const { return TerrainRuleValues::kRidgeWidthMax; }
    float kSpineReach() const { return TerrainRuleValues::kSpineReach; }
    float kRingReach() const { return TerrainRuleValues::kRingReach; }
    float kMinPiece() const { return TerrainRuleValues::kMinPiece; }
    float kPassFloor() const { return TerrainRuleValues::kPassFloor; }
    float kBarrierQuantile() const { return TerrainRuleValues::kBarrierQuantile; }
    float kBarrierWobble() const { return TerrainRuleValues::kBarrierWobble; }
    float kBarrierHalfWidth() const { return TerrainRuleValues::kBarrierHalfWidth; }
    float kPlateauInner() const { return TerrainRuleValues::kPlateauInner; }
    float kVentSpacing() const { return TerrainRuleValues::kVentSpacing; }
    float kLowlandRise() const { return TerrainRuleValues::kLowlandRise; }
    float kInlandScale() const { return TerrainRuleValues::kInlandScale; }
    float kMountainHeight() const { return TerrainRuleValues::kMountainHeight; }
    float kBarrierHeight() const { return TerrainRuleValues::kBarrierHeight; }
    float kVentHeight() const { return TerrainRuleValues::kVentHeight; }
    float kVentRadius() const { return TerrainRuleValues::kVentRadius; }
    float kRuggedAmp() const { return TerrainRuleValues::kRuggedAmp; }
    float kMinBasinDepth() const { return TerrainRuleValues::kMinBasinDepth; }
    float kHighlandRain() const { return TerrainRuleValues::kHighlandRain; }
    float kFlowMax() const { return TerrainRuleValues::kFlowMax; }
    float kFlowMin() const { return TerrainRuleValues::kFlowMin; }
    float kSourceMinElev() const { return TerrainRuleValues::kSourceMinElev; }
    float kMinRiverLength() const { return TerrainRuleValues::kMinRiverLength; }
    float kRidgeCost() const { return TerrainRuleValues::kRidgeCost; }
    float kBarrierCost() const { return TerrainRuleValues::kBarrierCost; }
    float kFirstShoresShare() const { return TerrainRuleValues::kFirstShoresShare; }
    float kAncientShare() const { return TerrainRuleValues::kAncientShare; }
    float kRiverCorridor() const { return TerrainRuleValues::kRiverCorridor; }
    float kAncientMinDepth() const { return TerrainRuleValues::kAncientMinDepth; }
    float kCarveMax() const { return TerrainRuleValues::kCarveMax; }
    float kCarveFullAcc() const { return TerrainRuleValues::kCarveFullAcc; }
    float kShelfWidth() const { return TerrainRuleValues::kShelfWidth; }
    float kCliffSlope() const { return TerrainRuleValues::kCliffSlope; }
    float kSteepSlope() const { return TerrainRuleValues::kSteepSlope; }
    float kPeakElev() const { return TerrainRuleValues::kPeakElev; }
    float kMountainElev() const { return TerrainRuleValues::kMountainElev; }
    float kHighlandElev() const { return TerrainRuleValues::kHighlandElev; }
    float kHillElev() const { return TerrainRuleValues::kHillElev; }
    float kHillSlope() const { return TerrainRuleValues::kHillSlope; }
    float kLatCool() const { return TerrainRuleValues::kLatCool; }
    float kLapse() const { return TerrainRuleValues::kLapse; }
    float kRiverPenalty() const { return TerrainRuleValues::kRiverPenalty; }
    float kMoistScale() const { return TerrainRuleValues::kMoistScale; }
    float kBandBlend() const { return TerrainRuleValues::kBandBlend; }
    float kAirDecay() const { return TerrainRuleValues::kAirDecay; }
    float kOrographic() const { return TerrainRuleValues::kOrographic; }
    float kProximityMix() const { return TerrainRuleValues::kProximityMix; }
    float kRainBonus() const { return TerrainRuleValues::kRainBonus; }
    float kWetlandMaxElev() const { return TerrainRuleValues::kWetlandMaxElev; }
    float kWetlandMoist() const { return TerrainRuleValues::kWetlandMoist; }
    float kPlateauWet() const { return TerrainRuleValues::kPlateauWet; }
    float kMaxFixupPasses() const { return TerrainRuleValues::kMaxFixupPasses; }
    float kT0_Grass() const { return TerrainRuleValues::kT0_Grass; }
    float kM0_Grass() const { return TerrainRuleValues::kM0_Grass; }
    float kWT_Grass() const { return TerrainRuleValues::kWT_Grass; }
    float kWM_Grass() const { return TerrainRuleValues::kWM_Grass; }
    float kT0_Forest() const { return TerrainRuleValues::kT0_Forest; }
    float kM0_Forest() const { return TerrainRuleValues::kM0_Forest; }
    float kWT_Forest() const { return TerrainRuleValues::kWT_Forest; }
    float kWM_Forest() const { return TerrainRuleValues::kWM_Forest; }
    float kT0_Desert() const { return TerrainRuleValues::kT0_Desert; }
    float kM0_Desert() const { return TerrainRuleValues::kM0_Desert; }
    float kWT_Desert() const { return TerrainRuleValues::kWT_Desert; }
    float kWM_Desert() const { return TerrainRuleValues::kWM_Desert; }
    float kT0_Tundra() const { return TerrainRuleValues::kT0_Tundra; }
    float kM0_Tundra() const { return TerrainRuleValues::kM0_Tundra; }
    float kWT_Tundra() const { return TerrainRuleValues::kWT_Tundra; }
    float kWM_Tundra() const { return TerrainRuleValues::kWM_Tundra; }
    float kT0_Rainforest() const { return TerrainRuleValues::kT0_Rainforest; }
    float kM0_Rainforest() const { return TerrainRuleValues::kM0_Rainforest; }
    float kWT_Rainforest() const { return TerrainRuleValues::kWT_Rainforest; }
    float kWM_Rainforest() const { return TerrainRuleValues::kWM_Rainforest; }
    float kMinRegionCells() const { return TerrainRuleValues::kMinRegionCells; }
    float kSeedBiomeBonus() const { return TerrainRuleValues::kSeedBiomeBonus; }
    float kBiomeMismatch() const { return TerrainRuleValues::kBiomeMismatch; }
    float kRegionRidgeCost() const { return TerrainRuleValues::kRegionRidgeCost; }
    float kMaxRegionShare() const { return TerrainRuleValues::kMaxRegionShare; }
    float kMaxRegionSlack() const { return TerrainRuleValues::kMaxRegionSlack; }
    float kMaxAbsorbedShare() const { return TerrainRuleValues::kMaxAbsorbedShare; }
    float kMaxFragmentShare() const { return TerrainRuleValues::kMaxFragmentShare; }
    float kNodeSpacing() const { return TerrainRuleValues::kNodeSpacing; }
    float kCorridorSpacing() const { return TerrainRuleValues::kCorridorSpacing; }
    float kCoastalGapReach() const { return TerrainRuleValues::kCoastalGapReach; }
    float kRiverNodeSpacing() const { return TerrainRuleValues::kRiverNodeSpacing; }
    float kCellsPerNode() const { return TerrainRuleValues::kCellsPerNode; }

    // Starts from Defaults(), then applies the file. Missing key -> default + warning;
    // out-of-range -> clamped + warning + degraded; unknown key -> ignored + warning.
    // Returns false (defaults kept, degraded set) only if v is not a JSON object.
    // Warnings beyond kMaxRuleWarnings are dropped (the last slot becomes "warnings:truncated").
    // Implemented as an adapter over ParameterFile::ApplyPack.
    bool LoadFromJson(const Json::Value& v, RuleWarnings& warnings);

    // Flat key -> value object including "rulesVersion".
    Json::Value ToJson() const;
};

} // namespace WorldGeneration
} // namespace CoW
