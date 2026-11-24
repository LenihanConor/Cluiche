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
    Array<uint8_t>  claimed;                          // kForcingKindCount * N, lowest-record-wins
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

// The ordered pass sequence (0, 1, 2, 3, 4, 5a..6b, 7, 8, 9, finish). Empty until the pass tasks fill it.
void RunPipeline(PassContext& context, Dia::ParallelExecutor::IParallelExecutor& executor);

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
