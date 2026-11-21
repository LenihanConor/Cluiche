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
#include <string>

namespace CoW {
namespace World {

// Filled in on a failed load: which field and why.
struct StratumError
{
    std::string field;
    std::string reason;
};

// Known assigner types (data selects by name). Further types arrive through the injection seam (Strata task 2).
enum class StratumAssignerType : uint8_t { CoastCostBands = 0 };

struct StratumSpec
{
    char     name[32] = {};
    float    share = 0.0f;           // sidecar-owned: share of main-landmass land in this stratum (ordered banding)
    uint8_t  regionsMin = 0;         // strata.json: region count range for this stratum
    uint8_t  regionsMax = 0;
    bool     mainLandmassOnly = false;
    int32_t  minCoastDepth = 0;      // strata.json: a cell closer to the coast than this falls to the previous stratum
};

struct StratumSetSpec
{
    static constexpr uint32_t kMaxStrata = 8;
    char                 id[32] = {};
    bool                 ordered = true;
    StratumAssignerType  assigner = StratumAssignerType::CoastCostBands;
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
    // The generator supports exactly this many strata in the depth set until Strata tasks 2-3 generalise it.
    static constexpr uint32_t kGeneratorStrata = 3;

    // All-or-nothing: on failure *this is unchanged and err names the field. strataText is the strata file,
    // sidecarText the parameter sidecar (per-stratum shares keyed "<set id>.<stratum name>.share"; defaults are the sidecar's).
    bool LoadFromText(const char* strataText, const char* sidecarText, StratumError& err);

    // Sets.
    uint32_t              SetCount() const { return mSetCount; }
    const StratumSetSpec& Set(uint32_t i) const { return mSets[i]; }
    // The set with this id, nullptr when absent.
    const StratumSetSpec* FindSet(const char* id) const;
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
