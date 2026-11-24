// A4 weather ca-pipeline Task 13: edge-case tests found by the AC gap audit (degenerate grids, forcing extremes,
// lifecycle misuse that must stay safe, and long-run range checks).
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
#include <gtest/gtest.h>

#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <cmath>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;

    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;

        explicit Rig(const WeatherTerrain& terrain) { EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry())); }

        void Capture(Season s = Season::Spring, uint32_t week = 0)
        {
            sim.Capture(params, s, week, WT::TestRegistry(), nullptr, hydro, influences);
        }
        void Step(Season s = Season::Spring, uint32_t week = 0)
        {
            sim.Step(params, s, week, WT::TestRegistry(), nullptr, hydro, influences, serial);
        }
        void StepWith(Dia::ParallelExecutor::IParallelExecutor& ex, Season s, uint32_t week)
        {
            sim.Step(params, s, week, WT::TestRegistry(), nullptr, hydro, influences, ex);
        }
    };

    bool InRange(float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; }

    // Every field of every cell is finite and inside its documented range.
    ::testing::AssertionResult AllInRange(const WeatherFields& f)
    {
        for (uint32_t c = 0; c < f.CellCount(); ++c)
        {
            if (!InRange(f.moisture[c], 0.0f, 1.0f) || !InRange(f.temperature[c], 0.0f, 1.0f)
                || !InRange(f.pressure[c], 0.0f, 1.0f) || !InRange(f.windSpeed[c], 0.0f, 1.0f)
                || !InRange(f.precipitation[c], 0.0f, 1.0f) || !InRange(f.cloudCover[c], 0.0f, 1.0f)
                || !InRange(f.waterToGround[c], 0.0f, 2.0f) || !InRange(f.snowPack[c], 0.0f, 1.0f)
                || !InRange(f.pressureAnomaly[c], -0.5f, 0.5f) || !std::isfinite(f.windDirection[c]))
                return ::testing::AssertionFailure() << "cell " << c << " out of range";
        }
        return ::testing::AssertionSuccess();
    }

    WeatherForcing Hold(uint32_t cell, float value, uint16_t weeks)
    {
        WeatherForcing f;
        f.kind = ForcingKind::PressureHold;
        f.cell = cell;
        f.value = value;
        f.weeksRemaining = weeks;
        return f;
    }

    CellSetHandle RegisterAll(WeatherSimulation& sim, uint32_t n)
    {
        Dia::Core::Containers::DynamicArray<uint32_t> all;
        all.Reserve(n);
        for (uint32_t i = 0; i < n; ++i) all.Add(i);
        return sim.RegisterCellSet(all);
    }
}

// ---- Degenerate grids -------------------------------------------------------------------------------------------

TEST(WeatherEdges, OneByOneGridStepsForAYearInRange)
{
    Rig rig(WT::MakeHexTerrain(1, 1));
    for (uint32_t w = 0; w < 52; ++w) rig.Step(static_cast<Season>((w / 13) % 4), w);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 52u);
    EXPECT_TRUE(AllInRange(rig.sim.Fields()));
}

TEST(WeatherEdges, GridSmallerThanMinRangeMatchesOneRangePerCellAndReverse)
{
    const WeatherTerrain terrain = WT::MakeHexTerrain(3, 3);
    Rig a(terrain), b(terrain), c(terrain);
    WT::FixedRangeExecutor perCell(9);
    WT::ReverseRangeExecutor reverse(2);
    for (uint32_t w = 0; w < 20; ++w)
    {
        a.Step(Season::Summer, w);
        b.StepWith(perCell, Season::Summer, w);
        c.StepWith(reverse, Season::Summer, w);
    }
    EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields()));
    EXPECT_TRUE(WT::SameFields(a.sim.Fields(), c.sim.Fields()));
}

TEST(WeatherEdges, SingleColumnAndSingleRowGridsStayInRangeAndExecutorIndependent)
{
    const uint32_t dims[][2] = { {1, 8}, {8, 1}, {1, 2}, {2, 1} };
    for (const auto& d : dims)
    {
        const WeatherTerrain terrain = WT::MakeHexTerrain(d[0], d[1]);
        Rig a(terrain), b(terrain);
        WT::FixedRangeExecutor perCell(d[0] * d[1]);
        for (uint32_t w = 0; w < 52; ++w)
        {
            a.Step(static_cast<Season>((w / 13) % 4), w);
            b.StepWith(perCell, static_cast<Season>((w / 13) % 4), w);
        }
        EXPECT_TRUE(AllInRange(a.sim.Fields())) << d[0] << "x" << d[1];
        EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields())) << d[0] << "x" << d[1];
    }
}

// ---- Forcing extremes --------------------------------------------------------------------------------------------

TEST(WeatherEdges, ForcingOnTheLastCellApplies)
{
    Rig rig(WT::MakeHexTerrain(6, 5));
    const uint32_t last = 6 * 5 - 1;
    ASSERT_TRUE(rig.sim.AddForcing(Hold(last, 0.8f, 2)));
    rig.Step();
    EXPECT_EQ(rig.sim.Fields().pressure[last], 0.8f);
    EXPECT_FALSE(rig.sim.AddForcing(Hold(last + 1, 0.8f, 2)));
}

TEST(WeatherEdges, CellSetContainingEveryCellMatchesPerCellForcings)
{
    const WeatherTerrain terrain = WT::MakeHexTerrain(6, 5);
    const uint32_t n = 30;
    Rig region(terrain), perCell(terrain);

    const CellSetHandle h = RegisterAll(region.sim, n);
    ASSERT_NE(h, kNoCellSet);
    WeatherForcing f = Hold(0, 0.7f, 3);
    f.cellSet = h;
    ASSERT_TRUE(region.sim.AddForcing(f));
    for (uint32_t i = 0; i < n; ++i) ASSERT_TRUE(perCell.sim.AddForcing(Hold(i, 0.7f, 3)));

    for (uint32_t w = 0; w < 4; ++w) { region.Step(Season::Spring, w); perCell.Step(Season::Spring, w); }
    EXPECT_TRUE(WT::SameFields(region.sim.Fields(), perCell.sim.Fields()));
    EXPECT_TRUE(AllInRange(region.sim.Fields()));
}

// ---- Lifecycle misuse that must stay safe ------------------------------------------------------------------------

TEST(WeatherEdges, RunTwiceOnOneCaptureIsSafeAndCommittedUntouched)
{
    Rig rig(WT::MakeHexTerrain(8, 8));
    WeatherFields before;
    WT::CloneFields(rig.sim.Fields(), before);
    rig.Capture();
    rig.sim.Run(rig.serial);
    rig.sim.Run(rig.serial);   // compounds on the working set by design
    EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), before));
    EXPECT_TRUE(AllInRange(rig.sim.GetWorkingFields()));
    rig.sim.Commit();
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 1u);
    EXPECT_TRUE(AllInRange(rig.sim.Fields()));
}

TEST(WeatherEdges, CommitWithoutCaptureIsSafeAndIncrementsTheWeek)
{
    Rig rig(WT::MakeHexTerrain(6, 5));
    rig.sim.Commit();   // flips to the never-captured working set (zero-filled at Initialise)
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 1u);
    EXPECT_TRUE(AllInRange(rig.sim.Fields()));
    rig.Step();         // the simulation recovers on the next full step
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 2u);
    EXPECT_TRUE(AllInRange(rig.sim.Fields()));
}

TEST(WeatherEdges, CaptureTwiceWithoutCommitEqualsCaptureOnce)
{
    const WeatherTerrain terrain = WT::MakeHexTerrain(8, 8);
    Rig once(terrain), twice(terrain);
    once.Capture(Season::Autumn, 30);
    once.sim.Run(once.serial);
    once.sim.Commit();
    twice.Capture(Season::Autumn, 30);
    twice.Capture(Season::Autumn, 30);
    twice.sim.Run(twice.serial);
    twice.sim.Commit();
    EXPECT_TRUE(WT::SameFields(once.sim.Fields(), twice.sim.Fields()));
    EXPECT_EQ(twice.sim.GetCommittedWeek(), 1u);
}

TEST(WeatherEdges, SetCommittedWeekAfterStepOverwritesAndCountingContinues)
{
    Rig rig(WT::MakeHexTerrain(6, 5));
    for (uint32_t w = 0; w < 3; ++w) rig.Step(Season::Spring, w);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 3u);
    rig.sim.SetCommittedWeek(100);
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 100u);
    rig.Step();
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 101u);
    EXPECT_TRUE(AllInRange(rig.sim.Fields()));
}

// ---- Long runs ---------------------------------------------------------------------------------------------------

TEST(WeatherEdges, TwoYearsWithWeekOfYearWrapStaysInRangeAndExecutorIndependent)
{
    WT::HexTerrainOptions o;
    o.latitudeRamp = true;
    const WeatherTerrain terrain = WT::MakeHexTerrain(10, 8, o);
    Rig a(terrain), b(terrain);
    WT::FixedRangeExecutor three(3);
    for (uint32_t w = 0; w < 104; ++w)
    {
        const uint32_t woy = w % 52;
        const Season season = static_cast<Season>((woy / 13) % 4);
        a.Step(season, woy);
        b.StepWith(three, season, woy);
        ASSERT_TRUE(AllInRange(a.sim.Fields())) << "week " << w;
    }
    EXPECT_EQ(a.sim.GetCommittedWeek(), 104u);
    EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields()));
}

TEST(WeatherEdges, ZeroWindEverywhereForAYearStaysInRangeWithNoTransport)
{
    WT::HexTerrainOptions o;
    o.latitudeRamp = true;
    const uint32_t w = 8, h = 6, n = w * h;
    Rig rig(WT::MakeHexTerrain(w, h, o));

    const CellSetHandle set = RegisterAll(rig.sim, n);
    ASSERT_NE(set, kNoCellSet);
    WeatherForcing f;
    f.kind = ForcingKind::WindOverride;
    f.cellSet = set;
    f.value = 0.0f;
    f.value2 = 0.0f;
    f.weeksRemaining = 0;   // until cleared
    ASSERT_TRUE(rig.sim.AddForcing(f));

    for (uint32_t wk = 0; wk < 52; ++wk)
    {
        rig.Step(static_cast<Season>((wk / 13) % 4), wk);
        ASSERT_TRUE(AllInRange(rig.sim.Fields())) << "week " << wk;
    }
    const WeatherFields& out = rig.sim.Fields();
    for (uint32_t c = 0; c < n; ++c)
    {
        EXPECT_EQ(out.windSpeed[c], 0.0f) << c;
        // Land never evaporates (no hydrology), never receives transported moisture and never lifts: it keeps its seed.
        if ((c % w) >= 1) EXPECT_EQ(out.moisture[c], rig.params.initialMoistureLand) << c;
    }
}

// ---- A4 weather-save Task 3: RestoreCommitted / RestoreCompleted ----------------------------------------------------
// Spec: docs/specs/applications/cow/systems/a4-weather/weather-save.md
namespace
{
    using FloatArray = Dia::Core::Containers::DynamicArray<float>;
    FloatArray WeatherFields::* const kAllFieldArrays[10] = {
        &WeatherFields::moisture, &WeatherFields::temperature, &WeatherFields::pressure, &WeatherFields::windDirection,
        &WeatherFields::windSpeed, &WeatherFields::precipitation, &WeatherFields::cloudCover, &WeatherFields::waterToGround,
        &WeatherFields::snowPack, &WeatherFields::pressureAnomaly };

    void FillDistinct(WeatherFields& f, uint32_t n, float salt)
    {
        f.Resize(n);
        for (uint32_t a = 0; a < 10; ++a)
            for (uint32_t c = 0; c < n; ++c) (f.*kAllFieldArrays[a])[c] = salt + float(a) * 0.01f + float(c) * 0.0001f;
    }

    using BiomeArray = Dia::Core::Containers::DynamicArray<BiomeIndex>;
    void FillBiomes(BiomeArray& b, uint32_t n, BiomeIndex value)
    {
        b.RemoveAll();
        b.Reserve(n);
        for (uint32_t i = 0; i < n; ++i) b.Add(static_cast<BiomeIndex>((value + i) % 13u));
    }
}

TEST(WeatherRestore, RestoreCommittedReadsBackAndSetsWeekAndBiomes)
{
    Rig rig(WT::MakeHexTerrain(8, 6));
    const uint32_t n = 48;
    WeatherFields in; FillDistinct(in, n, 0.2f);
    BiomeArray biomes; FillBiomes(biomes, n, 5);
    ASSERT_TRUE(rig.sim.RestoreCommitted(77u, in, biomes));
    EXPECT_EQ(rig.sim.GetCommittedWeek(), 77u);
    EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), in));
    for (uint32_t c = 0; c < n; ++c) EXPECT_EQ(rig.sim.Terrain().biome[c], biomes[c]);
}

TEST(WeatherRestore, RestoreCommittedRejectsEachMismatchWithoutChange)
{
    Rig rig(WT::MakeHexTerrain(8, 6));
    const uint32_t n = 48;
    rig.Step();
    WeatherFields before;
    WT::CloneFields(rig.sim.Fields(), before);
    const uint64_t weekBefore = rig.sim.GetCommittedWeek();
    BiomeArray biomes; FillBiomes(biomes, n, 5);

    for (uint32_t a = 0; a < 10; ++a)
    {
        WeatherFields bad; FillDistinct(bad, n, 0.3f);
        (bad.*kAllFieldArrays[a]).Remove();
        EXPECT_FALSE(rig.sim.RestoreCommitted(99u, bad, biomes)) << "array " << a;
        EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), before)) << "array " << a;
        EXPECT_EQ(rig.sim.GetCommittedWeek(), weekBefore);
    }
    WeatherFields good; FillDistinct(good, n, 0.3f);
    BiomeArray shortBiomes; FillBiomes(shortBiomes, n, 5);
    shortBiomes.Remove();
    EXPECT_FALSE(rig.sim.RestoreCommitted(99u, good, shortBiomes));
    EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), before));
    EXPECT_EQ(rig.sim.GetCommittedWeek(), weekBefore);
    EXPECT_EQ(rig.sim.Terrain().biome[0], 2);   // MakeHexTerrain default biome, untouched
}

TEST(WeatherRestore, RestoreCompletedRejectsEachMismatchWithoutChange)
{
    Rig rig(WT::MakeHexTerrain(8, 6));
    const uint32_t n = 48;
    rig.Capture();
    WeatherFields before;
    WT::CloneFields(rig.sim.GetWorkingFields(), before);
    for (uint32_t a = 0; a < 10; ++a)
    {
        WeatherFields bad; FillDistinct(bad, n, 0.3f);
        (bad.*kAllFieldArrays[a]).Remove();
        EXPECT_FALSE(rig.sim.RestoreCompleted(bad)) << "array " << a;
        EXPECT_TRUE(WT::SameFields(rig.sim.GetWorkingFields(), before)) << "array " << a;
    }
}

TEST(WeatherRestore, RejectedBeforeInitialise)
{
    WeatherSimulation sim;
    WeatherFields in; FillDistinct(in, 4, 0.2f);
    BiomeArray b4; FillBiomes(b4, 4, 1);
    EXPECT_FALSE(sim.RestoreCommitted(1u, in, b4));
    EXPECT_FALSE(sim.RestoreCompleted(in));
    EXPECT_FALSE(sim.IsInitialised());
}

TEST(WeatherRestore, RestoreCompletedThenCommitPublishesRestoredSet)
{
    Rig rig(WT::MakeHexTerrain(8, 6));
    rig.Step();
    const uint64_t week = rig.sim.GetCommittedWeek();
    WeatherFields in; FillDistinct(in, 48, 0.4f);
    ASSERT_TRUE(rig.sim.RestoreCompleted(in));
    EXPECT_TRUE(WT::SameFields(rig.sim.GetWorkingFields(), in));
    rig.sim.Commit();
    EXPECT_TRUE(WT::SameFields(rig.sim.Fields(), in));
    EXPECT_EQ(rig.sim.GetCommittedWeek(), week + 1);
}

TEST(WeatherRestore, StepAfterRestoreCommittedMatchesUninterruptedRun)
{
    for (uint32_t weeks : { 1u, 13u })
    {
        WT::HexTerrainOptions o; o.latitudeRamp = true; o.waterColumns = 2;
        const WeatherTerrain terrain = WT::MakeHexTerrain(10, 8, o);
        const uint32_t splitAt = 5;

        Rig whole(terrain);
        WeatherFields snapshot;
        for (uint32_t wk = 0; wk < splitAt + weeks; ++wk)
        {
            if (wk == splitAt) WT::CloneFields(whole.sim.Fields(), snapshot);
            whole.Step(static_cast<Season>((wk / 13) % 4), wk);
        }

        Rig resumed(terrain);
        ASSERT_TRUE(resumed.sim.RestoreCommitted(splitAt, snapshot, terrain.biome));
        for (uint32_t wk = splitAt; wk < splitAt + weeks; ++wk)
            resumed.Step(static_cast<Season>((wk / 13) % 4), wk);

        EXPECT_TRUE(WT::SameFields(resumed.sim.Fields(), whole.sim.Fields())) << "weeks " << weeks;
        EXPECT_EQ(resumed.sim.GetCommittedWeek(), whole.sim.GetCommittedWeek());
    }
}
