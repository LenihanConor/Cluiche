#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaPathfinding/Testing/GridSearchTestGrids.h"
#include "GridSearchAllocCounter.h"

using namespace Dia::Pathfinding;
using namespace Dia::Pathfinding::Testing;

namespace {
    template<class Cost>
    struct Result {
        std::vector<Cost> cost;
        std::vector<uint32_t> prev, seedId, order;
        uint32_t stopped = 12345u;
    };

    // Runs the search with every output requested; records settle order through the hook.
    template<class Cost, class G, class StepCost>
    Result<Cost> RunSearch(const G& g, std::vector<CostSeed<Cost>> seeds, StepCost step, GridSearchScratch& s,
                     const std::vector<uint8_t>* stopAt = nullptr)
    {
        Result<Cost> r;
        const uint32_t n = g.CellCount();
        r.cost.assign(n, Cost(-7));
        r.prev.assign(n, 777u);
        r.seedId.assign(n, 777u);
        DijkstraOptions<Cost> o;
        o.outCost = r.cost.data();
        o.outPrev = r.prev.data();
        o.outSeedId = r.seedId.data();
        if (stopAt) { o.stopAt = stopAt->data(); o.outStoppedAt = &r.stopped; }
        MultiSourceDijkstra<Cost>(g, seeds.data(), static_cast<uint32_t>(seeds.size()), step, o, s,
            [&](uint32_t c, Cost, uint32_t) { r.order.push_back(c); return Settle::Accept; });
        return r;
    }

    constexpr float kInfF = std::numeric_limits<float>::infinity();
    constexpr double kInfD = std::numeric_limits<double>::infinity();

    // Chain step: entering an odd cell costs 2, entering an even cell costs 1.
    template<class Cost> auto OddCosts() { return [](uint32_t, uint32_t to) { return Cost(1 + (to & 1u)); }; }
    template<class Cost> auto Unit() { return [](uint32_t, uint32_t) { return Cost(1); }; }
}

TEST(GridSearchDijkstra, ExactCostsFloat)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(6), { { 0, 0.0f, 5 } }, OddCosts<float>(), s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 2, 3, 5, 6, 8 }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5 }));
}

TEST(GridSearchDijkstra, ExactCostsDouble)
{
    GridSearchScratch s;
    const auto r = RunSearch<double>(ChainGraph(6), { { 0, 0.5, 5 } }, OddCosts<double>(), s);
    EXPECT_EQ(r.cost, (std::vector<double>{ 0.5, 2.5, 3.5, 5.5, 6.5, 8.5 }));
}

TEST(GridSearchDijkstra, ExactCostsOnTree)
{
    // 0 -> {1,2}; 1 -> {3}; 2 -> {4}. Entering cell c costs c.
    GridSearchScratch s;
    TreeGraph g({ kNoCell, 0, 0, 1, 2 });
    const auto r = RunSearch<float>(g, { { 0, 0.0f, 0 } }, [](uint32_t, uint32_t to) { return float(to); }, s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2, 4, 6 }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 1, 2, 3, 4 }));
}

TEST(GridSearchDijkstra, UnreachedCostIsInfinityAndPrevIsNoCell)
{
    GridSearchScratch s;
    // Cell 3 and beyond blocked.
    const auto r = RunSearch<float>(ChainGraph(6), { { 0, 0.0f, 9 } },
        [](uint32_t, uint32_t to) { return to >= 3 ? -1.0f : 1.0f; }, s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2, kInfF, kInfF, kInfF }));
    EXPECT_EQ(r.prev, (std::vector<uint32_t>{ kNoCell, 0, 1, kNoCell, kNoCell, kNoCell }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 9, 9, 9, kUnreached, kUnreached, kUnreached }));
}

TEST(GridSearchDijkstra, UnreachedCostIsInfinityDouble)
{
    GridSearchScratch s;
    const auto r = RunSearch<double>(ChainGraph(3), {}, Unit<double>(), s);
    EXPECT_EQ(r.cost, (std::vector<double>{ kInfD, kInfD, kInfD }));
}

TEST(GridSearchDijkstra, NegativeCostBlocksEdge)
{
    GridSearchScratch s;
    const auto r = RunSearch<double>(ChainGraph(5), { { 0, 0.0, 0 } },
        [](uint32_t, uint32_t to) { return to == 2 ? -0.5 : 1.0; }, s);
    EXPECT_EQ(r.cost, (std::vector<double>{ 0, 1, kInfD, kInfD, kInfD }));
}

TEST(GridSearchDijkstra, NegativeCostOneDirectionOnly)
{
    GridSearchScratch s;
    auto step = [](uint32_t from, uint32_t to) { return (from == 2 && to == 3) ? -1.0f : 1.0f; };
    const auto fwd = RunSearch<float>(ChainGraph(6), { { 0, 0.0f, 0 } }, step, s);
    EXPECT_EQ(fwd.cost, (std::vector<float>{ 0, 1, 2, kInfF, kInfF, kInfF }));
    const auto back = RunSearch<float>(ChainGraph(6), { { 5, 0.0f, 0 } }, step, s);
    EXPECT_EQ(back.cost, (std::vector<float>{ 5, 4, 3, 2, 1, 0 }));
}

TEST(GridSearchDijkstra, TieHeavyPopOrder)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(5), { { 2, 0.0f, 0 } }, Unit<float>(), s);
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 2, 1, 3, 0, 4 }));
    EXPECT_EQ(r.cost, (std::vector<float>{ 2, 1, 0, 1, 2 }));
}

TEST(GridSearchDijkstra, ZeroCostEdgesTieOrderByCellIndex)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(4), { { 1, 0.0f, 0 } }, [](uint32_t, uint32_t) { return 0.0f; }, s);
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 1, 0, 2, 3 }));
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 0, 0, 0 }));
    EXPECT_EQ(r.prev, (std::vector<uint32_t>{ 1, kNoCell, 1, 2 }));
}

TEST(GridSearchDijkstra, StopAtStopsAtFirstTarget)
{
    GridSearchScratch s;
    const std::vector<uint8_t> stop{ 0, 0, 0, 1, 0, 1 };
    const auto r = RunSearch<float>(ChainGraph(6), { { 0, 0.0f, 0 } }, Unit<float>(), s, &stop);
    EXPECT_EQ(r.stopped, 3u);
    EXPECT_EQ(r.cost[3], 3.0f);
    EXPECT_EQ(r.cost[4], kInfF);
    EXPECT_EQ(r.order.back(), 3u);
}

TEST(GridSearchDijkstra, StopAtOnSeedCellStopsImmediately)
{
    GridSearchScratch s;
    const std::vector<uint8_t> stop{ 0, 0, 1, 0, 0 };
    const auto r = RunSearch<float>(ChainGraph(5), { { 2, 1.5f, 0 } }, Unit<float>(), s, &stop);
    EXPECT_EQ(r.stopped, 2u);
    EXPECT_EQ(r.cost[2], 1.5f);
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 2 }));
    EXPECT_EQ(r.cost[1], kInfF);
    EXPECT_EQ(r.cost[3], kInfF);
}

TEST(GridSearchDijkstra, UnreachableStopAtRunsToCompletion)
{
    GridSearchScratch s;
    const std::vector<uint8_t> stop{ 0, 0, 0, 0, 1 };
    const auto r = RunSearch<float>(ChainGraph(5), { { 0, 0.0f, 0 } },
        [](uint32_t, uint32_t to) { return to == 4 ? -1.0f : 1.0f; }, s, &stop);
    EXPECT_EQ(r.stopped, kNoCell);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2, 3, kInfF }));
}

TEST(GridSearchDijkstra, OutStoppedAtWhenNothingStops)
{
    GridSearchScratch s;
    const std::vector<uint8_t> stop{ 0, 0, 0 };
    const auto r = RunSearch<float>(ChainGraph(3), { { 0, 0.0f, 0 } }, Unit<float>(), s, &stop);
    EXPECT_EQ(r.stopped, kNoCell);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2 }));
}

TEST(GridSearchDijkstra, OutPrevRebuildsPath)
{
    GridSearchScratch s;
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    const auto r = RunSearch<float>(g, { { 0, 0.0f, 0 } }, Unit<float>(), s);
    // Walk back from cell 15 to the seed; path length equals cost.
    uint32_t c = 15, hops = 0;
    while (r.prev[c] != kNoCell) { c = r.prev[c]; ++hops; ASSERT_LT(hops, 100u); }
    EXPECT_EQ(c, 0u);
    EXPECT_EQ(float(hops), r.cost[15]);
    EXPECT_EQ(r.prev[0], kNoCell);
}

TEST(GridSearchDijkstra, OutSeedIdLabelsCellsWithTwoSeeds)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(7), { { 0, 0.0f, 10 }, { 6, 0.0f, 20 } }, Unit<float>(), s);
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 10, 10, 10, 10, 20, 20, 20 }));
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2, 3, 2, 1, 0 }));
    EXPECT_EQ(r.prev[3], 2u);
    EXPECT_EQ(r.prev[4], 5u);
}

TEST(GridSearchDijkstra, SeedStartCostsAndStrictImprovement)
{
    GridSearchScratch s;
    // Seed 0 starts 5 later than seed 6.
    auto r = RunSearch<float>(ChainGraph(7), { { 0, 5.0f, 1 }, { 6, 0.0f, 2 } }, Unit<float>(), s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 5, 5, 4, 3, 2, 1, 0 }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 1, 2, 2, 2, 2, 2, 2 }));
    // Two seeds on one cell: lower cost wins; equal cost keeps the first.
    r = RunSearch<float>(ChainGraph(2), { { 0, 3.0f, 1 }, { 0, 2.0f, 2 } }, Unit<float>(), s);
    EXPECT_EQ(r.seedId[0], 2u);
    EXPECT_EQ(r.cost[0], 2.0f);
    r = RunSearch<float>(ChainGraph(2), { { 0, 2.0f, 1 }, { 0, 2.0f, 2 } }, Unit<float>(), s);
    EXPECT_EQ(r.seedId[0], 1u);
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 1 }));
}

TEST(GridSearchDijkstra, SingleCellGrid)
{
    GridSearchScratch s;
    const auto r = RunSearch<double>(ChainGraph(1), { { 0, 2.5, 4 } }, Unit<double>(), s);
    EXPECT_EQ(r.cost, (std::vector<double>{ 2.5 }));
    EXPECT_EQ(r.prev, (std::vector<uint32_t>{ kNoCell }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 4 }));
}

TEST(GridSearchDijkstra, DefaultHookAcceptsAll)
{
    GridSearchScratch s;
    ChainGraph g(4);
    std::vector<float> cost(4);
    DijkstraOptions<float> o;
    o.outCost = cost.data();
    const CostSeed<float> seed{ 0, 0.0f, 0 };
    MultiSourceDijkstra<float>(g, &seed, 1, Unit<float>(), o, s);
    EXPECT_EQ(cost, (std::vector<float>{ 0, 1, 2, 3 }));
}

TEST(GridSearchDijkstra, ScratchReusedSmallerAfterLarger)
{
    GridSearchScratch s;
    RunSearch<float>(ChainGraph(40), { { 0, 0.0f, 0 } }, Unit<float>(), s);
    const auto r = RunSearch<float>(ChainGraph(3), { { 2, 0.0f, 0 } }, Unit<float>(), s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 2, 1, 0 }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 2, 1, 0 }));
}

TEST(GridSearchDijkstra, SecondWarmCallAllocatesNothing)
{
    GridSearchScratch s;
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    std::vector<float> cost(16);
    std::vector<uint32_t> prev(16), id(16);
    DijkstraOptions<float> o;
    o.outCost = cost.data(); o.outPrev = prev.data(); o.outSeedId = id.data();
    const CostSeed<float> seeds[2] = { { 0, 0.0f, 1 }, { 15, 0.0f, 2 } };
    auto step = [](uint32_t a, uint32_t b) { return 1.0f + float((a + b) % 3); };
    MultiSourceDijkstra<float>(g, seeds, 2, step, o, s);
    GridSearchTest::AllocCounter counter;
    MultiSourceDijkstra<float>(g, seeds, 2, step, o, s);
    EXPECT_EQ(counter.Count(), 0u);
}

namespace {
    template<class Cost, class StepCost>
    void CrossCheckHex(StepCost step)
    {
        HexTopology topo(7, 6);
        HexNeighbours g(topo);
        const uint32_t n = g.CellCount();
        const std::vector<CostSeed<Cost>> seeds{ { 3, Cost(0), 1 }, { 30, Cost(2), 2 }, { 17, Cost(0), 3 } };

        // Naive O(n^2) reference: pick min (cost, cell) among unsettled finite cells.
        constexpr Cost inf = std::numeric_limits<Cost>::infinity();
        std::vector<Cost> ref(n, inf);
        std::vector<uint32_t> refPrev(n, kNoCell), refId(n, kUnreached), refOrder;
        std::vector<uint8_t> done(n, 0);
        for (uint32_t i = 0; i < seeds.size(); ++i)
            if (seeds[i].startCost < ref[seeds[i].cell]) { ref[seeds[i].cell] = seeds[i].startCost; refId[seeds[i].cell] = seeds[i].id; }
        for (;;) {
            uint32_t best = kNoCell;
            for (uint32_t c = 0; c < n; ++c)
                if (!done[c] && ref[c] != inf && (best == kNoCell || ref[c] < ref[best])) best = c;
            if (best == kNoCell) break;
            done[best] = 1;
            refOrder.push_back(best);
            g.ForEachNeighbour(best, [&](uint32_t nb) {
                if (done[nb]) return;
                const Cost e = step(best, nb);
                if (e < Cost(0)) return;
                const Cost cand = ref[best] + e;
                if (cand < ref[nb]) { ref[nb] = cand; refPrev[nb] = best; refId[nb] = refId[best]; }
            });
        }

        GridSearchScratch s;
        const auto r = RunSearch<Cost>(g, seeds, step, s);
        EXPECT_EQ(r.cost, ref);
        EXPECT_EQ(r.prev, refPrev);
        EXPECT_EQ(r.seedId, refId);
        EXPECT_EQ(r.order, refOrder);
    }
}

TEST(GridSearchDijkstra, HexCrossCheckAgainstNaiveFloat)
{
    CrossCheckHex<float>([](uint32_t a, uint32_t b) {
        return ((a + 2 * b) % 11 == 0) ? -1.0f : float((a * 7 + b * 13) % 4);   // zero costs, ties and blocked edges
    });
}

TEST(GridSearchDijkstra, HexCrossCheckAgainstNaiveDouble)
{
    CrossCheckHex<double>([](uint32_t a, uint32_t b) { return 0.25 * double(1 + (a * 5 + b) % 6); });
}
