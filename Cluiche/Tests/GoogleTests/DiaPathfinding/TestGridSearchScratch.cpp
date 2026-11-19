// TestGridSearchScratch.cpp - GridSearchScratch storage, heaps and retained capacity.

#include <gtest/gtest.h>

#include "DiaPathfinding/GridSearchScratch.h"
#include "GridSearchAllocCounter.h"

using namespace Dia::Pathfinding;
using GridSearchTest::AllocCounter;

namespace {
    template<class Cost>
    void CheckDijkstraOrder()
    {
        GridSearchScratch s;
        auto& h = s.Dijkstra<Cost>().heap;
        // cost, cell, seedId: deliberately shuffled with equal-cost ties
        const DijkstraEntry<Cost> in[] = {
            {Cost(2), 5, 0}, {Cost(1), 9, 1}, {Cost(2), 3, 2}, {Cost(1), 2, 3},
            {Cost(0), 7, 4}, {Cost(2), 4, 5}, {Cost(1), 2, 6}, {Cost(3), 0, 7}};
        for (const auto& e : in) h.Push(e);
        EXPECT_EQ(h.Size(), 8u);
        const uint32_t expectCells[] = {7, 2, 2, 9, 3, 4, 5, 0};
        const Cost expectCost[] = {Cost(0), Cost(1), Cost(1), Cost(1), Cost(2), Cost(2), Cost(2), Cost(3)};
        for (int i = 0; i < 8; ++i)
        {
            ASSERT_FALSE(h.Empty());
            const auto e = h.Pop();
            EXPECT_EQ(e.cell, expectCells[i]) << i;
            EXPECT_EQ(e.cost, expectCost[i]) << i;
        }
        EXPECT_TRUE(h.Empty());
    }
}

TEST(GridSearchScratchHeap, DijkstraFloatPopsInCostCellOrder) { CheckDijkstraOrder<float>(); }
TEST(GridSearchScratchHeap, DijkstraDoublePopsInCostCellOrder) { CheckDijkstraOrder<double>(); }

TEST(GridSearchScratchHeap, FloodHeapPopsInLevelFilledCellOrder)
{
    GridSearchScratch s;
    auto& h = s.FloodHeap();
    const FloodEntry in[] = {{2.f, 1.f, 4}, {1.f, 5.f, 2}, {1.f, 3.f, 9}, {1.f, 3.f, 1}, {2.f, 0.f, 8}, {0.f, 9.f, 6}};
    for (const auto& e : in) h.Push(e);
    const uint32_t expect[] = {6, 1, 9, 2, 8, 4};
    for (uint32_t c : expect) { EXPECT_EQ(h.Pop().cell, c); }
    EXPECT_TRUE(h.Empty());
}

TEST(GridSearchScratchHeap, ManyPushesPopSorted)
{
    GridSearchScratch s;
    auto& h = s.Dijkstra<float>().heap;
    uint32_t x = 12345;
    for (uint32_t i = 0; i < 500; ++i) { x = x * 1664525u + 1013904223u; h.Push({float(x % 17), i, 0}); }
    DijkstraEntry<float> prev = h.Pop();
    while (!h.Empty()) { auto e = h.Pop(); EXPECT_FALSE(e < prev); prev = e; }
}

TEST(GridSearchScratchQueue, FifoOrderAndClearKeepsCapacity)
{
    GridSearchScratch s;
    auto& q = s.Queue();
    for (uint32_t i = 0; i < 40; ++i) q.Push(i);
    for (uint32_t i = 0; i < 40; ++i) EXPECT_EQ(q.Pop(), i);
    EXPECT_TRUE(q.Empty());
    const uint32_t cap = q.Capacity();
    q.Clear();
    EXPECT_EQ(q.Capacity(), cap);
}

TEST(GridSearchScratchReserve, ReserveGrowsOnceThenWarmRefillAllocatesNothing)
{
    GridSearchScratch s;
    s.Reserve(1000);
    const uint64_t cap = s.TotalCapacity();
    EXPECT_GE(cap, 1000u);

    {
        AllocCounter counter;
        s.Reserve(1000);   // already large enough
        EXPECT_EQ(counter.Count(), 0u);
    }

    AllocCounter counter;
    s.Reset();
    for (uint32_t i = 0; i < 1000; ++i)
    {
        s.Queue().Push(i);
        s.Frontier().Add(i);
        s.NextFrontier().Add(i);
        s.PendingSeeds().Add({i, i, i});
        s.FloodHeap().Push({float(i % 7), 0.f, i});
        s.Dijkstra<float>().heap.Push({float(i % 5), i, 0});
        s.Dijkstra<double>().heap.Push({double(i % 5), i, 0});
        s.Dijkstra<float>().parked.Add({1.f, i, 0});
    }
    s.PrepareSettled(1000);
    EXPECT_EQ(counter.Count(), 0u);
    EXPECT_EQ(s.TotalCapacity(), cap);
}

TEST(GridSearchScratchReserve, ResetKeepsCapacity)
{
    GridSearchScratch s;
    s.Reserve(64);
    for (uint32_t i = 0; i < 300; ++i) s.Queue().Push(i);   // forces growth beyond reserve
    s.PrepareSettled(64);
    const uint64_t cap = s.TotalCapacity();
    s.Reset();
    EXPECT_EQ(s.TotalCapacity(), cap);
    EXPECT_TRUE(s.Queue().Empty());
    EXPECT_EQ(s.Settled().Size(), 0u);
}

TEST(GridSearchScratchSettled, PersistsUntilPrepared)
{
    GridSearchScratch s;
    s.PrepareSettled(8);
    s.Settled()[3u] = 1;
    EXPECT_EQ(s.Settled().Size(), 8u);
    EXPECT_EQ(s.Settled()[3u], 1);
    s.PrepareSettled(8);
    EXPECT_EQ(s.Settled()[3u], 0);
}

TEST(GridSearchScratchAllocCounter, CountsOnlyWhileArmed)
{
    { auto* p = new int(1); delete p; }
    AllocCounter c;
    EXPECT_EQ(c.Count(), 0u);
    auto* p = new int(2);
    EXPECT_EQ(c.Count(), 1u);
    delete p;
}
