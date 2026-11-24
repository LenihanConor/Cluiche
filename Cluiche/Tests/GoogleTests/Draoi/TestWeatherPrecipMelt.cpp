// A4 weather ca-pipeline Task 7: pass 8 Precipitation, pass 9 Snow melt + firn, Finish clamp and grid totals.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 16, 17, 18, 18a, 19, 32; EC-03, 06, 11, 13, 20)
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

    constexpr uint32_t kW = 9, kH = 9, kN = kW * kH;
    constexpr float kTwoPi = 6.28318530717958647692f;

    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;
        PassContext       ctx;
        uint32_t          week = 0;

        explicit Rig(const WeatherTerrain& terrain) { EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry())); }

        void Capture()
        {
            sim.Capture(params, Season::Spring, week, WT::TestRegistry(), nullptr, hydro, influences);
            ctx.terrain = &sim.Terrain();
            ctx.tables = const_cast<GridTables*>(&sim.Tables());
            ctx.inputs = &sim.Captured();
            ctx.fields = &Working();
            ctx.handoff = const_cast<WeatherHandoff*>(&sim.GetHandoff());
            ctx.totals = const_cast<WeatherGridTotals*>(&sim.GetWorkingTotals());
            ctx.profile = const_cast<WeatherStepProfile*>(&sim.GetLastProfile());
        }
        WeatherFields& Working() { return const_cast<WeatherFields&>(sim.GetWorkingFields()); }
        float& Glacier(uint32_t c) { return ctx.handoff->glacierInput[c]; }

        void Hydro(std::initializer_list<uint32_t> eligible, bool present = true)
        {
            hydro.Resize(kN);
            hydro.present = present;
            for (uint32_t c : eligible) hydro.glacierEligible[c] = 1;
        }

        // Every cell: the given moisture and temperature, no snow, no other state.
        void Uniform(float moisture, float temperature)
        {
            WeatherFields& f = Working();
            for (uint32_t c = 0; c < kN; ++c)
            {
                f.moisture[c] = moisture; f.temperature[c] = temperature;
                f.snowPack[c] = 0.0f; f.precipitation[c] = 0.0f; f.waterToGround[c] = 0.0f; f.cloudCover[c] = 0.0f;
                Glacier(c) = 0.0f;
            }
        }
    };

    WeatherTerrain Terrain() { return WT::MakeHexTerrain(kW, kH, WT::HexTerrainOptions()); }

    constexpr uint32_t kCell = 5;
}

// ---- Pass 8 Precipitation (AC 16) -----------------------------------------------------------------------------------

TEST(WeatherPrecipitation, RainAtOrAboveSnowThresholdGoesToWaterToGround)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.8f, rig.params.snowThreshold);   // exactly at the threshold: rain
    RunPrecipitationPass(rig.ctx, rig.serial);
    const float amount = (0.8f - rig.params.rainThreshold) * rig.params.precipRate;
    EXPECT_NEAR(rig.Working().waterToGround[kCell], amount, 1e-6f);
    EXPECT_NEAR(rig.Working().precipitation[kCell], amount, 1e-6f);   // exactly one add
    EXPECT_EQ(rig.Working().snowPack[kCell], 0.0f);
}

TEST(WeatherPrecipitation, SnowBelowSnowThresholdGoesToSnowPack)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.8f, rig.params.snowThreshold - 0.01f);
    RunPrecipitationPass(rig.ctx, rig.serial);
    const float amount = (0.8f - rig.params.rainThreshold) * rig.params.precipRate;
    EXPECT_NEAR(rig.Working().snowPack[kCell], amount, 1e-6f);
    EXPECT_NEAR(rig.Working().precipitation[kCell], amount, 1e-6f);
    EXPECT_EQ(rig.Working().waterToGround[kCell], 0.0f);
}

TEST(WeatherPrecipitation, MoistureIsReducedAndCloudCoverIsEightyPercentOfWhatIsLeft)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.8f, 0.35f);
    RunPrecipitationPass(rig.ctx, rig.serial);
    const float amount = (0.8f - rig.params.rainThreshold) * rig.params.precipRate;
    EXPECT_NEAR(rig.Working().moisture[kCell], 0.8f - amount, 1e-6f);
    EXPECT_NEAR(rig.Working().cloudCover[kCell], (0.8f - amount) * 0.8f, 1e-6f);
}

TEST(WeatherPrecipitation, ZeroMoistureGivesNoPrecipitationAndNothingNegative)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    RunPrecipitationPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
    {
        EXPECT_EQ(rig.Working().precipitation[c], 0.0f);
        EXPECT_EQ(rig.Working().moisture[c], 0.0f);
        EXPECT_EQ(rig.Working().snowPack[c], 0.0f);
        EXPECT_EQ(rig.Working().waterToGround[c], 0.0f);
    }
}

TEST(WeatherPrecipitation, MoistureAtOrBelowRainThresholdDoesNothing)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(rig.params.rainThreshold, 0.1f);
    rig.Working().cloudCover[kCell] = 0.7f;
    RunPrecipitationPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().precipitation[kCell], 0.0f);
    EXPECT_EQ(rig.Working().moisture[kCell], rig.params.rainThreshold);
    EXPECT_EQ(rig.Working().cloudCover[kCell], 0.7f);   // untouched when nothing falls
}

TEST(WeatherPrecipitation, WarmAirAtOrAboveCondensationTempHoldsItsMoisture)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.95f, rig.params.condensationTemp);
    RunPrecipitationPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().precipitation[kCell], 0.0f);
    EXPECT_EQ(rig.Working().moisture[kCell], 0.95f);
}

TEST(WeatherPrecipitation, SnowPackCapsAtOneAndExcessGoesToGlacierInputOnEligibleCellsOnly)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.Capture();
    rig.Uniform(1.0f, 0.1f);
    rig.Working().snowPack[kCell] = 0.9f;
    rig.Working().snowPack[kCell + 1] = 0.9f;
    RunPrecipitationPass(rig.ctx, rig.serial);
    const float amount = (1.0f - rig.params.rainThreshold) * rig.params.precipRate;   // 0.2
    EXPECT_EQ(rig.Working().snowPack[kCell], 1.0f);
    EXPECT_NEAR(rig.Glacier(kCell), 0.9f + amount - 1.0f, 1e-6f);
    EXPECT_EQ(rig.Working().snowPack[kCell + 1], 1.0f);
    EXPECT_EQ(rig.Glacier(kCell + 1), 0.0f);   // discarded on an ineligible cell
}

TEST(WeatherPrecipitation, NoHydrologyNeverFillsGlacierInput)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(1.0f, 0.1f);
    for (uint32_t c = 0; c < kN; ++c) rig.Working().snowPack[c] = 1.0f;
    RunPrecipitationPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_EQ(rig.Glacier(c), 0.0f);
}

TEST(WeatherPrecipitation, ReverseAndFixedRangesGiveIdenticalBytes)
{
    Rig a(Terrain()), b(Terrain()), c(Terrain());
    a.Capture(); b.Capture(); c.Capture();
    for (Rig* r : { &a, &b, &c })
    {
        for (uint32_t i = 0; i < kN; ++i)
        {
            r->Working().moisture[i] = float(i % 7) / 6.0f;
            r->Working().temperature[i] = float(i % 5) / 8.0f;
            r->Working().snowPack[i] = float(i % 3) * 0.4f;
        }
    }
    WT::ReverseRangeExecutor reverse(5);
    WT::FixedRangeExecutor fixed(7);
    RunPrecipitationPass(a.ctx, a.serial);
    RunPrecipitationPass(b.ctx, reverse);
    RunPrecipitationPass(c.ctx, fixed);
    EXPECT_TRUE(WT::SameFields(a.Working(), b.Working()));
    EXPECT_TRUE(WT::SameFields(a.Working(), c.Working()));
}

// ---- Pass 9 Snow melt and firn (AC 17, 18, 18a) ---------------------------------------------------------------------

TEST(WeatherSnowMelt, NoMeltAtOrBelowMeltThreshold)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.2f, rig.params.meltThreshold);
    rig.Working().snowPack[kCell] = 0.5f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().snowPack[kCell], 0.5f);
    EXPECT_EQ(rig.Working().waterToGround[kCell], 0.0f);
    EXPECT_EQ(rig.Working().temperature[kCell], rig.params.meltThreshold);
}

TEST(WeatherSnowMelt, MeltWithZeroSnowIsANoOp)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.2f, 0.9f);
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().waterToGround[kCell], 0.0f);
    EXPECT_EQ(rig.Working().temperature[kCell], 0.9f);
    EXPECT_EQ(rig.Working().snowPack[kCell], 0.0f);
}

TEST(WeatherSnowMelt, MeltMovesSnowToWaterToGroundAndCools)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.2f, 0.6f);
    rig.Working().snowPack[kCell] = 0.5f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    const float melt = 0.5f * ((0.6f - rig.params.meltThreshold) * rig.params.meltRate);   // GDD s11.2
    EXPECT_NEAR(rig.Working().snowPack[kCell], 0.5f - melt, 1e-6f);
    EXPECT_NEAR(rig.Working().waterToGround[kCell], melt, 1e-6f);
    EXPECT_NEAR(rig.Working().temperature[kCell], 0.6f - melt * rig.params.meltCooling, 1e-6f);
}

TEST(WeatherSnowMelt, CoolingNeverGoesBelowZero)
{
    Rig rig(Terrain());
    rig.params.meltCooling = 1000.0f;
    rig.Capture();
    rig.Uniform(0.2f, 0.9f);
    rig.Working().snowPack[kCell] = 1.0f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().temperature[kCell], 0.0f);
}

TEST(WeatherSnowMelt, MeltNeverExceedsTheSnowPack)
{
    Rig rig(Terrain());
    rig.params.meltRate = 100.0f;
    rig.Capture();
    rig.Uniform(0.2f, 0.9f);
    rig.Working().snowPack[kCell] = 0.4f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_GE(rig.Working().snowPack[kCell], 0.0f);
    EXPECT_NEAR(rig.Working().waterToGround[kCell], 0.4f, 1e-6f);
}

TEST(WeatherWater, WaterToGroundIsRainPlusMeltOnAHandCase)
{
    Rig rig(Terrain());
    rig.params.condensationTemp = 0.9f;   // let this warm cell rain
    rig.Capture();
    rig.Uniform(0.8f, 0.5f);
    rig.Working().snowPack[kCell] = 0.5f;
    RunPrecipitationPass(rig.ctx, rig.serial);
    RunSnowMeltPass(rig.ctx, rig.serial);
    const float rain = (0.8f - 0.6f) * 0.5f;                       // 0.1
    const float melt = 0.5f * ((0.5f - 0.35f) * 0.3f);            // 0.0225
    EXPECT_NEAR(rig.Working().waterToGround[kCell], rain + melt, 1e-6f);
}

TEST(WeatherWater, DryMeltFreeCellHasZeroWaterToGround)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.1f, 0.2f);
    RunPrecipitationPass(rig.ctx, rig.serial);
    RunSnowMeltPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_EQ(rig.Working().waterToGround[c], 0.0f);
}

TEST(WeatherWater, NoHydrologyGlacierInputIsZeroEverywhereEvenOnFirnWeek)
{
    Rig rig(Terrain());
    rig.hydro.Resize(kN);
    rig.hydro.present = false;
    for (uint32_t c = 0; c < kN; ++c) rig.hydro.glacierEligible[c] = 1;
    rig.week = 26;
    rig.Capture();
    rig.Uniform(1.0f, 0.1f);
    for (uint32_t c = 0; c < kN; ++c) rig.Working().snowPack[c] = 0.9f;
    RunPrecipitationPass(rig.ctx, rig.serial);
    RunSnowMeltPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_EQ(rig.Glacier(c), 0.0f);
}

TEST(WeatherFirn, SnowAboveTheFloorAtTheCheckWeekMovesToGlacierInput)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.week = rig.hydro.glacierCheckWeek;
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    rig.Working().snowPack[kCell] = 0.9f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Glacier(kCell), 0.7f, 1e-6f);
    EXPECT_NEAR(rig.Working().snowPack[kCell], 0.2f, 1e-6f);
}

TEST(WeatherFirn, SnowBelowTheFloorGivesNone)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.week = rig.hydro.glacierCheckWeek;
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    rig.Working().snowPack[kCell] = 0.15f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Glacier(kCell), 0.0f);
    EXPECT_EQ(rig.Working().snowPack[kCell], 0.15f);
}

TEST(WeatherFirn, OtherWeeksGiveNone)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.week = rig.hydro.glacierCheckWeek + 1;
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    rig.Working().snowPack[kCell] = 0.9f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Glacier(kCell), 0.0f);
    EXPECT_EQ(rig.Working().snowPack[kCell], 0.9f);
}

TEST(WeatherFirn, NonEligibleCellsGiveNone)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.week = rig.hydro.glacierCheckWeek;
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    rig.Working().snowPack[kCell + 1] = 0.9f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Glacier(kCell + 1), 0.0f);
    EXPECT_EQ(rig.Working().snowPack[kCell + 1], 0.9f);
}

TEST(WeatherFirn, UsesTheLatchedFloorAndWeek)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.hydro.glacierCheckWeek = 10;
    rig.hydro.glacierSnowFloor = 0.5f;
    rig.week = 10;
    rig.Capture();
    rig.Uniform(0.0f, 0.1f);
    rig.Working().snowPack[kCell] = 0.9f;
    RunSnowMeltPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Glacier(kCell), 0.4f, 1e-6f);
    EXPECT_NEAR(rig.Working().snowPack[kCell], 0.5f, 1e-6f);
}

TEST(WeatherFirn, SumWithAboveCapSnowStaysWithinZeroToTwoAfterFinish)
{
    Rig rig(Terrain());
    rig.Hydro({ kCell });
    rig.week = rig.hydro.glacierCheckWeek;
    rig.Capture();
    rig.Uniform(1.0f, 0.1f);
    rig.Working().snowPack[kCell] = 1.0f;
    RunPrecipitationPass(rig.ctx, rig.serial);   // above-cap 0.2 -> glacierInput
    RunSnowMeltPass(rig.ctx, rig.serial);        // firn 0.8 on top
    EXPECT_NEAR(rig.Glacier(kCell), 1.0f, 1e-6f);
    RunFinishPass(rig.ctx, rig.serial);
    EXPECT_GE(rig.Glacier(kCell), 0.0f);
    EXPECT_LE(rig.Glacier(kCell), 2.0f);
}

// ---- Finish (AC 19) -------------------------------------------------------------------------------------------------

TEST(WeatherFinish, ClampsEveryFieldToItsRange)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.0f, 0.0f);
    WeatherFields& f = rig.Working();
    f.moisture[0] = 1.7f;       f.moisture[1] = -0.4f;
    f.temperature[0] = 1.2f;    f.temperature[1] = -1.0f;
    f.pressure[0] = 3.0f;       f.pressure[1] = -2.0f;
    f.windSpeed[0] = 2.0f;      f.windSpeed[1] = -1.0f;
    f.cloudCover[0] = 5.0f;     f.cloudCover[1] = -5.0f;
    f.precipitation[0] = 4.0f;  f.precipitation[1] = -4.0f;
    f.snowPack[0] = 1.5f;       f.snowPack[1] = -0.5f;
    f.waterToGround[0] = 3.0f;  f.waterToGround[1] = -1.0f;
    rig.Glacier(0) = 3.0f;      rig.Glacier(1) = -1.0f;
    f.pressureAnomaly[0] = 0.9f; f.pressureAnomaly[1] = -0.9f;
    f.windDirection[0] = kTwoPi + 1.0f; f.windDirection[1] = -1.0f;
    RunFinishPass(rig.ctx, rig.serial);
    EXPECT_EQ(f.moisture[0], 1.0f);       EXPECT_EQ(f.moisture[1], 0.0f);
    EXPECT_EQ(f.temperature[0], 1.0f);    EXPECT_EQ(f.temperature[1], 0.0f);
    EXPECT_EQ(f.pressure[0], 1.0f);       EXPECT_EQ(f.pressure[1], 0.0f);
    EXPECT_EQ(f.windSpeed[0], 1.0f);      EXPECT_EQ(f.windSpeed[1], 0.0f);
    EXPECT_EQ(f.cloudCover[0], 1.0f);     EXPECT_EQ(f.cloudCover[1], 0.0f);
    EXPECT_EQ(f.precipitation[0], 1.0f);  EXPECT_EQ(f.precipitation[1], 0.0f);
    EXPECT_EQ(f.snowPack[0], 1.0f);       EXPECT_EQ(f.snowPack[1], 0.0f);
    EXPECT_EQ(f.waterToGround[0], 2.0f);  EXPECT_EQ(f.waterToGround[1], 0.0f);
    EXPECT_EQ(rig.Glacier(0), 2.0f);      EXPECT_EQ(rig.Glacier(1), 0.0f);
    EXPECT_EQ(f.pressureAnomaly[0], 0.5f); EXPECT_EQ(f.pressureAnomaly[1], -0.5f);
    EXPECT_NEAR(f.windDirection[0], 1.0f, 1e-5f);
    EXPECT_NEAR(f.windDirection[1], kTwoPi - 1.0f, 1e-5f);
    for (uint32_t c = 0; c < kN; ++c)
    {
        EXPECT_GE(f.windDirection[c], 0.0f);
        EXPECT_LT(f.windDirection[c], kTwoPi);
    }
}

TEST(WeatherFinish, NaNAndInfinityBecomeInRange)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0.0f, 0.0f);
    WeatherFields& f = rig.Working();
    f.moisture[0] = NAN; f.temperature[0] = NAN; f.pressure[0] = NAN; f.snowPack[0] = NAN;
    f.pressureAnomaly[0] = NAN; f.windDirection[0] = NAN; f.windDirection[1] = INFINITY;
    f.waterToGround[0] = NAN; rig.Glacier(0) = NAN;
    RunFinishPass(rig.ctx, rig.serial);
    EXPECT_EQ(f.moisture[0], 0.0f);
    EXPECT_EQ(f.temperature[0], 0.0f);
    EXPECT_EQ(f.pressure[0], 0.0f);
    EXPECT_EQ(f.snowPack[0], 0.0f);
    EXPECT_EQ(f.pressureAnomaly[0], 0.0f);
    EXPECT_EQ(f.waterToGround[0], 0.0f);
    EXPECT_EQ(rig.Glacier(0), 0.0f);
    EXPECT_EQ(f.windDirection[0], 0.0f);
    EXPECT_EQ(f.windDirection[1], 0.0f);
}

TEST(WeatherFinish, PipelineEndsAtOneAfterHeatInflowOverflow)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(2.0f, 1.0f);   // moisture above 1 as well
    RunFinishPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().moisture[kCell], 1.0f);
    EXPECT_EQ(rig.Working().temperature[kCell], 1.0f);
}

// ---- AC 32 grid totals ----------------------------------------------------------------------------------------------

namespace
{
    void Randomise(Rig& rig, uint32_t seed)
    {
        uint32_t s = seed;
        auto next = [&s]() { s = s * 1664525u + 1013904223u; return float((s >> 8) & 0xFFFFu) / 65535.0f; };
        WeatherFields& f = rig.Working();
        for (uint32_t c = 0; c < kN; ++c)
        {
            f.moisture[c] = next() * (next() < 0.5f ? 1.0f : 1e-4f);
            f.snowPack[c] = next();
            f.precipitation[c] = next() < 0.4f ? 0.0f : next();
        }
    }

    WeatherGridTotals Reference(const WeatherFields& f)
    {
        WeatherGridTotals t;
        double m = 0.0, s = 0.0;
        for (uint32_t c = 0; c < kN; ++c)
        {
            m += double(f.moisture[c]);
            s += double(f.snowPack[c]);
            if (f.precipitation[c] > 0.0f) ++t.rainingCellCount;
        }
        t.meanMoisture = m / double(kN);
        t.totalSnowPack = s;
        return t;
    }
}

TEST(WeatherTotals, EqualACellOrderReferenceBitForBit)
{
    Rig rig(Terrain());
    rig.Capture();
    Randomise(rig, 99u);
    RunFinishPass(rig.ctx, rig.serial);
    const WeatherGridTotals ref = Reference(rig.Working());   // after the clamp
    const WeatherGridTotals& got = rig.sim.GetWorkingTotals();
    EXPECT_EQ(0, memcmp(&got.meanMoisture, &ref.meanMoisture, sizeof(double)));
    EXPECT_EQ(0, memcmp(&got.totalSnowPack, &ref.totalSnowPack, sizeof(double)));
    EXPECT_EQ(got.rainingCellCount, ref.rainingCellCount);
    EXPECT_GT(got.rainingCellCount, 0u);
    EXPECT_LT(got.rainingCellCount, kN);
}

TEST(WeatherTotals, IdenticalAtAnyRangeCount)
{
    Rig a(Terrain()), b(Terrain()), c(Terrain());
    a.Capture(); b.Capture(); c.Capture();
    Randomise(a, 7u); Randomise(b, 7u); Randomise(c, 7u);
    WT::ReverseRangeExecutor reverse(5);
    WT::FixedRangeExecutor fixed(7);
    RunFinishPass(a.ctx, a.serial);
    RunFinishPass(b.ctx, reverse);
    RunFinishPass(c.ctx, fixed);
    const WeatherGridTotals& ta = a.sim.GetWorkingTotals();
    EXPECT_EQ(0, memcmp(&ta.meanMoisture, &b.sim.GetWorkingTotals().meanMoisture, sizeof(double)));
    EXPECT_EQ(0, memcmp(&ta.meanMoisture, &c.sim.GetWorkingTotals().meanMoisture, sizeof(double)));
    EXPECT_EQ(0, memcmp(&ta.totalSnowPack, &b.sim.GetWorkingTotals().totalSnowPack, sizeof(double)));
    EXPECT_EQ(0, memcmp(&ta.totalSnowPack, &c.sim.GetWorkingTotals().totalSnowPack, sizeof(double)));
    EXPECT_EQ(ta.rainingCellCount, b.sim.GetWorkingTotals().rainingCellCount);
    EXPECT_EQ(ta.rainingCellCount, c.sim.GetWorkingTotals().rainingCellCount);
}

TEST(WeatherTotals, CommitLatchesTheTotalsOfTheStep)
{
    Rig rig(Terrain());
    EXPECT_EQ(rig.sim.GetGridTotals().rainingCellCount, 0u);
    rig.sim.Step(rig.params, Season::Spring, 3, WT::TestRegistry(), nullptr, rig.hydro, rig.influences, rig.serial);
    const WeatherGridTotals ref = Reference(rig.sim.Fields());
    const WeatherGridTotals& got = rig.sim.GetGridTotals();
    EXPECT_EQ(0, memcmp(&got.meanMoisture, &ref.meanMoisture, sizeof(double)));
    EXPECT_EQ(0, memcmp(&got.totalSnowPack, &ref.totalSnowPack, sizeof(double)));
    EXPECT_EQ(got.rainingCellCount, ref.rainingCellCount);
    EXPECT_GT(got.meanMoisture, 0.0);
}

// ---- Full pipeline: AC 19, EC-03, EC-06, EC-20 ----------------------------------------------------------------------

namespace
{
    void ExpectAllInRange(const WeatherFields& f, const WeatherHandoff& h, const char* what, uint32_t week)
    {
        for (uint32_t c = 0; c < kN; ++c)
        {
            auto in = [](float v, float lo, float hi) { return v >= lo && v <= hi; };   // false for NaN
            ASSERT_TRUE(in(f.moisture[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.temperature[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.pressure[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.windSpeed[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.cloudCover[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.precipitation[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.snowPack[c], 0.0f, 1.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.waterToGround[c], 0.0f, 2.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(h.glacierInput[c], 0.0f, 2.0f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(in(f.pressureAnomaly[c], -0.5f, 0.5f)) << what << " week " << week << " cell " << c;
            ASSERT_TRUE(f.windDirection[c] >= 0.0f && f.windDirection[c] < kTwoPi) << what << " week " << week << " cell " << c;
        }
    }

    void RunYear(Rig& rig, float baseline, const char* what)
    {
        SeasonalBaselineInput input;
        input.temperature.Reserve(kN);
        for (uint32_t c = 0; c < kN; ++c) input.temperature.Add(baseline);
        for (uint32_t w = 0; w < 52; ++w)
        {
            rig.sim.Step(rig.params, Season::Summer, w, WT::TestRegistry(), &input, rig.hydro, rig.influences, rig.serial);
            ExpectAllInRange(rig.sim.Fields(), rig.sim.GetHandoff(), what, w);
            if (::testing::Test::HasFatalFailure()) return;
        }
    }
}

TEST(WeatherPipelineYear, TemperatureExtremesStayInRangeForAYear)
{
    WT::HexTerrainOptions o; o.latitudeRamp = true; o.waterColumns = 2;
    {
        Rig rig(WT::MakeHexTerrain(kW, kH, o));
        rig.Hydro({ 10, 11, 12, 20 });
        RunYear(rig, 0.0f, "cold");
    }
    {
        Rig rig(WT::MakeHexTerrain(kW, kH, o));
        rig.Hydro({ 10, 11, 12, 20 });
        RunYear(rig, 1.0f, "hot");
    }
}

TEST(WeatherPipelineYear, LatitudeRampWithWaterStripStaysFiniteAndInRange)
{
    WT::HexTerrainOptions o; o.latitudeRamp = true; o.waterColumns = 2;
    Rig rig(WT::MakeHexTerrain(kW, kH, o));
    rig.Hydro({ 10, 11, 12, 20 });
    for (uint32_t w = 0; w < 52; ++w)
    {
        const Season s = static_cast<Season>((w / 13) % 4);
        rig.sim.Step(rig.params, s, w, WT::TestRegistry(), nullptr, rig.hydro, rig.influences, rig.serial);
        ExpectAllInRange(rig.sim.Fields(), rig.sim.GetHandoff(), "ramp", w);
        if (::testing::Test::HasFatalFailure()) return;
    }
    const WeatherGridTotals& t = rig.sim.GetGridTotals();
    EXPECT_TRUE(t.meanMoisture >= 0.0 && t.meanMoisture <= 1.0);
    EXPECT_GE(t.totalSnowPack, 0.0);
    EXPECT_LE(t.rainingCellCount, kN);
}

TEST(WeatherPipelineYear, ColdCellKeepsItsSnowAtOneAllYear)
{
    Rig rig(Terrain());
    WeatherFields& committed = const_cast<WeatherFields&>(rig.sim.Fields());
    for (uint32_t c = 0; c < kN; ++c) { committed.snowPack[c] = 1.0f; committed.temperature[c] = 0.0f; }   // start cold too
    SeasonalBaselineInput cold;
    cold.temperature.Reserve(kN);
    for (uint32_t c = 0; c < kN; ++c) cold.temperature.Add(0.0f);
    for (uint32_t w = 0; w < 52; ++w)
    {
        rig.sim.Step(rig.params, Season::Winter, w, WT::TestRegistry(), &cold, rig.hydro, rig.influences, rig.serial);
        for (uint32_t c = 0; c < kN; ++c)
        {
            ASSERT_LT(rig.sim.Fields().temperature[c], rig.params.meltThreshold) << "week " << w;
            ASSERT_EQ(rig.sim.Fields().snowPack[c], 1.0f) << "week " << w << " cell " << c;
        }
    }
}
