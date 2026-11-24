#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <queue>
#include <vector>

#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaPathfinding/Testing/GridSearchTestGrids.h"
#include "GridSearchAllocCounter.h"

using namespace Dia::Pathfinding;
using Dia::Geometry2D::HexTopology;
using Dia::Geometry2D::kNoCell;
using namespace Dia::Pathfinding::Testing;

namespace {
    struct Flooded {
        std::vector<float> filled, level;
    };

    template<class G>
    Flooded RunFlood(const G& g, const std::vector<float>& elev, const std::vector<uint8_t>& outlet, float eps,
                GridSearchScratch& s)
    {
        Flooded r;
        r.filled.assign(g.CellCount(), -99.0f);
        r.level.assign(g.CellCount(), -99.0f);
        PriorityFlood(g, elev.data(), outlet.data(), eps, r.filled.data(), r.level.data(), s);
        return r;
    }
}

TEST(GridSearchPriorityFlood, ChainDepressionFillsToSpillLevel)
{
    // Outlet at 0. Hand trace (eps 0.5): cell1 level 4 filled 4; cell2 (pit, raw 1) level 4 filled 4.5;
    // cell3 level 4 filled 5; cell4 level 6 filled 6; cell5 (pit) level 6 filled 6.5.
    GridSearchScratch s;
    const auto r = RunFlood(ChainGraph(6), { 0, 4, 1, 2, 6, 1 }, { 1, 0, 0, 0, 0, 0 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 0, 4, 4.5f, 5, 6, 6.5f }));
    EXPECT_EQ(r.level, (std::vector<float>{ 0, 4, 4, 4, 6, 6 }));
}

TEST(GridSearchPriorityFlood, HexRingOutletsFillInteriorToRidge)
{
    // Border cells are outlets at elevation 3; the four interior cells (5, 6, 9, 10) sit at 1 and each
    // touches a border cell. Borders pop before interior entries (equal level, lower filled), so every
    // interior cell is claimed by a border parent: level 3, filled 3 + 0.5.
    GridSearchScratch s;
    const HexTopology topo = MakeSmallHexGrid();
    const HexNeighbours g(topo);
    std::vector<float> elev(16, 3.0f);
    std::vector<uint8_t> outlet(16, 1);
    for (uint32_t c : { 5u, 6u, 9u, 10u }) { elev[c] = 1.0f; outlet[c] = 0; }
    const auto r = RunFlood(g, elev, outlet, 0.5f, s);
    for (uint32_t c = 0; c < 16; ++c) {
        const bool interior = !outlet[c];
        EXPECT_EQ(r.level[c], 3.0f) << c;
        EXPECT_EQ(r.filled[c], interior ? 3.5f : 3.0f) << c;
    }
}

TEST(GridSearchPriorityFlood, UnreachedCellsKeepRawElevation)
{
    // Directed edges 0->1 and 2->3; only cell 0 is an outlet, so cells 2 and 3 are never reached.
    GridSearchScratch s;
    TreeGraph g({ kNoCell, 0, kNoCell, 2 });
    const auto r = RunFlood(g, { 2, 1, 7, 8 }, { 1, 0, 0, 0 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 2, 2.5f, 7, 8 }));
    EXPECT_EQ(r.level, (std::vector<float>{ 2, 2, 7, 8 }));
}

TEST(GridSearchPriorityFlood, NoOutletsLeavesRawElevation)
{
    GridSearchScratch s;
    const auto r = RunFlood(ChainGraph(4), { 3, 1, 2, 5 }, { 0, 0, 0, 0 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 3, 1, 2, 5 }));
    EXPECT_EQ(r.level, (std::vector<float>{ 3, 1, 2, 5 }));
}

TEST(GridSearchPriorityFlood, AllOutletsLeavesRawElevation)
{
    GridSearchScratch s;
    const auto r = RunFlood(ChainGraph(4), { 3, 1, 2, 5 }, { 1, 1, 1, 1 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 3, 1, 2, 5 }));
    EXPECT_EQ(r.level, (std::vector<float>{ 3, 1, 2, 5 }));
}

TEST(GridSearchPriorityFlood, FlatPlateauGivesEpsilonStaircase)
{
    GridSearchScratch s;
    const auto r = RunFlood(ChainGraph(5), { 1, 1, 1, 1, 1 }, { 1, 0, 0, 0, 0 }, 0.25f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 1, 1.25f, 1.5f, 1.75f, 2.0f }));
    EXPECT_EQ(r.level, (std::vector<float>{ 1, 1, 1, 1, 1 }));
}

TEST(GridSearchPriorityFlood, SingleCellGrid)
{
    GridSearchScratch s;
    auto r = RunFlood(ChainGraph(1), { 4 }, { 1 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 4 }));
    EXPECT_EQ(r.level, (std::vector<float>{ 4 }));
    r = RunFlood(ChainGraph(1), { 4 }, { 0 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 4 }));
    EXPECT_EQ(r.level, (std::vector<float>{ 4 }));
}

TEST(GridSearchPriorityFlood, NullOutLevelIsAccepted)
{
    GridSearchScratch s;
    std::vector<float> filled(6, -1.0f);
    const std::vector<float> elev{ 0, 4, 1, 2, 6, 1 };
    const std::vector<uint8_t> outlet{ 1, 0, 0, 0, 0, 0 };
    PriorityFlood(ChainGraph(6), elev.data(), outlet.data(), 0.5f, filled.data(), nullptr, s);
    EXPECT_EQ(filled, (std::vector<float>{ 0, 4, 4.5f, 5, 6, 6.5f }));
}

TEST(GridSearchPriorityFlood, ScratchReusedSmallerAfterLarger)
{
    GridSearchScratch s;
    RunFlood(ChainGraph(40), std::vector<float>(40, 1.0f), [] { std::vector<uint8_t> o(40, 0); o[0] = 1; return o; }(), 0.5f, s);
    const auto r = RunFlood(ChainGraph(6), { 0, 4, 1, 2, 6, 1 }, { 1, 0, 0, 0, 0, 0 }, 0.5f, s);
    EXPECT_EQ(r.filled, (std::vector<float>{ 0, 4, 4.5f, 5, 6, 6.5f }));
    EXPECT_EQ(r.level, (std::vector<float>{ 0, 4, 4, 4, 6, 6 }));
}

TEST(GridSearchPriorityFlood, SecondWarmCallAllocatesNothing)
{
    GridSearchScratch s;
    const HexTopology topo = MakeSmallHexGrid();
    const HexNeighbours g(topo);
    std::vector<float> elev(16);
    std::vector<uint8_t> outlet(16, 0);
    for (uint32_t i = 0; i < 16; ++i) elev[i] = float((i * 7u) % 5u);
    outlet[0] = outlet[15] = 1;
    std::vector<float> filled(16), level(16);
    PriorityFlood(g, elev.data(), outlet.data(), 1e-5f, filled.data(), level.data(), s);
    GridSearchTest::AllocCounter counter;
    PriorityFlood(g, elev.data(), outlet.data(), 1e-5f, filled.data(), level.data(), s);
    EXPECT_EQ(counter.Count(), 0u);
}

namespace {
    // Reference copy of the S-1 algorithm (TerrainDrainage.cpp stage 5 fill) with std::priority_queue.
    struct RefEntry { float level; float filled; uint32_t cell; };
    struct RefLater {
        bool operator()(const RefEntry& a, const RefEntry& b) const {
            if (a.level != b.level) return a.level > b.level;
            if (a.filled != b.filled) return a.filled > b.filled;
            return a.cell > b.cell;
        }
    };

    void RefFlood(const HexTopology& topo, const std::vector<float>& elev, const std::vector<uint8_t>& outlet,
                  float eps, std::vector<float>& filled, std::vector<float>& level)
    {
        const uint32_t n = topo.CellCount();
        filled.assign(n, 0.0f);
        level.assign(n, 0.0f);
        std::vector<uint8_t> visited(n, 0);
        std::priority_queue<RefEntry, std::vector<RefEntry>, RefLater> heap;
        for (uint32_t i = 0; i < n; ++i) {
            filled[i] = elev[i];
            level[i] = elev[i];
            if (outlet[i]) { visited[i] = 1; heap.push(RefEntry{ level[i], filled[i], i }); }
        }
        while (!heap.empty()) {
            const RefEntry e = heap.top();
            heap.pop();
            for (uint32_t k = 0; k < 6; ++k) {
                const uint32_t nb = topo.NeighbourAt(e.cell, k);
                if (nb == kNoCell || visited[nb]) continue;
                visited[nb] = 1;
                const float raw = elev[nb];
                level[nb] = std::max(raw, e.level);
                filled[nb] = std::max(raw, e.filled + eps);
                heap.push(RefEntry{ level[nb], filled[nb], nb });
            }
        }
    }

    bool BitEqual(const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
    }
}

TEST(GridSearchPriorityFlood, MatchesNaiveS1ReferenceBitwiseOnHex)
{
    GridSearchScratch s;
    for (uint32_t seed = 1; seed <= 5; ++seed) {
        const HexTopology topo(9, 8);
        const HexNeighbours g(topo);
        const uint32_t n = topo.CellCount();
        uint32_t state = seed * 2654435761u;
        auto next = [&] { state = state * 1664525u + 1013904223u; return (state >> 8) & 0xFFFFu; };
        std::vector<float> elev(n);
        std::vector<uint8_t> outlet(n, 0);
        for (uint32_t i = 0; i < n; ++i) {
            elev[i] = float(next() % 40u) * 0.1f;   // plenty of exact ties
            outlet[i] = (next() % 9u == 0u) ? 1 : 0;
        }
        outlet[seed] = 1;
        std::vector<float> rf, rl;
        RefFlood(topo, elev, outlet, 1e-5f, rf, rl);
        const auto r = RunFlood(g, elev, outlet, 1e-5f, s);
        EXPECT_TRUE(BitEqual(r.filled, rf)) << "filled, seed " << seed;
        EXPECT_TRUE(BitEqual(r.level, rl)) << "level, seed " << seed;
    }
}
