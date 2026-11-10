// TestHexTopology.cpp — GoogleTest coverage for Dia::Pathfinding::HexTopology.

#include <gtest/gtest.h>
#include <cmath>
#include <queue>
#include <vector>

#include "DiaPathfinding/HexTopology.h"

using namespace Dia::Pathfinding;

namespace {
    constexpr float kPi = 3.14159265358979f;

    // Spec table: {dx, dy} for even column then odd column. Order NE,N,NW,SW,S,SE.
    const int kEven[6][2] = { {+1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {0, +1}, {+1, 0} };
    const int kOdd[6][2]  = { {+1, 0}, {0, -1}, {-1, 0}, {-1, +1}, {0, +1}, {+1, +1} };

    void CheckTableOnGrid(int w, int h) {
        HexTopology t(w, h);
        for (int col = 1; col < w - 1; ++col) {
            CellCoord c{ col, h / 2 };
            uint32_t cell = t.ToIndex(c);
            for (uint32_t d = 0; d < 6; ++d) {
                const int* off = (col % 2 == 0) ? kEven[d] : kOdd[d];
                CellCoord e{ c.x + off[0], c.y + off[1] };
                EXPECT_EQ(t.NeighbourAt(cell, d), t.ToIndex(e)) << "col " << col << " dir " << d;
            }
        }
    }
}

TEST(HexTopology, NeighbourTableEvenWidth) { CheckTableOnGrid(6, 6); }
TEST(HexTopology, NeighbourTableOddWidth)  { CheckTableOnGrid(7, 5); }

TEST(HexTopology, NorthSouthSameColumn) {
    HexTopology t(5, 5);
    for (int x : { 2, 3 }) {
        uint32_t cell = t.ToIndex({ x, 2 });
        EXPECT_EQ(t.NeighbourAt(cell, 1), t.ToIndex({ x, 1 }));
        EXPECT_EQ(t.NeighbourAt(cell, 4), t.ToIndex({ x, 3 }));
    }
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
                const int* off = (c.x % 2 == 0) ? kEven[d] : kOdd[d];
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
    const float deg = kPi / 180.0f;
    EXPECT_EQ(HexTopology::DirectionFromAngle(0.0f), 0u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(30.0f * deg), 0u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(90.0f * deg), 1u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(210.0f * deg), 3u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(270.0f * deg), 4u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-30.0f * deg), 5u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(30.0f * deg + 2.0f * kPi - 1e-4f), 0u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-270.0f * deg), 1u);
    EXPECT_EQ(HexTopology::DirectionFromAngle(-720.0f * deg - 30.0f * deg), 5u);
}

TEST(HexTopology, DirectionVectorIsUnitAtAngle) {
    for (uint32_t d = 0; d < 6; ++d) {
        auto v = HexTopology::DirectionVector(d);
        float a = (30.0f + 60.0f * (float)d) * kPi / 180.0f;
        EXPECT_NEAR(v.X(), std::cos(a), 1e-5f);
        EXPECT_NEAR(v.Y(), -std::sin(a), 1e-5f);
        EXPECT_NEAR(v.X() * v.X() + v.Y() * v.Y(), 1.0f, 1e-5f);
    }
    EXPECT_NEAR(HexTopology::DirectionVector(1).X(), 0.0f, 1e-5f);
    EXPECT_NEAR(HexTopology::DirectionVector(1).Y(), -1.0f, 1e-5f);
}

TEST(HexTopology, CellCenterFormula) {
    const float size = 2.0f;
    auto c = HexTopology::CellCenter({ 3, 2 }, size);
    EXPECT_NEAR(c.X(), 1.5f * size * 3.0f, 1e-4f);
    EXPECT_NEAR(c.Y(), std::sqrt(3.0f) * size * 2.5f, 1e-4f);
    auto e = HexTopology::CellCenter({ 2, 2 }, size);
    EXPECT_NEAR(e.Y(), std::sqrt(3.0f) * size * 2.0f, 1e-4f);
}

TEST(HexTopology, CellFromPointRoundTrip) {
    HexTopology t(12, 9);
    for (float size : { 1.0f, 2.5f, 17.0f })
        for (uint32_t i = 0; i < t.CellCount(); ++i) {
            CellCoord c = t.ToCoord(i);
            CellCoord r = HexTopology::CellFromPoint(HexTopology::CellCenter(c, size), size);
            EXPECT_EQ(r.x, c.x) << size << " " << c.x << "," << c.y;
            EXPECT_EQ(r.y, c.y) << size << " " << c.x << "," << c.y;
        }
}

TEST(HexTopology, CellFromPointNearCorners) {
    HexTopology t(12, 9);
    for (float size : { 1.0f, 2.5f, 17.0f })
        for (uint32_t i = 0; i < t.CellCount(); ++i) {
            CellCoord c = t.ToCoord(i);
            auto ctr = HexTopology::CellCenter(c, size);
            for (int k = 0; k < 6; ++k) {
                float a = (float)k * 60.0f * kPi / 180.0f;
                Dia::Maths::Vector2D p(ctr.X() + 0.2f * size * std::cos(a), ctr.Y() - 0.2f * size * std::sin(a));
                CellCoord r = HexTopology::CellFromPoint(p, size);
                EXPECT_EQ(r.x, c.x);
                EXPECT_EQ(r.y, c.y);
            }
        }
}

TEST(HexTopology, CellFromPointBeyondEdge) {
    HexTopology t(12, 9);
    const float size = 3.0f;
    for (uint32_t i = 0; i < t.CellCount(); ++i) {
        CellCoord c = t.ToCoord(i);
        auto ctr = HexTopology::CellCenter(c, size);
        for (uint32_t d = 0; d < 6; ++d) {
            uint32_t n = t.NeighbourAt(i, d);
            if (n == kNoCell) continue;
            auto dv = HexTopology::DirectionVector(d);
            float dist = 0.55f * std::sqrt(3.0f) * size;
            Dia::Maths::Vector2D p(ctr.X() + dv.X() * dist, ctr.Y() + dv.Y() * dist);
            CellCoord r = HexTopology::CellFromPoint(p, size);
            EXPECT_EQ(t.ToIndex(r), n) << c.x << "," << c.y << " d=" << d;
        }
    }
}

TEST(HexTopology, NeighbourCentresMatchDirectionVector) {
    HexTopology t(12, 9);
    for (float size : { 1.0f, 2.5f, 17.0f })
        for (uint32_t i = 0; i < t.CellCount(); ++i) {
            auto a = HexTopology::CellCenter(t.ToCoord(i), size);
            for (uint32_t d = 0; d < 6; ++d) {
                uint32_t n = t.NeighbourAt(i, d);
                if (n == kNoCell) continue;
                auto b = HexTopology::CellCenter(t.ToCoord(n), size);
                float dx = b.X() - a.X(), dy = b.Y() - a.Y();
                float len = std::sqrt(dx * dx + dy * dy);
                EXPECT_NEAR(len, std::sqrt(3.0f) * size, 1e-3f * size);
                auto dv = HexTopology::DirectionVector(d);
                EXPECT_NEAR(dx / len, dv.X(), 1e-4f);
                EXPECT_NEAR(dy / len, dv.Y(), 1e-4f);
            }
        }
}
