// A4 weather ca-pipeline Task 3: grid tables and the Capture / Run / Commit / Step lifecycle (no passes yet).
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 1, 3, 4, 5 and the setup half of 6)
#include <gtest/gtest.h>

#include <DiaGeometry2D/Grid/HexTopology.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;
    using Dia::Geometry2D::kNoCell;
    using Dia::Geometry2D::kHexDirections;

    using FloatArray = Dia::Core::Containers::DynamicArray<float>;

    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;

        explicit Rig(const WeatherTerrain& terrain) { EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry())); }

        void Capture(Season s = Season::Spring, uint32_t week = 0, const SeasonalBaselineInput* b = nullptr)
        {
            sim.Capture(params, s, week, WT::TestRegistry(), b, hydro, influences);
        }
        void Step(Season s = Season::Spring, uint32_t week = 0)
        {
            sim.Step(params, s, week, WT::TestRegistry(), nullptr, hydro, influences, serial);
        }
        // The caller-visible working set is read-only; tests poke it to prove commit/capture isolation.
        WeatherFields& MutableWorking() { return const_cast<WeatherFields&>(sim.GetWorkingFields()); }
    };

    WeatherForcing Forcing(ForcingKind k, uint32_t cell, uint16_t weeks)
    {
        WeatherForcing f;
        f.kind = k;
        f.cell = cell;
        f.value = 0.5f;
        f.value2 = 0.25f;
        f.weeksRemaining = weeks;
        return f;
    }
}

// ---- Grid tables ------------------------------------------------------------------------------------------------

TEST(WeatherLifecycle, NeighbourTableMatchesHexTopologyOnOddAndEvenGrids)
{
    const uint32_t sizes[][2] = { {1, 1}, {2, 2}, {3, 3}, {4, 5}, {5, 4}, {6, 5}, {7, 6}, {8, 3} };
    for (const auto& s : sizes)
    {
        Rig rig(WT::MakeHexTerrain(s[0], s[1]));
        const Dia::Geometry2D::HexTopology topology(static_cast<int>(s[0]), static_cast<int>(s[1]));
        const uint32_t n = s[0] * s[1];
        ASSERT_EQ(rig.sim.Neighbours().Size(), n * kHexDirections) << s[0] << "x" << s[1];
        bool sawEdge = false;
        for (uint32_t c = 0; c < n; ++c)
            for (uint32_t d = 0; d < kHexDirections; ++d)
            {
                const uint32_t expected = topology.NeighbourAt(c, d);
                ASSERT_EQ(rig.sim.Neighbours()[c * kHexDirections + d], expected) << s[0] << "x" << s[1] << " cell " << c << " dir " << d;
                sawEdge = sawEdge || expected == kNoCell;
            }
        EXPECT_TRUE(sawEdge) << "every grid has edge cells";
    }
}

TEST(WeatherLifecycle, ScratchAndStorageSizedAtInitialise)
{
    Rig rig(WT::MakeHexTerrain(7, 5));
    const uint32_t n = 35;
    const GridTables& t = rig.sim.Tables();
    EXPECT_EQ(t.cellCount, n);
    EXPECT_EQ(t.moistureOut.Size(), n);
    EXPECT_EQ(t.heatOut.Size(), n);
    EXPECT_EQ(t.anomalyOut.Size(), n);
    EXPECT_EQ(t.dirIndex.Size(), n);
    EXPECT_EQ(t.dTemperature.Size(), n);
    EXPECT_EQ(t.dMoisture.Size(), n);
    EXPECT_EQ(t.claimed.Size(), n * kForcingKindCount);
    EXPECT_EQ(rig.sim.GetHandoff().influenceWater.Size(), n);
    EXPECT_EQ(rig.sim.GetHandoff().glacierInput.Size(), n);
    EXPECT_EQ(rig.sim.GetWorkingFields().CellCount(), n);
    EXPECT_EQ(rig.sim.Fields().CellCount(), n);
    EXPECT_EQ(rig.sim.Captured().seasonBaseline.Size(), n);
    EXPECT_EQ(rig.sim.Captured().orographicFactor.Size(), n);
    EXPECT_EQ(rig.sim.Captured().climate.windShift.Size(), n);
    EXPECT_EQ(rig.sim.Captured().cellSets.cells.Size(), n * kMaxCellSets);
}

TEST(WeatherLifecycle, ReinitialiseRebuildsTables)
{
    Rig rig(WT::MakeHexTerrain(4, 4));
    EXPECT_EQ(rig.sim.Neighbours().Size(), 16u * 6);
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 1, 3)));
    rig.Step();
    ASSERT_TRUE(rig.sim.Initialise(WT::MakeHexTerrain(6, 3), rig.params, WT::TestRegistry()));
    EXPECT_EQ(rig.sim.Neighbours().Size(), 18u * 6);
    EXPECT_EQ(rig.sim.Tables().moistureOut.Size(), 18u);
    EXPECT_EQ(rig.sim.GetWorkingFields().CellCount(), 18u);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 0u);
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
}

TEST(WeatherLifecycle, FailedInitialiseLeavesTablesAndStateUnchanged)
{
    Rig rig(WT::MakeHexTerrain(5, 4));
    rig.Step();
    WT::HexTerrainOptions noWater;
    noWater.waterColumns = 0;
    EXPECT_FALSE(rig.sim.Initialise(WT::MakeHexTerrain(9, 9, noWater), rig.params, WT::TestRegistry()));
    EXPECT_EQ(rig.sim.Neighbours().Size(), 20u * 6);
    EXPECT_EQ(rig.sim.Tables().cellCount, 20u);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 1u);
    EXPECT_EQ(rig.sim.GetWorkingFields().CellCount(), 20u);
}

// ---- Capture / Run / Commit ---------------------------------------------------------------------------------------

TEST(WeatherLifecycle, CommittedFieldsUnchangedBetweenCaptureAndCommit)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    rig.Capture();
    WeatherFields before;
    WT::CloneFields(rig.sim.Fields(), before);

    for (int i = 0; i < 3; ++i)
    {
        rig.MutableWorking().temperature[3] = 0.9f + 0.01f * float(i);   // however the working set changes
        rig.MutableWorking().moisture[0] = 0.0f;
        rig.sim.Run(rig.serial);
        EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), before)) << "after Run " << i;
    }
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 0u);
}

TEST(WeatherLifecycle, CaptureCopiesCommittedIntoWorking)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    rig.MutableWorking().temperature[2] = 0.77f;   // stale working data
    rig.Capture();
    EXPECT_TRUE(WT::SameFields(rig.sim.GetWorkingFields(), rig.sim.Fields()));
}

TEST(WeatherLifecycle, CommitSwapsWorkingIntoCommittedAndIncrementsWeek)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    rig.Capture();
    rig.MutableWorking().temperature[5] = 0.123f;
    const float* workingData = &rig.sim.GetWorkingFields().temperature[0u];
    const float* committedData = &rig.sim.Fields().temperature[0u];
    rig.sim.Commit();
    EXPECT_FLOAT_EQ(rig.sim.Fields().temperature[5], 0.123f);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 1u);
    // A swap, not a copy: the buffers exchanged roles.
    EXPECT_EQ(&rig.sim.Fields().temperature[0u], workingData);
    EXPECT_EQ(&rig.sim.GetWorkingFields().temperature[0u], committedData);
}

TEST(WeatherLifecycle, SetCommittedWeekOverwritesCounter)
{
    Rig rig(WT::MakeHexTerrain(4, 4));
    rig.sim.SetCommittedWeek(40);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 40u);
    rig.Step();
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 41u);
}

TEST(WeatherLifecycle, CaptureTwiceIsIdempotentOnCommitted)
{
    Rig a(WT::MakeHexTerrain(6, 4));
    Rig b(WT::MakeHexTerrain(6, 4));
    a.Capture(Season::Summer, 14);
    a.Capture(Season::Summer, 14);
    a.sim.Run(a.serial);   // Run relaxes the working set in place, so it is once per Capture
    a.sim.Commit();

    b.Capture(Season::Summer, 14);
    b.sim.Run(b.serial);
    b.sim.Commit();

    EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields()));
    EXPECT_EQ(a.sim.GetCommittedWeek(), b.sim.GetCommittedWeek());
}

TEST(WeatherLifecycle, StepEqualsCaptureRunCommitBitForBit)
{
    Rig a(WT::MakeHexTerrain(7, 5));
    Rig b(WT::MakeHexTerrain(7, 5));
    ASSERT_TRUE(a.sim.AddForcing(Forcing(ForcingKind::Rain, 3, 2)));
    ASSERT_TRUE(b.sim.AddForcing(Forcing(ForcingKind::Rain, 3, 2)));
    for (uint32_t w = 0; w < 4; ++w)
    {
        a.Step(Season::Autumn, w);
        b.Capture(Season::Autumn, w);
        b.sim.Run(b.serial);
        b.sim.Commit();
        EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields())) << "week " << w;
        EXPECT_TRUE(WT::SameFields(a.sim.GetWorkingFields(), b.sim.GetWorkingFields())) << "week " << w;
        EXPECT_EQ(a.sim.GetCommittedWeek(), b.sim.GetCommittedWeek());
        EXPECT_EQ(a.sim.ForcingCount(), b.sim.ForcingCount());
    }
}

TEST(WeatherLifecycle, RunWorksWithAnyExecutor)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    WT::FixedRangeExecutor three(3);
    WT::ReverseRangeExecutor reverse;
    rig.Capture();
    rig.sim.Run(three);
    rig.sim.Run(reverse);
    rig.sim.Commit();
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 1u);
}

TEST(WeatherLifecycle, LifecycleCallsBeforeInitialiseAreSafeNoOps)
{
    WeatherSimulation sim;
    WeatherParameters params = WT::MakeTestParams();
    HydrologyInputs hydro;
    InfluenceSnapshot influences;
    Dia::ParallelExecutor::SerialExecutor serial;
    sim.Step(params, Season::Spring, 0, WT::TestRegistry(), nullptr, hydro, influences, serial);
    EXPECT_EQ(sim.GetCommittedWeek(), 0u);
    EXPECT_FALSE(sim.AddForcing(Forcing(ForcingKind::Rain, 0, 1)));
}

TEST(WeatherLifecycle, RunAndCommitDoNotReallocateBuffers)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    const float* a = &rig.sim.Fields().temperature[0u];
    const float* b = &rig.sim.GetWorkingFields().temperature[0u];
    const uint32_t* nb = &rig.sim.Neighbours()[0u];
    const float* scratch = &rig.sim.Tables().moistureOut[0u];
    const uint32_t cap = rig.sim.Neighbours().Capacity();
    for (int i = 0; i < 5; ++i) rig.Step(Season::Winter, uint32_t(i));
    const float* c = &rig.sim.Fields().temperature[0u];
    const float* d = &rig.sim.GetWorkingFields().temperature[0u];
    EXPECT_TRUE((c == a && d == b) || (c == b && d == a));
    EXPECT_EQ(&rig.sim.Neighbours()[0u], nb);
    EXPECT_EQ(&rig.sim.Tables().moistureOut[0u], scratch);
    EXPECT_EQ(rig.sim.Neighbours().Capacity(), cap);
}

// ---- Forcings ----------------------------------------------------------------------------------------------------

TEST(WeatherLifecycle, ForcingExpiresAfterItsWeeks)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 2, 3)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
}

TEST(WeatherLifecycle, ForcingsKeepOrderWhenSomeExpire)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::PressureHold, 1, 5)));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::MoistureHold, 2, 1)));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 3, 5)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 2u);
    rig.Capture();   // the latched copy keeps record order
    EXPECT_EQ(rig.sim.Captured().forcings.Size(), 2u);
    EXPECT_EQ(rig.sim.Captured().forcings[0u].kind, ForcingKind::PressureHold);
    EXPECT_EQ(rig.sim.Captured().forcings[0u].weeksRemaining, 4u);
    EXPECT_EQ(rig.sim.Captured().forcings[1u].kind, ForcingKind::Rain);
}

TEST(WeatherLifecycle, WindOverrideWithZeroWeeksPersists)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::WindOverride, 4, 0)));
    for (int i = 0; i < 10; ++i) rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
}

TEST(WeatherLifecycle, WindOverrideWithWeeksExpiresInsteadOfBecomingPermanent)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::WindOverride, 4, 2)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
}

TEST(WeatherLifecycle, AddForcingFailsAtSixtyFourAndDropsNothing)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    for (uint32_t i = 0; i < kMaxForcings; ++i)
        ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::MoistureHold, i % 24, 5)));
    EXPECT_FALSE(rig.sim.AddForcing(Forcing(ForcingKind::MoistureHold, 0, 5)));
    EXPECT_EQ(rig.sim.ForcingCount(), kMaxForcings);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), kMaxForcings);
}

TEST(WeatherLifecycle, AddForcingRejectsBadCellAndZeroWeeksForNonWind)
{
    Rig rig(WT::MakeHexTerrain(6, 4));
    EXPECT_FALSE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 24, 2)));
    EXPECT_FALSE(rig.sim.AddForcing(Forcing(ForcingKind::PressureHold, 0, 0)));
    EXPECT_FALSE(rig.sim.AddForcing(Forcing(ForcingKind::MoistureHold, 0, 0)));
    EXPECT_FALSE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 0, 0)));
    WeatherForcing badSet = Forcing(ForcingKind::PressureHold, 0, 2);
    badSet.cellSet = kMaxCellSets;
    EXPECT_FALSE(rig.sim.AddForcing(badSet));
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
    EXPECT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 23, 1)));
}

// ---- Latched inputs ---------------------------------------------------------------------------------------------

TEST(WeatherLifecycle, CaptureLatchesSeasonWeekAndParameters)
{
    Rig rig(WT::MakeHexTerrain(4, 4));
    rig.params.rainThreshold = 0.77f;
    rig.Capture(Season::Winter, 41);
    rig.params.rainThreshold = 0.0f;   // poisoning the caller's copy later changes nothing
    EXPECT_EQ(rig.sim.Captured().season, Season::Winter);
    EXPECT_EQ(rig.sim.Captured().weekOfYear, 41u);
    EXPECT_FLOAT_EQ(rig.sim.Captured().params.rainThreshold, 0.77f);
}

TEST(WeatherLifecycle, CaptureLatchesForcingCopy)
{
    Rig rig(WT::MakeHexTerrain(4, 4));
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 2, 4)));
    rig.Capture();
    ASSERT_TRUE(rig.sim.AddForcing(Forcing(ForcingKind::Rain, 3, 4)));   // after Capture: not in the latched copy
    EXPECT_EQ(rig.sim.Captured().forcings.Size(), 1u);
    EXPECT_EQ(rig.sim.Captured().forcings[0u].cell, 2u);
}

TEST(WeatherLifecycle, BaselineLatchedPerCellFromTableByBiome)
{
    // Distinct biome per cell, including out of range and kNoBiome.
    WeatherTerrain t = WT::MakeHexTerrain(4, 3);
    for (uint32_t i = 0; i < 12; ++i) t.biome[i] = BiomeIndex(i % 5);
    t.biome[10] = 200;       // out of range -> registry fallback value
    t.biome[11] = kNoBiome;  // 0.5
    Rig rig(t);
    rig.Capture(Season::Summer, 20);

    const float* row = rig.params.BaselineRow(Season::Summer);
    const BiomeIndex fallback = WT::TestRegistry().Role(BiomeRole::Fallback);
    ASSERT_NE(fallback, kNoBiome);
    const FloatArray& base = rig.sim.Captured().seasonBaseline;
    for (uint32_t i = 0; i < 10; ++i) EXPECT_FLOAT_EQ(base[i], row[i % 5]) << "cell " << i;
    EXPECT_FLOAT_EQ(base[10], row[fallback]);
    EXPECT_FLOAT_EQ(base[11], 0.5f);
}

TEST(WeatherLifecycle, PerCellBaselineInputWinsOverTable)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    SeasonalBaselineInput input;
    input.temperature.Reserve(12);
    for (uint32_t i = 0; i < 12; ++i) input.temperature.Add(0.01f * float(i + 1));
    rig.Capture(Season::Spring, 0, &input);
    for (uint32_t i = 0; i < 12; ++i) EXPECT_FLOAT_EQ(rig.sim.Captured().seasonBaseline[i], 0.01f * float(i + 1));

    input.temperature[0u] = 9.0f;   // mutating the caller's input after Capture changes nothing
    EXPECT_FLOAT_EQ(rig.sim.Captured().seasonBaseline[0u], 0.01f);
}

TEST(WeatherLifecycle, WrongLengthBaselineInputFallsBackToTable)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    SeasonalBaselineInput input;
    input.temperature.Reserve(5);
    for (uint32_t i = 0; i < 5; ++i) input.temperature.Add(0.9f);
    rig.Capture(Season::Spring, 0, &input);
    EXPECT_FLOAT_EQ(rig.sim.Captured().seasonBaseline[0u], rig.params.BaselineRow(Season::Spring)[2]);
}

TEST(WeatherLifecycle, BaselineFollowsTheSeasonAtEachCapture)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    rig.Capture(Season::Spring);
    EXPECT_FLOAT_EQ(rig.sim.Captured().seasonBaseline[5u], rig.params.baselineSpring[2]);
    rig.Capture(Season::Winter);
    EXPECT_FLOAT_EQ(rig.sim.Captured().seasonBaseline[5u], rig.params.baselineWinter[2]);
}

TEST(WeatherLifecycle, HydrologyDefaultsWhenNotPresent)
{
    Rig rig(WT::MakeHexTerrain(4, 3));   // column 0 is water
    rig.Capture();
    const HydrologyInputs& h = rig.sim.Captured().hydrology;
    ASSERT_EQ(h.openWater.Size(), 12u);
    EXPECT_FALSE(h.present);
    EXPECT_FLOAT_EQ(h.openWater[0u], 1.0f);
    EXPECT_FLOAT_EQ(h.openWater[1u], 0.0f);
    EXPECT_FLOAT_EQ(h.surfaceWetness[0u], 0.0f);
    EXPECT_EQ(h.glacierEligible[3u], 0);
}

TEST(WeatherLifecycle, HydrologyPresentIsLatchedWhenSizedCorrectly)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    rig.hydro.Resize(12);
    rig.hydro.present = true;
    rig.hydro.surfaceWetness[4u] = 0.6f;
    rig.hydro.openWater[4u] = 0.25f;
    rig.hydro.glacierEligible[4u] = 1;
    rig.hydro.glacierCheckWeek = 30;
    rig.hydro.glacierSnowFloor = 0.4f;
    rig.Capture();
    const HydrologyInputs& h = rig.sim.Captured().hydrology;
    EXPECT_TRUE(h.present);
    EXPECT_FLOAT_EQ(h.surfaceWetness[4u], 0.6f);
    EXPECT_FLOAT_EQ(h.openWater[4u], 0.25f);
    EXPECT_EQ(h.glacierEligible[4u], 1);
    EXPECT_EQ(h.glacierCheckWeek, 30u);
    EXPECT_FLOAT_EQ(h.glacierSnowFloor, 0.4f);
}

TEST(WeatherLifecycle, HydrologyPresentButMismatchedCountsAsAbsent)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    rig.hydro.Resize(5);
    rig.hydro.present = true;
    rig.hydro.surfaceWetness[0u] = 0.9f;
    rig.Capture();
    const HydrologyInputs& h = rig.sim.Captured().hydrology;
    EXPECT_FALSE(h.present);
    EXPECT_EQ(h.surfaceWetness.Size(), 12u);
    EXPECT_FLOAT_EQ(h.surfaceWetness[0u], 0.0f);
}

TEST(WeatherLifecycle, OrographicFactorAndBandTakeEffectAtNextCapture)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    rig.Capture();
    EXPECT_FLOAT_EQ(rig.sim.Captured().orographicFactor[3u], 1.0f);   // default

    FloatArray factor;
    factor.Reserve(12);
    for (uint32_t i = 0; i < 12; ++i) factor.Add(2.0f);
    EXPECT_TRUE(rig.sim.SetOrographicFactor(factor));
    WesterlyBand band;
    band.tradeEnd = 0.1f;
    rig.sim.SetWesterlyBand(band);
    EXPECT_FLOAT_EQ(rig.sim.Captured().orographicFactor[3u], 1.0f);   // not yet
    EXPECT_FLOAT_EQ(rig.sim.Captured().westerlyBand.tradeEnd, 0.25f);

    rig.Capture();
    EXPECT_FLOAT_EQ(rig.sim.Captured().orographicFactor[3u], 2.0f);
    EXPECT_FLOAT_EQ(rig.sim.Captured().westerlyBand.tradeEnd, 0.1f);
}

TEST(WeatherLifecycle, OrographicFactorWrongLengthRejected)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    FloatArray factor;
    factor.Reserve(5);
    for (uint32_t i = 0; i < 5; ++i) factor.Add(3.0f);
    EXPECT_FALSE(rig.sim.SetOrographicFactor(factor));
    rig.Capture();
    EXPECT_FLOAT_EQ(rig.sim.Captured().orographicFactor[0u], 1.0f);

    WeatherSimulation uninitialised;
    EXPECT_FALSE(uninitialised.SetOrographicFactor(factor));
}

TEST(WeatherLifecycle, ClimateOffsetsLatchNeutralDefaults)
{
    Rig rig(WT::MakeHexTerrain(4, 3));
    rig.Capture();
    const ClimateOffsets& c = rig.sim.Captured().climate;
    ASSERT_EQ(c.tempOffset.Size(), 12u);
    EXPECT_FLOAT_EQ(c.tempOffset[7u], 0.0f);
    EXPECT_FLOAT_EQ(c.evapMultiplier[7u], 1.0f);
    EXPECT_FLOAT_EQ(c.windShift[7u], 0.0f);
}

TEST(WeatherLifecycle, FixtureParamsHoldGddDefaults)
{
    const WeatherParameters p = WT::MakeTestParams();
    EXPECT_FLOAT_EQ(p.latitudeCoolingFactor, 0.3f);
    EXPECT_FLOAT_EQ(p.cloudDecay, 0.5f);
    EXPECT_FLOAT_EQ(p.precipRate, 0.5f);
    EXPECT_TRUE(p.edgeInflow);
    EXPECT_EQ(p.influenceMaxRadius, 32);
    EXPECT_FLOAT_EQ(p.baselineSummer[5], 0.95f);
}
