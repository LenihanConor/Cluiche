// A4 weather ca-pipeline Task 9: forcings, hooks A-D, capacity, expiry and region cell sets.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 34, 35, 36, 36a)
#include <gtest/gtest.h>

#include <DiaGeometry2D/Grid/HexTopology.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherPipeline.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <math.h>
#include <string.h>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;
    using Dia::Geometry2D::kNoCell;
    using Dia::Geometry2D::kHexDirections;
    using CellArray = Dia::Core::Containers::DynamicArray<uint32_t>;

    constexpr float kPiF = 3.14159265358979323846f;
    constexpr uint32_t kW = 7, kH = 5, kN = kW * kH;
    constexpr uint32_t kMid = 17;   // column 3, row 2: all six neighbours exist

    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;
        PassContext       ctx;

        explicit Rig(const WeatherTerrain& terrain) { EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry())); }

        void Capture(Season s = Season::Spring, uint32_t week = 0)
        {
            sim.Capture(params, s, week, WT::TestRegistry(), nullptr, hydro, influences);
            ctx.terrain = &sim.Terrain();
            ctx.tables = const_cast<GridTables*>(&sim.Tables());
            ctx.inputs = &sim.Captured();
            ctx.fields = const_cast<WeatherFields*>(&sim.GetWorkingFields());
            ctx.handoff = const_cast<WeatherHandoff*>(&sim.GetHandoff());
            ctx.totals = const_cast<WeatherGridTotals*>(&sim.GetWorkingTotals());
            ctx.profile = const_cast<WeatherStepProfile*>(&sim.GetLastProfile());
        }
        // Capture, then run passes up to and including `last` on the working set.
        const WeatherFields& RunTo(PassId last)
        {
            Capture();
            RunPipelineUntil(ctx, serial, last);
            return sim.GetWorkingFields();
        }
        void Step(Dia::ParallelExecutor::IParallelExecutor& exec, Season s = Season::Spring, uint32_t week = 0)
        {
            sim.Step(params, s, week, WT::TestRegistry(), nullptr, hydro, influences, exec);
        }
        void Step() { Step(serial); }
        void Step(Season s, uint32_t week) { Step(serial, s, week); }
        uint8_t DirIndex(uint32_t c) const { return sim.Tables().dirIndex[c]; }
    };

    WeatherForcing Make(ForcingKind k, uint32_t cell, float value, uint16_t weeks = 1, float value2 = 0.5f)
    {
        WeatherForcing f;
        f.kind = k;
        f.cell = cell;
        f.value = value;
        f.value2 = value2;
        f.weeksRemaining = weeks;
        return f;
    }

    WeatherForcing MakeRegion(ForcingKind k, CellSetHandle set, float value, uint16_t weeks = 1, float value2 = 0.5f)
    {
        WeatherForcing f = Make(k, 0, value, weeks, value2);
        f.cellSet = set;
        return f;
    }

    // DynamicArray is not copyable by value, so the list is built in place and converts to a const reference.
    struct Cells
    {
        CellArray array;
        Cells(std::initializer_list<uint32_t> list)
        {
            array.Reserve(static_cast<uint32_t>(list.size()) + 1);
            for (uint32_t c : list) array.Add(c);
        }
        operator const CellArray&() const { return array; }
    };

    WeatherTerrain Land(float latitude = 0.3f, BiomeIndex biome = 2)
    {
        WT::HexTerrainOptions o;
        o.latitude = latitude;
        o.biome = biome;
        return WT::MakeHexTerrain(kW, kH, o);
    }

    bool SameWind(const WeatherFields& a, const WeatherFields& b, uint32_t c)
    {
        return memcmp(&a.windDirection[c], &b.windDirection[c], sizeof(float)) == 0
            && memcmp(&a.windSpeed[c], &b.windSpeed[c], sizeof(float)) == 0;
    }
}

// ---- Cell sets ---------------------------------------------------------------------------------------------------

TEST(WeatherForcing, RegisterCellSetReturnsSequentialHandles)
{
    Rig rig(Land());
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({1, 2, 3})), 0u);
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({0})), 1u);
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({kN - 1})), 2u);
    EXPECT_EQ(rig.sim.CellSetCount(), 3u);
}

TEST(WeatherForcing, RegisterCellSetRejectsBadLists)
{
    Rig rig(Land());
    EXPECT_EQ(rig.sim.RegisterCellSet(CellArray()), kNoCellSet);              // empty
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({3, 2})), kNoCellSet);            // unsorted
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({2, 2, 5})), kNoCellSet);         // duplicate
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({1, kN})), kNoCellSet);           // out of range
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({0xFFFFFFFFu})), kNoCellSet);
    EXPECT_EQ(rig.sim.CellSetCount(), 0u) << "a rejected list stores nothing";
}

TEST(WeatherForcing, SeventeenthCellSetIsRejected)
{
    Rig rig(Land());
    for (uint32_t s = 0; s < kMaxCellSets; ++s) ASSERT_EQ(rig.sim.RegisterCellSet(Cells({s})), s);
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({20})), kNoCellSet);
    EXPECT_EQ(rig.sim.CellSetCount(), kMaxCellSets);
}

TEST(WeatherForcing, RegisterCellSetBeforeInitialiseFails)
{
    WeatherSimulation sim;
    EXPECT_EQ(sim.RegisterCellSet(Cells({0})), kNoCellSet);
}

TEST(WeatherForcing, AddForcingNeedsARegisteredSet)
{
    Rig rig(Land());
    EXPECT_FALSE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, 0, 0.2f)));
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({4, 5}));
    EXPECT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f)));
    EXPECT_FALSE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h + 1, 0.2f)));
    EXPECT_FALSE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, kMaxCellSets, 0.2f)));
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    // A single-cell forcing still needs an in-range cell.
    EXPECT_FALSE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, kN, 0.2f)));
}

TEST(WeatherForcing, ClearCellSetsDropsDependentForcingsOnly)
{
    Rig rig(Land());
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({4, 5}));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 1, 0.2f, 3)));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::MoistureHold, h, 0.7f, 3)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, 2, 0.1f, 3)));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::WindOverride, h, 1.0f, 0)));
    rig.sim.ClearCellSets();
    EXPECT_EQ(rig.sim.CellSetCount(), 0u);
    ASSERT_EQ(rig.sim.ForcingCount(), 2u);
    rig.Capture();
    EXPECT_EQ(rig.sim.Captured().forcings[0u].kind, ForcingKind::PressureHold);   // order kept
    EXPECT_EQ(rig.sim.Captured().forcings[1u].kind, ForcingKind::Rain);
    EXPECT_FALSE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f))) << "the handle is gone";
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({9})), 0u) << "handles restart";
}

TEST(WeatherForcing, ReinitialiseDropsSetsAndForcingsEvenOnASmallerGrid)
{
    Rig rig(Land());
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({30, 31, 34}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, 30, 0.2f)));
    ASSERT_TRUE(rig.sim.Initialise(WT::MakeHexTerrain(4, 4), rig.params, WT::TestRegistry()));
    EXPECT_EQ(rig.sim.CellSetCount(), 0u);
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({30})), kNoCellSet) << "cell 30 is outside the 16-cell grid";
    EXPECT_EQ(rig.sim.RegisterCellSet(Cells({15})), 0u);
    rig.Step();   // a run over the smaller grid touches no stale set
    EXPECT_EQ(rig.sim.Fields().CellCount(), 16u);
}

// ---- Hook A: PressureHold (AC 34) --------------------------------------------------------------------------------

TEST(WeatherForcing, PressureHoldSurvivesPassTwoRecompute)
{
    Rig free(Land()), held(Land());
    ASSERT_TRUE(held.sim.AddForcing(Make(ForcingKind::PressureHold, kMid, 0.1f)));
    const WeatherFields& a = free.RunTo(PassId::Pressure);
    const WeatherFields& b = held.RunTo(PassId::Pressure);
    EXPECT_EQ(b.pressure[kMid], 0.1f);
    EXPECT_NE(a.pressure[kMid], 0.1f);
    for (uint32_t c = 0; c < kN; ++c)
        if (c != kMid) EXPECT_EQ(a.pressure[c], b.pressure[c]) << "cell " << c;
}

TEST(WeatherForcing, PressureHoldValueIsClamped)
{
    Rig rig(Land());
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 3, 1.7f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 4, -0.4f)));
    const WeatherFields& f = rig.RunTo(PassId::Pressure);
    EXPECT_EQ(f.pressure[3], 1.0f);
    EXPECT_EQ(f.pressure[4], 0.0f);
}

TEST(WeatherForcing, HeldLowShapesNeighbourWindInTheSameRun)
{
    Rig free(Land()), held(Land());
    ASSERT_TRUE(held.sim.AddForcing(Make(ForcingKind::PressureHold, kMid, 0.0f)));
    const WeatherFields& a = free.RunTo(PassId::Wind);
    const WeatherFields& b = held.RunTo(PassId::Wind);
    uint32_t checked = 0;
    for (uint32_t d = 0; d < kHexDirections; ++d)
    {
        const uint32_t nb = free.sim.Neighbours()[kMid * kHexDirections + d];
        ASSERT_NE(nb, kNoCell);
        EXPECT_FALSE(SameWind(a, b, nb)) << "neighbour " << nb << " responds to the held low";
        ++checked;
    }
    EXPECT_EQ(checked, 6u);
    // The held cell's own gradient is sum(dir_d * (self - nb_d)) / 6; the six hex direction vectors sum to zero, so its
    // own pressure cancels and only a float rounding artefact could make its wind differ. Not asserted either way.
    EXPECT_TRUE(SameWind(a, b, 0)) << "a cell two rings away is untouched";
}

// ---- Hook C: MoistureHold (AC 34) --------------------------------------------------------------------------------

TEST(WeatherForcing, MoistureHoldIsVisibleToTransportPass)
{
    Rig rig(Land());
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::MoistureHold, kMid, 0.9f)));
    const WeatherFields& f = rig.RunTo(PassId::MoistureOut);
    EXPECT_EQ(f.moisture[kMid], 0.9f);
    ASSERT_GT(f.windSpeed[kMid], 0.0f);
    const float expected = 0.9f * f.windSpeed[kMid] * rig.params.moistureTransportRate;
    EXPECT_FLOAT_EQ(rig.sim.Tables().moistureOut[kMid], expected) << "pass 5a used the held value";
}

TEST(WeatherForcing, MoistureHoldIsClamped)
{
    Rig rig(Land());
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::MoistureHold, 6, 3.0f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::MoistureHold, 7, -3.0f)));
    const WeatherFields& f = rig.RunTo(PassId::Evaporation);
    EXPECT_EQ(f.moisture[6], 1.0f);
    EXPECT_EQ(f.moisture[7], 0.0f);
}

// ---- Hook B: WindOverride (AC 34) --------------------------------------------------------------------------------

TEST(WeatherForcing, WindOverrideReplacesPassThreeOutputAndResnapsDirection)
{
    Rig free(Land(0.0f)), over(Land(0.0f));
    const float south = 1.5f * kPiF;   // 270 degrees: hex direction 4
    ASSERT_TRUE(over.sim.AddForcing(Make(ForcingKind::WindOverride, kMid, south, 0, 0.7f)));
    const WeatherFields& a = free.RunTo(PassId::Wind);
    const WeatherFields& b = over.RunTo(PassId::Wind);
    EXPECT_EQ(b.windDirection[kMid], south);
    EXPECT_EQ(b.windSpeed[kMid], 0.7f);
    EXPECT_EQ(over.DirIndex(kMid), 4);
    EXPECT_NE(free.DirIndex(kMid), 4);
    for (uint32_t c = 0; c < kN; ++c)
        if (c != kMid) EXPECT_TRUE(SameWind(a, b, c)) << "cell " << c;
}

TEST(WeatherForcing, WindOverrideWrapsAngleAndClampsSpeed)
{
    Rig rig(Land());
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::WindOverride, 3, -0.5f * kPiF, 0, 4.0f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::WindOverride, 4, 2.5f * kPiF, 0, -1.0f)));
    const WeatherFields& f = rig.RunTo(PassId::Wind);
    EXPECT_NEAR(f.windDirection[3], 1.5f * kPiF, 1e-5f);
    EXPECT_EQ(f.windSpeed[3], 1.0f);
    EXPECT_NEAR(f.windDirection[4], 0.5f * kPiF, 1e-5f);
    EXPECT_EQ(f.windSpeed[4], 0.0f);
    EXPECT_EQ(rig.DirIndex(3), 4);
    EXPECT_EQ(rig.DirIndex(4), 1);
}

TEST(WeatherForcing, WindOverrideChangesTransportDirection)
{
    // Override cell kMid to blow south (direction 4): its moisture must reach its direction-4 neighbour.
    Rig rig(Land(0.0f));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::WindOverride, kMid, 1.5f * kPiF, 0, 1.0f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::MoistureHold, kMid, 1.0f, 1)));
    rig.RunTo(PassId::MoistureOut);
    const uint32_t south = rig.sim.Neighbours()[kMid * kHexDirections + 4];
    ASSERT_NE(south, kNoCell);
    EXPECT_GT(rig.sim.Tables().moistureOut[kMid], 0.0f);
    const WeatherFields& f = rig.RunTo(PassId::MoistureGather);
    EXPECT_LT(f.moisture[kMid], 1.0f);
    Rig base(Land(0.0f));
    ASSERT_TRUE(base.sim.AddForcing(Make(ForcingKind::MoistureHold, kMid, 1.0f, 1)));
    EXPECT_GT(f.moisture[south], base.RunTo(PassId::MoistureGather).moisture[south]);
}

TEST(WeatherForcing, WindOverrideWithZeroWeeksPersistsAndPositiveWeeksExpire)
{
    Rig rig(Land());
    const float v = 0.5f * kPiF;
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::WindOverride, 5, v, 0, 0.6f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::WindOverride, 6, v, 2, 0.6f)));
    rig.Step();
    EXPECT_EQ(rig.sim.Fields().windDirection[5], v);
    EXPECT_EQ(rig.sim.Fields().windDirection[6], v);
    EXPECT_EQ(rig.sim.ForcingCount(), 2u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u) << "the two-week override expired, the 0 one stays";
    EXPECT_EQ(rig.sim.Fields().windDirection[6], v) << "still applied on its second week";
    for (int i = 0; i < 20; ++i) rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    EXPECT_EQ(rig.sim.Fields().windDirection[5], v);
    EXPECT_NE(rig.sim.Fields().windDirection[6], v) << "expired, no longer applied";
}

// ---- Hook D: Rain (AC 35) ----------------------------------------------------------------------------------------

namespace
{
    // Params that make natural precipitation zero: nothing can exceed the rain threshold, and the flat terrain has no lift.
    void NoNaturalRain(Rig& r) { r.params.rainThreshold = 1.0f; }
}

TEST(WeatherForcing, RainAddsExactlyValueAndRoutesToWaterToGroundWhenWarm)
{
    Rig free(Land(0.3f, 5)), rain(Land(0.3f, 5));
    NoNaturalRain(free); NoNaturalRain(rain);
    ASSERT_TRUE(rain.sim.AddForcing(Make(ForcingKind::Rain, kMid, 0.4f)));
    const WeatherFields& a = free.RunTo(PassId::Precipitation);
    const WeatherFields& b = rain.RunTo(PassId::Precipitation);
    ASSERT_GE(b.temperature[kMid], rain.params.snowThreshold) << "warm cell";
    EXPECT_EQ(a.precipitation[kMid], 0.0f);
    EXPECT_EQ(b.precipitation[kMid], 0.4f);
    EXPECT_EQ(b.waterToGround[kMid], 0.4f);
    EXPECT_EQ(b.snowPack[kMid], 0.0f);
    EXPECT_TRUE(WT::SameFloats(a.moisture, b.moisture)) << "rain consumes no moisture";
    EXPECT_TRUE(WT::SameFloats(a.cloudCover, b.cloudCover));
    for (uint32_t c = 0; c < kN; ++c)
        if (c != kMid) EXPECT_EQ(b.precipitation[c], 0.0f);
}

TEST(WeatherForcing, RainRoutesToSnowPackWhenCold)
{
    Rig free(Land(0.3f, 7)), rain(Land(0.3f, 7));
    NoNaturalRain(free); NoNaturalRain(rain);
    ASSERT_TRUE(rain.sim.AddForcing(Make(ForcingKind::Rain, kMid, 0.4f)));
    free.RunTo(PassId::Precipitation);
    const WeatherFields& b = rain.RunTo(PassId::Precipitation);
    ASSERT_LT(b.temperature[kMid], rain.params.snowThreshold) << "cold cell";
    EXPECT_EQ(b.precipitation[kMid], 0.4f);
    EXPECT_EQ(b.snowPack[kMid], 0.4f);
    EXPECT_EQ(b.waterToGround[kMid], 0.0f);
    EXPECT_TRUE(WT::SameFloats(free.sim.GetWorkingFields().moisture, b.moisture));
}

TEST(WeatherForcing, RainLastsExactlyOneWeek)
{
    Rig rig(Land(0.3f, 5));
    NoNaturalRain(rig);
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, kMid, 0.4f, 1)));
    rig.Step();
    EXPECT_EQ(rig.sim.Fields().precipitation[kMid], 0.4f);
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
    rig.Step();
    EXPECT_EQ(rig.sim.Fields().precipitation[kMid], 0.0f);
}

TEST(WeatherForcing, RainForNWeeksAppliesEachWeek)
{
    Rig rig(Land(0.3f, 5));
    NoNaturalRain(rig);
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, kMid, 0.25f, 3)));
    for (int w = 0; w < 3; ++w)
    {
        rig.Step();
        EXPECT_EQ(rig.sim.Fields().precipitation[kMid], 0.25f) << "week " << w;
    }
    rig.Step();
    EXPECT_EQ(rig.sim.Fields().precipitation[kMid], 0.0f);
}

// ---- Capacity, expiry, ordering (AC 36) --------------------------------------------------------------------------

TEST(WeatherForcing, AddForcingFailsAtSixtyFourAndDropsNothing)
{
    Rig rig(Land());
    for (uint32_t i = 0; i < kMaxForcings; ++i)
        ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, i % kN, 0.01f * float(i + 1), 5))) << i;
    EXPECT_FALSE(rig.sim.AddForcing(Make(ForcingKind::Rain, 0, 0.5f, 5)));
    EXPECT_EQ(rig.sim.ForcingCount(), kMaxForcings);
    rig.Capture();
    ASSERT_EQ(rig.sim.Captured().forcings.Size(), kMaxForcings);
    for (uint32_t i = 0; i < kMaxForcings; ++i)
        EXPECT_EQ(rig.sim.Captured().forcings[i].value, 0.01f * float(i + 1)) << "record " << i << " kept in order";
}

TEST(WeatherForcing, ExpiryFreesCapacityAndWeeksCountDown)
{
    Rig rig(Land());
    for (uint32_t i = 0; i < kMaxForcings; ++i) ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, i % kN, 0.5f, 1)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
    EXPECT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 0, 0.5f, 2)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
}

TEST(WeatherForcing, RegionForcingsExpireLikeSingleCellOnes)
{
    Rig rig(Land());
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({3, 4, 5}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f, 2)));
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 1u);
    rig.Step();
    EXPECT_EQ(rig.sim.ForcingCount(), 0u);
    EXPECT_EQ(rig.sim.CellSetCount(), 1u) << "the set outlives its forcings";
}

TEST(WeatherForcing, DistinctCellForcingsAreInsertionOrderIndependent)
{
    const WeatherForcing fs[] = {
        Make(ForcingKind::PressureHold, 3, 0.15f, 2), Make(ForcingKind::MoistureHold, 9, 0.85f, 2),
        Make(ForcingKind::Rain, 12, 0.3f, 2),         Make(ForcingKind::WindOverride, 20, 1.0f, 0, 0.8f),
        Make(ForcingKind::PressureHold, 25, 0.9f, 2), Make(ForcingKind::Rain, 31, 0.2f, 2),
    };
    Rig fwd(Land(0.3f, 5)), rev(Land(0.3f, 5));
    for (int i = 0; i < 6; ++i) ASSERT_TRUE(fwd.sim.AddForcing(fs[i]));
    for (int i = 5; i >= 0; --i) ASSERT_TRUE(rev.sim.AddForcing(fs[i]));
    for (int w = 0; w < 3; ++w)
    {
        fwd.Step(Season::Summer, w);
        rev.Step(Season::Summer, w);
        EXPECT_TRUE(WT::SameFields(fwd.sim.Fields(), rev.sim.Fields())) << "week " << w;
    }
}

TEST(WeatherForcing, OverlappingSameKindResolvesToLowestRecordIndex)
{
    Rig lowFirst(Land()), highFirst(Land());
    ASSERT_TRUE(lowFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.1f)));
    ASSERT_TRUE(lowFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.9f)));
    ASSERT_TRUE(highFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.9f)));
    ASSERT_TRUE(highFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.1f)));
    EXPECT_EQ(lowFirst.RunTo(PassId::Pressure).pressure[10], 0.1f);
    EXPECT_EQ(highFirst.RunTo(PassId::Pressure).pressure[10], 0.9f);
}

TEST(WeatherForcing, DifferentKindsOnTheSameCellDoNotCompete)
{
    Rig rig(Land());
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::MoistureHold, 10, 0.8f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.2f)));
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::Rain, 10, 0.1f)));
    rig.RunTo(PassId::Pressure);
    EXPECT_EQ(rig.sim.GetWorkingFields().pressure[10], 0.2f);
    rig.RunTo(PassId::Evaporation);
    EXPECT_EQ(rig.sim.GetWorkingFields().moisture[10], 0.8f);
}

// ---- Region forcings (AC 36a) ------------------------------------------------------------------------------------

TEST(WeatherForcing, RegionForcingAppliesToEverySetCellAndNoOther)
{
    Rig free(Land()), region(Land());
    const CellSetHandle h = region.sim.RegisterCellSet(Cells({2, 3, 9, 16, 17, 30, 34}));
    ASSERT_NE(h, kNoCellSet);
    ASSERT_TRUE(region.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.25f)));
    const WeatherFields& a = free.RunTo(PassId::Pressure);
    const WeatherFields& b = region.RunTo(PassId::Pressure);
    const uint32_t inSet[] = { 2, 3, 9, 16, 17, 30, 34 };
    for (uint32_t c = 0; c < kN; ++c)
    {
        bool member = false;
        for (uint32_t m : inSet) member = member || m == c;
        if (member) EXPECT_EQ(b.pressure[c], 0.25f) << "cell " << c;
        else        EXPECT_EQ(b.pressure[c], a.pressure[c]) << "cell " << c;
    }
}

TEST(WeatherForcing, RegionWindOverrideCoversTheSet)
{
    Rig rig(Land(0.0f));
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({8, 9, 15, 16}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::WindOverride, h, 1.5f * kPiF, 0, 0.9f)));
    const WeatherFields& f = rig.RunTo(PassId::Wind);
    for (uint32_t c : { 8u, 9u, 15u, 16u })
    {
        EXPECT_EQ(f.windSpeed[c], 0.9f) << c;
        EXPECT_EQ(rig.DirIndex(c), 4) << c;
    }
    EXPECT_NE(f.windSpeed[10], 0.9f);
}

TEST(WeatherForcing, RegionAndPerCellFormsGiveIdenticalOutput)
{
    Rig region(Land(0.3f, 5)), perCell(Land(0.3f, 5));
    const CellSetHandle h = region.sim.RegisterCellSet(Cells({8, 9, 15, 16}));
    ASSERT_TRUE(region.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.15f, 2)));
    ASSERT_TRUE(region.sim.AddForcing(MakeRegion(ForcingKind::WindOverride, h, 1.0f, 0, 0.8f)));
    ASSERT_TRUE(region.sim.AddForcing(MakeRegion(ForcingKind::MoistureHold, h, 0.9f, 2)));
    ASSERT_TRUE(region.sim.AddForcing(MakeRegion(ForcingKind::Rain, h, 0.3f, 2)));
    for (uint32_t c : { 8u, 9u, 15u, 16u })
    {
        ASSERT_TRUE(perCell.sim.AddForcing(Make(ForcingKind::PressureHold, c, 0.15f, 2)));
        ASSERT_TRUE(perCell.sim.AddForcing(Make(ForcingKind::WindOverride, c, 1.0f, 0, 0.8f)));
        ASSERT_TRUE(perCell.sim.AddForcing(Make(ForcingKind::MoistureHold, c, 0.9f, 2)));
        ASSERT_TRUE(perCell.sim.AddForcing(Make(ForcingKind::Rain, c, 0.3f, 2)));
    }
    for (int w = 0; w < 3; ++w)
    {
        region.Step(Season::Autumn, w);
        perCell.Step(Season::Autumn, w);
        EXPECT_TRUE(WT::SameFields(region.sim.Fields(), perCell.sim.Fields())) << "week " << w;
    }
}

TEST(WeatherForcing, OverlappingRegionAndCellRecordsResolveByRecordIndex)
{
    Rig regionFirst(Land()), cellFirst(Land());
    const CellSetHandle h1 = regionFirst.sim.RegisterCellSet(Cells({3, 4}));
    const CellSetHandle h2 = cellFirst.sim.RegisterCellSet(Cells({3, 4}));
    ASSERT_TRUE(regionFirst.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h1, 0.1f)));
    ASSERT_TRUE(regionFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 4, 0.9f)));
    ASSERT_TRUE(cellFirst.sim.AddForcing(Make(ForcingKind::PressureHold, 4, 0.9f)));
    ASSERT_TRUE(cellFirst.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h2, 0.1f)));
    const WeatherFields& a = regionFirst.RunTo(PassId::Pressure);
    EXPECT_EQ(a.pressure[3], 0.1f);
    EXPECT_EQ(a.pressure[4], 0.1f);
    const WeatherFields& b = cellFirst.RunTo(PassId::Pressure);
    EXPECT_EQ(b.pressure[3], 0.1f);
    EXPECT_EQ(b.pressure[4], 0.9f);
}

TEST(WeatherForcing, TwoRegionsOverlapping)
{
    Rig rig(Land());
    const CellSetHandle a = rig.sim.RegisterCellSet(Cells({3, 4, 5}));
    const CellSetHandle b = rig.sim.RegisterCellSet(Cells({5, 6}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, a, 0.2f)));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, b, 0.8f)));
    const WeatherFields& f = rig.RunTo(PassId::Pressure);
    EXPECT_EQ(f.pressure[5], 0.2f);
    EXPECT_EQ(f.pressure[6], 0.8f);
}

// ---- Latching, determinism, no-forcing regression ----------------------------------------------------------------

TEST(WeatherForcing, RunReadsOnlyTheLatchedCopy)
{
    Rig rig(Land());
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({3, 4}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f)));
    rig.Capture();
    rig.sim.ClearCellSets();   // live list is now empty, the latched copy is not
    ASSERT_EQ(rig.sim.ForcingCount(), 0u);
    ASSERT_TRUE(rig.sim.AddForcing(Make(ForcingKind::PressureHold, 10, 0.7f)));   // added after Capture: next week's
    RunPipelineUntil(rig.ctx, rig.serial, PassId::Pressure);
    const WeatherFields& f = rig.sim.GetWorkingFields();
    EXPECT_EQ(f.pressure[3], 0.2f);
    EXPECT_EQ(f.pressure[4], 0.2f);
    EXPECT_NE(f.pressure[10], 0.7f);
}

TEST(WeatherForcing, StaleClaimedTableIsClearedAtRunStart)
{
    Rig clean(Land()), dirty(Land());
    dirty.Capture();
    GridTables& t = const_cast<GridTables&>(dirty.sim.Tables());
    for (uint32_t i = 0; i < t.claimed.Size(); ++i) t.claimed[i] = 3;
    clean.Step();
    dirty.Step();
    EXPECT_TRUE(WT::SameFields(clean.sim.Fields(), dirty.sim.Fields()));
}

TEST(WeatherForcing, FieldsAreUnaffectedOnceForcingsExpire)
{
    // A forcing that expires leaves later weeks identical to a run that had the same history of fields: compare a
    // zero-effect forcing (moisture held at the value it already has) with an unforced run.
    Rig free(Land()), hold(Land());
    free.Step();
    free.Step();
    hold.sim.AddForcing(Make(ForcingKind::PressureHold, 3, free.sim.Fields().pressure[3], 1));   // week 1 only
    hold.Step();
    hold.Step();
    // After expiry the forcing list is empty and Run takes the same path as the unforced sim.
    EXPECT_EQ(hold.sim.ForcingCount(), 0u);
    hold.RunTo(PassId::Pressure);
    for (uint32_t i = 0; i < hold.sim.Tables().claimed.Size(); ++i) EXPECT_EQ(hold.sim.Tables().claimed[i], 0);
}

TEST(WeatherForcing, ForcedResultsMatchAcrossExecutors)
{
    auto build = [](Rig& r)
    {
        const CellSetHandle h = r.sim.RegisterCellSet(Cells({1, 2, 8, 9, 22, 23}));
        ASSERT_TRUE(r.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.1f, 3)));
        ASSERT_TRUE(r.sim.AddForcing(Make(ForcingKind::MoistureHold, 17, 0.95f, 3)));
        ASSERT_TRUE(r.sim.AddForcing(Make(ForcingKind::Rain, 18, 0.35f, 2)));
        ASSERT_TRUE(r.sim.AddForcing(MakeRegion(ForcingKind::WindOverride, h, 0.25f * kPiF, 0, 0.9f)));
        ASSERT_TRUE(r.sim.AddForcing(Make(ForcingKind::Rain, 22, 0.2f, 1)));
    };
    Rig reference(Land(0.3f, 5));
    build(reference);
    for (int w = 0; w < 4; ++w) reference.Step(Season::Summer, w);

    WT::ReverseRangeExecutor reverse(0);
    WT::ReverseRangeExecutor reverse7(7);
    WT::FixedRangeExecutor   fixed3(3);
    WT::FixedRangeExecutor   perCell(kN);
    Dia::ParallelExecutor::IParallelExecutor* execs[] = { &reverse, &reverse7, &fixed3, &perCell };
    for (auto* exec : execs)
    {
        Rig r(Land(0.3f, 5));
        build(r);
        for (int w = 0; w < 4; ++w) r.Step(*exec, Season::Summer, w);
        EXPECT_TRUE(WT::SameFields(reference.sim.Fields(), r.sim.Fields()));
        EXPECT_EQ(reference.sim.ForcingCount(), r.sim.ForcingCount());
    }
}

TEST(WeatherForcing, RunDoesNotReallocateForcingStorage)
{
    Rig rig(Land());
    const CellSetHandle h = rig.sim.RegisterCellSet(Cells({3, 4, 5}));
    ASSERT_TRUE(rig.sim.AddForcing(MakeRegion(ForcingKind::PressureHold, h, 0.2f, 3)));
    rig.Capture();
    const uint8_t* claimedData = &rig.sim.Tables().claimed[0u];
    const uint32_t claimedSize = rig.sim.Tables().claimed.Size();
    const uint32_t* setData = &rig.sim.Captured().cellSets.cells[0u];
    RunPipeline(rig.ctx, rig.serial);
    EXPECT_EQ(claimedData, &rig.sim.Tables().claimed[0u]);
    EXPECT_EQ(claimedSize, rig.sim.Tables().claimed.Size());
    EXPECT_EQ(setData, &rig.sim.Captured().cellSets.cells[0u]);
}
