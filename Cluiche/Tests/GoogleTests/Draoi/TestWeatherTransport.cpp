// A4 weather ca-pipeline Task 5: pass 4 Evaporation, 5a/5b moisture + anomaly transport, 5c anomaly decay,
// 6a/6b heat transport.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 11, 12, 12a, 12b, 13, 20, 28)
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

    constexpr uint32_t kW = 9, kH = 9, kN = kW * kH;
    constexpr uint32_t kMid = 4 * kW + 4;   // interior cell, at least 4 cells from every edge

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

        // Every cell blows in direction d at `speed`; moisture, temperature and anomaly zeroed.
        void Uniform(uint32_t d, float speed)
        {
            WeatherFields& f = Working();
            for (uint32_t c = 0; c < kN; ++c)
            {
                ctx.tables->dirIndex[c] = static_cast<uint8_t>(d);
                f.windSpeed[c] = speed;
                f.moisture[c] = 0.0f; f.temperature[c] = 0.0f; f.pressureAnomaly[c] = 0.0f;
            }
        }
        uint32_t Downwind(uint32_t c) const { return sim.Tables().neighbours[c * kHexDirections + sim.Tables().dirIndex[c]]; }
    };

    WeatherTerrain Terrain() { return WT::MakeHexTerrain(kW, kH, WT::HexTerrainOptions()); }

    void RunTransport(Rig& rig, Dia::ParallelExecutor::IParallelExecutor& ex)
    {
        RunMoistureOutPass(rig.ctx, ex);
        RunMoistureGatherPass(rig.ctx, ex);
    }

    // ---- deterministic mixed-magnitude field ----
    uint32_t Next(uint32_t& s) { s = s * 1664525u + 1013904223u; return s >> 8; }
    float Unit(uint32_t& s) { return float(Next(s) & 0xFFFFu) / 65535.0f; }

    void Seed(Rig& rig, uint32_t seed)
    {
        WeatherFields& f = rig.Working();
        uint32_t s = seed;
        for (uint32_t c = 0; c < kN; ++c)
        {
            rig.ctx.tables->dirIndex[c] = static_cast<uint8_t>(Next(s) % 6);
            f.windSpeed[c] = 0.3f + 0.7f * Unit(s);
            f.moisture[c] = Unit(s) * powf(10.0f, -float(Next(s) % 6));
            f.temperature[c] = Unit(s) * powf(10.0f, -float(Next(s) % 4));
            f.pressureAnomaly[c] = (Unit(s) - 0.5f) * powf(10.0f, -float(Next(s) % 6));
        }
    }

    // Independent reference of the spec's gather: ascending d, then the edge term last.
    float RefInflow(uint32_t c, const GridTables& t, const float* out, bool edge, bool reverse)
    {
        float sum = 0.0f;
        for (uint32_t i = 0; i < 6; ++i)
        {
            const uint32_t d = reverse ? 5 - i : i;
            const uint32_t n = t.neighbours[c * 6 + d];
            if (n == kNoCell) continue;
            if (t.dirIndex[n] != (d + 3) % 6) continue;
            sum += out[n];
        }
        if (edge && t.neighbours[c * 6 + (t.dirIndex[c] + 3) % 6] == kNoCell) sum += out[c];
        return sum;
    }

    // Reference for one transport (moisture-like quantity v with rate): returns new values.
    void RefTransport(const GridTables& t, const float* v, const float* speed, float rate, bool edge, bool reverse,
                      float* outScratch, float* result)
    {
        for (uint32_t c = 0; c < kN; ++c)
            outScratch[c] = t.neighbours[c * 6 + t.dirIndex[c]] == kNoCell ? 0.0f : v[c] * speed[c] * rate;
        for (uint32_t c = 0; c < kN; ++c)
            result[c] = v[c] - outScratch[c] + RefInflow(c, t, outScratch, edge, reverse);
    }

    void CopyOut(const Dia::Core::Containers::DynamicArray<float>& a, float* dst) { for (uint32_t i = 0; i < kN; ++i) dst[i] = a[i]; }
}

// ---- Pass 4 Evaporation (AC 11) -------------------------------------------------------------------------------------

TEST(WeatherPassEvap, SaturatedWaterCellGainsNothing)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().temperature[0] = 0.9f;
    rig.Working().moisture[0] = 1.0f;
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().moisture[0], 1.0f);
}

TEST(WeatherPassEvap, WaterCellGainsPrimaryAmountSelfLimited)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().temperature[0] = 0.5f;
    rig.Working().moisture[0] = 0.2f;
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[0], 0.2f + 0.5f * rig.params.oceanEvapRate * 0.8f, 1e-6f);
}

TEST(WeatherPassEvap, EvapMultiplierScalesPrimary)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Latched().climate.evapMultiplier[0] = 0.5f;
    rig.Working().temperature[0] = 0.5f;
    rig.Working().moisture[0] = 0.0f;
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[0], 0.5f * rig.params.oceanEvapRate * 0.5f, 1e-6f);
}

TEST(WeatherPassEvap, NoHydrologyLandCellGainsNothing)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().temperature[5] = 0.9f;
    rig.Working().moisture[5] = 0.3f;
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().moisture[5], 0.3f);   // openWater 0, surfaceWetness 0 with no A5
}

TEST(WeatherPassEvap, SecondaryUsesSurfaceWetnessAndGroundRate)
{
    Rig rig(Terrain());
    rig.hydro.Resize(kN);
    rig.hydro.present = true;
    rig.hydro.surfaceWetness[5] = 0.5f;
    rig.Capture();
    rig.Working().temperature[5] = 0.5f;
    rig.Working().moisture[5] = 0.1f;
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[5], 0.1f + 0.5f * 0.5f * rig.params.groundEvapRate, 1e-6f);
}

TEST(WeatherPassEvap, SecondaryNeverMovesMoreThanTheCellHoldsNorNegative)
{
    Rig rig(Terrain());
    rig.params.groundEvapRate = 10.0f;   // would demand far more than the cell holds
    rig.hydro.Resize(kN);
    rig.hydro.present = true;
    rig.hydro.surfaceWetness[5] = 0.3f;
    rig.hydro.surfaceWetness[6] = 0.3f;
    rig.Capture();
    rig.Working().temperature[5] = 1.0f; rig.Working().moisture[5] = 0.0f;
    rig.Working().temperature[6] = 0.0f; rig.Working().moisture[6] = 0.4f;   // cold: no evaporation
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[5], 0.3f, 1e-6f);
    EXPECT_EQ(rig.Working().moisture[6], 0.4f);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_GE(rig.Working().moisture[c], 0.0f);
}

TEST(WeatherPassEvap, OpenWaterFractionScalesPrimaryAndOverridesWaterSource)
{
    Rig rig(Terrain());
    rig.hydro.Resize(kN);
    rig.hydro.present = true;
    rig.hydro.openWater[0] = 0.25f;   // a water-source cell, A5 says a quarter open
    rig.hydro.openWater[5] = 1.0f;    // a land cell A5 says is a lake
    rig.Capture();
    for (uint32_t c : { 0u, 5u }) { rig.Working().temperature[c] = 0.5f; rig.Working().moisture[c] = 0.0f; }
    RunEvaporationPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[0], 0.5f * rig.params.oceanEvapRate * 0.25f, 1e-6f);
    EXPECT_NEAR(rig.Working().moisture[5], 0.5f * rig.params.oceanEvapRate, 1e-6f);
}

// ---- Passes 5a/5b moisture transport (AC 12, 20, EC-16) -------------------------------------------------------------

TEST(WeatherTransportMoisture, PulseMovesOneCellDownwindAtTheStatedRate)
{
    for (uint32_t d = 0; d < 6; ++d)
    {
        Rig rig(Terrain());
        rig.params.edgeInflow = false;
        rig.Capture();
        rig.Uniform(d, 1.0f);
        rig.Working().moisture[kMid] = 0.5f;
        const uint32_t down = rig.Downwind(kMid);
        ASSERT_NE(down, kNoCell);
        RunTransport(rig, rig.serial);
        EXPECT_NEAR(rig.Working().moisture[down], 0.5f * rig.params.moistureTransportRate, 1e-6f) << "d=" << d;
        EXPECT_NEAR(rig.Working().moisture[kMid], 0.5f * (1.0f - rig.params.moistureTransportRate), 1e-6f) << "d=" << d;
    }
}

TEST(WeatherTransportMoisture, WindSpeedScalesTheOutflow)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 0.5f);
    rig.Working().moisture[kMid] = 0.8f;
    RunTransport(rig, rig.serial);
    EXPECT_NEAR(rig.Working().moisture[rig.Downwind(kMid)], 0.8f * 0.5f * rig.params.moistureTransportRate, 1e-6f);
}

TEST(WeatherTransportMoisture, ClosedInteriorConservesTotalOverSeveralSteps)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 1.0f);
    rig.Working().moisture[kMid] = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        RunTransport(rig, rig.serial);
        float total = 0.0f;
        for (uint32_t c = 0; c < kN; ++c) total += rig.Working().moisture[c];
        EXPECT_NEAR(total, 1.0f, 1e-5f) << "step " << i;
    }
}

TEST(WeatherTransportMoisture, OffMapDownwindKeepsItsMoisture)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 1.0f);
    uint32_t edgeCell = kNoCell;
    for (uint32_t c = 0; c < kN; ++c) if (rig.Downwind(c) == kNoCell) { edgeCell = c; break; }
    ASSERT_NE(edgeCell, kNoCell);
    rig.Working().moisture[edgeCell] = 0.6f;
    RunTransport(rig, rig.serial);
    EXPECT_EQ(rig.Working().moisture[edgeCell], 0.6f);
}

TEST(WeatherTransportMoisture, ZeroWindTransportsNothing)
{
    Rig rig(Terrain());   // edge_inflow on: the edge term is 0 as well when out is 0
    rig.Capture();
    Seed(rig, 7u);
    for (uint32_t c = 0; c < kN; ++c) rig.Working().windSpeed[c] = 0.0f;
    float before[kN], beforeA[kN];
    CopyOut(rig.Working().moisture, before);
    CopyOut(rig.Working().pressureAnomaly, beforeA);
    RunTransport(rig, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
    {
        EXPECT_EQ(rig.Working().moisture[c], before[c]);
        EXPECT_EQ(rig.Working().pressureAnomaly[c], beforeA[c]);
    }
}

TEST(WeatherTransportMoisture, UniformMoistureStaysUniformExceptDownwindEdgeWithEdgeInflowOn)
{
    // Spec text (AC 25) says uniform stays uniform everywhere; per the gather as written, a downwind-edge cell has
    // outflow 0 but still receives its upwind neighbour's outflow, so it gains. Interior and upwind-edge cells hold.
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0, 1.0f);
    for (uint32_t c = 0; c < kN; ++c) rig.Working().moisture[c] = 0.4f;
    RunTransport(rig, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
    {
        if (rig.Downwind(c) == kNoCell) continue;
        EXPECT_NEAR(rig.Working().moisture[c], 0.4f, 1e-6f) << "cell " << c;
    }
}

TEST(WeatherTransportMoisture, InflowOrderMatchesReferenceBitForBitAndOrderMatters)
{
    Rig rig(Terrain());
    rig.Capture();
    Seed(rig, 12345u);
    {   // Six neighbours all blowing into kMid with mixed magnitudes, so summation order changes the float result.
        const float mags[6] = { 0.9f, 3e-8f, 0.4f, 5e-8f, 0.123f, 7e-8f };
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t n = rig.sim.Tables().neighbours[kMid * 6 + d];
            rig.ctx.tables->dirIndex[n] = static_cast<uint8_t>((d + 3) % 6);
            rig.Working().windSpeed[n] = 1.0f;
            rig.Working().moisture[n] = mags[d];
        }
        rig.ctx.tables->dirIndex[kMid] = 0;
    }
    float v[kN], sp[kN], outS[kN], fwd[kN], rev[kN];
    CopyOut(rig.Working().moisture, v);
    CopyOut(rig.Working().windSpeed, sp);
    const GridTables& t = rig.sim.Tables();
    RefTransport(t, v, sp, rig.params.moistureTransportRate, true, false, outS, fwd);
    RefTransport(t, v, sp, rig.params.moistureTransportRate, true, true, outS, rev);
    uint32_t differing = 0;
    for (uint32_t c = 0; c < kN; ++c) if (memcmp(&fwd[c], &rev[c], sizeof(float)) != 0) ++differing;
    ASSERT_GT(differing, 0u) << "case does not discriminate summation order";

    RunTransport(rig, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
        EXPECT_EQ(memcmp(&fwd[c], &rig.Working().moisture[c], sizeof(float)), 0) << "cell " << c;
}

// ---- Anomaly transport (AC 12a) -------------------------------------------------------------------------------------

TEST(WeatherTransportAnomaly, SignedPulseMovesOneCellDownwindAtMoistureRate)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(1, 1.0f);
    rig.Working().pressureAnomaly[kMid] = -0.4f;
    const uint32_t down = rig.Downwind(kMid);
    RunTransport(rig, rig.serial);
    EXPECT_NEAR(rig.Working().pressureAnomaly[down], -0.4f * rig.params.moistureTransportRate, 1e-6f);
    EXPECT_NEAR(rig.Working().pressureAnomaly[kMid], -0.4f * (1.0f - rig.params.moistureTransportRate), 1e-6f);
}

TEST(WeatherTransportAnomaly, OffMapDownwindKeepsItsAnomaly)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 1.0f);
    uint32_t edgeCell = kNoCell;
    for (uint32_t c = 0; c < kN; ++c) if (rig.Downwind(c) == kNoCell) { edgeCell = c; break; }
    ASSERT_NE(edgeCell, kNoCell);
    rig.Working().pressureAnomaly[edgeCell] = -0.25f;
    RunTransport(rig, rig.serial);
    EXPECT_EQ(rig.Working().pressureAnomaly[edgeCell], -0.25f);
}

TEST(WeatherTransportAnomaly, UniformAnomalyStaysUniformBeforeDecayWithEdgeInflowOn)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Uniform(0, 1.0f);
    for (uint32_t c = 0; c < kN; ++c) rig.Working().pressureAnomaly[c] = -0.2f;
    RunTransport(rig, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
    {
        if (rig.Downwind(c) == kNoCell) continue;   // see the moisture uniform test for the downwind-edge note
        EXPECT_NEAR(rig.Working().pressureAnomaly[c], -0.2f, 1e-6f) << "cell " << c;
    }
}

TEST(WeatherTransportAnomaly, InflowOrderMatchesReferenceBitForBit)
{
    Rig rig(Terrain());
    rig.Capture();
    Seed(rig, 999u);
    float v[kN], sp[kN], outS[kN], fwd[kN];
    CopyOut(rig.Working().pressureAnomaly, v);
    CopyOut(rig.Working().windSpeed, sp);
    RefTransport(rig.sim.Tables(), v, sp, rig.params.moistureTransportRate, true, false, outS, fwd);
    RunTransport(rig, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
        EXPECT_EQ(memcmp(&fwd[c], &rig.Working().pressureAnomaly[c], sizeof(float)), 0) << "cell " << c;
}

// ---- Pass 5c Anomaly decay (AC 12b) ---------------------------------------------------------------------------------

TEST(WeatherPassAnomalyDecay, MultipliesByExactlyTheDecay)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().pressureAnomaly[3] = 0.2f;
    rig.Working().pressureAnomaly[4] = -0.13f;
    RunAnomalyDecayPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().pressureAnomaly[3], 0.2f * rig.params.pressureAnomalyDecay);
    EXPECT_EQ(rig.Working().pressureAnomaly[4], -0.13f * rig.params.pressureAnomalyDecay);
}

TEST(WeatherPassAnomalyDecay, ClampsToPlusMinusMax)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().pressureAnomaly[3] = 0.9f;
    rig.Working().pressureAnomaly[4] = -0.9f;
    RunAnomalyDecayPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().pressureAnomaly[3], rig.params.pressureAnomalyMax);
    EXPECT_EQ(rig.Working().pressureAnomaly[4], -rig.params.pressureAnomalyMax);
}

TEST(WeatherPassAnomalyDecay, SeededLowRoughlyHalvesInTwoWeeks)
{
    Rig rig(Terrain());
    rig.Capture();
    rig.Working().pressureAnomaly[3] = -0.3f;
    RunAnomalyDecayPass(rig.ctx, rig.serial);
    RunAnomalyDecayPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().pressureAnomaly[3], -0.15f, 0.01f);
}

TEST(WeatherPassAnomalyDecay, ZeroStaysZero)
{
    Rig rig(Terrain());
    rig.Capture();
    for (uint32_t c = 0; c < kN; ++c) rig.Working().pressureAnomaly[c] = 0.0f;
    RunAnomalyDecayPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_EQ(rig.Working().pressureAnomaly[c], 0.0f);
}

// ---- Passes 6a/6b heat transport (AC 13) ----------------------------------------------------------------------------

TEST(WeatherTransportHeat, PulseMovesDownwindAtHeatRate)
{
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(2, 1.0f);
    rig.Working().temperature[kMid] = 0.5f;
    const uint32_t down = rig.Downwind(kMid);
    RunHeatOutPass(rig.ctx, rig.serial);
    RunHeatGatherPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[down], 0.5f * rig.params.heatTransportRate, 1e-6f);
    EXPECT_NEAR(rig.Working().temperature[kMid], 0.5f * (1.0f - rig.params.heatTransportRate), 1e-6f);
}

TEST(WeatherTransportHeat, TemperatureNeverGoesNegative)
{
    Rig rig(Terrain());
    rig.params.heatTransportRate = 20.0f;   // outflow far exceeds the cell
    rig.Capture();
    Seed(rig, 4242u);
    RunHeatOutPass(rig.ctx, rig.serial);
    RunHeatGatherPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_GE(rig.Working().temperature[c], 0.0f);
}

TEST(WeatherTransportHeat, FullCellWithInflowIsMaxOfRemainderPlusInflow)
{
    // The pass itself does not clamp to 1: max(t - out, 0) + inflow. The finish pass (task 7) clamps [0,1].
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 1.0f);
    const uint32_t down = rig.Downwind(kMid);
    rig.Working().temperature[kMid] = 1.0f;
    rig.Working().temperature[down] = 1.0f;
    rig.Working().windSpeed[down] = 0.0f;   // receiver sends nothing
    RunHeatOutPass(rig.ctx, rig.serial);
    RunHeatGatherPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[down], 1.0f + rig.params.heatTransportRate, 1e-6f);
    RunFinishPass(rig.ctx, rig.serial);   // the finish clamp brings it back to exactly 1.0
    EXPECT_EQ(rig.Working().temperature[down], 1.0f);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_LE(rig.Working().temperature[c], 1.0f);
}

TEST(WeatherTransportHeat, InflowOrderMatchesReferenceBitForBit)
{
    Rig rig(Terrain());
    rig.Capture();
    Seed(rig, 31337u);
    float v[kN], sp[kN], outS[kN];
    CopyOut(rig.Working().temperature, v);
    CopyOut(rig.Working().windSpeed, sp);
    const GridTables& t = rig.sim.Tables();
    for (uint32_t c = 0; c < kN; ++c)
        outS[c] = t.neighbours[c * 6 + t.dirIndex[c]] == kNoCell ? 0.0f : v[c] * sp[c] * rig.params.heatTransportRate;
    float ref[kN];
    for (uint32_t c = 0; c < kN; ++c)
    {
        const float rem = v[c] - outS[c];
        ref[c] = (rem > 0.0f ? rem : 0.0f) + RefInflow(c, t, outS, true, false);
    }
    RunHeatOutPass(rig.ctx, rig.serial);
    RunHeatGatherPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < kN; ++c)
        EXPECT_EQ(memcmp(&ref[c], &rig.Working().temperature[c], sizeof(float)), 0) << "cell " << c;
}

// ---- Boundaries across both transports (AC 25, 26, 27, EC-16) ------------------------------------------------------------

namespace
{
    constexpr uint32_t kWeeks = 52;

    // One week of both transports with the wind held fixed (passes 5a, 5b, 6a, 6b).
    void RunWeek(Rig& rig)
    {
        RunMoistureOutPass(rig.ctx, rig.serial);
        RunMoistureGatherPass(rig.ctx, rig.serial);
        RunHeatOutPass(rig.ctx, rig.serial);
        RunHeatGatherPass(rig.ctx, rig.serial);
    }

    void SetUniformField(Rig& rig, float moisture, float temperature)
    {
        for (uint32_t c = 0; c < kN; ++c) { rig.Working().moisture[c] = moisture; rig.Working().temperature[c] = temperature; }
    }
}

TEST(WeatherTransportBoundary, UniformFieldsHoldFor52WeeksExceptDownwindEdgeCellsWhichOnlyGain)
{
    // AC 25 says a uniform field stays uniform with edge_inflow on. Known spec conflict with EC-16: a cell whose downwind
    // neighbour is off-map keeps its outflow (out = 0) yet still receives its upwind neighbour's outflow, so it
    // accumulates. Every other cell, including upwind-edge cells (edge term cancels their outflow), must hold.
    for (uint32_t d = 0; d < 6; ++d)
    {
        Rig rig(Terrain());
        rig.Capture();
        rig.Uniform(d, 1.0f);
        SetUniformField(rig, 0.4f, 0.4f);
        const float mRate = rig.params.moistureTransportRate, hRate = rig.params.heatTransportRate;
        for (uint32_t w = 0; w < kWeeks; ++w) RunWeek(rig);
        uint32_t edgeCells = 0;
        for (uint32_t c = 0; c < kN; ++c)
        {
            if (rig.Downwind(c) == kNoCell)
            {
                ++edgeCells;
                EXPECT_GE(rig.Working().moisture[c], 0.4f - 1e-5f) << "d=" << d << " cell " << c;
                EXPECT_LE(rig.Working().moisture[c], 0.4f + kWeeks * 0.4f * mRate + 1e-4f) << "d=" << d << " cell " << c;
                EXPECT_GE(rig.Working().temperature[c], 0.4f - 1e-5f) << "d=" << d << " cell " << c;
                EXPECT_LE(rig.Working().temperature[c], 0.4f + kWeeks * 0.4f * hRate + 1e-4f) << "d=" << d << " cell " << c;
            }
            else
            {
                EXPECT_NEAR(rig.Working().moisture[c], 0.4f, 1e-5f) << "d=" << d << " cell " << c;
                EXPECT_NEAR(rig.Working().temperature[c], 0.4f, 1e-5f) << "d=" << d << " cell " << c;
            }
        }
        EXPECT_GT(edgeCells, 0u);
    }
}

TEST(WeatherTransportBoundary, EdgeInflowOffUpwindEdgeCellsLoseMoistureAndHeatEveryWeek)
{
    // AC 26: with no edge inflow the upwind-edge cells (upwind off-map, downwind on-map) have nothing coming in.
    Rig rig(Terrain());
    rig.params.edgeInflow = false;
    rig.Capture();
    rig.Uniform(0, 1.0f);
    SetUniformField(rig, 0.4f, 0.4f);
    uint32_t upwindEdge[kN]; uint32_t count = 0;
    for (uint32_t c = 0; c < kN; ++c)
    {
        const uint32_t up = rig.sim.Tables().neighbours[c * 6 + (rig.sim.Tables().dirIndex[c] + 3) % 6];
        if (up == kNoCell && rig.Downwind(c) != kNoCell) upwindEdge[count++] = c;
    }
    ASSERT_GT(count, 0u);
    const float mKeep = 1.0f - rig.params.moistureTransportRate, hKeep = 1.0f - rig.params.heatTransportRate;
    float expectM = 0.4f, expectH = 0.4f;
    for (uint32_t w = 0; w < 5; ++w)
    {
        float prevM[kN], prevH[kN];
        CopyOut(rig.Working().moisture, prevM);
        CopyOut(rig.Working().temperature, prevH);
        RunWeek(rig);
        expectM *= mKeep; expectH *= hKeep;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t c = upwindEdge[i];
            EXPECT_LT(rig.Working().moisture[c], prevM[c]) << "week " << w << " cell " << c;
            EXPECT_LT(rig.Working().temperature[c], prevH[c]) << "week " << w << " cell " << c;
            EXPECT_NEAR(rig.Working().moisture[c], expectM, 1e-5f) << "week " << w << " cell " << c;
            EXPECT_NEAR(rig.Working().temperature[c], expectH, 1e-5f) << "week " << w << " cell " << c;
        }
    }
}

TEST(WeatherTransportBoundary, CellWithBothNeighboursOffMapGainsAndLosesNothing)
{
    // AC 27: a 1-wide column has no NE or SW neighbour, so with wind NE (d = 0) every cell has an off-map downwind and
    // an off-map upwind. Its own outflow stays in the cell (EC-16) and the edge term returns only that zero.
    for (int edge = 0; edge < 2; ++edge)
    {
        WT::HexTerrainOptions o;
        Rig rig(WT::MakeHexTerrain(1, 3, o));
        rig.params.edgeInflow = edge != 0;
        rig.Capture();
        WeatherFields& f = rig.Working();
        const GridTables& t = rig.sim.Tables();
        for (uint32_t c = 0; c < 3; ++c)
        {
            rig.ctx.tables->dirIndex[c] = 0;
            f.windSpeed[c] = 1.0f;
            f.moisture[c] = 0.3f + 0.1f * float(c);
            f.temperature[c] = 0.5f + 0.1f * float(c);
            f.pressureAnomaly[c] = 0.0f;
            ASSERT_EQ(t.neighbours[c * 6 + 0], kNoCell);
            ASSERT_EQ(t.neighbours[c * 6 + 3], kNoCell);
        }
        for (uint32_t w = 0; w < kWeeks; ++w)
        {
            RunMoistureOutPass(rig.ctx, rig.serial);
            RunMoistureGatherPass(rig.ctx, rig.serial);
            RunHeatOutPass(rig.ctx, rig.serial);
            RunHeatGatherPass(rig.ctx, rig.serial);
        }
        for (uint32_t c = 0; c < 3; ++c)
        {
            EXPECT_EQ(f.moisture[c], 0.3f + 0.1f * float(c)) << "edge=" << edge << " cell " << c;
            EXPECT_EQ(f.temperature[c], 0.5f + 0.1f * float(c)) << "edge=" << edge << " cell " << c;
        }
    }
}

// ---- Range independence ---------------------------------------------------------------------------------------------

TEST(WeatherTransportExecutors, ReverseAndFixedRangesGiveIdenticalBytes)
{
    Rig a(Terrain()), b(Terrain()), c(Terrain());
    a.Capture(); b.Capture(); c.Capture();
    Seed(a, 2024u); Seed(b, 2024u); Seed(c, 2024u);
    // Evaporation reads latched water sources: identical across rigs.
    WT::ReverseRangeExecutor reverse(5);
    WT::FixedRangeExecutor fixed(7);
    auto run = [](Rig& r, Dia::ParallelExecutor::IParallelExecutor& ex)
    {
        RunEvaporationPass(r.ctx, ex);
        RunMoistureOutPass(r.ctx, ex);
        RunMoistureGatherPass(r.ctx, ex);
        RunAnomalyDecayPass(r.ctx, ex);
        RunHeatOutPass(r.ctx, ex);
        RunHeatGatherPass(r.ctx, ex);
    };
    run(a, a.serial);
    run(b, reverse);
    run(c, fixed);
    EXPECT_TRUE(WT::SameFields(a.Working(), b.Working()));
    EXPECT_TRUE(WT::SameFields(a.Working(), c.Working()));
}

TEST(WeatherTransportPipeline, RunPipelineUntilStopsAfterEachNewPass)
{
    Rig rig(Terrain());
    rig.Capture();
    EXPECT_EQ(PassId::kLast, PassId::Finish);
    RunPipelineUntil(rig.ctx, rig.serial, PassId::kLast);
    for (uint32_t c = 0; c < kN; ++c) EXPECT_GE(rig.Working().temperature[c], 0.0f);
}
