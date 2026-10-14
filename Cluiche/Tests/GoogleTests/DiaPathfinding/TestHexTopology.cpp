// TestHexTopology.cpp — GoogleTest coverage for Dia::Pathfinding::HexTopology.

#include <gtest/gtest.h>
#include <cmath>
#include <queue>
#include <vector>

#include "DiaPathfinding/HexTopology.h"

using namespace Dia::Pathfinding;

namespace {
    constexpr float kPi = 3.14159265358979f;

    // Spec table: {dx, dy} for even row then odd row.
    const int kEven[6][2] = { {+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1} };
    const int kOdd[6][2]  = { {+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1} };

    void CheckTableOnGrid(int w, int h) {
        HexTopology t(w, h);
        for (int row = 1; row < h - 1; ++row) {
            CellCoord c{ w / 2, row };
            uint32_t cell = t.ToIndex(c);
            for (uint32_t d = 0; d < 6; ++d) {
                const int* off = (row % 2 == 0) ? kEven[d] : kOdd[d];
                CellCoord e{ c.x + off[0], c.y + off[1] };
                EXPECT_EQ(t.NeighbourAt(cell, d), t.ToIndex(e)) << "row " << row << " dir " << d;
            }
        }
    }
}

TEST(HexTopology, NeighbourTableEvenHeight) { CheckTableOnGrid(6, 6); }
TEST(HexTopology, NeighbourTableOddHeight)  { CheckTableOnGrid(5, 7); }

TEST(HexTopology, EastWestSameRow) {
    HexTopology t(5, 5);
    uint32_t cell = t.ToIndex({ 2, 2 });
    EXPECT_EQ(t.NeighbourAt(cell, 0), t.ToIndex({ 3, 2 }));
    EXPECT_EQ(t.NeighbourAt(cell, 3), t.ToIndex({ 1, 2 }));
}

TEST(HexTopology, IndexRoundTripAndBounds) {
    HexTopology t(5, 4);
    EXPECT_EQ(t.CellCount(), 20u);
    for (uint32_t i = 0; i < t.CellCount(); ++i) {
        CellCoord c = t.ToCoord(i);
        EXPECT_TRUE(t.IsInBounds(c));
        EXPECT_EQ(t.ToIndex(c), i);
    }
    EXPECT_EQ(t.ToIndex({ 3, 2 }), 13u);
    EXPECT_TRUE(t.IsInBounds({ 0, 0 }));
    EXPECT_TRUE(t.IsInBounds({ 4, 3 }));
    EXPECT_FALSE(t.IsInBounds({ -1, 0 }));
    EXPECT_FALSE(t.IsInBounds({ 0, -1 }));
    EXPECT_FALSE(t.IsInBounds({ 5, 0 }));
    EXPECT_FALSE(t.IsInBounds({ 0, 4 }));
}

TEST(HexTopology, NeighbourEdgesAndSymmetry) {
    const int sizes[][2] = { {1, 1}, {1, 5}, {5, 1}, {2, 2}, {5, 4}, {6, 5} };
    for (auto& s : sizes) {
        HexTopology t(s[0], s[1]);
        for (uint32_t cell = 0; cell < t.CellCount(); ++cell) {
            CellCoord c = t.ToCoord(cell);
            uint32_t count = 0;
            uint32_t seen[6];
            for (uint32_t d = 0; d < 6; ++d) {
                const int* off = (c.y % 2 == 0) ? kEven[d] : kOdd[d];
                CellCoord e{ c.x + off[0], c.y + off[1] };
                uint32_t n = t.NeighbourAt(cell, d);
                if (!t.IsInBounds(e)) {
                    EXPECT_EQ(n, kNoCell);
                } else {
                    EXPECT_EQ(n, t.ToIndex(e));
                    EXPECT_EQ(t.NeighbourAt(n, (d + 3) % 6), cell);
                    seen[count++] = n;
                }
            }
            for (uint32_t i = 0; i < count; ++i)
                for (uint32_t j = i + 1; j < count; ++j)
                    EXPECT_NE(seen[i], seen[j]);
        }
    }
    HexTopology big(6, 5);
    EXPECT_EQ([&] { uint32_t n = 0; for (uint32_t d = 0; d < 6; ++d) n += big.NeighbourAt(big.ToIndex({ 2, 2 }), d) != kNoCell; return n; }(), 6u);
    EXPECT_LT([&] { uint32_t n = 0; for (uint32_t d = 0; d < 6; ++d) n += big.NeighbourAt(0, d) != kNoCell; return n; }(), 6u);
}

TEST(HexTopology, DistanceMatchesBfs) {
    HexTopology t(10, 8);
    const uint32_t n = t.CellCount();
    for (uint32_t src = 0; src < n; ++src) {
        std::vector<int> dist(n, -1);
        std::queue<uint32_t> q;
        dist[src] = 0;
        q.push(src);
        while (!q.empty()) {
            uint32_t c = q.front(); q.pop();
            for (uint32_t d = 0; d < 6; ++d) {
                uint32_t nb = t.NeighbourAt(c, d);
                if (nb != kNoCell && dist[nb] < 0) { dist[nb] = dist[c] + 1; q.push(nb); }
            }
        }
        for (uint32_t dst = 0; dst < n; ++dst) {
            uint32_t d1 = HexTopology::Distance(t.ToCoord(src), t.ToCoord(dst));
            EXPECT_EQ(d1, (uint32_t)dist[dst]);
            EXPECT_EQ(d1, HexTopology::Distance(t.ToCoord(dst), t.ToCoord(src)));
        }
    }
    EXPECT_EQ(HexTopology::Distance({ 3, 3 }, { 3, 3 }), 0u);
}

TEST(HexTopology, DirectionFromAngle) {
    EXPECT_EQ(HexTopology::DirectionFromAngle(0.0f), 0u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(kPi / 3.0f), 1u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(kPi), 3u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-kPi / 3.0f), 5u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(2.0f * kPi - 1e-4f), 0u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-2.0f * kPi + kPi / 3.0f), 1u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-4.0f * kPi - kPi / 3.0f), 5u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(7.0f * kPi / 3.0f), 1u);
}

TEST(HexTopology, DirectionVectorIsUnitAtAngle) {
    for (uint32_t d = 0; d < 6; ++d) {
        auto v = HexTopology::DirectionVector(d);
        float a = (float)d * kPi / 3.0f;
        EXPECT_NEAR(v.X(), std::cos(a), 1e-5f);
        EXPECT_NEAR(v.Y(), std::sin(a), 1e-5f);
        EXPECT_NEAR(v.X() * v.X() + v.Y() * v.Y(), 1.0f, 1e-5f);
    }
}
