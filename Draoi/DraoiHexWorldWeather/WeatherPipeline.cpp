// A4 weather pipeline: cellular-automaton passes (solar, pressure, wind, evaporation, transport, etc).
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
// Run call tree rules: no heap allocation, no std containers, no RNG, no atomics. Every ParallelFor lambda captures a
// single reference so it stays inside std::function's small buffer.
#include <DraoiHexWorldWeather/WeatherPipeline.h>

#include <DiaGeometry2D/Grid/HexTopology.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <math.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

namespace
{
    constexpr float kTwoPiF = 6.28318530717958647692f;
    // Profile category: the weather run is a DiaDeferredStep job, so its zones group under that category.
    constexpr Dia::Observation::Profile::ProfileCategory kWeatherCategory = Dia::Observation::Profile::Category::kDiaDeferredStep;

    inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // Angle in [0, 2pi) from an on-screen counter-clockwise angle in any range.
    inline float Wrap2Pi(float a)
    {
        a = fmodf(a, kTwoPiF);
        if (a < 0.0f) a += kTwoPiF;
        if (a >= kTwoPiF) a = 0.0f;
        return a;
    }
}

WindVector PressureGradient(uint32_t cell, const WeatherFields& fields, const GridTables& tables)
{
    const float self = fields.pressure[cell];
    float gx = 0.0f, gy = 0.0f;
    for (uint32_t d = 0; d < Dia::Geometry2D::kHexDirections; ++d)
    {
        const uint32_t nb = tables.neighbours[cell * Dia::Geometry2D::kHexDirections + d];
        if (nb == Dia::Geometry2D::kNoCell) continue;   // contributes nothing, still counted in the /6
        const float diff = self - fields.pressure[nb];
        gx += kDirectionVectors[d].x * diff;
        gy += kDirectionVectors[d].y * diff;
    }
    WindVector g;
    g.x = gx / 6.0f;
    g.y = gy / 6.0f;
    return g;
}

// Pass 1 Solar (GDD s3).
void RunSolarPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass1.Solar", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const CapturedInputs& in = *context.inputs;
        const WeatherParameters& p = in.params;
        WeatherFields& f = *context.fields;
        for (uint32_t c = begin; c < end; ++c)
        {
            // Weekly reset first.
            f.precipitation[c] = 0.0f;
            f.waterToGround[c] = 0.0f;
            context.handoff->glacierInput[c] = 0.0f;
            f.cloudCover[c] *= (1.0f - p.cloudDecay);

            const float baseline = Clamp01(in.seasonBaseline[c] + in.climate.tempOffset[c]);
            const float target = SeasonalTargetTemperature(baseline, context.terrain->latitude[c], p.latitudeCoolingFactor);
            const float solarInput = 1.0f - f.cloudCover[c] * p.cloudAlbedo;
            const float heatCapacity = (context.terrain->waterSource[c] != WaterSource::None) ? p.waterHeatCapacity : p.landHeatCapacity;
            const float delta = (target - f.temperature[c]) * heatCapacity * solarInput;
            // Feature 7 hook: Influence dTemperature is added here (then clamped [0,1]).
            f.temperature[c] = Clamp01(f.temperature[c] + delta);
        }
    });
}

// Pass 2 Pressure (GDD s4).
void RunPressurePass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass2.Pressure", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const float sensitivity = context.inputs->params.pressureSensitivity;
        WeatherFields& f = *context.fields;
        for (uint32_t c = begin; c < end; ++c)
        {
            f.pressure[c] = Clamp01(0.5f - (f.temperature[c] - 0.5f) * sensitivity + f.pressureAnomaly[c]);
        }
        // Hook A (pressure hold) is added by task 9: a hold overwrites pressure for its cells after this loop.
    });
}

// Pass 3 Wind (GDD s5). windDirection is the on-screen counter-clockwise angle in [0, 2pi), matching
// kDirectionVectors: with the y-down vector (vx, vy), angle = atan2(-vy, vx).
void RunWindPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass3.Wind", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const CapturedInputs& in = *context.inputs;
        const WeatherParameters& p = in.params;
        WeatherFields& f = *context.fields;
        const uint32_t width = context.terrain->width;
        for (uint32_t c = begin; c < end; ++c)
        {
            float effLat = context.terrain->latitude[c] - in.climate.windShift[c];
            effLat = effLat < 0.0f ? 0.0f : (effLat > 1.0f ? 1.0f : effLat);
            const float prevailing = PrevailingDirection(effLat, in.westerlyBand);

            const WindVector g = PressureGradient(c, f, *context.tables);
            const float vx = cosf(prevailing) * p.prevailingStrength + g.x * p.pressureStrength;
            const float vy = -sinf(prevailing) * p.prevailingStrength + g.y * p.pressureStrength;
            const float mag = sqrtf(vx * vx + vy * vy);

            float angle = prevailing;
            float speed = 0.0f;
            if (mag > 0.0f)
            {
                angle = atan2f(-vy, vx);
                speed = mag > 1.0f ? 1.0f : mag;
            }
            angle = Wrap2Pi(angle);
            f.windDirection[c] = angle;
            f.windSpeed[c] = speed;
            context.tables->dirIndex[c] = SnapWindDirection(angle, c % width);
        }
        // Hook B (wind override) is added by task 9: overrides replace the result and re-snap dirIndex.
    });
}

// Pass 4 Evaporation (GDD s6).
void RunEvaporationPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass4.Evaporation", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const CapturedInputs& in = *context.inputs;
        const WeatherParameters& p = in.params;
        WeatherFields& f = *context.fields;
        for (uint32_t c = begin; c < end; ++c)
        {
            const float temp = f.temperature[c];
            float m = f.moisture[c];
            // 6.1 primary: self-limiting at saturation.
            const float open = in.hydrology.openWater[c];
            if (open > 0.0f)
                m += temp * p.oceanEvapRate * in.climate.evapMultiplier[c] * open * (1.0f - m);
            // 6.2 secondary: capped at the ground water the cell holds; surfaceWetness is latched 0 with no A5.
            const float wet = in.hydrology.surfaceWetness[c];
            if (wet > 0.0f)
            {
                float fromGround = wet * (temp * p.groundEvapRate);
                if (fromGround > wet) fromGround = wet;
                if (fromGround > 0.0f) m += fromGround;
            }
            f.moisture[c] = Clamp01(m);
        }
        // Feature 7 hook: Influence dMoisture is added here (then clamped [0,1]).
        // Hook C (moisture hold) is added by task 9, after the influence delta, so a hold wins.
    });
}

// Pass 5a: outflow scratch for moisture and pressureAnomaly.
void RunMoistureOutPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass5a.MoistureOut", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const float rate = context.inputs->params.moistureTransportRate;
        const WeatherFields& f = *context.fields;
        GridTables& t = *context.tables;
        for (uint32_t c = begin; c < end; ++c)
        {
            if (t.neighbours[c * Dia::Geometry2D::kHexDirections + t.dirIndex[c]] == Dia::Geometry2D::kNoCell)
            {
                t.moistureOut[c] = 0.0f;   // EC-16: stays in the cell
                t.anomalyOut[c] = 0.0f;
            }
            else
            {
                t.moistureOut[c] = f.moisture[c] * f.windSpeed[c] * rate;
                t.anomalyOut[c] = f.pressureAnomaly[c] * f.windSpeed[c] * rate;
            }
        }
    });
}

namespace
{
    // Inflow to `c` from `out`: neighbour directions ascending (a neighbour n sends to c when its dirIndex points back
    // at c, i.e. (d+3)%6), then the edge term last.
    inline float GatherInflow(uint32_t c, const GridTables& t, const float* out, bool edgeInflow)
    {
        constexpr uint32_t kDirs = Dia::Geometry2D::kHexDirections;
        float inflow = 0.0f;
        for (uint32_t d = 0; d < kDirs; ++d)
        {
            const uint32_t n = t.neighbours[c * kDirs + d];
            if (n != Dia::Geometry2D::kNoCell && t.dirIndex[n] == (d + 3) % kDirs)
                inflow += out[n];
        }
        if (edgeInflow && t.neighbours[c * kDirs + (t.dirIndex[c] + 3) % kDirs] == Dia::Geometry2D::kNoCell)
            inflow += out[c];
        return inflow;
    }
}

// Pass 5b: gather. Reads only *Out and dirIndex of neighbours; writes its own cells.
void RunMoistureGatherPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass5b.MoistureGather", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const bool edge = context.inputs->params.edgeInflow;
        WeatherFields& f = *context.fields;
        const GridTables& t = *context.tables;
        for (uint32_t c = begin; c < end; ++c)
        {
            f.moisture[c] = f.moisture[c] - t.moistureOut[c] + GatherInflow(c, t, &t.moistureOut[0u], edge);
            f.pressureAnomaly[c] = f.pressureAnomaly[c] - t.anomalyOut[c] + GatherInflow(c, t, &t.anomalyOut[0u], edge);
        }
    });
}

// Pass 5c (GDD s4.1).
void RunAnomalyDecayPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass5c.AnomalyDecay", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const WeatherParameters& p = context.inputs->params;
        WeatherFields& f = *context.fields;
        for (uint32_t c = begin; c < end; ++c)
        {
            float a = f.pressureAnomaly[c] * p.pressureAnomalyDecay;
            if (a > p.pressureAnomalyMax) a = p.pressureAnomalyMax;
            if (a < -p.pressureAnomalyMax) a = -p.pressureAnomalyMax;
            f.pressureAnomaly[c] = a;
        }
    });
}

// Pass 6a (GDD s8).
void RunHeatOutPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass6a.HeatOut", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const float rate = context.inputs->params.heatTransportRate;
        const WeatherFields& f = *context.fields;
        GridTables& t = *context.tables;
        for (uint32_t c = begin; c < end; ++c)
        {
            t.heatOut[c] = (t.neighbours[c * Dia::Geometry2D::kHexDirections + t.dirIndex[c]] == Dia::Geometry2D::kNoCell)
                ? 0.0f : f.temperature[c] * f.windSpeed[c] * rate;
        }
    });
}

// Pass 6b: temperature = max(temperature - out, 0) + inflow (CH-07). The [0,1] clamp is the finish pass's.
void RunHeatGatherPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    DIA_PROFILE_SCOPE("Weather.Pass6b.HeatGather", kWeatherCategory);
    executor.ParallelFor(context.tables->cellCount, kMinRange, [&context](uint32_t begin, uint32_t end)
    {
        const bool edge = context.inputs->params.edgeInflow;
        WeatherFields& f = *context.fields;
        const GridTables& t = *context.tables;
        for (uint32_t c = begin; c < end; ++c)
        {
            const float rem = f.temperature[c] - t.heatOut[c];
            f.temperature[c] = (rem > 0.0f ? rem : 0.0f) + GatherInflow(c, t, &t.heatOut[0u], edge);
        }
    });
}

void RunPipelineUntil(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor, PassId lastPass)
{
    // Pass 0 (Influence) is skipped until Feature 7.
    if (lastPass < PassId::Solar) return;
    RunSolarPass(context, executor);
    if (lastPass < PassId::Pressure) return;
    RunPressurePass(context, executor);
    if (lastPass < PassId::Wind) return;
    RunWindPass(context, executor);
    if (lastPass < PassId::Evaporation) return;
    RunEvaporationPass(context, executor);
    if (lastPass < PassId::MoistureOut) return;
    RunMoistureOutPass(context, executor);
    if (lastPass < PassId::MoistureGather) return;
    RunMoistureGatherPass(context, executor);
    if (lastPass < PassId::AnomalyDecay) return;
    RunAnomalyDecayPass(context, executor);
    if (lastPass < PassId::HeatOut) return;
    RunHeatOutPass(context, executor);
    if (lastPass < PassId::HeatGather) return;
    RunHeatGatherPass(context, executor);
}

void RunPipeline(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    RunPipelineUntil(context, executor, PassId::kLast);
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
