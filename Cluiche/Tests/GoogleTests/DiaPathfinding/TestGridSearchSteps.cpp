#include <gtest/gtest.h>

#include <deque>
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
    constexpr uint32_t U = kUnreached;

    auto AllPassable() { return [](uint32_t) { return true; }; }

    struct Result {
        std::vector<uint32_t> steps;
        std::vector<uint32_t> ids;
    };

    template<class G, class P>
    Result RunSteps(const G& g, const std::vector<StepSeed>& seeds, P passable, uint32_t maxSteps, GridSearchScratch& scratch)
    {
        Result r;
        r.steps.assign(g.CellCount(), 7u);
        r.ids.assign(g.CellCount(), 7u);
        MultiSourceSteps(g, seeds.data(), static_cast<uint32_t>(seeds.size()), passable, maxSteps,
                         r.steps.data(), r.ids.data(), scratch);
        return r;
    }

    template<class G, class P>
    Result RunSteps(const G& g, const std::vector<StepSeed>& seeds, P passable, uint32_t maxSteps)
    {
        GridSearchScratch scratch;
        return RunSteps(g, seeds, passable, maxSteps, scratch);
    }

    // Plain pre-seeded FIFO BFS (all start offsets 0).
    template<class G, class P>
    Result NaiveBfs(const G& g, const std::vector<StepSeed>& seeds, P passable, uint32_t maxSteps)
    {
        Result r;
        r.steps.assign(g.CellCount(), kUnreached);
        r.ids.assign(g.CellCount(), kUnreached);
        std::deque<uint32_t> q;
        for (const StepSeed& s : seeds) {
            if (s.cell >= g.CellCount() || !passable(s.cell) || r.steps[s.cell] != kUnreached) continue;
            r.steps[s.cell] = 0;
            r.ids[s.cell] = s.id;
            q.push_back(s.cell);
        }
        while (!q.empty()) {
            const uint32_t c = q.front();
            q.pop_front();
            if (r.steps[c] >= maxSteps) continue;
            g.ForEachNeighbour(c, [&](uint32_t n) {
                if (passable(n) && r.steps[n] == kUnreached) {
                    r.steps[n] = r.steps[c] + 1;
                    r.ids[n] = r.ids[c];
                    q.push_back(n);
                }
            });
        }
        return r;
    }
}

TEST(GridSearchSteps, ChainSingleSeedExactSteps)
{
    ChainGraph g(6);
    Result r = RunSteps(g, {{0, 0, 9}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, 4, 5}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{9, 9, 9, 9, 9, 9}));
}

TEST(GridSearchSteps, ChainTwoSeedsNearestWins)
{
    ChainGraph g(8);
    Result r = RunSteps(g, {{0, 0, 10}, {7, 0, 20}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, 3, 2, 1, 0}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{10, 10, 10, 10, 20, 20, 20, 20}));
}

TEST(GridSearchSteps, EqualDistanceTiesGoToLowerSeedIndex)
{
    ChainGraph g(7);
    Result r = RunSteps(g, {{0, 0, 10}, {6, 0, 20}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, 2, 1, 0}));
    EXPECT_EQ(r.ids[3], 10u);

    Result swapped = RunSteps(g, {{6, 0, 20}, {0, 0, 10}}, AllPassable(), 100);
    EXPECT_EQ(swapped.ids[3], 20u);
}

TEST(GridSearchSteps, SeedIdIsStoredNotIndex)
{
    ChainGraph g(3);
    Result r = RunSteps(g, {{2, 0, 500}}, AllPassable(), 100);
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{500, 500, 500}));
}

TEST(GridSearchSteps, TreeGraphDirectedSteps)
{
    TreeGraph g({kNoCell, 0, 0, 1, 1, 2});
    Result r = RunSteps(g, {{0, 0, 1}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 1, 2, 2, 2}));
}

TEST(GridSearchSteps, StartOffsetsHonoured)
{
    ChainGraph g(8);
    // A: cell 0 offset 3 (id 1); B: cell 7 offset 0 (id 2).
    Result r = RunSteps(g, {{0, 3, 1}, {7, 0, 2}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{3, 4, 5, 4, 3, 2, 1, 0}));
    // Cell 2 ties at 5; the injected seed's expansion precedes the older seed's frontier cell.
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{1, 1, 1, 2, 2, 2, 2, 2}));
}

TEST(GridSearchSteps, HigherOffsetSeedNeverOverwritesEarlierReach)
{
    ChainGraph g(5);
    // Seed 1 sits on a cell already reached at step 1 by seed 0; its offset 4 is worse, so it is dropped.
    Result r = RunSteps(g, {{0, 0, 1}, {1, 4, 2}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, 4}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{1, 1, 1, 1, 1}));
}

TEST(GridSearchSteps, SeedOffsetLowerThanReachOverrides)
{
    ChainGraph g(5);
    // Seed 0 reaches cell 3 at step 3; seed 1 starts there at 1 and wins.
    Result r = RunSteps(g, {{0, 0, 1}, {3, 1, 2}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 1, 2}));
    EXPECT_EQ(r.ids[3], 2u);
}

TEST(GridSearchSteps, MaxStepsInclusive)
{
    ChainGraph g(8);
    Result r = RunSteps(g, {{0, 0, 1}}, AllPassable(), 3);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, U, U, U, U}));
    EXPECT_EQ(r.ids[4], U);
}

TEST(GridSearchSteps, MaxStepsZeroOnlySeeds)
{
    ChainGraph g(4);
    Result r = RunSteps(g, {{1, 0, 5}}, AllPassable(), 0);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{U, 0, U, U}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{U, 5, U, U}));
}

TEST(GridSearchSteps, SeedBeyondMaxStepsStaysUnreached)
{
    ChainGraph g(4);
    Result r = RunSteps(g, {{0, 5, 1}}, AllPassable(), 3);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{U, U, U, U}));
}

TEST(GridSearchSteps, ImpassableCellsStayUnreached)
{
    ChainGraph g(6);
    Result r = RunSteps(g, {{0, 0, 1}}, [](uint32_t c) { return c != 3; }, 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, U, U, U}));
    EXPECT_EQ(r.ids[3], U);
}

TEST(GridSearchSteps, SeedOnImpassableCellIgnored)
{
    ChainGraph g(4);
    Result r = RunSteps(g, {{1, 0, 1}, {3, 0, 2}}, [](uint32_t c) { return c != 1; }, 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{U, U, 1, 0}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{U, U, 2, 2}));
}

TEST(GridSearchSteps, SeedOutOfRangeIgnored)
{
    ChainGraph g(3);
    Result r = RunSteps(g, {{99, 0, 1}, {0, 0, 2}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2}));
}

TEST(GridSearchSteps, ZeroSeedsAllUnreached)
{
    ChainGraph g(4);
    Result r = RunSteps(g, {}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{U, U, U, U}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{U, U, U, U}));
}

TEST(GridSearchSteps, AllCellsImpassable)
{
    ChainGraph g(4);
    Result r = RunSteps(g, {{0, 0, 1}, {2, 0, 2}}, [](uint32_t) { return false; }, 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{U, U, U, U}));
}

TEST(GridSearchSteps, NullSeedIdOutputAllowed)
{
    ChainGraph g(4);
    GridSearchScratch scratch;
    std::vector<StepSeed> seeds{{0, 0, 1}};
    std::vector<uint32_t> steps(4, 7u);
    MultiSourceSteps(g, seeds.data(), 1, AllPassable(), 100, steps.data(), nullptr, scratch);
    EXPECT_EQ(steps, (std::vector<uint32_t>{0, 1, 2, 3}));
}

TEST(GridSearchSteps, DuplicateSeedsOnOneCellLowestIndexWins)
{
    ChainGraph g(3);
    Result r = RunSteps(g, {{1, 0, 5}, {1, 0, 6}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{1, 0, 1}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{5, 5, 5}));
}

TEST(GridSearchSteps, DuplicateSeedsLowestOffsetWins)
{
    ChainGraph g(3);
    Result r = RunSteps(g, {{1, 2, 7}, {1, 0, 8}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{1, 0, 1}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{8, 8, 8}));
}

TEST(GridSearchSteps, TyingOffsetsOnDifferentCells)
{
    ChainGraph g(5);
    Result r = RunSteps(g, {{0, 2, 1}, {4, 2, 2}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{2, 3, 4, 3, 2}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{1, 1, 1, 2, 2}));
}

TEST(GridSearchSteps, SingleCellGrid)
{
    HexTopology topo(1, 1);
    HexNeighbours g(topo);
    Result r = RunSteps(g, {{0, 0, 3}}, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{3}));
}

TEST(GridSearchSteps, ThinGridsMatchReference)
{
    for (auto dims : {std::pair<uint32_t, uint32_t>{1, 6}, std::pair<uint32_t, uint32_t>{6, 1}}) {
        HexTopology topo(dims.first, dims.second);
        HexNeighbours g(topo);
        std::vector<StepSeed> seeds{{0, 0, 1}, {5, 0, 2}};
        Result r = RunSteps(g, seeds, AllPassable(), 100);
        Result ref = NaiveBfs(g, seeds, AllPassable(), 100);
        EXPECT_EQ(r.steps, ref.steps);
        EXPECT_EQ(r.ids, ref.ids);
        EXPECT_EQ(r.steps[0], 0u);
        EXPECT_EQ(r.steps[5], 0u);
    }
}

TEST(GridSearchSteps, HexGridSingleSeedNeighboursAreOneStep)
{
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    Result r = RunSteps(g, {{5, 0, 1}}, AllPassable(), 100);
    EXPECT_EQ(r.steps[5], 0u);
    g.ForEachNeighbour(5, [&](uint32_t n) { EXPECT_EQ(r.steps[n], 1u); });
    for (uint32_t s : r.steps) EXPECT_NE(s, U);
}

TEST(GridSearchSteps, HexGridMatchesNaiveFifoBfs)
{
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    std::vector<StepSeed> seeds{{0, 0, 11}, {15, 0, 22}, {6, 0, 33}, {6, 0, 44}, {99, 0, 55}};
    auto pass = [](uint32_t c) { return c != 9 && c != 10; };
    for (uint32_t maxSteps : {0u, 1u, 2u, 3u, 100u}) {
        Result r = RunSteps(g, seeds, pass, maxSteps);
        Result ref = NaiveBfs(g, seeds, pass, maxSteps);
        EXPECT_EQ(r.steps, ref.steps) << "maxSteps " << maxSteps;
        EXPECT_EQ(r.ids, ref.ids) << "maxSteps " << maxSteps;
    }
}

TEST(GridSearchSteps, ScratchReusedSmallerAfterLarger)
{
    GridSearchScratch scratch;
    ChainGraph big(20);
    RunSteps(big, {{0, 0, 1}, {19, 3, 2}}, AllPassable(), 100, scratch);
    ChainGraph small(4);
    Result r = RunSteps(small, {{3, 0, 9}}, AllPassable(), 100, scratch);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{3, 2, 1, 0}));
    EXPECT_EQ(r.ids, (std::vector<uint32_t>{9, 9, 9, 9}));
}

TEST(GridSearchSteps, SecondWarmCallAllocatesNothing)
{
    HexTopology topo = MakeSmallHexGrid();
    HexNeighbours g(topo);
    GridSearchScratch scratch;
    std::vector<StepSeed> seeds{{0, 0, 1}, {15, 2, 2}, {6, 1, 3}};
    std::vector<uint32_t> steps(g.CellCount());
    std::vector<uint32_t> ids(g.CellCount());
    MultiSourceSteps(g, seeds.data(), 3, AllPassable(), 100, steps.data(), ids.data(), scratch);
    {
        GridSearchTest::AllocCounter counter;
        MultiSourceSteps(g, seeds.data(), 3, AllPassable(), 100, steps.data(), ids.data(), scratch);
        EXPECT_EQ(counter.Count(), 0u);
    }
}

// Tree: 0 -> {1,2}; 1 -> {3,4}; 2 -> {5}; 5 -> {6}. Depths: 0,1,1,2,2,2,3.
TEST(GridSearchSteps, AdjacencyTreeExactSteps) {
    AdjacencyEdge edges[] = {{2, 5}, {0, 2}, {1, 4}, {5, 6}, {0, 1}, {1, 3}};
    AdjacencyGraph g;
    g.Build(7, edges, 6);
    GridSearchScratch scratch;
    auto r = RunSteps(g, {{0, 0, 0}}, AllPassable(), 100, scratch);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 1, 2, 2, 2, 3}));
}

// Forest: roots 0 and 4. 0 -> {1,2}; 4 -> {5}; 5 -> {6}; cell 3 isolated.
TEST(GridSearchSteps, AdjacencyForestTwoSeeds) {
    AdjacencyEdge edges[] = {{0, 1}, {0, 2}, {4, 5}, {5, 6}};
    AdjacencyGraph g;
    g.Build(7, edges, 4);
    auto both = RunSteps(g, {{0, 0, 10}, {4, 0, 20}}, AllPassable(), 100);
    EXPECT_EQ(both.steps, (std::vector<uint32_t>{0, 1, 1, U, 0, 1, 2}));
    EXPECT_EQ(both.ids, (std::vector<uint32_t>{10, 10, 10, U, 20, 20, 20}));
    auto one = RunSteps(g, {{0, 0, 0}}, AllPassable(), 100);
    EXPECT_EQ(one.steps, (std::vector<uint32_t>{0, 1, 1, U, U, U, U}));
}

// NodesRiver-style: children graph from parent array, longest depth from root seeds.
TEST(GridSearchSteps, AdjacencyLongestDepthFromRoots) {
    const uint32_t N = kNoCell;
    const std::vector<uint32_t> parent = {N, 0, 1, 2, 0, N, 5};
    std::vector<AdjacencyEdge> edges;
    std::vector<StepSeed> seeds;
    for (uint32_t i = 0; i < parent.size(); ++i) {
        if (parent[i] == N) { seeds.push_back({i, 0, i}); } else { edges.push_back({parent[i], i}); }
    }
    AdjacencyGraph g;
    g.Build(static_cast<uint32_t>(parent.size()), edges.data(), static_cast<uint32_t>(edges.size()));
    auto r = RunSteps(g, seeds, AllPassable(), 100);
    EXPECT_EQ(r.steps, (std::vector<uint32_t>{0, 1, 2, 3, 1, 0, 1}));
    uint32_t best = 0;
    for (uint32_t i = 1; i < r.steps.size(); ++i) { if (r.steps[i] > r.steps[best]) { best = i; } }
    EXPECT_EQ(best, 3u);
    EXPECT_EQ(r.steps[best], 3u);
}
