// A4 weather simulation: owned state, Initialise and seeding. Feature 2 adds the weekly step on top.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-data.md (Feature 1)
#pragma once

#include <DraoiHexWorldWeather/WeatherParameters.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace Draoi {
namespace HexWorld {

class BiomeRegistry;

namespace Weather {

// clamp(baseline * (1 - latitudeCoolingFactor * latitude), 0, 1). Shared by seeding and Feature 2's solar pass, so
// week-0 temperature and the converged solar target cannot disagree.
float SeasonalTargetTemperature(float baseline, float latitude, float latitudeCoolingFactor);

class WeatherSimulation
{
public:
    // Validates, then builds the new state aside and swaps it in, so a failed call leaves the previous state unchanged.
    // Returns false (nothing changes) when the terrain is inconsistent or empty, has no water-source cell (EC-01), or
    // `baseline` is given with a length other than the cell count. The caller's terrain is copied, not retained.
    // Seeds spring: temperature from the baseline (the optional per-cell input, else params.seasonalBaseline by biome;
    // an unknown or out-of-range biome takes the registry's fallback biome value, kNoBiome takes 0.5) times the
    // latitude modifier; moisture by water source; pressure 0.5; every other field 0. Reads no S-1 climate layer.
    bool Initialise(const WeatherTerrain& terrain, const WeatherParameters& params, const BiomeRegistry& registry,
                    const SeasonalBaselineInput* baseline = nullptr);

    bool                  IsInitialised() const { return mInitialised; }
    const WeatherFields&  Fields() const { return mFields; }
    const WeatherTerrain& Terrain() const { return mTerrain; }

private:
    static void SeedFields(Season season, const WeatherTerrain& terrain, const WeatherParameters& params,
                           const BiomeRegistry& registry, const SeasonalBaselineInput* baseline, WeatherFields& out);

    // Feature 2 builds its grid tables here. Called last in Initialise; empty in Feature 1.
    void BuildGridTables();

    WeatherTerrain mTerrain;
    WeatherFields  mFields;
    bool           mInitialised = false;
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
