// A4 weather ca-pipeline Task 8: whole-pipeline integration tests.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 23, 31)
#include <gtest/gtest.h>

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

    // Deterministic generator in [0, 1).
    struct Lcg
    {
        uint32_t s;
        explicit Lcg(uint32_t seed) : s(seed) {}
        float Next()
        {
            s = s * 1664525u + 1013904223u;
            return float(s >> 8) / float(1u << 24);
        }
    };

    // ---- AC 23 ----------------------------------------------------------------------------------------------------

    constexpr uint32_t kZW = 40, kZH = 20, kZN = kZW * kZH;
    constexpr uint32_t kWeeks = 12;

    struct Centroid { double col; double row; double mass; };

    Centroid MassCentroid(const WeatherFields& f)
    {
        Centroid r = { 0.0, 0.0, 0.0 };
        for (uint32_t c = 0; c < kZN; ++c)
        {
            const double m = f.moisture[c];
            r.col += m * double(c % kZW);
            r.row += m * double(c / kZW);
            r.mass += m;
        }
        r.col /= r.mass;
        r.row /= r.mass;
        return r;
    }

    // Pulse of moisture 0.5 in one interior cell of an otherwise dry, flat, uniform world. Only transport acts.
    Centroid RunPulse(float latitude, uint32_t startCol, uint32_t startRow, Centroid& start, float& windSpeed)
    {
        WT::HexTerrainOptions o; o.latitude = latitude; o.waterColumns = 1;
        WeatherParameters p = WT::MakeTestParams();
        p.oceanEvapRate = 0.0f; p.groundEvapRate = 0.0f;
        p.orographicPrecipRate = 0.0f; p.orographicCoolingRate = 0.0f;
        p.rainThreshold = 1.0f;
        p.moistureTransportRate = 1.0f;
        p.edgeInflow = false;
        windSpeed = p.prevailingStrength;

        WeatherSimulation sim;
        EXPECT_TRUE(sim.Initialise(WT::MakeHexTerrain(kZW, kZH, o), p, WT::TestRegistry()));
        WeatherFields& committed = const_cast<WeatherFields&>(sim.Fields());
        for (uint32_t c = 0; c < kZN; ++c) committed.moisture[c] = 0.0f;
        committed.moisture[startRow * kZW + startCol] = 0.5f;
        start = MassCentroid(committed);

        HydrologyInputs hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;
        for (uint32_t w = 0; w < kWeeks; ++w)
        {
            sim.Step(p, Season::Spring, w, WT::TestRegistry(), nullptr, hydro, influences, serial);
        }
        return MassCentroid(sim.Fields());
    }
}

// Wind speed is prevailingStrength (uniform pressure, no gradient) and transport moves moisture*speed*rate one column
// per step, so the centroid advances speed * rate columns a week; both zigzag directions advance a column.
TEST(WeatherIntegration, TradeWindPulseDriftsEastWithNoNetRowDrift)
{
    Centroid start, end; float speed;
    end = RunPulse(0.1f, 10, 10, start, speed);
    const double expected = double(kWeeks) * speed;
    EXPECT_NEAR(end.mass, start.mass, 1e-3);                    // nothing lost or gained
    EXPECT_NEAR(end.col - start.col, expected, 0.5);
    EXPECT_GT(end.col - start.col, 0.0);
    EXPECT_LE(fabs(end.row - start.row), 1.0);
}

TEST(WeatherIntegration, WesterlyPulseDriftsWestWithNoNetRowDrift)
{
    Centroid start, end; float speed;
    end = RunPulse(0.45f, 28, 10, start, speed);
    const double expected = double(kWeeks) * speed;
    EXPECT_NEAR(end.mass, start.mass, 1e-3);
    EXPECT_NEAR(start.col - end.col, expected, 0.5);
    EXPECT_GT(start.col - end.col, 0.0);
    EXPECT_LE(fabs(end.row - start.row), 1.0);
}

// ---- AC 31 first assertion ------------------------------------------------------------------------------------------

namespace
{
    constexpr uint32_t kW = 20, kH = 12, kN = kW * kH;

    struct World
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;

        World()
        {
            WT::HexTerrainOptions o; o.latitudeRamp = true; o.waterColumns = 3;
            WeatherTerrain t = WT::MakeHexTerrain(kW, kH, o);
            Lcg rng(12345u);
            for (uint32_t c = 0; c < kN; ++c) t.elevation[c] = (c % kW) < 3 ? 0.0f : rng.Next();
            EXPECT_TRUE(sim.Initialise(t, params, WT::TestRegistry()));

            WeatherFields& f = const_cast<WeatherFields&>(sim.Fields());
            Lcg state(777u);
            for (uint32_t c = 0; c < kN; ++c)
            {
                f.pressureAnomaly[c] = (state.Next() - 0.5f) * 0.4f;
                f.moisture[c] = state.Next();
                f.temperature[c] = state.Next();
                f.snowPack[c] = state.Next() * 0.5f;
            }
        }

        void Steps(uint32_t count, Dia::ParallelExecutor::IParallelExecutor& ex)
        {
            for (uint32_t w = 0; w < count; ++w)
            {
                sim.Step(params, static_cast<Season>((w / 2) % 4), w, WT::TestRegistry(), nullptr, hydro, influences, ex);
            }
        }
    };

    void ExpectIdentical(const World& a, const World& b, const char* what)
    {
        EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields())) << what;
        const WeatherGridTotals& ta = a.sim.GetGridTotals();
        const WeatherGridTotals& tb = b.sim.GetGridTotals();
        EXPECT_EQ(0, memcmp(&ta.meanMoisture, &tb.meanMoisture, sizeof(double))) << what;
        EXPECT_EQ(0, memcmp(&ta.totalSnowPack, &tb.totalSnowPack, sizeof(double))) << what;
        EXPECT_EQ(ta.rainingCellCount, tb.rainingCellCount) << what;
        EXPECT_EQ(a.sim.GetCommittedWeek(), b.sim.GetCommittedWeek()) << what;
    }

    void CompareAll(uint32_t steps)
    {
        World ref;
        Dia::ParallelExecutor::SerialExecutor serial;
        ref.Steps(steps, serial);
        EXPECT_GT(ref.sim.GetGridTotals().meanMoisture, 0.0);

        { World w; WT::ReverseRangeExecutor ex;   w.Steps(steps, ex); ExpectIdentical(ref, w, "reverse default"); }
        { World w; WT::ReverseRangeExecutor ex(7); w.Steps(steps, ex); ExpectIdentical(ref, w, "reverse 7"); }
        { World w; WT::FixedRangeExecutor ex(1);  w.Steps(steps, ex); ExpectIdentical(ref, w, "fixed 1"); }
        { World w; WT::FixedRangeExecutor ex(3);  w.Steps(steps, ex); ExpectIdentical(ref, w, "fixed 3"); }
        { World w; WT::FixedRangeExecutor ex(17); w.Steps(steps, ex); ExpectIdentical(ref, w, "fixed 17"); }
        { World w; WT::FixedRangeExecutor ex(kN); w.Steps(steps, ex); ExpectIdentical(ref, w, "fixed N"); }
        { World w; WT::ReverseRangeExecutor ex(kN); w.Steps(steps, ex); ExpectIdentical(ref, w, "reverse N"); }
    }
}

TEST(WeatherIntegration, OneStepIsByteIdenticalAtAnyRangeCountAndOrder)
{
    CompareAll(1);
}

TEST(WeatherIntegration, EightStepsAcrossSeasonsAreByteIdenticalAtAnyRangeCountAndOrder)
{
    CompareAll(8);
}
