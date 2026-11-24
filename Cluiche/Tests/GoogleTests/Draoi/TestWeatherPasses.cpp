// A4 weather ca-pipeline Task 4: passes 1 Solar, 2 Pressure, 3 Wind.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 7, 8, 9, 9a, 10, 10a)
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

    constexpr float kPiF = 3.14159265358979323846f;
    constexpr float kBaselineBiome2Spring = 0.55f;   // MakeTestParams, biome 2

    // One simulation captured for spring, with the working set exposed for poking and a PassContext over it.
    struct Rig
    {
        WeatherParameters params = WT::MakeTestParams();
        WeatherSimulation sim;
        HydrologyInputs   hydro;
        InfluenceSnapshot influences;
        Dia::ParallelExecutor::SerialExecutor serial;
        PassContext       ctx;

        // Params edits must precede Capture (they are latched there), so construction only initialises.
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
        uint8_t DirIndex(uint32_t c) const { return sim.Tables().dirIndex[c]; }
    };

    WeatherTerrain Terrain(uint32_t w, uint32_t h, float latitude)
    {
        WT::HexTerrainOptions o;
        o.latitude = latitude;
        return WT::MakeHexTerrain(w, h, o);
    }

    void SetAll(Dia::Core::Containers::DynamicArray<float>& a, float v)
    {
        for (uint32_t i = 0; i < a.Size(); ++i) a[i] = v;
    }

    // Circular distance between two angles.
    float AngleDiff(float a, float b)
    {
        float d = fabsf(a - b);
        return d > kPiF ? 2.0f * kPiF - d : d;
    }
}

// ---- Pass 1 Solar (AC 7, 8) ----------------------------------------------------------------------------------------

TEST(WeatherPassSolar, EquatorLandRelaxesTowardBaselineAtLandHeatCapacity)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.Capture();
    const uint32_t c = 3;   // land (column 3)
    rig.Working().temperature[c] = 0.0f;
    rig.Working().cloudCover[c] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[c], kBaselineBiome2Spring * rig.params.landHeatCapacity, 1e-6f);
}

TEST(WeatherPassSolar, RepeatedRunsConvergeToTheTarget)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.Capture();
    const uint32_t c = 3;
    rig.Working().temperature[c] = 0.0f;
    for (int i = 0; i < 60; ++i) RunSolarPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[c], kBaselineBiome2Spring, 1e-4f);
}

TEST(WeatherPassSolar, WaterCellRelaxesMoreSlowlyThanLand)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.Capture();
    const uint32_t water = 4 * 1, land = 4 * 1 + 3;   // row 1: column 0 is the ocean strip
    rig.Working().temperature[water] = 0.0f;
    rig.Working().temperature[land] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[water], kBaselineBiome2Spring * rig.params.waterHeatCapacity, 1e-6f);
    EXPECT_LT(rig.Working().temperature[water], rig.Working().temperature[land]);
}

TEST(WeatherPassSolar, CloudAlbedoReducesTheDelta)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.params.cloudDecay = 0.0f;   // keep the cloud for the pass
    rig.Capture();
    const uint32_t clear = 3, cloudy = 7;
    rig.Working().temperature[clear] = 0.0f;  rig.Working().cloudCover[clear] = 0.0f;
    rig.Working().temperature[cloudy] = 0.0f; rig.Working().cloudCover[cloudy] = 1.0f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_LT(rig.Working().temperature[cloudy], rig.Working().temperature[clear]);
    EXPECT_NEAR(rig.Working().temperature[cloudy],
                kBaselineBiome2Spring * rig.params.landHeatCapacity * (1.0f - rig.params.cloudAlbedo), 1e-6f);
}

TEST(WeatherPassSolar, PoleTargetIsTheSeasonalTargetHelperValue)
{
    Rig rig(Terrain(4, 4, 1.0f));
    rig.Capture();
    const uint32_t c = 3;
    rig.Working().temperature[c] = 0.0f;
    rig.Working().cloudCover[c] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    const float target = SeasonalTargetTemperature(rig.sim.Captured().seasonBaseline[c], 1.0f, rig.params.latitudeCoolingFactor);
    EXPECT_FLOAT_EQ(rig.Working().temperature[c], 0.0f + (target - 0.0f) * rig.params.landHeatCapacity * 1.0f);
    // Latitude 1.0 cools the baseline by exactly latitudeCoolingFactor.
    EXPECT_NEAR(target, kBaselineBiome2Spring * (1.0f - rig.params.latitudeCoolingFactor), 1e-6f);
}

TEST(WeatherPassSolar, ClimateTempOffsetShiftsTheTarget)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.Capture();
    rig.Latched().climate.tempOffset[3] = 0.1f;
    rig.Working().temperature[3] = 0.0f;
    rig.Working().temperature[7] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().temperature[3], (kBaselineBiome2Spring + 0.1f) * rig.params.landHeatCapacity, 1e-6f);
    EXPECT_NEAR(rig.Working().temperature[7], kBaselineBiome2Spring * rig.params.landHeatCapacity, 1e-6f);
}

TEST(WeatherPassSolar, TemperatureStaysInUnitRange)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.Capture();
    rig.Working().temperature[3] = 1.0f;
    rig.Working().temperature[7] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 16; ++c)
    {
        EXPECT_GE(rig.Working().temperature[c], 0.0f);
        EXPECT_LE(rig.Working().temperature[c], 1.0f);
    }
}

TEST(WeatherPassSolar, WeeklyResetZeroesPrecipitationWaterToGroundAndGlacierInput)
{
    Rig rig(Terrain(4, 4, 0.3f));
    rig.Capture();
    for (uint32_t c = 0; c < 16; ++c)
    {
        rig.Working().precipitation[c] = 0.7f;
        rig.Working().waterToGround[c] = 0.4f;
        rig.ctx.handoff->glacierInput[c] = 1.5f;
    }
    RunSolarPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 16; ++c)
    {
        EXPECT_EQ(rig.Working().precipitation[c], 0.0f);
        EXPECT_EQ(rig.Working().waterToGround[c], 0.0f);
        EXPECT_EQ(rig.ctx.handoff->glacierInput[c], 0.0f);
    }
}

TEST(WeatherPassSolar, CloudCoverDecaysByOneMinusCloudDecayBeforeUse)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.params.cloudDecay = 0.25f;
    rig.Capture();
    rig.Working().cloudCover[3] = 0.8f;
    rig.Working().temperature[3] = 0.0f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().cloudCover[3], 0.6f, 1e-6f);
    // The decayed cloud, not the old one, drives the albedo.
    EXPECT_NEAR(rig.Working().temperature[3],
                kBaselineBiome2Spring * rig.params.landHeatCapacity * (1.0f - 0.6f * rig.params.cloudAlbedo), 1e-6f);
}

TEST(WeatherPassSolar, CloudCoverUnchangedAtZeroDecay)
{
    Rig rig(Terrain(4, 4, 0.0f));
    rig.params.cloudDecay = 0.0f;
    rig.Capture();
    rig.Working().cloudCover[3] = 0.8f;
    RunSolarPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().cloudCover[3], 0.8f);
}

// ---- Pass 2 Pressure (AC 9, 9a) -------------------------------------------------------------------------------------

TEST(WeatherPassPressure, TemperatureMapsToPressureAtDefaultSensitivity)
{
    Rig rig(Terrain(4, 4, 0.3f));
    rig.Capture();
    rig.Working().temperature[0] = 1.0f;
    rig.Working().temperature[1] = 0.0f;
    rig.Working().temperature[2] = 0.5f;
    RunPressurePass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().pressure[0], 0.2f, 1e-6f);
    EXPECT_NEAR(rig.Working().pressure[1], 0.8f, 1e-6f);
    EXPECT_NEAR(rig.Working().pressure[2], 0.5f, 1e-6f);
}

TEST(WeatherPassPressure, ResultIsClampedToUnitRange)
{
    Rig rig(Terrain(4, 4, 0.3f));
    rig.params.pressureSensitivity = 4.0f;
    rig.Capture();
    rig.Working().temperature[0] = 1.0f;
    rig.Working().temperature[1] = 0.0f;
    RunPressurePass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().pressure[0], 0.0f);
    EXPECT_EQ(rig.Working().pressure[1], 1.0f);
}

TEST(WeatherPassPressure, AnomalyIsAddedThenClamped)
{
    Rig rig(Terrain(4, 4, 0.3f));
    rig.Capture();
    rig.Working().temperature[0] = 0.5f;  rig.Working().pressureAnomaly[0] = -0.3f;
    rig.Working().temperature[1] = 0.0f;  rig.Working().pressureAnomaly[1] = 0.5f;    // 0.8 + 0.5 -> 1.0
    rig.Working().temperature[2] = 1.0f;  rig.Working().pressureAnomaly[2] = -0.5f;   // 0.2 - 0.5 -> 0.0
    RunPressurePass(rig.ctx, rig.serial);
    EXPECT_NEAR(rig.Working().pressure[0], 0.2f, 1e-6f);
    EXPECT_EQ(rig.Working().pressure[1], 1.0f);
    EXPECT_EQ(rig.Working().pressure[2], 0.0f);
}

TEST(WeatherPassPressure, ZeroAnomalyEqualsThePreAnomalyFormulaBitForBit)
{
    Rig rig(Terrain(8, 8, 0.3f));
    rig.Capture();
    const float s = rig.params.pressureSensitivity;
    for (uint32_t c = 0; c < 64; ++c) rig.Working().temperature[c] = float(c) / 63.0f;
    RunPressurePass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 64; ++c)
    {
        float expected = 0.5f - (rig.Working().temperature[c] - 0.5f) * s;
        expected = expected < 0.0f ? 0.0f : (expected > 1.0f ? 1.0f : expected);
        const float got = rig.Working().pressure[c];
        EXPECT_EQ(0, memcmp(&expected, &got, sizeof(float))) << "cell " << c;
    }
}

// ---- Pass 3 Wind (AC 10, 10a) ---------------------------------------------------------------------------------------

TEST(WeatherPassWind, UniformPressureGivesPrevailingOnlyInTheTradeBand)
{
    Rig rig(Terrain(4, 4, 0.1f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 16; ++c)
    {
        EXPECT_LT(AngleDiff(rig.Working().windDirection[c], 0.0f), 1e-5f);
        EXPECT_NEAR(rig.Working().windSpeed[c], rig.params.prevailingStrength, 1e-6f);
        // East tie: even column snaps to 5, odd column to 0.
        EXPECT_EQ(rig.DirIndex(c), (c % 4) % 2 == 0 ? 5 : 0) << "cell " << c;
    }
}

TEST(WeatherPassWind, UniformPressureGivesWesterlyInTheWesterlyBand)
{
    Rig rig(Terrain(4, 4, 0.45f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 16; ++c)
    {
        EXPECT_LT(AngleDiff(rig.Working().windDirection[c], kPiF), 1e-4f);
        EXPECT_GE(rig.Working().windDirection[c], 0.0f);
        EXPECT_LT(rig.Working().windDirection[c], 2.0f * kPiF);
        // West tie: even column 3, odd column 2.
        EXPECT_EQ(rig.DirIndex(c), (c % 4) % 2 == 0 ? 3 : 2) << "cell " << c;
    }
}

TEST(WeatherPassWind, PolarBandIsEasterly)
{
    Rig rig(Terrain(4, 4, 0.9f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    EXPECT_LT(AngleDiff(rig.Working().windDirection[5], 0.0f), 1e-5f);
}

TEST(WeatherPassWind, MidTransitionPointsNorth)
{
    Rig rig(Terrain(4, 4, 0.3f));   // halfway through the trade -> westerly lerp: pi/2
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    EXPECT_LT(AngleDiff(rig.Working().windDirection[5], kPiF * 0.5f), 1e-4f);
    EXPECT_EQ(rig.DirIndex(5), 1);
}

TEST(WeatherPassWind, PrevailingDirectionIsContinuousAcrossBothTransitions)
{
    WT::HexTerrainOptions o;
    o.latitudeRamp = true;   // latitude = row / 100
    Rig rig(WT::MakeHexTerrain(2, 101, o));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t row = 0; row + 1 < 101; ++row)
    {
        const float a = rig.Working().windDirection[row * 2 + 1];
        const float b = rig.Working().windDirection[(row + 1) * 2 + 1];
        EXPECT_LT(AngleDiff(a, b), 0.35f) << "row " << row;   // lerp slope pi/0.1 * 0.01 = 0.314 per row
    }
}

TEST(WeatherPassWind, HighPressureCellPushesNeighbourWindAway)
{
    Rig rig(Terrain(5, 5, 0.3f));
    rig.params.prevailingStrength = 0.0f;   // gradient only
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    const Dia::Geometry2D::HexTopology topo(5, 5);
    const uint32_t centre = 2 * 5 + 2;
    rig.Working().pressure[centre] = 1.0f;
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t d = 0; d < kHexDirections; ++d)
    {
        const uint32_t nb = topo.NeighbourAt(centre, d);
        ASSERT_NE(nb, kNoCell);
        EXPECT_EQ(rig.DirIndex(nb), d) << "dir " << d;
        EXPECT_NEAR(rig.Working().windSpeed[nb], rig.params.pressureStrength * 0.5f / 6.0f, 1e-6f);
    }
}

TEST(WeatherPassWind, LowPressureCellPullsNeighbourWindIn)
{
    Rig rig(Terrain(5, 5, 0.3f));
    rig.params.prevailingStrength = 0.0f;
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    const Dia::Geometry2D::HexTopology topo(5, 5);
    const uint32_t centre = 2 * 5 + 2;
    rig.Working().pressure[centre] = 0.0f;
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t d = 0; d < kHexDirections; ++d)
    {
        const uint32_t nb = topo.NeighbourAt(centre, d);
        EXPECT_EQ(rig.DirIndex(nb), (d + 3) % 6) << "dir " << d;   // towards the centre
    }
}

TEST(WeatherPassWind, ZeroMagnitudeWindKeepsThePrevailingDirectionWithZeroSpeed)
{
    Rig rig(Terrain(4, 4, 0.45f));
    rig.params.prevailingStrength = 0.0f;
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    EXPECT_EQ(rig.Working().windSpeed[5], 0.0f);
    EXPECT_LT(AngleDiff(rig.Working().windDirection[5], kPiF), 1e-5f);
    EXPECT_EQ(rig.DirIndex(5), 2);   // column 1 (odd) west tie
}

TEST(WeatherPassWind, SpeedIsClampedToUnitRange)
{
    Rig rig(Terrain(4, 4, 0.1f));
    rig.params.prevailingStrength = 3.0f;
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    for (uint32_t c = 0; c < 16; ++c) EXPECT_EQ(rig.Working().windSpeed[c], 1.0f);
}

TEST(WeatherPassWind, NonDefaultWesterlyBandMovesTheTransition)
{
    WesterlyBand band;
    band.tradeEnd = 0.10f; band.westerlyStart = 0.20f; band.westerlyEnd = 0.40f; band.polarStart = 0.50f;
    {
        Rig rig(Terrain(4, 4, 0.15f));   // mid-lerp in the custom band, still trade in the default one
        rig.sim.SetWesterlyBand(band);
        rig.Capture();
        SetAll(rig.Working().pressure, 0.5f);
        RunWindPass(rig.ctx, rig.serial);
        EXPECT_LT(AngleDiff(rig.Working().windDirection[5], kPiF * 0.5f), 1e-4f);
    }
    {
        Rig rig(Terrain(4, 4, 0.30f));   // westerly in the custom band, mid-lerp in the default one
        rig.sim.SetWesterlyBand(band);
        rig.Capture();
        SetAll(rig.Working().pressure, 0.5f);
        RunWindPass(rig.ctx, rig.serial);
        EXPECT_LT(AngleDiff(rig.Working().windDirection[5], kPiF), 1e-4f);
    }
}

TEST(WeatherPassWind, WindShiftMovesBandBoundariesPoleward)
{
    // Effective latitude = latitude - shift. Latitude 0.35 is the start of the westerlies; shifted by 0.1 it is 0.25,
    // the end of the trade band, so the wind is still easterly.
    Rig rig(Terrain(4, 4, 0.35f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    RunWindPass(rig.ctx, rig.serial);
    EXPECT_LT(AngleDiff(rig.Working().windDirection[5], kPiF), 1e-4f);

    SetAll(rig.Latched().climate.windShift, 0.1f);
    RunWindPass(rig.ctx, rig.serial);
    EXPECT_LT(AngleDiff(rig.Working().windDirection[5], 0.0f), 1e-4f);
}

TEST(WeatherPassWind, ZeroShiftEqualsUnshiftedAndShiftedMatchesTheShiftedLatitudeExactly)
{
    // 0.375 - 0.125 and 0.25 are exact in binary, so the effective latitudes are bit-identical.
    Rig shifted(Terrain(4, 4, 0.375f));
    shifted.Capture();
    SetAll(shifted.Working().pressure, 0.5f);
    SetAll(shifted.Latched().climate.windShift, 0.125f);
    RunWindPass(shifted.ctx, shifted.serial);

    Rig plain(Terrain(4, 4, 0.25f));
    plain.Capture();
    SetAll(plain.Working().pressure, 0.5f);
    RunWindPass(plain.ctx, plain.serial);

    EXPECT_TRUE(WT::SameFloats(shifted.Working().windDirection, plain.Working().windDirection));
    EXPECT_TRUE(WT::SameFloats(shifted.Working().windSpeed, plain.Working().windSpeed));

    // Shift 0 leaves the unshifted function untouched.
    Rig zero(Terrain(4, 4, 0.375f));
    zero.Capture();
    SetAll(zero.Working().pressure, 0.5f);
    SetAll(zero.Latched().climate.windShift, 0.0f);
    RunWindPass(zero.ctx, zero.serial);
    Rig none(Terrain(4, 4, 0.375f));
    none.Capture();
    SetAll(none.Working().pressure, 0.5f);
    RunWindPass(none.ctx, none.serial);
    EXPECT_TRUE(WT::SameFloats(zero.Working().windDirection, none.Working().windDirection));
}

TEST(WeatherPassWind, MissingNeighboursStillCountInTheDivideBySix)
{
    Rig rig(Terrain(5, 5, 0.3f));
    rig.Capture();
    for (uint32_t c = 0; c < 25; ++c) rig.Working().pressure[c] = 0.1f + 0.02f * float(c);
    const Dia::Geometry2D::HexTopology topo(5, 5);
    const uint32_t cells[] = { 0, 4, 2, 12 };   // corners, a top-edge cell, the interior
    for (uint32_t cell : cells)
    {
        float ex = 0.0f, ey = 0.0f;
        uint32_t existing = 0;
        for (uint32_t d = 0; d < kHexDirections; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(cell, d);
            if (nb == kNoCell) continue;
            ++existing;
            const float diff = rig.Working().pressure[cell] - rig.Working().pressure[nb];
            ex += kDirectionVectors[d].x * diff;
            ey += kDirectionVectors[d].y * diff;
        }
        const WindVector g = PressureGradient(cell, rig.Working(), rig.sim.Tables());
        EXPECT_NEAR(g.x, ex / 6.0f, 1e-6f) << "cell " << cell;
        EXPECT_NEAR(g.y, ey / 6.0f, 1e-6f) << "cell " << cell;
        if (existing < 6 && (fabsf(ex) > 1e-3f || fabsf(ey) > 1e-3f))
        {
            // Dividing by the number of existing neighbours would give a different (larger) vector.
            EXPECT_GT(fabsf(ex / float(existing) - g.x) + fabsf(ey / float(existing) - g.y), 1e-4f);
        }
    }
}

TEST(WeatherPassWind, UniformPressureHasZeroGradientEverywhereIncludingEdges)
{
    Rig rig(Terrain(5, 5, 0.3f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.5f);
    for (uint32_t c = 0; c < 25; ++c)
    {
        const WindVector g = PressureGradient(c, rig.Working(), rig.sim.Tables());
        EXPECT_EQ(g.x, 0.0f);
        EXPECT_EQ(g.y, 0.0f);
    }
}

// ---- Pass order -----------------------------------------------------------------------------------------------------

TEST(WeatherPassOrder, RunPipelineUntilStopsAfterTheNamedPass)
{
    Rig rig(Terrain(4, 4, 0.3f));
    rig.Capture();
    SetAll(rig.Working().pressure, 0.123f);
    SetAll(rig.Working().windSpeed, 0.0f);

    RunPipelineUntil(rig.ctx, rig.serial, PassId::Solar);
    EXPECT_EQ(rig.Working().pressure[5], 0.123f);   // pass 2 has not run
    EXPECT_EQ(rig.Working().windSpeed[5], 0.0f);

    RunPipelineUntil(rig.ctx, rig.serial, PassId::Pressure);
    EXPECT_NE(rig.Working().pressure[5], 0.123f);
    EXPECT_EQ(rig.Working().windSpeed[5], 0.0f);    // pass 3 has not run

    RunPipelineUntil(rig.ctx, rig.serial, PassId::Wind);
    EXPECT_GT(rig.Working().windSpeed[5], 0.0f);
}

TEST(WeatherPassOrder, StepRunsSolarPressureAndWindThroughTheLifecycle)
{
    Rig rig(Terrain(4, 4, 0.1f));
    rig.sim.Step(rig.params, Season::Spring, 0, WT::TestRegistry(), nullptr, rig.hydro, rig.influences, rig.serial);
    const WeatherFields& f = rig.sim.Fields();
    for (uint32_t c = 0; c < 16; ++c)
    {
        EXPECT_GT(f.windSpeed[c], 0.0f) << "cell " << c;
        EXPECT_GE(f.pressure[c], 0.0f);
        EXPECT_LE(f.pressure[c], 1.0f);
    }
}

TEST(WeatherPassOrder, ResultIsIdenticalAtAnyRangeCount)
{
    Rig a(Terrain(16, 16, 0.4f));
    Rig b(Terrain(16, 16, 0.4f));
    WT::FixedRangeExecutor one(1), many(7);
    WeatherFields fa, fb;
    a.sim.Step(a.params, Season::Summer, 3, WT::TestRegistry(), nullptr, a.hydro, a.influences, one);
    b.sim.Step(b.params, Season::Summer, 3, WT::TestRegistry(), nullptr, b.hydro, b.influences, many);
    EXPECT_TRUE(WT::SameFields(a.sim.Fields(), b.sim.Fields()));
}
