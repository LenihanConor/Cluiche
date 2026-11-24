#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <DraoiHexWorld/BiomeRegistry.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

namespace
{
    // DynamicArray::operator=, Assign and Swap truncate to the destination's capacity, so replace by hand.
    template <class T>
    void Take(Dia::Core::Containers::DynamicArray<T>& dst, const Dia::Core::Containers::DynamicArray<T>& src)
    {
        dst.RemoveAll();
        dst.Reserve(src.Size());
        for (uint32_t i = 0; i < src.Size(); ++i) dst.Add(src[i]);
    }
}

float SeasonalTargetTemperature(float baseline, float latitude, float latitudeCoolingFactor)
{
    const float t = baseline * (1.0f - latitudeCoolingFactor * latitude);
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

void WeatherSimulation::SeedFields(Season season, const WeatherTerrain& terrain, const WeatherParameters& params,
                                   const BiomeRegistry& registry, const SeasonalBaselineInput* baseline, WeatherFields& out)
{
    const uint32_t n = terrain.width * terrain.height;
    out.Resize(n);

    // One lookup per biome, built once: per-cell work is then an index, with no registry call.
    uint32_t count = registry.Count();
    if (count > kMaxBaselineBiomes) count = kMaxBaselineBiomes;
    const float* row = params.BaselineRow(season);
    const BiomeIndex fallback = registry.Role(BiomeRole::Fallback);
    const float fallbackValue = (fallback != kNoBiome && fallback < count) ? row[fallback] : 0.5f;

    for (uint32_t i = 0; i < n; ++i)
    {
        float base;
        if (baseline != nullptr)
        {
            base = baseline->temperature[i];
        }
        else
        {
            const BiomeIndex b = terrain.biome[i];
            base = (b < count) ? row[b] : (b == kNoBiome ? 0.5f : fallbackValue);
        }
        out.temperature[i] = SeasonalTargetTemperature(base, terrain.latitude[i], params.latitudeCoolingFactor);
        out.moisture[i]    = (terrain.waterSource[i] != WaterSource::None) ? params.initialMoistureWater : params.initialMoistureLand;
        out.pressure[i]    = 0.5f;
    }
}

bool WeatherSimulation::Initialise(const WeatherTerrain& terrain, const WeatherParameters& params,
                                   const BiomeRegistry& registry, const SeasonalBaselineInput* baseline)
{
    if (!terrain.IsConsistent()) return false;

    const uint32_t n = terrain.width * terrain.height;
    bool anyWater = false;
    for (uint32_t i = 0; i < n && !anyWater; ++i) anyWater = (terrain.waterSource[i] != WaterSource::None);
    if (!anyWater) return false;   // EC-01

    if (baseline != nullptr && baseline->temperature.Size() != n) return false;

    // Every failure path is above, so from here on the call cannot fail and the members can be built in place:
    // a rejected call has already returned with the previous state untouched.
    Take(mTerrain.elevation, terrain.elevation);
    Take(mTerrain.latitude, terrain.latitude);
    Take(mTerrain.waterSource, terrain.waterSource);
    Take(mTerrain.biome, terrain.biome);
    mTerrain.width  = terrain.width;
    mTerrain.height = terrain.height;
    SeedFields(Season::Spring, mTerrain, params, registry, baseline, mFields);
    mInitialised = true;
    BuildGridTables();
    return true;
}

void WeatherSimulation::BuildGridTables()
{
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
