#include <DraoiHexWorldWeather/WeatherParameters.h>

#include <DraoiHexWorld/BiomeRegistry.h>

#include <type_traits>

namespace Draoi {
namespace HexWorld {
namespace Weather {

static_assert(kMaxBaselineBiomes == BiomeRegistry::kMaxBiomes, "baseline columns must hold every registry biome");
static_assert(std::is_standard_layout_v<WeatherParameters>, "ParamBinding requires a standard-layout struct");

void BindWeatherParameters(Dia::Attribute::ParamBinding<WeatherParameters>& b, uint32_t biomeCount)
{
    using Dia::Core::StringCRC;
    using P = WeatherParameters;

    b.Bind(StringCRC("solar.latitude_cooling_factor"), &P::latitudeCoolingFactor);
    b.Bind(StringCRC("solar.cloud_albedo"), &P::cloudAlbedo);
    b.Bind(StringCRC("solar.water_heat_capacity"), &P::waterHeatCapacity);
    b.Bind(StringCRC("solar.land_heat_capacity"), &P::landHeatCapacity);
    b.Bind(StringCRC("solar.cloud_decay"), &P::cloudDecay);
    b.Bind(StringCRC("pressure.pressure_sensitivity"), &P::pressureSensitivity);
    b.Bind(StringCRC("wind.prevailing_strength"), &P::prevailingStrength);
    b.Bind(StringCRC("wind.pressure_strength"), &P::pressureStrength);
    b.Bind(StringCRC("evaporation.ocean_evap_rate"), &P::oceanEvapRate);
    b.Bind(StringCRC("evaporation.ground_evap_rate"), &P::groundEvapRate);
    b.Bind(StringCRC("transport.moisture_transport_rate"), &P::moistureTransportRate);
    b.Bind(StringCRC("transport.heat_transport_rate"), &P::heatTransportRate);
    b.Bind(StringCRC("orographic.cooling_rate"), &P::orographicCoolingRate);
    b.Bind(StringCRC("orographic.condensation_temp"), &P::condensationTemp);
    b.Bind(StringCRC("orographic.precip_rate"), &P::orographicPrecipRate);
    b.Bind(StringCRC("precipitation.rain_threshold"), &P::rainThreshold);
    b.Bind(StringCRC("precipitation.snow_threshold"), &P::snowThreshold);
    b.Bind(StringCRC("precipitation.precip_rate"), &P::precipRate);
    b.Bind(StringCRC("snow.melt_threshold"), &P::meltThreshold);
    b.Bind(StringCRC("snow.melt_rate"), &P::meltRate);
    b.Bind(StringCRC("snow.melt_cooling"), &P::meltCooling);
    b.Bind(StringCRC("init.initial_moisture_water"), &P::initialMoistureWater);
    b.Bind(StringCRC("init.initial_moisture_land"), &P::initialMoistureLand);
    b.Bind(StringCRC("boundary.edge_inflow"), &P::edgeInflow);

    b.Bind(StringCRC("influence.max_strength"), &P::influenceMaxStrength);
    b.Bind(StringCRC("influence.max_delta_pressure"), &P::influenceMaxDeltaPressure);
    b.Bind(StringCRC("influence.max_delta_moisture"), &P::influenceMaxDeltaMoisture);
    b.Bind(StringCRC("influence.max_delta_temperature"), &P::influenceMaxDeltaTemperature);
    b.Bind(StringCRC("influence.max_lifetime_weeks"), &P::influenceMaxLifetimeWeeks);
    b.Bind(StringCRC("influence.max_active"), &P::influenceMaxActive);
    b.Bind(StringCRC("influence.preview_fade_days"), &P::influencePreviewFadeDays);
    b.Bind(StringCRC("influence.pressure_anomaly_decay"), &P::pressureAnomalyDecay);
    b.Bind(StringCRC("influence.pressure_anomaly_max"), &P::pressureAnomalyMax);
    b.Bind(StringCRC("influence.cap_water"), &P::influenceCapWater);
    b.Bind(StringCRC("influence.max_radius"), &P::influenceMaxRadius);
    b.Bind(StringCRC("influence.max_set_cells"), &P::influenceMaxSetCells);
    b.Bind(StringCRC("influence.ground_water_factor"), &P::influenceGroundWaterFactor);

    b.BindMap(StringCRC("baseline.spring"), &P::baselineSpring, biomeCount);
    b.BindMap(StringCRC("baseline.summer"), &P::baselineSummer, biomeCount);
    b.BindMap(StringCRC("baseline.autumn"), &P::baselineAutumn, biomeCount);
    b.BindMap(StringCRC("baseline.winter"), &P::baselineWinter, biomeCount);
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
