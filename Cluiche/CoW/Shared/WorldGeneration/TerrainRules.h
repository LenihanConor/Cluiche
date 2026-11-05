// S-1 Terrain Generator: data-driven rule constants shared by all generator stages.
// Pure logic, no Dia module dependencies beyond StringCRC/DynamicArrayC/jsoncpp.
// Loaded from Assets/CoW/Global/WorldGen/terrain_rules.json; missing/out-of-range values
// fall back to defaults/clamps with a warning so a bad file never blocks generation.
// To add a rule: add ONE row to TERRAIN_RULES_TABLE below (loader, ToJson and tests follow).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

// X(name, default, min, max). The JSON key is the stringified name (e.g. "kCoastElev").
// Integer-valued rules are stored as float; use TerrainRules::GetInt for them.
// kWarpAmp and kCoastAmp defaults are guesses (the spec gives no value).
#define TERRAIN_RULES_TABLE(X) \
    X(kCoastElev,        0.02f,  0.005f, 0.1f) \
    X(kBorderWidth,      2.0f,   0.0f,   8.0f) \
    X(kEdgeFade,         0.5f,   0.0f,   1.0f) \
    X(kShelfScale,       6.0f,   1.0f,   32.0f) \
    X(kMinLandmass,      6.0f,   1.0f,   64.0f) \
    X(kMinIslandShare,   0.02f,  0.0f,   0.2f) \
    X(kMaxIslandShare,   0.95f,  0.3f,   1.0f) \
    X(kIslandGapMin,     2.0f,   0.0f,   16.0f) \
    X(kIslandGapMax,     7.0f,   1.0f,   32.0f) \
    X(kIslandSpacing,    6.0f,   1.0f,   32.0f) \
    X(kWarpAmp,          0.15f,  0.0f,   1.0f) \
    X(kCoastAmp,         0.12f,  0.0f,   1.0f) \
    X(kRidgeCellMin,     0.35f,  0.0f,   1.0f) \
    X(kRidgeWidthMin,    1.2f,   0.5f,   8.0f) \
    X(kRidgeWidthMax,    2.8f,   0.5f,   16.0f) \
    X(kSpineReach,       0.8f,   0.0f,   1.0f) \
    X(kRingReach,        0.6f,   0.0f,   1.0f) \
    X(kMinPiece,         2.0f,   1.0f,   32.0f) \
    X(kPassFloor,        0.2f,   0.0f,   1.0f) \
    X(kBarrierQuantile,  0.75f,  0.0f,   1.0f) \
    X(kBarrierWobble,    1.5f,   0.0f,   8.0f) \
    X(kBarrierHalfWidth, 1.5f,   0.5f,   8.0f) \
    X(kPlateauInner,     0.5f,   0.0f,   1.0f) \
    X(kVentSpacing,      4.0f,   1.0f,   32.0f) \
    X(kLowlandRise,      0.25f,  0.0f,   1.0f) \
    X(kInlandScale,      6.0f,   1.0f,   32.0f) \
    X(kMountainHeight,   0.6f,   0.0f,   1.0f) \
    X(kBarrierHeight,    0.35f,  0.0f,   1.0f) \
    X(kVentHeight,       0.5f,   0.0f,   1.0f) \
    X(kVentRadius,       2.0f,   0.0f,   8.0f) \
    X(kRuggedAmp,        0.12f,  0.0f,   0.5f) \
    X(kMinBasinDepth,    0.01f,  0.0f,   0.2f) \
    X(kHighlandRain,     2.0f,   0.0f,   16.0f) \
    X(kFlowMax,          400.0f, 1.0f,   100000.0f) \
    X(kFlowMin,          12.0f,  1.0f,   10000.0f) \
    X(kSourceMinElev,    0.3f,   0.0f,   1.0f) \
    X(kMinRiverLength,   4.0f,   1.0f,   64.0f) \
    X(kRidgeCost,        4.0f,   0.0f,   32.0f) \
    X(kBarrierCost,      4.0f,   0.0f,   32.0f) \
    X(kFirstShoresShare, 0.45f,  0.0f,   1.0f) \
    X(kAncientShare,     0.15f,  0.0f,   1.0f) \
    X(kRiverCorridor,    0.25f,  0.0f,   1.0f) \
    X(kAncientMinDepth,  3.0f,   0.0f,   16.0f) \
    X(kCarveMax,         0.12f,  0.0f,   0.5f) \
    X(kCarveFullAcc,     8.0f,   1.0f,   64.0f) \
    X(kShelfWidth,       3.0f,   0.0f,   16.0f) \
    X(kCliffSlope,       0.18f,  0.0f,   1.0f) \
    X(kSteepSlope,       0.10f,  0.0f,   1.0f) \
    X(kPeakElev,         0.80f,  0.0f,   1.0f) \
    X(kMountainElev,     0.55f,  0.0f,   1.0f) \
    X(kHighlandElev,     0.35f,  0.0f,   1.0f) \
    X(kHillElev,         0.15f,  0.0f,   1.0f) \
    X(kHillSlope,        0.05f,  0.0f,   1.0f)

enum class Rule : uint32_t
{
#define TERRAIN_RULE_ENUM(name, def, lo, hi) name,
    TERRAIN_RULES_TABLE(TERRAIN_RULE_ENUM)
#undef TERRAIN_RULE_ENUM
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

// The rule table (kRuleCount rows, indexed by Rule).
const RuleDef& GetRuleDef(Rule r);
const RuleDef& GetRuleDef(uint32_t index);

constexpr uint32_t kDefaultRulesVersion = 1;

struct TerrainRules
{
    uint32_t rulesVersion = kDefaultRulesVersion;
    float    values[kRuleCount] = {};
    bool     degraded = false;   // set when any value was out of range or invalid

    static TerrainRules Defaults();

    float Get(Rule r) const { return values[static_cast<uint32_t>(r)]; }
    int   GetInt(Rule r) const { return static_cast<int>(Get(r) + (Get(r) >= 0.0f ? 0.5f : -0.5f)); }

#define TERRAIN_RULE_ACCESSOR(name, def, lo, hi) float name() const { return Get(Rule::name); }
    TERRAIN_RULES_TABLE(TERRAIN_RULE_ACCESSOR)
#undef TERRAIN_RULE_ACCESSOR

    // Starts from Defaults(), then applies the file. Missing key -> default + warning;
    // out-of-range -> clamped + warning + degraded; unknown key -> ignored + warning.
    // Returns false (defaults kept, degraded set) only if v is not a JSON object.
    // Warnings beyond kMaxRuleWarnings are dropped (the last slot becomes "warnings:truncated").
    bool LoadFromJson(const Json::Value& v, RuleWarnings& warnings);

    // Flat key -> value object including "rulesVersion".
    Json::Value ToJson() const;
};

} // namespace WorldGeneration
} // namespace CoW
