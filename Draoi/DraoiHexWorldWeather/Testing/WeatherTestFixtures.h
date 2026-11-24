// A4 weather test helper: fixtures for per-pass unit tests (Feature 2 passes).
// Ships with the library (no gtest dependency). Never loads the CoW sidecar: parameters are set by hand.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
#pragma once

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldWeather/WeatherParameters.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>

#include <cstdint>
#include <cstring>

namespace Draoi {
namespace HexWorld {
namespace Weather {
namespace Testing {

// Every WeatherParameters field at the GDD section 14 default (the sidecar's defaults). The baseline columns carry the
// 13 shipped biome values per season; indices past them hold 0.5.
inline WeatherParameters MakeTestParams()
{
    WeatherParameters p;
    p.latitudeCoolingFactor = 0.3f; p.cloudAlbedo = 0.4f; p.waterHeatCapacity = 0.3f; p.landHeatCapacity = 0.8f;
    p.cloudDecay = 0.5f;
    p.pressureSensitivity = 0.6f;
    p.prevailingStrength = 0.6f; p.pressureStrength = 0.4f;
    p.oceanEvapRate = 0.3f; p.groundEvapRate = 0.05f;
    p.moistureTransportRate = 0.4f; p.heatTransportRate = 0.2f;
    p.orographicCoolingRate = 0.3f; p.condensationTemp = 0.4f; p.orographicPrecipRate = 0.6f;
    p.rainThreshold = 0.6f; p.snowThreshold = 0.3f; p.precipRate = 0.5f;
    p.meltThreshold = 0.35f; p.meltRate = 0.3f; p.meltCooling = 0.1f;
    p.initialMoistureWater = 0.8f; p.initialMoistureLand = 0.3f;
    p.edgeInflow = true;
    p.influenceMaxStrength = 1.0f; p.influenceMaxDeltaPressure = 0.3f; p.influenceMaxDeltaMoisture = 0.3f;
    p.influenceMaxDeltaTemperature = 0.15f;
    p.influenceMaxLifetimeWeeks = 13; p.influenceMaxActive = 64;
    p.influencePreviewFadeDays = 1.0f; p.pressureAnomalyDecay = 0.7f; p.pressureAnomalyMax = 0.5f; p.influenceCapWater = 1.0f;
    p.influenceMaxRadius = 32; p.influenceMaxSetCells = 8192;
    p.influenceGroundWaterFactor = 0.05f;

    static const float kSpring[13] = { 0.5f, 0.46f, 0.55f, 0.48f, 0.5f, 0.78f, 0.22f, 0.12f, 0.85f, 0.7f, 0.6f, 0.62f, 0.7f };
    static const float kSummer[13] = { 0.58f, 0.6f, 0.72f, 0.64f, 0.66f, 0.95f, 0.4f, 0.3f, 0.88f, 0.8f, 0.8f, 0.8f, 0.88f };
    static const float kAutumn[13] = { 0.54f, 0.5f, 0.58f, 0.5f, 0.52f, 0.8f, 0.24f, 0.14f, 0.85f, 0.72f, 0.62f, 0.64f, 0.72f };
    static const float kWinter[13] = { 0.44f, 0.34f, 0.38f, 0.3f, 0.34f, 0.58f, 0.08f, 0.02f, 0.82f, 0.6f, 0.42f, 0.42f, 0.5f };
    for (uint32_t b = 0; b < kMaxBaselineBiomes; ++b)
    {
        p.baselineSpring[b] = b < 13 ? kSpring[b] : 0.5f;
        p.baselineSummer[b] = b < 13 ? kSummer[b] : 0.5f;
        p.baselineAutumn[b] = b < 13 ? kAutumn[b] : 0.5f;
        p.baselineWinter[b] = b < 13 ? kWinter[b] : 0.5f;
    }
    return p;
}

// The shipped biome registry (BiomeTestRegistry), for Capture / Initialise calls.
inline const BiomeRegistry& TestRegistry() { return Draoi::HexWorld::Testing::ShippedBiomeRegistry(); }

struct HexTerrainOptions
{
    float       latitude = 0.3f;        // constant latitude unless latitudeRamp
    bool        latitudeRamp = false;   // latitude = row / (height - 1), 0 at the top row
    uint32_t    waterColumns = 1;       // columns [0, waterColumns) are an Ocean strip (Initialise needs a water cell)
    BiomeIndex  biome = 2;
    float       elevation = 0.3f;
};

// Hand-built width x height terrain: flat land with an optional latitude ramp and a water strip down the west edge.
inline WeatherTerrain MakeHexTerrain(uint32_t width, uint32_t height, const HexTerrainOptions& o = HexTerrainOptions())
{
    WeatherTerrain t;
    t.width = width;
    t.height = height;
    const uint32_t n = width * height;
    t.elevation.Reserve(n); t.latitude.Reserve(n); t.waterSource.Reserve(n); t.biome.Reserve(n);
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t col = i % width, row = i / width;
        t.elevation.Add(o.elevation);
        t.latitude.Add(o.latitudeRamp ? float(row) / float(height > 1 ? height - 1 : 1) : o.latitude);
        t.waterSource.Add(col < o.waterColumns ? WaterSource::Ocean : WaterSource::None);
        t.biome.Add(o.biome);
    }
    return t;
}

// Bitwise equality of two float arrays.
inline bool SameFloats(const Dia::Core::Containers::DynamicArray<float>& a, const Dia::Core::Containers::DynamicArray<float>& b)
{
    return a.Size() == b.Size() && (a.Size() == 0 || std::memcmp(&a[0u], &b[0u], a.Size() * sizeof(float)) == 0);
}

// Bitwise equality of all ten fields.
inline bool SameFields(const WeatherFields& a, const WeatherFields& b)
{
    return SameFloats(a.moisture, b.moisture) && SameFloats(a.temperature, b.temperature) && SameFloats(a.pressure, b.pressure)
        && SameFloats(a.windDirection, b.windDirection) && SameFloats(a.windSpeed, b.windSpeed)
        && SameFloats(a.precipitation, b.precipitation) && SameFloats(a.cloudCover, b.cloudCover)
        && SameFloats(a.waterToGround, b.waterToGround) && SameFloats(a.snowPack, b.snowPack)
        && SameFloats(a.pressureAnomaly, b.pressureAnomaly);
}

// Deep copy of a field set (DynamicArray assignment truncates, so rebuild by hand).
inline void CloneFields(const WeatherFields& src, WeatherFields& dst)
{
    dst.Resize(src.CellCount());
    for (uint32_t i = 0; i < src.CellCount(); ++i)
    {
        dst.moisture[i] = src.moisture[i];           dst.temperature[i] = src.temperature[i];
        dst.pressure[i] = src.pressure[i];           dst.windDirection[i] = src.windDirection[i];
        dst.windSpeed[i] = src.windSpeed[i];         dst.precipitation[i] = src.precipitation[i];
        dst.cloudCover[i] = src.cloudCover[i];       dst.waterToGround[i] = src.waterToGround[i];
        dst.snowPack[i] = src.snowPack[i];           dst.pressureAnomaly[i] = src.pressureAnomaly[i];
    }
}

} // namespace Testing
} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
