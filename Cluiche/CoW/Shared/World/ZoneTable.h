// Depth zone table: the ordered zone names of a game's world, loaded from Global/World/zones.json plus the
// sidecar Global/World/zones.schema.json (a Dia parameter file that owns the generator zone shares).
// A DepthZone is a plain index: 0 = none/ocean, 1..Count() = zone k. Zone k and zone k+1 are separated by barrier k.
// No include of anything under WorldGeneration/.
#pragma once

#include <Shared/World/CellVocabulary.h>

#include <cstdint>
#include <string>

namespace CoW {
namespace World {

// Filled in on a failed load: which field and why.
struct ZoneError
{
    std::string field;
    std::string reason;
};

class ZoneTable
{
public:
    // The generator has three zone slots; count generalisation is deferred, so more zones are refused.
    static constexpr uint32_t kMaxZones = 3;
    static constexpr uint32_t kMaxNameLength = 31;

    // All-or-nothing: on failure *this is unchanged and err names the field. zonesText is the zones file,
    // sidecarText the zone parameter sidecar (shares; defaults are the sidecar's).
    bool LoadFromText(const char* zonesText, const char* sidecarText, ZoneError& err);

    uint32_t Count() const { return mCount; }
    // Name of zone index z: "None" for 0, the declared name for 1..Count(), nullptr when out of range.
    const char* Name(WorldGeneration::DepthZone z) const;
    // Index of a zone name ("None" -> 0); -1 when the table has no such zone.
    int         IndexOf(const char* name) const;
    // The innermost zone (index Count()); kNoZone for an empty table.
    WorldGeneration::DepthZone Last() const { return static_cast<WorldGeneration::DepthZone>(mCount); }

    // Generator shares (sidecar parameters): the share of main-landmass land in the first zone, and in the last zone.
    float       FirstShare() const { return mFirstShare; }
    float       LastShare() const { return mLastShare; }

private:
    char     mNames[kMaxZones][kMaxNameLength + 1] = {};
    uint32_t mCount = 0;
    float    mFirstShare = 0.0f;
    float    mLastShare = 0.0f;
};

} // namespace World
} // namespace CoW
