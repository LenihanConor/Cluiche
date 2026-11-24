// A4 weather tuning parameters as a plain struct, filled once per step by ParamBinding::Snapshot.
// The sidecar (weather_parameters.schema.json, CoW data) is the only place a parameter's kind, range, default and
// description live; this struct carries no defaults and there is no compiled fallback.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-data.md (Feature 1, world-sim review amendment 1)
#pragma once

#include <DraoiHexWorldWeather/WeatherTypes.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {
namespace Weather {

// Capacity of the per-biome baseline columns; equals BiomeRegistry::kMaxBiomes (checked in WeatherParameters.cpp).
constexpr uint32_t kMaxBaselineBiomes = 254;

// Standard layout (ParamBinding offset arithmetic). Members are grouped as in the sidecar / GDD section 14.
struct WeatherParameters
{
    // solar
    float latitudeCoolingFactor = 0.0f, cloudAlbedo = 0.0f, waterHeatCapacity = 0.0f, landHeatCapacity = 0.0f, cloudDecay = 0.0f;
    // pressure
    float pressureSensitivity = 0.0f;
    // wind
    float prevailingStrength = 0.0f, pressureStrength = 0.0f;
    // evaporation
    float oceanEvapRate = 0.0f, groundEvapRate = 0.0f;
    // transport
    float moistureTransportRate = 0.0f, heatTransportRate = 0.0f;
    // orographic
    float orographicCoolingRate = 0.0f, condensationTemp = 0.0f, orographicPrecipRate = 0.0f;
    // precipitation
    float rainThreshold = 0.0f, snowThreshold = 0.0f, precipRate = 0.0f;
    // snow
    float meltThreshold = 0.0f, meltRate = 0.0f, meltCooling = 0.0f;
    // init and boundary
    float initialMoistureWater = 0.0f, initialMoistureLand = 0.0f;
    bool  edgeInflow = false;
    // generic influence caps (per-kind values live in Feature 7's catalogue)
    float   influenceMaxStrength = 0.0f, influenceMaxDeltaPressure = 0.0f, influenceMaxDeltaMoisture = 0.0f,
            influenceMaxDeltaTemperature = 0.0f;
    int32_t influenceMaxLifetimeWeeks = 0, influenceMaxActive = 0;
    float   influencePreviewFadeDays = 0.0f, pressureAnomalyDecay = 0.0f, pressureAnomalyMax = 0.0f, influenceCapWater = 0.0f;
    int32_t influenceMaxRadius = 0, influenceMaxSetCells = 0;
    float   influenceGroundWaterFactor = 0.0f;
    // Fallback seasonal baseline temperature per biome, [season][BiomeIndex]; only the first registry Count() entries
    // of each row are meaningful. Used when no SeasonalBaselineInput is supplied.
    float seasonalBaseline[kSeasonCount][kMaxBaselineBiomes] = {};
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
