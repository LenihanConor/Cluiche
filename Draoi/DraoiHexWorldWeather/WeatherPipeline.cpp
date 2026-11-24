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

void RunPipelineUntil(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor, PassId lastPass)
{
    // Pass 0 (Influence) is skipped until Feature 7.
    if (lastPass < PassId::Solar) return;
    RunSolarPass(context, executor);
    if (lastPass < PassId::Pressure) return;
    RunPressurePass(context, executor);
    if (lastPass < PassId::Wind) return;
    RunWindPass(context, executor);
}

void RunPipeline(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor)
{
    RunPipelineUntil(context, executor, PassId::kLast);
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
