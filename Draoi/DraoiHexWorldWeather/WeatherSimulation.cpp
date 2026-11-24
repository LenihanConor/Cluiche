#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <DiaGeometry2D/Grid/HexTopology.h>
#include <DiaObservation/Log/DiaLog.h>
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

    template <class T>
    void SizeTo(Dia::Core::Containers::DynamicArray<T>& a, uint32_t n, const T& value)
    {
        a.RemoveAll();
        a.Reserve(n);
        for (uint32_t i = 0; i < n; ++i) a.Add(value);
    }

    // Capacity is kept: only reserve when it is too small, so a same-size Capture never reallocates.
    template <class T>
    void Refill(Dia::Core::Containers::DynamicArray<T>& dst, const Dia::Core::Containers::DynamicArray<T>& src)
    {
        dst.RemoveAll();
        if (dst.Capacity() < src.Size()) dst.Reserve(src.Size());
        for (uint32_t i = 0; i < src.Size(); ++i) dst.Add(src[i]);
    }

    // Same-size element copy; no allocation.
    template <class T>
    void CopyInto(Dia::Core::Containers::DynamicArray<T>& dst, const Dia::Core::Containers::DynamicArray<T>& src)
    {
        for (uint32_t i = 0; i < src.Size(); ++i) dst[i] = src[i];
    }

    void CopyFields(WeatherFields& dst, const WeatherFields& src)
    {
        CopyInto(dst.moisture, src.moisture);       CopyInto(dst.temperature, src.temperature);
        CopyInto(dst.pressure, src.pressure);       CopyInto(dst.windDirection, src.windDirection);
        CopyInto(dst.windSpeed, src.windSpeed);     CopyInto(dst.precipitation, src.precipitation);
        CopyInto(dst.cloudCover, src.cloudCover);   CopyInto(dst.waterToGround, src.waterToGround);
        CopyInto(dst.snowPack, src.snowPack);       CopyInto(dst.pressureAnomaly, src.pressureAnomaly);
    }

    bool FieldsMatch(const WeatherFields& f, uint32_t n)
    {
        return f.moisture.Size() == n && f.temperature.Size() == n && f.pressure.Size() == n && f.windDirection.Size() == n
            && f.windSpeed.Size() == n && f.precipitation.Size() == n && f.cloudCover.Size() == n
            && f.waterToGround.Size() == n && f.snowPack.Size() == n && f.pressureAnomaly.Size() == n;
    }

    // The one baseline resolution shared by seeding and Capture: the season's table row by biome, the registry's
    // fallback biome value for an unknown or out-of-range biome, 0.5 for kNoBiome.
    struct BaselineResolver
    {
        const float* row;
        uint32_t     count;
        float        fallbackValue;

        BaselineResolver(Season season, const WeatherParameters& params, const BiomeRegistry& registry)
        {
            count = registry.Count();
            if (count > kMaxBaselineBiomes) count = kMaxBaselineBiomes;
            row = params.BaselineRow(season);
            const BiomeIndex fallback = registry.Role(BiomeRole::Fallback);
            fallbackValue = (fallback != kNoBiome && fallback < count) ? row[fallback] : 0.5f;
        }
        float For(BiomeIndex b) const { return (b < count) ? row[b] : (b == kNoBiome ? 0.5f : fallbackValue); }
    };
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
    const BaselineResolver resolver(season, params, registry);

    for (uint32_t i = 0; i < n; ++i)
    {
        const float base = (baseline != nullptr) ? baseline->temperature[i] : resolver.For(terrain.biome[i]);
        out.temperature[i] = SeasonalTargetTemperature(base, terrain.latitude[i], params.latitudeCoolingFactor);
        out.moisture[i]    = (terrain.waterSource[i] != WaterSource::None) ? params.initialMoistureWater : params.initialMoistureLand;
        out.pressure[i]    = 0.5f;
    }
}

bool WeatherSimulation::Initialise(const WeatherTerrain& terrain, const WeatherParameters& params,
                                   const BiomeRegistry& registry, const SeasonalBaselineInput* baseline)
{
    if (!terrain.IsConsistent())
    {
        DIA_LOG_ERROR("Weather", "WeatherSimulation: Initialise rejected, terrain arrays are inconsistent or empty (%ux%u)",
                      terrain.width, terrain.height);
        return false;
    }

    const uint32_t n = terrain.width * terrain.height;
    bool anyWater = false;
    for (uint32_t i = 0; i < n && !anyWater; ++i) anyWater = (terrain.waterSource[i] != WaterSource::None);
    if (!anyWater)   // EC-01
    {
        DIA_LOG_ERROR("Weather", "WeatherSimulation: Initialise rejected, no water-source cell in %u cells (EC-01)", n);
        return false;
    }

    if (baseline != nullptr && baseline->temperature.Size() != n)
    {
        DIA_LOG_ERROR("Weather", "WeatherSimulation: Initialise rejected, baseline has %u entries for %u cells",
                      baseline->temperature.Size(), n);
        return false;
    }

    // Every failure path is above, so from here on the call cannot fail and the members can be built in place:
    // a rejected call has already returned with the previous state untouched.
    Take(mTerrain.elevation, terrain.elevation);
    Take(mTerrain.latitude, terrain.latitude);
    Take(mTerrain.waterSource, terrain.waterSource);
    Take(mTerrain.biome, terrain.biome);
    mTerrain.width  = terrain.width;
    mTerrain.height = terrain.height;
    mCommittedIndex = 0;
    SeedFields(Season::Spring, mTerrain, params, registry, baseline, mBuffers[mCommittedIndex]);
    mBuffers[1 - mCommittedIndex].Resize(n);   // working set sized identically; Commit flips, never reallocates
    mInitialised = true;
    BuildGridTables();
    return true;
}

void WeatherSimulation::BuildGridTables()
{
    const uint32_t n = mTerrain.width * mTerrain.height;
    mTables.cellCount = n;

    const Dia::Geometry2D::HexTopology topology(static_cast<int>(mTerrain.width), static_cast<int>(mTerrain.height));
    SizeTo(mTables.neighbours, n * Dia::Geometry2D::kHexDirections, Dia::Geometry2D::kNoCell);
    for (uint32_t c = 0; c < n; ++c)
        for (uint32_t d = 0; d < Dia::Geometry2D::kHexDirections; ++d)
            mTables.neighbours[c * Dia::Geometry2D::kHexDirections + d] = topology.NeighbourAt(c, d);

    SizeTo(mTables.moistureOut, n, 0.0f);
    SizeTo(mTables.heatOut, n, 0.0f);
    SizeTo(mTables.anomalyOut, n, 0.0f);
    SizeTo(mTables.dirIndex, n, uint8_t(0));
    SizeTo(mTables.dTemperature, n, 0.0f);
    SizeTo(mTables.dMoisture, n, 0.0f);
    SizeTo(mTables.claimed, n * kForcingKindCount, uint8_t(0));

    mHandoff.Resize(n);

    mForcings.RemoveAll();
    mForcings.Reserve(kMaxForcings);
    SizeTo(mCellSets.cells, n * kMaxCellSets, 0u);
    for (uint32_t s = 0; s < kMaxCellSets; ++s) mCellSets.counts[s] = 0;
    mCellSets.used = 0;

    // Latched-input store, allocated once; Capture only overwrites elements.
    mCaptured.season = Season::Spring;
    mCaptured.weekOfYear = 0;
    SizeTo(mCaptured.seasonBaseline, n, 0.0f);
    SizeTo(mCaptured.orographicFactor, n, 1.0f);
    mCaptured.forcings.RemoveAll();
    mCaptured.forcings.Reserve(kMaxForcings);
    SizeTo(mCaptured.cellSets.cells, n * kMaxCellSets, 0u);
    for (uint32_t s = 0; s < kMaxCellSets; ++s) mCaptured.cellSets.counts[s] = 0;
    mCaptured.cellSets.used = 0;
    mCaptured.hydrology.Resize(n);
    SizeTo(mCaptured.climate.tempOffset, n, 0.0f);
    SizeTo(mCaptured.climate.evapMultiplier, n, 1.0f);
    SizeTo(mCaptured.climate.windShift, n, 0.0f);

    SizeTo(mPendingOrographic, n, 1.0f);
    mPendingBand = WesterlyBand();

    mCommittedWeek = 0;
    mTotals = WeatherGridTotals();
    mCommittedTotals = WeatherGridTotals();
    mProfile = WeatherStepProfile();
}

bool WeatherSimulation::SetOrographicFactor(const Dia::Core::Containers::DynamicArray<float>& factor)
{
    if (!mInitialised || factor.Size() != mPendingOrographic.Size())
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: SetOrographicFactor rejected (initialised %d, %u entries for %u cells)",
                        mInitialised ? 1 : 0, factor.Size(), mPendingOrographic.Size());
        return false;
    }
    CopyInto(mPendingOrographic, factor);
    return true;
}

void WeatherSimulation::Capture(const WeatherParameters& params, Season season, uint32_t weekOfYear,
                                const BiomeRegistry& registry, const SeasonalBaselineInput* baseline,
                                const HydrologyInputs& hydro, const InfluenceSnapshot& influences)
{
    if (!mInitialised)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: Capture ignored, not initialised");
        return;
    }
    const uint32_t n = mTables.cellCount;

    CopyFields(mBuffers[1 - mCommittedIndex], mBuffers[mCommittedIndex]);

    mCaptured.params = params;
    mCaptured.season = season;
    mCaptured.weekOfYear = weekOfYear;
    mCaptured.westerlyBand = mPendingBand;
    mCaptured.influences = influences;
    CopyInto(mCaptured.orographicFactor, mPendingOrographic);

    // Baseline: per-cell input wins (when the right length), else the table row by biome.
    const bool useInput = (baseline != nullptr && baseline->temperature.Size() == n);
    if (baseline != nullptr && !useInput)
        DIA_LOG_WARNING("Weather", "WeatherSimulation: Capture ignored per-cell baseline (%u entries for %u cells), using the table",
                        baseline->temperature.Size(), n);
    const BaselineResolver resolver(season, params, registry);
    for (uint32_t c = 0; c < n; ++c)
        mCaptured.seasonBaseline[c] = useInput ? baseline->temperature[c] : resolver.For(mTerrain.biome[c]);

    // Forcings and cell sets.
    Refill(mCaptured.forcings, mForcings);
    mCaptured.cellSets.used = mCellSets.used;
    for (uint32_t s = 0; s < kMaxCellSets; ++s)
    {
        mCaptured.cellSets.counts[s] = mCellSets.counts[s];
        for (uint32_t i = 0; i < mCellSets.counts[s]; ++i)
            mCaptured.cellSets.cells[s * n + i] = mCellSets.cells[s * n + i];
    }

    // Hydrology: a present input must carry N entries in every array, else it counts as absent.
    HydrologyInputs& h = mCaptured.hydrology;
    const bool hydroOk = hydro.present && hydro.surfaceWetness.Size() == n && hydro.openWater.Size() == n
                      && hydro.glacierEligible.Size() == n;
    if (hydro.present && !hydroOk)
        DIA_LOG_WARNING("Weather", "WeatherSimulation: Capture treats hydrology as absent, array sizes do not match %u cells", n);
    h.present = hydroOk;
    h.glacierCheckWeek = hydro.glacierCheckWeek;
    h.glacierSnowFloor = hydro.glacierSnowFloor;
    for (uint32_t c = 0; c < n; ++c)
    {
        if (hydroOk)
        {
            h.surfaceWetness[c]  = hydro.surfaceWetness[c];
            h.openWater[c]       = hydro.openWater[c];
            h.glacierEligible[c] = hydro.glacierEligible[c];
        }
        else
        {
            h.surfaceWetness[c]  = 0.0f;
            h.openWater[c]       = (mTerrain.waterSource[c] != WaterSource::None) ? 1.0f : 0.0f;
            h.glacierEligible[c] = 0;
        }
    }

    // Climate offsets reset to their neutral defaults; Feature 8's LatchClimate overwrites them.
    for (uint32_t c = 0; c < n; ++c)
    {
        mCaptured.climate.tempOffset[c] = 0.0f;
        mCaptured.climate.evapMultiplier[c] = 1.0f;
        mCaptured.climate.windShift[c] = 0.0f;
    }
}

void WeatherSimulation::LatchClimate(uint64_t)
{
    // Feature 8.
}

void WeatherSimulation::Run(Dia::ParallelExecutor::IParallelExecutor& executor)
{
    if (!mInitialised)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: Run ignored, not initialised");
        return;
    }
    PassContext context;
    context.terrain = &mTerrain;
    context.tables = &mTables;
    context.inputs = &mCaptured;
    context.fields = &mBuffers[1 - mCommittedIndex];
    context.handoff = &mHandoff;
    context.totals = &mTotals;
    mProfile = WeatherStepProfile();   // the profile covers the last Run only
    WeatherProfileAccumulator timing;
    context.profile = &mProfile;
    context.timing = &timing;
    RunPipeline(context, executor);
    uint64_t totalCpu = 0;
    for (uint32_t i = 0; i < WeatherStepProfile::kPassCount; ++i)
    {
        mProfile.cpuMicros[i] = timing.cpuMicros[i].load(std::memory_order_relaxed);
        totalCpu += mProfile.cpuMicros[i];
    }
    mProfile.totalCpuMicros = totalCpu;
    mProfile.lastRangeCount = executor.GetLastRangeCount();
}

void WeatherSimulation::Commit()
{
    if (!mInitialised)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: Commit ignored, not initialised");
        return;
    }
    mCommittedIndex = 1 - mCommittedIndex;
    ++mCommittedWeek;
    mCommittedTotals = mTotals;

    // Decrement and compact in record order. WindOverride with weeksRemaining 0 lasts until cleared.
    uint32_t keep = 0;
    for (uint32_t i = 0; i < mForcings.Size(); ++i)
    {
        WeatherForcing f = mForcings[i];
        if (f.weeksRemaining != 0)
        {
            --f.weeksRemaining;
            if (f.weeksRemaining == 0) continue;   // expired
        }
        mForcings[keep++] = f;
    }
    while (mForcings.Size() > keep) mForcings.Remove();
}

void WeatherSimulation::Step(const WeatherParameters& params, Season season, uint32_t weekOfYear,
                             const BiomeRegistry& registry, const SeasonalBaselineInput* baseline,
                             const HydrologyInputs& hydro, const InfluenceSnapshot& influences,
                             Dia::ParallelExecutor::IParallelExecutor& executor)
{
    Capture(params, season, weekOfYear, registry, baseline, hydro, influences);
    Run(executor);
    Commit();
}

CellSetHandle WeatherSimulation::RegisterCellSet(const Dia::Core::Containers::DynamicArray<uint32_t>& sortedCells)
{
    if (!mInitialised || mCellSets.used >= kMaxCellSets)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: RegisterCellSet rejected (initialised %d, %u of %u sets used)",
                        mInitialised ? 1 : 0, mCellSets.used, kMaxCellSets);
        return kNoCellSet;
    }
    const uint32_t n = mTables.cellCount;
    const uint32_t count = sortedCells.Size();
    if (count == 0 || count > n)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: RegisterCellSet rejected, %u cells for a %u-cell grid", count, n);
        return kNoCellSet;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        if (sortedCells[i] >= n)
        {
            DIA_LOG_WARNING("Weather", "WeatherSimulation: RegisterCellSet rejected, cell %u out of range (%u cells)", sortedCells[i], n);
            return kNoCellSet;
        }
        if (i > 0 && sortedCells[i] <= sortedCells[i - 1])   // unsorted or duplicate
        {
            DIA_LOG_WARNING("Weather", "WeatherSimulation: RegisterCellSet rejected, list unsorted or duplicated at index %u", i);
            return kNoCellSet;
        }
    }
    const uint32_t handle = mCellSets.used;
    for (uint32_t i = 0; i < count; ++i) mCellSets.cells[handle * n + i] = sortedCells[i];
    mCellSets.counts[handle] = count;
    ++mCellSets.used;
    return static_cast<CellSetHandle>(handle);
}

void WeatherSimulation::ClearCellSets()
{
    for (uint32_t s = 0; s < kMaxCellSets; ++s) mCellSets.counts[s] = 0;
    mCellSets.used = 0;
    uint32_t keep = 0;
    for (uint32_t i = 0; i < mForcings.Size(); ++i)
        if (mForcings[i].cellSet == kNoCellSet) mForcings[keep++] = mForcings[i];
    while (mForcings.Size() > keep) mForcings.Remove();
}

bool WeatherSimulation::AddForcing(const WeatherForcing& forcing)
{
    if (!mInitialised || mForcings.Size() >= kMaxForcings)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: AddForcing rejected (initialised %d, %u of %u forcings active)",
                        mInitialised ? 1 : 0, mForcings.Size(), kMaxForcings);
        return false;
    }
    if (forcing.cellSet == kNoCellSet)
    {
        if (forcing.cell >= mTables.cellCount)
        {
            DIA_LOG_WARNING("Weather", "WeatherSimulation: AddForcing rejected, cell %u out of range (%u cells)", forcing.cell, mTables.cellCount);
            return false;
        }
    }
    else if (forcing.cellSet >= mCellSets.used)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: AddForcing rejected, cell set %u not registered (%u sets)",
                        static_cast<unsigned>(forcing.cellSet), mCellSets.used);
        return false;
    }
    if (forcing.kind != ForcingKind::WindOverride && forcing.weeksRemaining == 0)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: AddForcing rejected, kind %u needs weeksRemaining >= 1",
                        static_cast<unsigned>(forcing.kind));
        return false;
    }
    mForcings.Add(forcing);
    return true;
}

void WeatherSimulation::ClearWindOverrides()
{
    uint32_t keep = 0;
    for (uint32_t i = 0; i < mForcings.Size(); ++i)
    {
        if (mForcings[i].kind == ForcingKind::WindOverride) continue;
        if (keep != i) mForcings[keep] = mForcings[i];
        ++keep;
    }
    while (mForcings.Size() > keep) mForcings.Remove();
}

bool WeatherSimulation::SetCellBiome(uint32_t cell, BiomeIndex biome)
{
    if (!mInitialised || cell >= mTerrain.biome.Size())
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: SetCellBiome rejected (initialised %d, cell %u)", mInitialised ? 1 : 0, cell);
        return false;
    }
    mTerrain.biome[cell] = biome;
    return true;
}

bool WeatherSimulation::RestoreCommitted(uint64_t week, const WeatherFields& fields,
                                         const Dia::Core::Containers::DynamicArray<BiomeIndex>& biomes)
{
    const uint32_t n = mTables.cellCount;
    if (!mInitialised || !FieldsMatch(fields, n) || biomes.Size() != n)
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: RestoreCommitted rejected (initialised %d, %u biomes for %u cells)",
                        mInitialised ? 1 : 0, biomes.Size(), n);
        return false;
    }
    CopyFields(mBuffers[mCommittedIndex], fields);
    for (uint32_t c = 0; c < n; ++c) mTerrain.biome[c] = biomes[c];
    mCommittedWeek = week;
    return true;
}

bool WeatherSimulation::RestoreCompleted(const WeatherFields& fields)
{
    if (!mInitialised || !FieldsMatch(fields, mTables.cellCount))
    {
        DIA_LOG_WARNING("Weather", "WeatherSimulation: RestoreCompleted rejected (initialised %d, cells %u)",
                        mInitialised ? 1 : 0, mTables.cellCount);
        return false;
    }
    CopyFields(mBuffers[1 - mCommittedIndex], fields);
    return true;
}

} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
