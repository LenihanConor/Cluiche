#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "DiaPathfinding/AdjacencyGraph.h"
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

// ---- Task 7: settle hook (Skip / Park), ResumeParked, ContinueDijkstra, PushAll mode ----------------------

namespace {
    template<class Cost>
    struct Out {
        std::vector<Cost> cost;
        std::vector<uint32_t> prev, id;
        DijkstraOptions<Cost> o;
        explicit Out(uint32_t n) : cost(n, Cost(-7)), prev(n, 777u), id(n, 777u)
        {
            o.outCost = cost.data(); o.outPrev = prev.data(); o.outSeedId = id.data();
        }
    };

    template<class Cost>
    void CheckParkResumeContinue()
    {
        GridSearchScratch s;
        ChainGraph g(5);
        Out<Cost> out(5);
        const CostSeed<Cost> seed{ 2, Cost(0), 0 };
        std::vector<uint32_t> phase0, phase1;
        MultiSourceDijkstra<Cost>(g, &seed, 1, Unit<Cost>(), out.o, s,
            [&](uint32_t c, Cost, uint32_t) {
                const Settle v = (c == 1 || c == 3) ? Settle::Park : Settle::Accept;
                if (v == Settle::Accept) phase0.push_back(c);
                return v;
            });
        EXPECT_EQ(phase0, (std::vector<uint32_t>{ 2 }));
        EXPECT_EQ(ResumeParked<Cost>(s), 2u);
        EXPECT_EQ(ResumeParked<Cost>(s), 0u);              // nothing left parked
        ContinueDijkstra<Cost>(g, Unit<Cost>(), out.o, s,
            [&](uint32_t c, Cost, uint32_t) { phase1.push_back(c); return Settle::Accept; });
        EXPECT_EQ(phase1, (std::vector<uint32_t>{ 1, 3, 0, 4 }));
        EXPECT_EQ(out.cost, (std::vector<Cost>{ 2, 1, 0, 1, 2 }));
    }
}

TEST(GridSearchDijkstra, ParkHoldsUntilResumeThenContinueFloat) { CheckParkResumeContinue<float>(); }
TEST(GridSearchDijkstra, ParkHoldsUntilResumeThenContinueDouble) { CheckParkResumeContinue<double>(); }

TEST(GridSearchDijkstra, ParkedCellsAreNotSettledAndStayUnexpanded)
{
    GridSearchScratch s;
    ChainGraph g(5);
    Out<float> out(5);
    const CostSeed<float> seed{ 2, 0.0f, 0 };
    MultiSourceDijkstra<float>(g, &seed, 1, Unit<float>(), out.o, s,
        [](uint32_t c, float, uint32_t) { return c == 2 ? Settle::Accept : Settle::Park; });
    EXPECT_EQ(out.cost, (std::vector<float>{ kInfF, 1, 0, 1, kInfF }));   // 0 and 4 never pushed
}

TEST(GridSearchDijkstra, ContinueWithDifferentHookSkipsCell)
{
    GridSearchScratch s;
    ChainGraph g(5);
    Out<float> out(5);
    const CostSeed<float> seed{ 2, 0.0f, 0 };
    MultiSourceDijkstra<float>(g, &seed, 1, Unit<float>(), out.o, s,
        [](uint32_t c, float, uint32_t) { return (c == 1 || c == 3) ? Settle::Park : Settle::Accept; });
    EXPECT_EQ(ResumeParked<float>(s), 2u);
    ContinueDijkstra<float>(g, Unit<float>(), out.o, s,
        [](uint32_t c, float, uint32_t) { return c == 3 ? Settle::Skip : Settle::Accept; });
    // Cell 3 skipped: never settled, cell 4 never reached; cost[3] keeps its push-time value.
    EXPECT_EQ(out.cost, (std::vector<float>{ 2, 1, 0, 1, kInfF }));
}

TEST(GridSearchDijkstra, ParkEveryEntryThenResume)
{
    GridSearchScratch s;
    ChainGraph g(4);
    Out<float> out(4);
    const CostSeed<float> seed{ 0, 0.0f, 3 };
    uint32_t calls = 0;
    MultiSourceDijkstra<float>(g, &seed, 1, Unit<float>(), out.o, s,
        [&](uint32_t, float, uint32_t) { ++calls; return Settle::Park; });
    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, kInfF, kInfF, kInfF }));
    EXPECT_EQ(ResumeParked<float>(s), 1u);
    ContinueDijkstra<float>(g, Unit<float>(), out.o, s);
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, 1, 2, 3 }));
    EXPECT_EQ(out.id, (std::vector<uint32_t>{ 3, 3, 3, 3 }));
    EXPECT_EQ(out.prev, (std::vector<uint32_t>{ kNoCell, 0, 1, 2 }));
}

TEST(GridSearchDijkstra, ResumeWithNothingParkedIsNoOp)
{
    GridSearchScratch s;
    ChainGraph g(4);
    Out<float> out(4);
    const CostSeed<float> seed{ 0, 0.0f, 0 };
    MultiSourceDijkstra<float>(g, &seed, 1, Unit<float>(), out.o, s);
    EXPECT_EQ(ResumeParked<float>(s), 0u);
    ContinueDijkstra<float>(g, Unit<float>(), out.o, s);
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, 1, 2, 3 }));
}

TEST(GridSearchDijkstra, ParkResumeCycleWarmAllocatesNothing)
{
    GridSearchScratch s;
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    Out<float> out(16);
    const CostSeed<float> seeds[2] = { { 0, 0.0f, 1 }, { 15, 0.0f, 2 } };
    auto step = [](uint32_t a, uint32_t b) { return 1.0f + float((a + b) % 3); };
    auto cycle = [&] {
        MultiSourceDijkstra<float>(g, seeds, 2, step, out.o, s,
            [](uint32_t, float cost, uint32_t) { return cost >= 2.0f ? Settle::Park : Settle::Accept; });
        const uint32_t n = ResumeParked<float>(s);
        ContinueDijkstra<float>(g, step, out.o, s);
        return n;
    };
    const uint32_t parkedFirst = cycle();
    EXPECT_GT(parkedFirst, 0u);
    GridSearchTest::AllocCounter counter;
    EXPECT_EQ(cycle(), parkedFirst);
    EXPECT_EQ(counter.Count(), 0u);
}

TEST(GridSearchDijkstra, PushAllSkipLeavesCellClaimableByLaterEntry)
{
    // Chain 0..4, region 1 seeded at 0, region 2 at 4. Region 2 steps cost 2, region 1 steps cost 1.
    // Region 1 reaches cell 2 at cost 2 but is Skipped there (a cap); region 2 higher-cost entry (4) must
    // still claim it. Hand trace: settle 0(1),4(2); push (1,1,1),(2,3,2); settle 1; push (2,2,1);
    // (2,2,1) Skip; settle 3 (cost 2) push (4,2,2); settle 2 id 2 cost 4.
    GridSearchScratch s;
    ChainGraph g(5);
    Out<float> out(5);
    out.o.pushAll = true;
    const CostSeed<float> seeds[2] = { { 0, 0.0f, 1 }, { 4, 0.0f, 2 } };
    auto step = [&](uint32_t from, uint32_t) { return out.id[from] == 2u ? 2.0f : 1.0f; };
    MultiSourceDijkstra<float>(g, seeds, 2, step, out.o, s,
        [](uint32_t c, float, uint32_t id) { return (id == 1 && c >= 2) ? Settle::Skip : Settle::Accept; });
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, 1, 4, 2, 0 }));
    EXPECT_EQ(out.id, (std::vector<uint32_t>{ 1, 1, 2, 2, 2 }));
    EXPECT_EQ(out.prev, (std::vector<uint32_t>{ kNoCell, 0, 3, 4, kNoCell }));
}

TEST(GridSearchDijkstra, PushAllEqualCostCellTieResolvesToLowerSeedId)
{
    GridSearchScratch s;
    ChainGraph g(3);
    Out<float> out(3);
    out.o.pushAll = true;
    const CostSeed<float> seeds[2] = { { 0, 0.0f, 7 }, { 2, 0.0f, 5 } };   // id 7 listed first
    MultiSourceDijkstra<float>(g, seeds, 2, Unit<float>(), out.o, s);
    EXPECT_EQ(out.id, (std::vector<uint32_t>{ 7, 5, 5 }));
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, 1, 0 }));
    EXPECT_EQ(out.prev, (std::vector<uint32_t>{ kNoCell, 2, kNoCell }));
}

TEST(GridSearchDijkstra, StrictModeTieRuleLowerSeedIdWinsEqualCost)
{
    GridSearchScratch s;
    ChainGraph g(3);
    Out<float> out(3);
    const CostSeed<float> seeds[2] = { { 0, 0.0f, 7 }, { 2, 0.0f, 5 } };
    MultiSourceDijkstra<float>(g, seeds, 2, Unit<float>(), out.o, s);
    EXPECT_EQ(out.id, (std::vector<uint32_t>{ 7, 5, 5 }));
    EXPECT_EQ(out.prev, (std::vector<uint32_t>{ kNoCell, 2, kNoCell }));
    EXPECT_EQ(out.cost, (std::vector<float>{ 0, 1, 0 }));
}

TEST(GridSearchDijkstra, HeapOrdersEqualCostCellBySeedId)
{
    GridSearchScratch s;
    auto& h = s.Dijkstra<float>().heap;
    h.Push({ 1.0f, 4, 9 }); h.Push({ 1.0f, 4, 2 }); h.Push({ 1.0f, 4, 5 });
    EXPECT_EQ(h.Pop().seedId, 2u);
    EXPECT_EQ(h.Pop().seedId, 5u);
    EXPECT_EQ(h.Pop().seedId, 9u);
}

namespace {
    // Two-phase capped / floored region growth on a chain of 7 cells. Seeds: region 1 at 0, region 2 at 3,
    // region 3 at 6. Step cost 3 for regions 1 and 3, 1 for region 2 (read from outSeedId[from]).
    struct RegionFixture {
        GridSearchScratch s;
        ChainGraph g{ 7 };
        Out<float> out{ 7 };
        std::vector<uint32_t> claimed = std::vector<uint32_t>(4, 0u);
        std::vector<CostSeed<float>> seeds{ { 0, 0.0f, 1 }, { 3, 0.0f, 2 }, { 6, 0.0f, 3 } };
        uint32_t capCells = 4, floorCells = 2;
        RegionFixture() { out.o.pushAll = true; }
        float Step(uint32_t from) const { return out.id[from] == 2u ? 1.0f : 3.0f; }
    };
}

TEST(GridSearchDijkstra, RegionCapOnlyFixture)
{
    RegionFixture f;
    MultiSourceDijkstra<float>(f.g, f.seeds.data(), 3, [&](uint32_t from, uint32_t) { return f.Step(from); }, f.out.o, f.s,
        [&](uint32_t, float, uint32_t id) {
            if (f.claimed[id] >= f.capCells) return Settle::Skip;
            ++f.claimed[id];
            return Settle::Accept;
        });
    EXPECT_EQ(f.out.id, (std::vector<uint32_t>{ 1, 2, 2, 2, 2, 3, 3 }));
    EXPECT_EQ(f.out.cost, (std::vector<float>{ 0, 2, 1, 0, 1, 3, 0 }));
    EXPECT_EQ(f.claimed, (std::vector<uint32_t>{ 0, 1, 4, 2 }));
}

TEST(GridSearchDijkstra, RegionTwoPhaseFloorAndCapFixture)
{
    RegionFixture f;
    auto step = [&](uint32_t from, uint32_t) { return f.Step(from); };
    std::vector<uint32_t> parkedSeq;
    MultiSourceDijkstra<float>(f.g, f.seeds.data(), 3, step, f.out.o, f.s,
        [&](uint32_t c, float, uint32_t id) {
            if (f.claimed[id] >= f.capCells) return Settle::Skip;
            if (f.claimed[id] >= f.floorCells) { parkedSeq.push_back(c); return Settle::Park; }
            ++f.claimed[id];
            return Settle::Accept;
        });
    // Phase 0 parks (1,4,r2), (2,1,r2) and (6,4,r3).
    EXPECT_EQ(parkedSeq, (std::vector<uint32_t>{ 4, 1, 4 }));
    EXPECT_EQ(ResumeParked<float>(f.s), 3u);
    ContinueDijkstra<float>(f.g, step, f.out.o, f.s,
        [&](uint32_t, float, uint32_t id) {
            if (f.claimed[id] >= f.capCells) return Settle::Skip;
            ++f.claimed[id];
            return Settle::Accept;
        });
    EXPECT_EQ(f.out.id, (std::vector<uint32_t>{ 1, 1, 2, 2, 2, 3, 3 }));
    EXPECT_EQ(f.out.cost, (std::vector<float>{ 0, 3, 1, 0, 1, 3, 0 }));
    EXPECT_EQ(f.claimed, (std::vector<uint32_t>{ 0, 2, 3, 2 }));
}

TEST(GridSearchDijkstra, ContinueDijkstraDoubleCompiles)
{
    GridSearchScratch s;
    ChainGraph g(3);
    Out<double> out(3);
    const CostSeed<double> seed{ 0, 0.0, 0 };
    MultiSourceDijkstra<double>(g, &seed, 1, Unit<double>(), out.o, s,
        [](uint32_t, double, uint32_t) { return Settle::Park; });
    EXPECT_EQ(ResumeParked<double>(s), 1u);
    ContinueDijkstra<double>(g, Unit<double>(), out.o, s);
    EXPECT_EQ(out.cost, (std::vector<double>{ 0, 1, 2 }));
}

// ---- Task 24 gap audit ------------------------------------------------------------------------------------

TEST(GridSearchDijkstra, AllEdgesBlockedOnlySeedSettles)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(4), { { 1, 0.0f, 3 } }, [](uint32_t, uint32_t) { return -1.0f; }, s);
    EXPECT_EQ(r.cost, (std::vector<float>{ kInfF, 0, kInfF, kInfF }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 1 }));
    EXPECT_EQ(r.prev, (std::vector<uint32_t>{ kNoCell, kNoCell, kNoCell, kNoCell }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ kUnreached, 3, kUnreached, kUnreached }));
}

TEST(GridSearchDijkstra, TyingSeedCostsOnDifferentCells)
{
    GridSearchScratch s;
    const auto r = RunSearch<float>(ChainGraph(5), { { 0, 2.0f, 1 }, { 4, 2.0f, 2 } }, Unit<float>(), s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 2, 3, 4, 3, 2 }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 1, 1, 1, 2, 2 }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 4, 1, 3, 2 }));
}

TEST(GridSearchDijkstra, ThinHexGridsCostEqualsIndexFromCellZero)
{
    for (auto dims : { std::pair<uint32_t, uint32_t>{ 1, 6 }, std::pair<uint32_t, uint32_t>{ 6, 1 } }) {
        GridSearchScratch s;
        HexTopology topo(dims.first, dims.second);
        HexNeighbours g(topo);
        const auto r = RunSearch<float>(g, { { 0, 0.0f, 1 } }, Unit<float>(), s);
        EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 2, 3, 4, 5 }));
        EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5 }));
    }
}

TEST(GridSearchDijkstra, HexTieHeavyPopOrderIsCostThenCellIndex)
{
    // Unit costs from interior cell 5; its neighbours (direction order 6,1,4,8,9,10) all tie at cost 1 and
    // must pop by ascending cell index, whatever order they were pushed in.
    GridSearchScratch s;
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    const auto r = RunSearch<float>(g, { { 5, 0.0f, 0 } }, Unit<float>(), s);
    ASSERT_EQ(r.order.size(), 16u);
    EXPECT_EQ(std::vector<uint32_t>(r.order.begin(), r.order.begin() + 7), (std::vector<uint32_t>{ 5, 1, 4, 6, 8, 9, 10 }));
    for (size_t i = 1; i < r.order.size(); ++i) {
        const uint32_t a = r.order[i - 1], b = r.order[i];
        EXPECT_TRUE(r.cost[a] < r.cost[b] || (r.cost[a] == r.cost[b] && a < b)) << "pop " << i;
    }
}

TEST(GridSearchDijkstra, AdjacencyForestWithIsolatedCell)
{
    // Forest: 0 -> {1,2}, 1 -> 3, 4 -> 5; cell 6 isolated.
    AdjacencyEdge edges[] = { { 4, 5 }, { 1, 3 }, { 0, 2 }, { 0, 1 } };
    AdjacencyGraph g;
    g.Build(7, edges, 4);
    GridSearchScratch s;
    const auto r = RunSearch<float>(g, { { 0, 0.0f, 10 }, { 4, 0.0f, 20 } }, Unit<float>(), s);
    EXPECT_EQ(r.cost, (std::vector<float>{ 0, 1, 1, 2, 0, 1, kInfF }));
    EXPECT_EQ(r.order, (std::vector<uint32_t>{ 0, 4, 1, 2, 5, 3 }));
    EXPECT_EQ(r.seedId, (std::vector<uint32_t>{ 10, 10, 10, 10, 20, 20, kUnreached }));
    const auto iso = RunSearch<double>(g, { { 6, 1.5, 7 } }, Unit<double>(), s);
    EXPECT_EQ(iso.cost, (std::vector<double>{ kInfD, kInfD, kInfD, kInfD, kInfD, kInfD, 1.5 }));
}
