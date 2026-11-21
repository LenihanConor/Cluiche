#include <gtest/gtest.h>
#include <DiaPathfinding/FindPath.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

namespace {

    constexpr int kWidth  = 104;
    constexpr int kHeight = 64;
    constexpr int kReps   = 5;

    struct Lcg {
        uint32_t s;
        explicit Lcg(uint32_t seed) : s(seed) {}
        uint32_t Next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    };

    // The < 1 ms sweep requirement (AC10) only applies to optimised builds. Debug builds
    // run unoptimised code with iterator/assert overhead, so we only enforce a generous
    // sanity bound there (it merely prevents a hang) and report the measured time.
#ifdef NDEBUG
    constexpr double kSweepLimitMs = 1.0;
#else
    constexpr double kSweepLimitMs = 200.0;
#endif

    template <typename F>
    double BestOfMs(F&& fn)
    {
        double best = 1e30;
        for (int r = 0; r < kReps; ++r)
        {
            auto t0 = std::chrono::steady_clock::now();
            fn();
            auto t1 = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return best;
    }
}

TEST(SLOW_HexGridPerformance, NeighbourSweep_FullGrid)
{
    HexPathGrid grid(kWidth, kHeight);
    HexTopology topo(kWidth, kHeight);
    const uint32_t cells = topo.CellCount();
    ASSERT_EQ(cells, 6656u);

    volatile uint64_t sink = 0;

    const double topoMs = BestOfMs([&]() {
        uint64_t sum = 0;
        for (uint32_t c = 0; c < cells; ++c)
            for (uint32_t d = 0; d < kHexDirections; ++d)
                sum += topo.NeighbourAt(c, d);
        sink = sink + sum;
    });

    const double gridMs = BestOfMs([&]() {
        uint64_t sum = 0;
        DynamicArrayC<CellCoord, 16> nb;
        for (uint32_t c = 0; c < cells; ++c)
        {
            grid.GetNeighbours(topo.ToCoord(c), nb);
            sum += nb.Size();
        }
        sink = sink + sum;
    });

    std::cout << "[HexGridPerformance] NeighbourAt sweep (6656 x 6): " << topoMs << " ms, "
              << "GetNeighbours sweep: " << gridMs << " ms\n";
    RecordProperty("NeighbourAtSweepMs", std::to_string(topoMs));
    RecordProperty("GetNeighboursSweepMs", std::to_string(gridMs));

    EXPECT_LT(topoMs, kSweepLimitMs);
    EXPECT_LT(gridMs, kSweepLimitMs);
    EXPECT_NE(sink, 0u);
}

TEST(SLOW_HexGridPerformance, FindPath_1000Queries_ScatteredObstacles)
{
    HexPathGrid grid(kWidth, kHeight);
    Lcg rng(12345u);

    std::vector<CellCoord> passable;
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x)
        {
            CellCoord c{x, y};
            if (rng.Next() % 100 < 20)
                grid.SetPassable(c, false);
            else
                passable.push_back(c);
        }
    ASSERT_FALSE(passable.empty());

    FlatCostProvider costs;
    int successes = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i)
    {
        CellCoord a = passable[rng.Next() % passable.size()];
        CellCoord b = passable[rng.Next() % passable.size()];
        PathResult r = FindPath(grid, a, b, costs);
        if (r.success) ++successes;
    }
    auto t1 = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "[HexGridPerformance] 1000 FindPath calls: " << totalMs << " ms total, "
              << successes << " succeeded\n";
    RecordProperty("FindPath1000TotalMs", std::to_string(totalMs));
    RecordProperty("FindPath1000Successes", successes);

    EXPECT_GT(successes, 0);
}
