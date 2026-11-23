// Biome registry for mapping biome identifiers to properties.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>
#include <DraoiHexWorld/CellVocabulary.h>
#include <DraoiHexWorld/StratumTable.h>

#include <cstdint>
#include <memory>

namespace Draoi {
namespace HexWorld {

// BiomeIndex / kNoBiome are declared in CellVocabulary.h (Cell stores a BiomeIndex; avoids a circular include).

using TagId = uint8_t;
constexpr TagId kNoTag = 0xFF;

enum class BiomeRole : uint8_t { Ocean, Lake, Fallback };

// A pin rule is an open id declared in biomes.json ("pin": {"rule": "<name>", ...}); the registry stores it as a
// StringCRC and does not know what rules exist. The rule table lives with the generator, which supplies this
// predicate (via the optional load argument) so an undeclared rule id fails the load.
using PinRuleValidator = bool (*)(Dia::Core::StringCRC rule);

struct ClimateEnvelope { float t0, m0, wt, wm; };

// Filled in on a failed Load: which field, which biome id, which row, and why.
struct RegistryError
{
    Dia::Core::Containers::String32  field;
    Dia::Core::Containers::String64  id;
    uint32_t                         row = 0;
    Dia::Core::Containers::String256 reason;
};

class BiomeRegistry
{
public:
    static constexpr uint32_t kMaxBiomes = 254;

    BiomeRegistry();
    ~BiomeRegistry();
    BiomeRegistry(BiomeRegistry&&) noexcept;
    BiomeRegistry& operator=(BiomeRegistry&&) noexcept;
    BiomeRegistry(const BiomeRegistry&) = delete;
    BiomeRegistry& operator=(const BiomeRegistry&) = delete;

    void Swap(BiomeRegistry& other) noexcept;

    // All-or-nothing: on failure *this is unchanged. A non-null knownPinRule rejects any pin rule id it returns
    // false for (field "pin", naming the row); null accepts any rule id.
    // `zones` is the game's depth zone table: a biome's "zones" names resolve against it (an unknown name fails the
    // load, field "zones"), and the registry keeps a copy, readable through Strata().
    bool Load(const Dia::Core::FilePath& path, const StratumTable& zones, RegistryError& err, PinRuleValidator knownPinRule = nullptr);
    bool LoadFromText(const char* json, const StratumTable& zones, RegistryError& err, PinRuleValidator knownPinRule = nullptr);

    uint32_t    Count() const;
    BiomeIndex  Find(Dia::Core::StringCRC id) const;
    Dia::Core::StringCRC Id(BiomeIndex b) const;
    const char* IdText(BiomeIndex b) const;
    const char* Name(BiomeIndex b) const;
    uint32_t    Colour(BiomeIndex b) const;   // 0xRRGGBB

    bool        HasTag(BiomeIndex b, TagId tag) const;
    uint64_t    TagMask(BiomeIndex b) const;
    TagId       FindTag(Dia::Core::StringCRC tag) const;
    // The fixed tag vocabulary (TagId is the index): lets callers enumerate tags instead of repeating the list.
    static uint32_t    TagCount();
    static const char* TagName(TagId tag);   // nullptr when tag >= TagCount()

    // Known id -> its index. Unknown id -> Role(Fallback), with one DIA_LOG_WARNING per
    // distinct unknown id per registry instance (up to 64 distinct ids are tracked; once
    // that cap is reached further new ids fall back silently). Empty registry -> kNoBiome.
    BiomeIndex  ResolveOrFallback(Dia::Core::StringCRC id) const;

    BiomeIndex  Role(BiomeRole role) const;
    bool        IsGenerated(BiomeIndex b) const;
    bool        IsValidInStratum(BiomeIndex b, StratumIndex zone) const;
    const StratumTable& Strata() const;   // empty (Count 0) until a load succeeds
    // Live tweak of the loaded stratum table's sidecar parameters (StratumTable::SetParams). False on an empty registry.
    bool        SetStrataParams(const std::vector<std::pair<std::string, double>>& changes, StratumError& err, bool* changed = nullptr);
    bool        AreForbidden(BiomeIndex a, BiomeIndex b) const;

    const ClimateEnvelope* Climate(BiomeIndex b) const;
    const BiomeIndex*      ClimateBiomes(uint32_t& count) const;
    const BiomeIndex*      PinsByPriority(uint32_t& count) const;
    Dia::Core::StringCRC   Pin(BiomeIndex b) const;       // pin rule id; default StringCRC when b has no pin
    const char*            PinRuleText(BiomeIndex b) const;   // the rule name as written in the file ("" when unpinned)
    BiomeIndex             Plateau(bool wet) const;

    uint64_t    Hash() const;

    struct Data;   // opaque; defined in the .cpp

private:
    std::unique_ptr<Data> mData;
};

} // namespace HexWorld
} // namespace Draoi
