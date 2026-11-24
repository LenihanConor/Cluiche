// A4 weather pipeline: latched inputs, grid tables and the pass sequence.
// Later tasks fill RunPipeline with the passes; every pass reads only PassContext.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaParallelExecutor/IParallelExecutor.h>
#include <DraoiHexWorldWeather/WeatherForcing.h>
#include <DraoiHexWorldWeather/WeatherParameters.h>
#include <DraoiHexWorldWeather/WeatherStepProfile.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>
#include <DraoiHexWorldWeather/WindDirection.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {

// Cells per ParallelFor range at minimum.
constexpr uint32_t kMinRange = 256;

// Built once at Initialise (BuildGridTables) and reused: Run never allocates.
struct GridTables
{
    template <class T> using Array = Dia::Core::Containers::DynamicArray<T>;

    Array<uint32_t> neighbours;                       // N * 6, kNoCell off the grid
    Array<float>    moistureOut, heatOut, anomalyOut; // stable-read scratch
    Array<uint8_t>  dirIndex;                         // snapped wind direction per cell
    Array<float>    dTemperature, dMoisture;          // pass 0 per-cell influence deltas
    Array<uint8_t>  claimed;                          // kForcingKindCount * N: winning record index + 1, 0 = none
    uint32_t        cellCount = 0;
};

// Everything Run reads that the caller could otherwise mutate, copied at Capture. Run never touches caller objects.
struct CapturedInputs
{
    template <class T> using Array = Dia::Core::Containers::DynamicArray<T>;

    WeatherParameters params;
    Season            season = Season::Spring;
    uint32_t          weekOfYear = 0;
    Array<float>      seasonBaseline;      // per cell, resolved at Capture
    WesterlyBand      westerlyBand;
    Array<float>      orographicFactor;    // per cell, default 1
    Array<WeatherForcing> forcings;        // capacity kMaxForcings
    CellSetStore      cellSets;
    HydrologyInputs   hydrology;           // always N entries once captured
    InfluenceSnapshot influences;
    ClimateOffsets    climate;             // per cell: tempOffset 0, evapMultiplier 1, windShift 0 by default
};

struct PassContext
{
    const WeatherTerrain*   terrain = nullptr;
    GridTables*             tables = nullptr;
    const CapturedInputs*   inputs = nullptr;
    WeatherFields*          fields = nullptr;   // the working set
    WeatherHandoff*         handoff = nullptr;
    WeatherGridTotals*      totals = nullptr;
    WeatherStepProfile*     profile = nullptr;
};

// Pass identifiers in run order (pass 10 is retired). Later pass tasks append their entries before kLast.
enum class PassId : uint8_t
{
    Influence = 0, Solar, Pressure, Wind, Evaporation, MoistureOut, MoistureGather, AnomalyDecay, HeatOut, HeatGather,
    Orographic, Precipitation, SnowMelt, Finish,
    kLast = Finish
};

// The ordered pass sequence (0, 1, 2, 3, 4, 5a..6b, 7, 8, 9, finish). Filled in by the pass tasks.
void RunPipeline(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);

// Test hook: runs the pipeline in order and stops after `lastPass` (inclusive). RunPipeline == RunPipelineUntil(kLast).
void RunPipelineUntil(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor, PassId lastPass);

// Individual passes (each is one ParallelFor over the cells), exposed so they can be tested alone.
void RunSolarPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);     // pass 1
void RunPressurePass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);  // pass 2
void RunWindPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);      // pass 3
void RunEvaporationPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);     // pass 4
void RunMoistureOutPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);     // pass 5a (+ anomalyOut)
void RunMoistureGatherPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);  // pass 5b (+ anomaly)
void RunAnomalyDecayPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);    // pass 5c
void RunHeatOutPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);         // pass 6a
void RunHeatGatherPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);      // pass 6b

void RunOrographicPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);      // pass 7

void RunPrecipitationPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);   // pass 8
void RunSnowMeltPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);       // pass 9 (+ firn)
// Clamps every field to its range, then the serial grid totals (into context.totals).
void RunFinishPass(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);

// Resolves the latched forcings into per-cell winners (serial, record order, no allocation): GridTables::claimed
// [kind * cellCount + cell] = lowest covering record index + 1, 0 = none. RunPipelineUntil calls it first; passes run
// alone (tests) call it after Capture. Reads only the latched forcings and cell sets.
void ResolveForcings(PassContext& context);

// Routes `amount` of condensate that fell on `cell` (passes 7 and 8): adds it to `precipitation`; below snowThreshold
// it goes to `snowPack` (capped 1.0, CH-04; the excess goes to handoff glacierInput on latched glacierEligible cells
// and is discarded elsewhere), otherwise to `waterToGround`. Does not touch `moisture`. amount <= 0 does nothing.
// Reads the cell's own temperature; writes only the cell's own entries.
void RouteCondensate(PassContext& context, uint32_t cell, float amount);

// Pressure-driven push on `cell`'s wind: (1/6) * sum over the 6 directions d of kDirectionVectors[d] * (p[cell] - p[nb]).
// A higher-pressure neighbour pushes the wind away from it, a lower one pulls it in. kNoCell neighbours add nothing
// but still count in the divide by 6. Vectors are y-down world space (same as kDirectionVectors). Unscaled (no
// pressureStrength). Reads `pressure` only.
WindVector PressureGradient(uint32_t cell, const WeatherFields& fields, const GridTables& tables);

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
