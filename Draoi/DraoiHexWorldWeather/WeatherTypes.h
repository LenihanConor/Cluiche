// A4 weather data types: season, per-cell fields, hand-off structs, terrain input.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-data.md (Feature 1, Draoi layer amendment)
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DraoiHexWorld/CellVocabulary.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {
namespace Weather {

enum class Season : uint8_t { Spring, Summer, Autumn, Winter };
constexpr uint32_t kSeasonCount = 4;

// Structure-of-arrays weather state, one entry per cell. Radians [0, 2pi) for windDirection, [0, 1] for most others,
// pressureAnomaly is signed in [-0.5, 0.5] (0 = none).
struct WeatherFields
{
    Dia::Core::Containers::DynamicArray<float> moisture, temperature, pressure, windDirection, windSpeed,
                                               precipitation, cloudCover, waterToGround, snowPack, pressureAnomaly;

    // Fully replaces every array with n zeros (a smaller second Resize leaves no stale values).
    void Resize(uint32_t n);
    uint32_t CellCount() const { return moisture.Size(); }
};

// Per-week values A4 hands to A5. Types only here; Feature 2 fills them. Not saved.
struct WeatherHandoff
{
    Dia::Core::Containers::DynamicArray<float> glacierInput;     // [0, 2]
    Dia::Core::Containers::DynamicArray<float> influenceWater;   // Feature 7's signed water weight
    void Resize(uint32_t n);
};

// A5's previous committed week, fed back to A4. `present` is false when no A5 is attached.
struct HydrologyInputs
{
    Dia::Core::Containers::DynamicArray<float>   surfaceWetness, openWater;
    Dia::Core::Containers::DynamicArray<uint8_t> glacierEligible;
    uint32_t glacierCheckWeek = 26;
    float    glacierSnowFloor = 0.2f;   // the defaults are test values; A5's sidecar is the source
    bool     present = false;
    void Resize(uint32_t n);
};

// Read-only after Initialise.
struct WeatherTerrain
{
    Dia::Core::Containers::DynamicArray<float>       elevation, latitude;   // [0, 1]; latitude 0 = equator, 1 = pole
    Dia::Core::Containers::DynamicArray<WaterSource> waterSource;
    Dia::Core::Containers::DynamicArray<BiomeIndex>  biome;
    uint32_t width = 0, height = 0;

    // True when every array has width*height entries and the cell count is non-zero.
    bool IsConsistent() const;
};

// Optional per-cell baseline temperature for one season ([0, 1]); A1's package supplies the fill. When absent, the
// baseline comes from the sidecar table carried in WeatherParameters.
struct SeasonalBaselineInput
{
    Dia::Core::Containers::DynamicArray<float> temperature;
};

// Feature 7 influence snapshot: queried state before and after Step, to populate a cache for preview and commit.
struct InfluenceSnapshot
{
    bool IsEmpty() const { return true; }
};

// Feature 8 climate offset arrays per zone: temperature, evaporation, and wind adjustments.
struct ClimateOffsets
{
    Dia::Core::Containers::DynamicArray<float> tempOffset, evapMultiplier, windShift;
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
