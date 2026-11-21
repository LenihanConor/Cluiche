// Stratum table: the stratum sets of a game's world, loaded from Global/World/strata.json plus the sidecar
// Global/World/strata.schema.json (a Dia parameter file that owns the tunable per-stratum numbers).
// A stratum is a named class of cells that gameplay cares about; a stratum set is one named classification of the
// world's cells into strata. The set called "depth" is the one the generator stores in Cell::stratum.
// A StratumIndex is a plain index: 0 = none/ocean, 1..Count() = stratum k. In an ordered set, stratum k and k+1 are
// separated by barrier k.
// No include of anything under WorldGeneration/.
#pragma once

#include <Shared/World/CellVocabulary.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Dia::Attribute { class ParameterFile; }

namespace CoW {
namespace World {

// Filled in on a failed load: which field and why.
struct StratumError
{
    std::string field;
    std::string reason;
};

// The assigner type a set names by default and the one built in. Further types arrive through the injection seam
// (WorldGeneration/StrataAssigner.h); a load accepts only the types its AssignerTypeCheck knows.
constexpr const char* kBuiltinAssignerType = "coast_cost_bands";
// Says whether an assigner type name is known. An empty check accepts only kBuiltinAssignerType.
using AssignerTypeCheck = std::function<bool(const char* typeName)>;

// The banding metrics the built-in coast_cost_bands assigner knows by name; any other metric is the name of a cell field
// registered with the assigner registry (WorldGeneration/StrataAssigner.h). A load accepts only the metrics its MetricCheck knows.
constexpr const char* kMetricCoastCost = "coast_cost";   // travel cost from the coast (today's banding; the default)
constexpr const char* kMetricElevation = "elevation";    // the raw elevation layer
constexpr const char* kMetricLatitude  = "latitude";     // |2*(row+0.5)/height - 1|: 0 on the equator, 1 at the poles
// Says whether a banding metric name is known. An empty check accepts only the three built-in metrics.
using MetricCheck = std::function<bool(const char* metric)>;

// Per-stratum attribute types. DiaAttribute stores floats, ints, bools and enums; strings are not supported.
enum class StratumAttributeType : uint8_t { Float, Int, Bool };

// One attribute a set declares (strata.json "attribute_schema"): its type, legal range and default.
struct StratumAttributeDecl
{
    char                 name[32] = {};
    StratumAttributeType type = StratumAttributeType::Float;
    float                min = 0.0f;
    float                max = 0.0f;
    float                def = 0.0f;
};

struct StratumSpec
{
    static constexpr uint32_t kMaxAttributes = 8;
    char     name[32] = {};
    float    share = 0.0f;           // sidecar-owned: share of main-landmass land in this stratum (ordered banding)
    uint8_t  regionsMin = 0;         // strata.json: region count range for this stratum
    uint8_t  regionsMax = 0;
    bool     mainLandmassOnly = false;
    int32_t  minCoastDepth = 0;      // a cell closer to the coast than this falls to the previous stratum. strata.json declares it
                                     // (the authored default); the sidecar key "<set>.<stratum>.min_coast_depth" owns the live value
    bool     declaresMinCoastDepth = false;   // strata.json declared min_coast_depth, so the sidecar carries the key
    // Effective value of each attribute the set declares (index = the set's attribute index). The sidecar key
    // "<set>.<stratum>.attr.<attribute>" owns it; strata.json's value (else the schema default) is the sidecar default.
    float    attributeValue[kMaxAttributes] = {};
};

struct StratumSetSpec
{
    static constexpr uint32_t kMaxStrata = 8;
    char                 id[32] = {};
    bool                 ordered = true;
    char                 assignerType[32] = "coast_cost_bands";   // the assigner the set names (registry key)
    char                 metric[32] = "coast_cost";   // what coast_cost_bands bands on: coast_cost, elevation, latitude or a cell field
    uint32_t             attributeCount = 0;
    StratumAttributeDecl attributes[StratumSpec::kMaxAttributes];
    uint32_t             count = 0;
    StratumSpec          strata[kMaxStrata];
};

class StratumTable
{
public:
    static constexpr uint32_t kMaxStrata = StratumSetSpec::kMaxStrata;
    static constexpr uint32_t kMaxSets = 8;
    static constexpr uint32_t kMaxNameLength = 31;
    // The set id the generator stores in Cell::stratum.
    static constexpr const char* kDepthSetId = "depth";

    // All-or-nothing: on failure *this is unchanged and err names the field. strataText is the strata file,
    // sidecarText the parameter sidecar (per-stratum shares keyed "<set id>.<stratum name>.share"; a stratum that declares
    // min_coast_depth also needs "<set id>.<stratum name>.min_coast_depth"; defaults are the sidecar's).
    // knownAssignerType decides which "assigner.type" names load (see AssignerTypeCheck); an unknown one fails the load.
    // knownMetric decides which "assigner.metric" names load (see MetricCheck); an unknown one fails the load naming it.
    // A set whose metric is not coast_cost may not declare min_coast_depth or main_landmass_only (they are coast-distance rules).
    // A set may declare "attribute_schema" ({ name: { type: float|int|bool, min, max, default } }) and each stratum "attributes"
    // ({ name: value }); a fault (unknown attribute, wrong type, out of range) fails the load naming set, stratum and attribute.
    // Every (stratum, attribute) pair needs a sidecar parameter "<set>.<stratum>.attr.<attribute>" (the live value).
    bool LoadFromText(const char* strataText, const char* sidecarText, StratumError& err, const AssignerTypeCheck& knownAssignerType = nullptr,
                      const MetricCheck& knownMetric = nullptr);

    // Live tweak: sets sidecar parameters (keys as in the sidecar, e.g. "depth.FirstShores.share") and re-derives the table.
    // All-or-nothing: an unknown key, an out-of-range value or a set whose shares stop completing 1 fails with *this unchanged and
    // err naming the key. *changed (optional) is false when every value equals the current one (nothing is touched then).
    bool SetParams(const std::vector<std::pair<std::string, double>>& changes, StratumError& err, bool* changed = nullptr);

    // Attribute of stratum z (1..count) of set setIndex. TryAttribute is false for an unknown set, stratum or attribute;
    // Attribute returns 0 then.
    bool  TryAttribute(uint32_t setIndex, WorldGeneration::StratumIndex z, const char* name, float& out) const;
    float Attribute(uint32_t setIndex, WorldGeneration::StratumIndex z, const char* name) const;
    // The loaded sidecar (the DiaAttribute schema and values behind every sidecar-owned number); null before a load.
    const Dia::Attribute::ParameterFile* Sidecar() const { return mSidecar.get(); }

    // Sets.
    uint32_t              SetCount() const { return mSetCount; }
    const StratumSetSpec& Set(uint32_t i) const { return mSets[i]; }
    // The set with this id, nullptr when absent.
    const StratumSetSpec* FindSet(const char* id) const;
    // Index of the set with this id in Set(i); -1 when absent.
    int                   SetIndexOf(const char* id) const;
    // Sets other than "depth": the ones the generator stores as extra cell layers (WorldStructure::extraStrata).
    uint32_t              ExtraSetCount() const { return mSetCount > 0 && mDepthIndex >= 0 ? mSetCount - 1 : mSetCount; }
    // Name of stratum z within any set: "None" for 0, the declared name for 1..set.count, nullptr when out of range.
    static const char*    NameIn(const StratumSetSpec& set, WorldGeneration::StratumIndex z);
    // Index of a stratum name within a set ("None" -> 0); -1 when the set has no such stratum.
    static int            IndexIn(const StratumSetSpec& set, const char* name);
    // The "depth" set; nullptr on an empty table.
    const StratumSetSpec* Depth() const { return mDepthIndex >= 0 ? &mSets[mDepthIndex] : nullptr; }

    // The depth set's strata (what the generator reads).
    uint32_t Count() const { return mDepthIndex >= 0 ? mSets[mDepthIndex].count : 0; }
    // Name of stratum index z: "None" for 0, the declared name for 1..Count(), nullptr when out of range.
    const char* Name(WorldGeneration::StratumIndex z) const;
    // Index of a stratum name ("None" -> 0); -1 when the table has no such stratum.
    int         IndexOf(const char* name) const;
    // The innermost stratum (index Count()); kNoStratum for an empty table.
    WorldGeneration::StratumIndex Last() const { return static_cast<WorldGeneration::StratumIndex>(Count()); }

    // Generator shares (sidecar parameters): the share of main-landmass land in the first stratum, and in the last.
    float       FirstShare() const { return mDepthIndex >= 0 ? mSets[mDepthIndex].strata[0].share : 0.0f; }
    float       LastShare() const { return mDepthIndex >= 0 ? mSets[mDepthIndex].strata[mSets[mDepthIndex].count - 1].share : 0.0f; }

private:
    // Reads every sidecar-owned number of cand (its sets already parsed) from file; validates them.
    static bool ApplySidecar(StratumTable& cand, const Dia::Attribute::ParameterFile& file, StratumError& err);

    StratumSetSpec        mSets[kMaxSets];
    std::string           mSidecarText;                                       // for SetParams (rebuilds the parameter file)
    std::vector<std::pair<std::string, double>> mOverrides;                   // live values set since the load
    std::shared_ptr<const Dia::Attribute::ParameterFile> mSidecar;           // immutable snapshot
    uint32_t              mSetCount = 0;
    int                   mDepthIndex = -1;   // index of the depth set in mSets
};

} // namespace World
} // namespace CoW
