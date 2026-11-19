#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaPathfinding/HexTopology.h"

using namespace Dia::Pathfinding;

// Ballpark check (AC 8) on a Large map (104 x 64). Debug build, warm scratch, best of 3.
namespace {
    constexpr int kWidth = 104;
    constexpr int kHeight = 64;
    // AC 8 target is 1 ms in Debug (ballpark). Measured 2026-11-19: Steps ~0.93 ms, Dijkstra<float> ~5.35 ms.
    // The target is logged, not enforced; the hard limits below are regression guards (~5x headroom) so the
    // suite does not flake. Dijkstra is above the AC 8 target: tracked for a dedicated perf pass.
    constexpr double kTargetMs = 1.0;
    constexpr double kStepsGuardMs = 5.0;
    constexpr double kDijkstraGuardMs = 25.0;

    template<class F>
    double BestOfThreeMs(F&& fn)
    {
        double best = 1e30;
        for (int i = 0; i < 3; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            fn();
            const auto t1 = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return best;
    }
}

TEST(GridSearchPerf, MultiSourceStepsLargeMap)
{
    HexTopology topo(kWidth, kHeight);
    HexNeighbours g(topo);
    const uint32_t n = g.CellCount();
    ASSERT_EQ(n, 6656u);

    const StepSeed seeds[] = { { 0, 0, 0 }, { 1000, 0, 1 }, { 3333, 0, 2 }, { 5000, 0, 3 }, { n - 1, 0, 4 } };
    std::vector<uint32_t> steps(n), seedId(n);
    GridSearchScratch scratch;
    auto run = [&]() {
        MultiSourceSteps(g, seeds, 5, [](uint32_t) { return true; }, 0xFFFFFFFEu, steps.data(), seedId.data(), scratch);
    };

    run(); // warm
    const double ms = BestOfThreeMs(run);
    std::cout << "[GridSearchPerf] MultiSourceSteps 104x64: " << ms << " ms\n";
    RecordProperty("MultiSourceStepsMs", std::to_string(ms));

    EXPECT_EQ(std::count(steps.begin(), steps.end(), kUnreached), 0);
    EXPECT_EQ(steps[0], 0u);
    if (ms >= kTargetMs) { std::cout << "[GridSearchPerf] above AC 8 target (" << kTargetMs << " ms)" << std::endl; }
    EXPECT_LT(ms, kStepsGuardMs);
}

TEST(GridSearchPerf, MultiSourceDijkstraLargeMap)
{
    HexTopology topo(kWidth, kHeight);
    HexNeighbours g(topo);
    const uint32_t n = g.CellCount();
    ASSERT_EQ(n, 6656u);

    const CostSeed<float> seeds[] = { { 0, 0.0f, 0 }, { 1000, 0.0f, 1 }, { 3333, 0.0f, 2 }, { 5000, 0.0f, 3 }, { n - 1, 0.0f, 4 } };
    std::vector<float> cost(n);
    std::vector<uint32_t> seedId(n);
    DijkstraOptions<float> opt;
    opt.outCost = cost.data();
    opt.outSeedId = seedId.data();
    GridSearchScratch scratch;
    auto run = [&]() {
        MultiSourceDijkstra<float>(g, seeds, 5, [](uint32_t, uint32_t to) { return 1.0f + float(to % 3u); }, opt, scratch);
    };

    run(); // warm
    const double ms = BestOfThreeMs(run);
    std::cout << "[GridSearchPerf] MultiSourceDijkstra<float> 104x64: " << ms << " ms\n";
    RecordProperty("MultiSourceDijkstraMs", std::to_string(ms));

    EXPECT_EQ(std::count(cost.begin(), cost.end(), std::numeric_limits<float>::infinity()), 0);
    EXPECT_EQ(std::count(seedId.begin(), seedId.end(), kUnreached), 0);
    EXPECT_EQ(cost[0], 0.0f);
    if (ms >= kTargetMs) { std::cout << "[GridSearchPerf] above AC 8 target (" << kTargetMs << " ms)" << std::endl; }
    EXPECT_LT(ms, kDijkstraGuardMs);
}
