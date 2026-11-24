// A4 weather ca-pipeline Task 6: pass 7 Orographic and the shared RouteCondensate helper.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 14, 15, 20; EC-05, EC-09)
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

    constexpr uint32_t kW = 9, kH = 9, kN = kW * kH;
    constexpr uint32_t kMid = 4 * kW + 4;

    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;
        PassContext       ctx;

        explicit Rig(const WeatherTerrain& terrain) { EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry())); }

        void Capture()
        {
            sim.Capture(params, Season::Spring, 0, WT::TestRegistry(), nullptr, hydro, influences);
            ctx.terrain = &sim.Terrain();
            ctx.tables = const_cast<GridTables*>(&sim.Tables());
            ctx.inputs = &sim.Captured();
            ctx.fields = &Working();
            ctx.handoff = const_cast<WeatherHandoff*>(&sim.GetHandoff());
            ctx.totals = const_cast<WeatherGridTotals*>(&sim.GetWorkingTotals());
            ctx.profile = const_cast<WeatherStepProfile*>(&sim.GetLastProfile());
        }
        WeatherFields& Working() { return const_cast<WeatherFields&>(sim.GetWorkingFields()); }
        CapturedInputs& Latched() { return const_cast<CapturedInputs&>(sim.Captured()); }

        // Every cell blows in direction d at `speed` with the given moisture and temperature.
        void Uniform(uint32_t d, float speed, float moisture, float temperature)
        {
            WeatherFields& f = Working();
            for (uint32_t c = 0; c < kN; ++c)
            {
                ctx.tables->dirIndex[c] = static_cast<uint8_t>(d);
                f.windSpeed[c] = speed;
                f.moisture[c] = moisture;
                f.temperature[c] = temperature;
                f.snowPack[c] = 0.0f;
                f.precipitation[c] = 0.0f; f.waterToGround[c] = 0.0f; f.cloudCover[c] = 0.0f;
            }
        }
    };

    WeatherTerrain FlatTerrain(float elevation)
    {
        WT::HexTerrainOptions o;
        o.elevation = elevation;
        return WT::MakeHexTerrain(kW, kH, o);
    }

    // Neighbour tables depend only on the grid size, so one probe serves every terrain.
    uint32_t Nb(uint32_t c, uint32_t d)
    {
        static Rig probe(FlatTerrain(0.3f));
        return probe.sim.Tables().neighbours[c * 6 + d];
    }

    // Flat 0.3 terrain with the listed cells overridden.
    struct Override { uint32_t cell; float elevation; };
    WeatherTerrain Terrain(std::initializer_list<Override> overrides)
    {
        WeatherTerrain t = FlatTerrain(0.3f);
        for (const Override& o : overrides) t.elevation[o.cell] = o.elevation;
        return t;
    }

    // Independent reference of the lift on a single cell (no routing).
    struct Lift { float temperature, moisture, amount; bool condensed; };
    Lift ReferenceLift(float temperature, float moisture, float diff, float factor, const WeatherParameters& p)
    {
        Lift l{ temperature, moisture, 0.0f, false };
        const float gain = diff * factor;
        l.temperature = temperature - gain * p.orographicCoolingRate;
        if (l.temperature < p.condensationTemp)
        {
            float a = moisture * gain * p.orographicPrecipRate;
            if (a > moisture) a = moisture;
            l.amount = a; l.moisture = moisture - a; l.condensed = true;
        }
        return l;
    }

    constexpr uint32_t kDir = 0;   // NE: upwind is neighbour 3
}

// ---- AC 14: windward lift -------------------------------------------------------------------------------------------

TEST(WeatherOrographic, WindwardCellOnASlopeCoolsCondensesAndRains)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.2f }, { kMid, 0.6f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    RunOrographicPass(rig.ctx, rig.serial);

    const WeatherFields& f = rig.Working();
    const float diff = 0.6f - 0.2f;
    EXPECT_NEAR(f.temperature[kMid], 0.5f - diff * rig.params.orographicCoolingRate, 1e-6f);   // 0.38
    const float amount = 0.5f * diff * rig.params.orographicPrecipRate;                          // 0.12
    EXPECT_NEAR(f.moisture[kMid], 0.5f - amount, 1e-6f);
    EXPECT_NEAR(f.precipitation[kMid], amount, 1e-6f);
    EXPECT_NEAR(f.waterToGround[kMid], amount, 1e-6f);   // 0.38 >= snow_threshold 0.3: rain
    EXPECT_EQ(f.snowPack[kMid], 0.0f);
    EXPECT_NEAR(f.cloudCover[kMid], 0.5f - amount, 1e-6f);
}

TEST(WeatherOrographic, WarmAirAfterCoolingDoesNotCondense)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.2f }, { kMid, 0.6f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.9f);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[kMid], 0.9f - 0.4f * 0.3f, 1e-6f);   // still cools
    EXPECT_EQ(rig.Working().moisture[kMid], 0.5f);
    EXPECT_EQ(rig.Working().precipitation[kMid], 0.0f);
    EXPECT_EQ(rig.Working().cloudCover[kMid], 0.0f);
}

TEST(WeatherOrographic, RingOfMountainsRainsOnWindwardFacesAndLeavesTheInteriorDry)
{
    // EC-09. Six peaks around a low centre; uniform wind. A peak rains iff its upwind neighbour is lower: the peak whose
    // upwind is outside the ring (windward face) and the one whose upwind is the centre both lift; the centre's upwind is a
    // peak, so the interior gets no lift and stays dry.
    Override ring[6];
    for (uint32_t d = 0; d < 6; ++d) ring[d] = { Nb(kMid, d), 0.8f };
    WeatherTerrain terrain = FlatTerrain(0.1f);
    for (const Override& o : ring) terrain.elevation[o.cell] = o.elevation;
    Rig rig(terrain);
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    RunOrographicPass(rig.ctx, rig.serial);

    const uint32_t windwardPeak = Nb(kMid, 3);   // sits on the upwind side of the centre
    EXPECT_GT(rig.Working().precipitation[windwardPeak], 0.0f);
    EXPECT_EQ(rig.Working().precipitation[kMid], 0.0f);
    EXPECT_EQ(rig.Working().moisture[kMid], 0.5f);
    EXPECT_EQ(rig.Working().temperature[kMid], 0.5f);
    for (uint32_t c = 0; c < kN; ++c)
    {
        const uint32_t up = Nb(c, (kDir + 3) % 6);
        const bool lifts = up != kNoCell && terrain.elevation[up] < terrain.elevation[c];
        EXPECT_EQ(rig.Working().precipitation[c] > 0.0f, lifts) << "cell " << c;
    }
}

TEST(WeatherOrographic, LeewardCellGetsLessRainThanTheWindwardCell)
{
    const uint32_t a = Nb(kMid, 3), b = kMid, c = Nb(kMid, kDir);   // a -> b (peak) -> c, along the wind
    Rig rig(Terrain({ { a, 0.1f }, { b, 0.8f }, { c, 0.1f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_GT(rig.Working().precipitation[b], 0.0f);
    EXPECT_LT(rig.Working().precipitation[c], rig.Working().precipitation[b]);
    EXPECT_EQ(rig.Working().precipitation[c], 0.0f);   // downslope: no lift at all
}

TEST(WeatherOrographic, DownslopeWindNeverLifts)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.8f }, { kMid, 0.2f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    RunOrographicPass(rig.ctx, rig.serial);
    // The cell below the high upwind neighbour is untouched (neighbours of the pair on the flat 0.3 terrain may lift).
    EXPECT_EQ(rig.Working().temperature[kMid], 0.5f);
    EXPECT_EQ(rig.Working().moisture[kMid], 0.5f);
    EXPECT_EQ(rig.Working().precipitation[kMid], 0.0f);
    EXPECT_EQ(rig.Working().cloudCover[kMid], 0.0f);
}

TEST(WeatherOrographic, EqualElevationNeverLifts)
{
    Rig rig(FlatTerrain(0.4f));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    WeatherFields before; WT::CloneFields(rig.Working(), before);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_TRUE(WT::SameFields(rig.Working(), before));
}

TEST(WeatherOrographic, OffMapUpwindNeverLifts)
{
    // Column 0 is high, the rest low: a column-0 cell either has an off-map upwind or a same-height one. Nothing lifts.
    WeatherTerrain terrain = FlatTerrain(0.1f);
    for (uint32_t r = 0; r < kH; ++r) terrain.elevation[r * kW] = 0.9f;
    Rig rig(terrain);
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    uint32_t offMap = 0;
    for (uint32_t r = 0; r < kH; ++r) if (Nb(r * kW, 3) == kNoCell) ++offMap;
    ASSERT_GT(offMap, 0u);
    WeatherFields before; WT::CloneFields(rig.Working(), before);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_TRUE(WT::SameFields(rig.Working(), before));
}

// ---- AC 20 / EC-05: zero wind ---------------------------------------------------------------------------------------

TEST(WeatherOrographic, ZeroWindNeverLiftsOnAStaleDirection)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.1f }, { kMid, 0.9f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.5f);
    WeatherFields before; WT::CloneFields(rig.Working(), before);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_TRUE(WT::SameFields(rig.Working(), before));
}

// ---- AC 15: routing -------------------------------------------------------------------------------------------------

TEST(WeatherOrographic, ColdLiftRoutesCondensateToSnowPackNotWater)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.2f }, { kMid, 0.6f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.3f);   // 0.3 - 0.12 = 0.18 < snow_threshold
    RunOrographicPass(rig.ctx, rig.serial);
    const float amount = 0.5f * 0.4f * rig.params.orographicPrecipRate;
    EXPECT_NEAR(rig.Working().snowPack[kMid], amount, 1e-6f);
    EXPECT_EQ(rig.Working().waterToGround[kMid], 0.0f);
    EXPECT_NEAR(rig.Working().precipitation[kMid], amount, 1e-6f);
}

TEST(WeatherOrographic, CondensateNeverExceedsMoisture)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.0f }, { kMid, 1.0f } }));
    rig.params.orographicPrecipRate = 50.0f;
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.4f, 0.5f);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().moisture[kMid], 0.0f);
    EXPECT_NEAR(rig.Working().precipitation[kMid], 0.4f, 1e-6f);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_GE(rig.Working().moisture[c], 0.0f);
}

TEST(WeatherOrographic, CloudCoverIsMinOfMoistureAndOneAfterTheSubtraction)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.2f }, { kMid, 0.6f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 3.0f, 0.5f);   // moisture stays above 1 after the 0.72 condensed
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[kMid], 3.0f - 3.0f * 0.4f * 0.6f, 1e-5f);
    EXPECT_EQ(rig.Working().cloudCover[kMid], 1.0f);
}

TEST(WeatherOrographic, FactorOneMatchesTheUnfactoredReferenceBitForBit)
{
    const uint32_t up = Nb(kMid, 3);
    Rig rig(Terrain({ { up, 0.2f }, { kMid, 0.6f } }));
    rig.Capture();
    rig.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    ASSERT_EQ(rig.Latched().orographicFactor[kMid], 1.0f);
    const Lift ref = ReferenceLift(0.5f, 0.5f, 0.6f - 0.2f, 1.0f, rig.params);
    ASSERT_TRUE(ref.condensed);
    RunOrographicPass(rig.ctx, rig.serial);
    EXPECT_EQ(memcmp(&ref.temperature, &rig.Working().temperature[kMid], sizeof(float)), 0);
    EXPECT_EQ(memcmp(&ref.moisture, &rig.Working().moisture[kMid], sizeof(float)), 0);
    EXPECT_EQ(memcmp(&ref.amount, &rig.Working().precipitation[kMid], sizeof(float)), 0);
}

TEST(WeatherOrographic, FactorTwoOnOneCellScalesItsLiftOnly)
{
    const uint32_t up = Nb(kMid, 3);
    const WeatherTerrain terrain = Terrain({ { up, 0.2f }, { kMid, 0.6f } });
    Rig one(terrain), two(terrain);
    one.Capture(); two.Capture();
    two.Latched().orographicFactor[kMid] = 2.0f;
    one.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    two.Uniform(kDir, 0.5f, 0.5f, 0.5f);
    RunOrographicPass(one.ctx, one.serial);
    RunOrographicPass(two.ctx, two.serial);

    const float cool1 = 0.5f - one.Working().temperature[kMid];
    const float cool2 = 0.5f - two.Working().temperature[kMid];
    EXPECT_NEAR(cool2, 2.0f * cool1, 1e-6f);
    EXPECT_NEAR(two.Working().precipitation[kMid], 2.0f * one.Working().precipitation[kMid], 1e-6f);
    const Lift ref = ReferenceLift(0.5f, 0.5f, 0.4f, 2.0f, two.params);
    EXPECT_NEAR(two.Working().moisture[kMid], ref.moisture, 1e-6f);
    for (uint32_t c = 0; c < kN; ++c)
    {
        if (c == kMid) continue;
        EXPECT_EQ(one.Working().precipitation[c], two.Working().precipitation[c]) << "cell " << c;
    }
}

TEST(WeatherOrographic, ReverseAndFixedRangesGiveIdenticalBytes)
{
    WeatherTerrain terrain = FlatTerrain(0.1f);
    for (uint32_t c = 0; c < kN; ++c) terrain.elevation[c] = 0.05f + 0.9f * float((c * 37u) % 11u) / 10.0f;
    Rig a(terrain), b(terrain), c(terrain);
    a.Capture(); b.Capture(); c.Capture();
    for (Rig* r : { &a, &b, &c }) r->Uniform(2, 0.7f, 0.6f, 0.5f);
    WT::ReverseRangeExecutor reverse(5);
    WT::FixedRangeExecutor fixed(7);
    RunOrographicPass(a.ctx, a.serial);
    RunOrographicPass(b.ctx, reverse);
    RunOrographicPass(c.ctx, fixed);
    EXPECT_TRUE(WT::SameFields(a.Working(), b.Working()));
    EXPECT_TRUE(WT::SameFields(a.Working(), c.Working()));
}

// ---- RouteCondensate ------------------------------------------------------------------------------------------------

TEST(WeatherRouteCondensate, RainAtOrAboveSnowThresholdGoesToWaterToGround)
{
    Rig rig(FlatTerrain(0.3f));
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, rig.params.snowThreshold);   // exactly at threshold: not below, so rain
    RouteCondensate(rig.ctx, 5, 0.25f);
    EXPECT_EQ(rig.Working().waterToGround[5], 0.25f);
    EXPECT_EQ(rig.Working().precipitation[5], 0.25f);
    EXPECT_EQ(rig.Working().snowPack[5], 0.0f);
    EXPECT_EQ(rig.ctx.handoff->glacierInput[5], 0.0f);
}

TEST(WeatherRouteCondensate, SnowBelowThresholdAccumulatesInSnowPack)
{
    Rig rig(FlatTerrain(0.3f));
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.1f);
    rig.Working().snowPack[5] = 0.5f;
    RouteCondensate(rig.ctx, 5, 0.25f);
    EXPECT_NEAR(rig.Working().snowPack[5], 0.75f, 1e-6f);
    EXPECT_EQ(rig.Working().waterToGround[5], 0.0f);
    EXPECT_EQ(rig.Working().precipitation[5], 0.25f);
}

TEST(WeatherRouteCondensate, SnowOverTheCapGoesToGlacierInputOnEligibleCells)
{
    Rig rig(FlatTerrain(0.3f));
    rig.hydro.Resize(kN);
    rig.hydro.present = true;
    rig.hydro.glacierEligible[5] = 1;
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.1f);
    rig.Working().snowPack[5] = 0.9f;
    rig.Working().snowPack[6] = 0.9f;
    RouteCondensate(rig.ctx, 5, 0.4f);
    EXPECT_EQ(rig.Working().snowPack[5], 1.0f);
    EXPECT_NEAR(rig.ctx.handoff->glacierInput[5], 0.3f, 1e-6f);
    EXPECT_EQ(rig.Working().precipitation[5], 0.4f);   // the whole amount fell, capped or not
}

TEST(WeatherRouteCondensate, SnowOverTheCapIsDiscardedOnIneligibleCells)
{
    Rig rig(FlatTerrain(0.3f));
    rig.hydro.Resize(kN);
    rig.hydro.present = true;
    rig.hydro.glacierEligible[5] = 1;
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.1f);
    rig.Working().snowPack[6] = 0.9f;
    RouteCondensate(rig.ctx, 6, 0.4f);
    EXPECT_EQ(rig.Working().snowPack[6], 1.0f);
    EXPECT_EQ(rig.ctx.handoff->glacierInput[6], 0.0f);
    EXPECT_EQ(rig.Working().waterToGround[6], 0.0f);
}

TEST(WeatherRouteCondensate, NoHydrologyDiscardsTheExcess)
{
    Rig rig(FlatTerrain(0.3f));
    rig.Capture();   // no A5: glacierEligible latched 0
    rig.Uniform(kDir, 0.0f, 0.5f, 0.1f);
    rig.Working().snowPack[5] = 1.0f;
    RouteCondensate(rig.ctx, 5, 0.5f);
    EXPECT_EQ(rig.Working().snowPack[5], 1.0f);
    EXPECT_EQ(rig.ctx.handoff->glacierInput[5], 0.0f);
}

TEST(WeatherRouteCondensate, ZeroOrNegativeAmountChangesNothing)
{
    Rig rig(FlatTerrain(0.3f));
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.1f);
    WeatherFields before; WT::CloneFields(rig.Working(), before);
    RouteCondensate(rig.ctx, 5, 0.0f);
    RouteCondensate(rig.ctx, 5, -0.3f);
    RouteCondensate(rig.ctx, 5, NAN);
    EXPECT_TRUE(WT::SameFields(rig.Working(), before));
    EXPECT_EQ(rig.ctx.handoff->glacierInput[5], 0.0f);
}

TEST(WeatherRouteCondensate, DoesNotTouchMoisture)
{
    Rig rig(FlatTerrain(0.3f));
    rig.Capture();
    rig.Uniform(kDir, 0.0f, 0.5f, 0.5f);
    RouteCondensate(rig.ctx, 5, 0.2f);
    EXPECT_EQ(rig.Working().moisture[5], 0.5f);
}

TEST(WeatherOrographicPipeline, OrographicRunsAfterHeatGatherAndBeforePrecipitation)
{
    EXPECT_EQ(PassId::kLast, PassId::Finish);
    EXPECT_LT(PassId::HeatGather, PassId::Orographic);
    EXPECT_LT(PassId::Orographic, PassId::Precipitation);
}
