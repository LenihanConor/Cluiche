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
#include <string>

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

struct StratumSpec
{
    char     name[32] = {};
    float    share = 0.0f;           // sidecar-owned: share of main-landmass land in this stratum (ordered banding)
    uint8_t  regionsMin = 0;         // strata.json: region count range for this stratum
    uint8_t  regionsMax = 0;
    bool     mainLandmassOnly = false;
    int32_t  minCoastDepth = 0;      // a cell closer to the coast than this falls to the previous stratum. strata.json declares it
                                     // (the authored default); the sidecar key "<set>.<stratum>.min_coast_depth" owns the live value
    bool     declaresMinCoastDepth = false;   // strata.json declared min_coast_depth, so the sidecar carries the key
};

struct StratumSetSpec
{
    static constexpr uint32_t kMaxStrata = 8;
    char                 id[32] = {};
    bool                 ordered = true;
    char                 assignerType[32] = "coast_cost_bands";   // the assigner the set names (registry key)
    char                 metric[32] = "coast_cost";
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
    bool LoadFromText(const char* strataText, const char* sidecarText, StratumError& err, const AssignerTypeCheck& knownAssignerType = nullptr);

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
    StratumSetSpec        mSets[kMaxSets];
    uint32_t              mSetCount = 0;
    int                   mDepthIndex = -1;   // index of the depth set in mSets
};

} // namespace World
} // namespace CoW
