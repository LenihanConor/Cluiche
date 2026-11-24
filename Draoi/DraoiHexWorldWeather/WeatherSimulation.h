// A4 weather simulation: owned state, Initialise and seeding, and the weekly Capture / Run / Commit lifecycle.
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-data.md (Feature 1),
//       docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (Feature 2)
#pragma once

#include <DiaParallelExecutor/IParallelExecutor.h>
#include <DraoiHexWorldWeather/WeatherForcing.h>
#include <DraoiHexWorldWeather/WeatherParameters.h>
#include <DraoiHexWorldWeather/WeatherPipeline.h>
#include <DraoiHexWorldWeather/WeatherStepProfile.h>
#include <DraoiHexWorldWeather/WeatherTypes.h>
#include <DraoiHexWorldWeather/WindDirection.h>

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
    // Also builds the grid tables and resets forcings, cell sets and the week counter.
    bool Initialise(const WeatherTerrain& terrain, const WeatherParameters& params, const BiomeRegistry& registry,
                    const SeasonalBaselineInput* baseline = nullptr);

    bool                  IsInitialised() const { return mInitialised; }
    // The committed fields (what the rest of the game reads). Unchanged between Capture and Commit.
    const WeatherFields&  Fields() const { return mBuffers[mCommittedIndex]; }
    const WeatherTerrain& Terrain() const { return mTerrain; }

    // ---- Lifecycle (ca-pipeline.md "Lifecycle") -------------------------------------------------------------------
    // Westerly band and per-cell orographic factor used from the next Capture on. The factor array must have one entry
    // per cell (false, nothing changes, otherwise, or before Initialise).
    void SetWesterlyBand(const WesterlyBand& band) { mPendingBand = band; }
    bool SetOrographicFactor(const Dia::Core::Containers::DynamicArray<float>& factor);

    // Copies committed -> working and latches every input Run reads. No allocation. `baseline` with a length other than
    // the cell count is ignored (the table row is used). `hydro.present` with any array not N long is treated as not
    // present; when not present the latched defaults are: surfaceWetness 0, openWater 1 on water-source cells else 0,
    // glacierEligible 0. Does nothing before a successful Initialise.
    void Capture(const WeatherParameters& params, Season season, uint32_t weekOfYear, const BiomeRegistry& registry,
                 const SeasonalBaselineInput* baseline, const HydrologyInputs& hydro, const InfluenceSnapshot& influences);
    void LatchClimate(uint64_t evalDay);   // Feature 8; no cycles = nothing latched
    // Reads only latched state; writes only the working set, handoff, totals and profile. Never allocates.
    void Run(Dia::ParallelExecutor::IParallelExecutor& executor);
    // Swaps working and committed, ++week, decrements forcing timers and drops the expired ones.
    void Commit();
    void Step(const WeatherParameters& params, Season season, uint32_t weekOfYear, const BiomeRegistry& registry,
              const SeasonalBaselineInput* baseline, const HydrologyInputs& hydro, const InfluenceSnapshot& influences,
              Dia::ParallelExecutor::IParallelExecutor& executor);

    // False when kMaxForcings are active, the cell (or cell-set handle) is out of range, or a non-wind kind has
    // weeksRemaining 0. WindOverride with weeksRemaining 0 lasts until cleared.
    bool     AddForcing(const WeatherForcing& forcing);
    uint32_t ForcingCount() const { return mForcings.Size(); }

    uint64_t GetCommittedWeek() const { return mCommittedWeek; }
    void     SetCommittedWeek(uint64_t week) { mCommittedWeek = week; }

    const WeatherHandoff&     GetHandoff() const { return mHandoff; }
    // Valid from Capture to Commit; the buffers are allocated once at Initialise.
    const WeatherFields&      GetWorkingFields() const { return mBuffers[1 - mCommittedIndex]; }
    // Totals of the last committed step (latched by Commit); the working totals are valid from Run to Commit.
    const WeatherGridTotals&  GetGridTotals() const { return mCommittedTotals; }
    const WeatherGridTotals&  GetWorkingTotals() const { return mTotals; }
    const WeatherStepProfile& GetLastProfile() const { return mProfile; }
    const GridTables&         Tables() const { return mTables; }
    const Dia::Core::Containers::DynamicArray<uint32_t>& Neighbours() const { return mTables.neighbours; }
    const CapturedInputs&     Captured() const { return mCaptured; }

private:
    static void SeedFields(Season season, const WeatherTerrain& terrain, const WeatherParameters& params,
                           const BiomeRegistry& registry, const SeasonalBaselineInput* baseline, WeatherFields& out);

    // Sizes every table, scratch array, working set and latched-input store once. Called last in Initialise.
    void BuildGridTables();

    WeatherTerrain mTerrain;
    WeatherFields  mBuffers[2];          // committed = mBuffers[mCommittedIndex], working = the other; Commit flips the index
    uint32_t       mCommittedIndex = 0;
    WeatherHandoff mHandoff;
    uint64_t       mCommittedWeek = 0;
    bool           mInitialised = false;

    GridTables     mTables;
    CapturedInputs mCaptured;
    Dia::Core::Containers::DynamicArray<WeatherForcing> mForcings;   // live list, capacity kMaxForcings
    CellSetStore   mCellSets;
    WesterlyBand   mPendingBand;
    Dia::Core::Containers::DynamicArray<float> mPendingOrographic;
    WeatherGridTotals  mTotals;            // working, written by Run
    WeatherGridTotals  mCommittedTotals;   // latched from mTotals by Commit
    WeatherStepProfile mProfile;
};

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
