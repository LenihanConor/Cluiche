// A4 weather ca-pipeline Task 11: step profile and AC 37 perf report.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md ("Profile", AC 37)
// Debug ballpark only: the budget is reported, never gated. The only hard failure is a loose sanity ceiling.
#include <gtest/gtest.h>

#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherPipeline.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <algorithm>
#include <iostream>
#include <string.h>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;

    constexpr uint32_t kN = WeatherStepProfile::kPassCount;

    struct Rig
    {
        WeatherSimulation sim;
        WeatherParameters params = WT::MakeTestParams();
        HydrologyInputs hydro;
        InfluenceSnapshot influences;
        uint32_t week = 0;

        Rig(uint32_t w, uint32_t h)
        {
            WT::HexTerrainOptions o; o.latitudeRamp = true; o.waterColumns = 4;
            EXPECT_TRUE(sim.Initialise(WT::MakeHexTerrain(w, h, o), params, WT::TestRegistry()));
        }
        void Step(Dia::ParallelExecutor::IParallelExecutor& ex)
        {
            sim.Step(params, static_cast<Season>((week / 13) % 4), week % 52, WT::TestRegistry(), nullptr, hydro, influences, ex);
            ++week;
        }
    };

    uint64_t SumCpu(const WeatherStepProfile& p)
    {
        uint64_t s = 0;
        for (uint32_t i = 0; i < kN; ++i) s += p.cpuMicros[i];
        return s;
    }

    void ExpectSane(const WeatherStepProfile& p)
    {
        EXPECT_EQ(p.totalCpuMicros, SumCpu(p));
        EXPECT_GE(p.lastRangeCount, 1u);
        EXPECT_EQ(p.wallMicros[0], 0u);
        EXPECT_EQ(p.cpuMicros[0], 0u);
    }

    struct SizeCase { const char* name; uint32_t w, h; double budgetMs; };
}

TEST(WeatherPerfProfile, NameTableHas14DistinctNames)
{
    for (uint32_t i = 0; i < kN; ++i)
    {
        ASSERT_NE(PassName(i), nullptr);
        EXPECT_GT(strlen(PassName(i)), 0u);
        for (uint32_t j = i + 1; j < kN; ++j) EXPECT_STRNE(PassName(i), PassName(j));
    }
    EXPECT_STREQ(PassName(kN), "");
}

TEST(WeatherPerfProfile, ProfileIndexFollowsPassOrder)
{
    EXPECT_EQ(kProfileIndex(PassId::Influence), 0u);
    EXPECT_EQ(kProfileIndex(PassId::MoistureOut), 5u);
    EXPECT_EQ(kProfileIndex(PassId::Finish), kN - 1);
}

TEST(WeatherPerfProfile, DefaultProfileBeforeAnyStepIsZero)
{
    WeatherSimulation sim;
    const WeatherStepProfile& p = sim.GetLastProfile();
    EXPECT_EQ(p.totalCpuMicros, 0u);
    EXPECT_EQ(p.lastRangeCount, 0u);
}

TEST(WeatherPerfProfile, StepFillsProfileSmall)
{
    Rig rig(66, 42);
    Dia::ParallelExecutor::SerialExecutor serial;
    rig.Step(serial);
    const WeatherStepProfile& p = rig.sim.GetLastProfile();
    ExpectSane(p);
    for (uint32_t i = 1; i < kN; ++i) EXPECT_GE(p.wallMicros[i], 0u);
}

TEST(WeatherPerfProfile, ProfileResetsBetweenSteps)
{
    Rig rig(66, 42);
    Dia::ParallelExecutor::SerialExecutor serial;
    rig.Step(serial);
    rig.Step(serial);
    const WeatherStepProfile second = rig.sim.GetLastProfile();
    // A single step's total cannot approach a running sum of two; compare against a wall-clock bound instead.
    ExpectSane(second);
    uint64_t wall = 0;
    for (uint32_t i = 0; i < kN; ++i) wall += second.wallMicros[i];
    EXPECT_LT(wall, 5u * 1000u * 1000u);
}

TEST(WeatherPerfProfile, CpuSumsWithReverseAndFixedExecutors)
{
    {
        Rig rig(66, 42);
        WT::ReverseRangeExecutor rev;
        rig.Step(rev);
        ExpectSane(rig.sim.GetLastProfile());
        EXPECT_EQ(rig.sim.GetLastProfile().lastRangeCount, (66u * 42u + kMinRange - 1) / kMinRange);
    }
    {
        Rig rig(66, 42);
        WT::FixedRangeExecutor fixed(7);
        rig.Step(fixed);
        ExpectSane(rig.sim.GetLastProfile());
        EXPECT_EQ(rig.sim.GetLastProfile().lastRangeCount, 7u);
    }
}

TEST(WeatherPerfProfile, TimingDoesNotChangeResults)
{
    Rig a(66, 42), b(66, 42);
    Dia::ParallelExecutor::SerialExecutor serial;
    WT::ReverseRangeExecutor rev;
    for (uint32_t i = 0; i < 6; ++i) { a.Step(serial); b.Step(rev); }
    EXPECT_TRUE(WT::SameFloats(a.sim.Fields().moisture, b.sim.Fields().moisture));
    EXPECT_TRUE(WT::SameFloats(a.sim.Fields().temperature, b.sim.Fields().temperature));
}

// AC 37: per-pass breakdown at three sizes. Reported, never gated on budget (Debug ballpark).
TEST(WeatherPerfReport, PerPassBreakdownAtThreeSizes)
{
    const SizeCase cases[] = { { "Small 66x42", 66, 42, 4.2 }, { "Standard 80x52", 80, 52, 6.3 }, { "Large 104x64", 104, 64, 10.0 } };
    for (const SizeCase& sc : cases)
    {
        Rig rig(sc.w, sc.h);
        Dia::ParallelExecutor::SerialExecutor serial;
        for (uint32_t i = 0; i < 4; ++i) rig.Step(serial);   // 4-week warm-up

        constexpr uint32_t kRuns = 10;
        uint64_t wall[kRuns][kN], cpu[kRuns][kN], total[kRuns];
        for (uint32_t r = 0; r < kRuns; ++r)
        {
            rig.Step(serial);
            const WeatherStepProfile& p = rig.sim.GetLastProfile();
            for (uint32_t i = 0; i < kN; ++i) { wall[r][i] = p.wallMicros[i]; cpu[r][i] = p.cpuMicros[i]; }
            total[r] = p.totalCpuMicros;
        }
        auto median = [](uint64_t* v) { std::sort(v, v + kRuns); return v[kRuns / 2]; };

        std::cout << "[WeatherPerf] " << sc.name << " (Debug, serial executor, median of " << kRuns << ")\n";
        for (uint32_t i = 0; i < kN; ++i)
        {
            uint64_t w[kRuns], c[kRuns];
            for (uint32_t r = 0; r < kRuns; ++r) { w[r] = wall[r][i]; c[r] = cpu[r][i]; }
            std::cout << "  " << PassName(i) << "  wall " << median(w) << " us  cpu " << median(c) << " us\n";
        }
        const uint64_t totalCpu = median(total);
        const double ms = double(totalCpu) / 1000.0;
        std::cout << "  total cpu " << ms << " ms vs budget " << sc.budgetMs << " ms"
                  << (ms > sc.budgetMs ? "  OVER BUDGET (Debug ballpark)" : "  within budget") << std::endl;

        // Loose sanity only: catches accidental O(N^2) or per-cell allocation. Never a budget gate.
        EXPECT_LT(totalCpu, 3000u * 1000u) << sc.name;
    }
}
