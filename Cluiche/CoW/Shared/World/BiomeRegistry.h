// Biome registry for mapping biome identifiers to properties.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>
#include <Shared/WorldGeneration/TerrainEnums.h>

#include <cstdint>
#include <memory>

namespace CoW {
namespace World {

// BiomeIndex / kNoBiome are declared in WorldGeneration/TerrainEnums.h (Cell stores a BiomeIndex; avoids a circular include).

using TagId = uint8_t;
constexpr TagId kNoTag = 0xFF;

enum class BiomeRole : uint8_t { Ocean, Lake, Fallback };
enum class PinRule : uint8_t { WaterOcean, WaterLake, Vent, Alpine, Wetland };

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

    // All-or-nothing: on failure *this is unchanged.
    bool Load(const Dia::Core::FilePath& path, RegistryError& err);
    bool LoadFromText(const char* json, RegistryError& err);

    uint32_t    Count() const;
    BiomeIndex  Find(Dia::Core::StringCRC id) const;
    Dia::Core::StringCRC Id(BiomeIndex b) const;
    const char* IdText(BiomeIndex b) const;
    const char* Name(BiomeIndex b) const;
    uint32_t    Colour(BiomeIndex b) const;   // 0xRRGGBB

    bool        HasTag(BiomeIndex b, TagId tag) const;
    uint64_t    TagMask(BiomeIndex b) const;
    TagId       FindTag(Dia::Core::StringCRC tag) const;

    // Known id -> its index. Unknown id -> Role(Fallback), with one DIA_LOG_WARNING per
    // distinct unknown id per registry instance (up to 64 distinct ids are tracked; once
    // that cap is reached further new ids fall back silently). Empty registry -> kNoBiome.
    BiomeIndex  ResolveOrFallback(Dia::Core::StringCRC id) const;

    BiomeIndex  Role(BiomeRole role) const;
    bool        IsGenerated(BiomeIndex b) const;
    bool        IsValidInZone(BiomeIndex b, WorldGeneration::DepthZone zone) const;
    bool        AreForbidden(BiomeIndex a, BiomeIndex b) const;

    const ClimateEnvelope* Climate(BiomeIndex b) const;
    const BiomeIndex*      ClimateBiomes(uint32_t& count) const;
    const BiomeIndex*      PinsByPriority(uint32_t& count) const;
    PinRule                Pin(BiomeIndex b) const;
    BiomeIndex             Plateau(bool wet) const;

    uint64_t    Hash() const;

    struct Data;   // opaque; defined in the .cpp

private:
    std::unique_ptr<Data> mData;
};

} // namespace World
} // namespace CoW
