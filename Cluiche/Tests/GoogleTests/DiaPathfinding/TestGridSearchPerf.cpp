#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <DiaObservation/Log/DiaLog.h>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaGeometry2D/Grid/HexTopology.h"

using namespace Dia::Pathfinding;
using Dia::Geometry2D::HexTopology;

// Ballpark check (AC 8) on a Large map (104 x 64). Debug build, warm scratch, best of 3.
namespace {
    constexpr int kWidth = 104;
    constexpr int kHeight = 64;
    // AC 8 target is 1 ms in Debug (ballpark). Measured 2026-11-19: Steps ~0.93 ms, Dijkstra<float> ~3.65 ms (was ~5.1 ms before hole-based heap sifts;
    // a std::priority_queue reference Dijkstra measures ~7.3 ms, i.e. helper ~0.5x of reference).
    // The target is logged, not enforced; the hard limits below are regression guards (Steps ~5x, Dijkstra ~3x headroom) so the
    // suite does not flake. Dijkstra is above the AC 8 target: tracked for a dedicated perf pass.
    constexpr double kTargetMs = 1.0;
    constexpr double kStepsGuardMs = 5.0;
    constexpr double kDijkstraGuardMs = 11.0;

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
    DIA_LOG_INFO("Test", "[GridSearchPerf] MultiSourceSteps 104x64: %g ms", ms);
    RecordProperty("MultiSourceStepsMs", std::to_string(ms));

    EXPECT_EQ(std::count(steps.begin(), steps.end(), kUnreached), 0);
    EXPECT_EQ(steps[0], 0u);
    if (ms >= kTargetMs) { DIA_LOG_INFO("Test", "[GridSearchPerf] above AC 8 target (%g ms)", kTargetMs); }
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
    DIA_LOG_INFO("Test", "[GridSearchPerf] MultiSourceDijkstra<float> 104x64: %g ms", ms);
    RecordProperty("MultiSourceDijkstraMs", std::to_string(ms));

    // Reference: plain std::priority_queue Dijkstra over the same graph and step cost (what the helper replaced).
    std::vector<float> refCost(n);
    auto refRun = [&]() {
        using Item = std::pair<float, uint32_t>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
        std::fill(refCost.begin(), refCost.end(), std::numeric_limits<float>::infinity());
        for (const auto& s : seeds) { refCost[s.cell] = s.startCost; pq.push({ s.startCost, s.cell }); }
        std::vector<uint8_t> done(n, 0);
        while (!pq.empty()) {
            const Item it = pq.top();
            pq.pop();
            if (done[it.second]) continue;
            done[it.second] = 1;
            g.ForEachNeighbour(it.second, [&](uint32_t nb) {
                if (done[nb]) return;
                const float cand = it.first + (1.0f + float(nb % 3u));
                if (cand < refCost[nb]) { refCost[nb] = cand; pq.push({ cand, nb }); }
            });
        }
    };
    refRun(); // warm
    const double refMs = BestOfThreeMs(refRun);
    DIA_LOG_INFO("Test", "[GridSearchPerf] std::priority_queue reference Dijkstra 104x64: %g ms (helper/reference = %gx)", refMs, ms / refMs);
    RecordProperty("ReferenceDijkstraMs", std::to_string(refMs));
    EXPECT_EQ(refCost, cost);

    EXPECT_EQ(std::count(cost.begin(), cost.end(), std::numeric_limits<float>::infinity()), 0);
    EXPECT_EQ(std::count(seedId.begin(), seedId.end(), kUnreached), 0);
    EXPECT_EQ(cost[0], 0.0f);
    if (ms >= kTargetMs) { DIA_LOG_INFO("Test", "[GridSearchPerf] above AC 8 target (%g ms)", kTargetMs); }
    EXPECT_LT(ms, kDijkstraGuardMs);
}
