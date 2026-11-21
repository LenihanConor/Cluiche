#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/SelectionOutline.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <chrono>
#include <math.h>
#include <stdio.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::CellCoord;

namespace
{
	const float kSize = 10.0f;
	const float kEps = 1e-3f;

	struct Fixture
	{
		Dia::Graphics::CellGridDesc grid;
		CellSelection sel;
		SelectionOutline outline;
		explicit Fixture(uint16_t w = 8, uint16_t h = 8) : grid(MakeGrid(w, h, kSize)) { sel.Reset(&grid); }
		void Select(int q, int r) { sel.Set(uint32_t(r) * sel.Width() + uint32_t(q), true); }
	};

	bool Near(float a, float b) { return fabsf(a - b) < kEps; }

	// Midpoint of the edge facing direction dir is centre + apothem * DirectionVector(dir).
	bool HasEdgeFacing(const SelectionOutline& o, int q, int r, uint32_t dir)
	{
		const auto c = HexTopology::CellCenter(CellCoord{ q, r }, kSize);
		const auto d = HexTopology::DirectionVector(dir);
		const float apothem = kSize * 0.8660254f;
		const float mx = c.X() + d.X() * apothem;
		const float my = c.Y() + d.Y() * apothem;
		for (uint32_t i = 0; i < o.EdgeCount(); ++i)
		{
			const OutlineEdge& e = o.Edges()[i];
			if (Near((e.x0 + e.x1) * 0.5f, mx) && Near((e.y0 + e.y1) * 0.5f, my))
			{
				return true;
			}
		}
		return false;
	}
}

TEST(SelectionOutlineTest, SingleInteriorCellSixEdgesFacingEachDirection)
{
	Fixture f;
	f.Select(3, 3);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 6u);
	for (uint32_t d = 0; d < 6; ++d)
	{
		EXPECT_TRUE(HasEdgeFacing(f.outline, 3, 3, d)) << "dir " << d;
	}
	for (uint32_t i = 0; i < f.outline.EdgeCount(); ++i)
	{
		const OutlineEdge& e = f.outline.Edges()[i];
		EXPECT_NEAR(hypotf(e.x1 - e.x0, e.y1 - e.y0), kSize, kEps);
	}
}

TEST(SelectionOutlineTest, CornerCellSixEdgesIncludingOffGridSides)
{
	Fixture f;
	f.Select(0, 0);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 6u);
	EXPECT_TRUE(HasEdgeFacing(f.outline, 0, 0, 1));	// N, off grid
	EXPECT_TRUE(HasEdgeFacing(f.outline, 0, 0, 2));	// NW, off grid
}

TEST(SelectionOutlineTest, TwoAdjacentEvenColumnCellsShareSideNotEmitted)
{
	Fixture f;
	f.Select(2, 2);
	f.Select(3, 2);		// (3,2) is SE (dir 5) of (2,2)
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 10u);
	EXPECT_FALSE(HasEdgeFacing(f.outline, 2, 2, 5));
	EXPECT_FALSE(HasEdgeFacing(f.outline, 3, 2, 2));
}

TEST(SelectionOutlineTest, TwoAdjacentOddColumnCellsShareSideNotEmitted)
{
	Fixture f;
	f.Select(3, 2);
	f.Select(4, 2);		// (4,2) is NE (dir 0) of (3,2)
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 10u);
	EXPECT_FALSE(HasEdgeFacing(f.outline, 3, 2, 0));
	EXPECT_FALSE(HasEdgeFacing(f.outline, 4, 2, 3));
}

TEST(SelectionOutlineTest, VerticalNeighboursShareSide)
{
	Fixture f;
	f.Select(2, 2);
	f.Select(2, 3);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 10u);
	EXPECT_FALSE(HasEdgeFacing(f.outline, 2, 2, 4));
}

TEST(SelectionOutlineTest, ThreeByThreeBlobPerimeterIs22)
{
	// 9 cells * 6 sides - 2 * 16 shared sides (6 vertical + 5 + 5 between column pairs) = 22.
	Fixture f;
	for (int q = 1; q <= 3; ++q)
	{
		for (int r = 1; r <= 3; ++r)
		{
			f.Select(q, r);
		}
	}
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 22u);
}

TEST(SelectionOutlineTest, RingWithUnselectedHoleHasOuterAndInnerPerimeter)
{
	// Hole (2,2); its six neighbours selected: 6*6 - 2*6 ring adjacencies = 24 (18 outer + 6 inner).
	Fixture f;
	const HexTopology topo(8, 8);
	const uint32_t hole = topo.ToIndex(CellCoord{ 2, 2 });
	for (uint32_t d = 0; d < 6; ++d)
	{
		f.sel.Set(topo.NeighbourAt(hole, d), true);
	}
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 24u);
	for (uint32_t d = 0; d < 6; ++d)
	{
		const auto n = topo.ToCoord(topo.NeighbourAt(hole, d));
		EXPECT_TRUE(HasEdgeFacing(f.outline, n.x, n.y, (d + 3) % 6));
	}
}

TEST(SelectionOutlineTest, FullLargeGridOnlyPerimeter)
{
	// 104x64 flat-top odd-q: N 104 + S 104 + W 128 + E 128 + diagonals along top/bottom (52 + 51 + 51 + 52) = 670.
	Fixture f(104, 64);
	f.sel.BeginEdit();
	for (uint32_t i = 0; i < f.sel.CellCount(); ++i)
	{
		f.sel.Set(i, true);
	}
	f.sel.EndEdit();
	f.outline.Update(f.sel);
	EXPECT_FALSE(f.outline.TooLarge());
	EXPECT_EQ(f.outline.EdgeCount(), 670u);
	EXPECT_EQ(f.outline.SelectedCountForBanner(), 6656u);
}

TEST(SelectionOutlineTest, ExceedingCapIsTooLargeAndDrawsNothing)
{
	// Isolated cells (q%3==0, r%2==0): 35 * 32 = 1120 cells * 6 = 6720 edges > 4000.
	Fixture f(104, 64);
	f.sel.BeginEdit();
	uint32_t n = 0;
	for (int q = 0; q < 104; q += 3)
	{
		for (int r = 0; r < 64; r += 2)
		{
			f.Select(q, r);
			++n;
		}
	}
	f.sel.EndEdit();
	f.outline.Update(f.sel);
	EXPECT_TRUE(f.outline.TooLarge());
	EXPECT_EQ(f.outline.EdgeCount(), 0u);
	EXPECT_EQ(f.outline.SelectedCountForBanner(), n);
	EXPECT_EQ(f.sel.Count(), n);	// selection unaffected
	FakeDebugDraw draw;
	f.outline.Draw(draw);
	EXPECT_TRUE(draw.lines.empty());
}

TEST(SelectionOutlineTest, AtCapIsNotTooLargeOneMoreIs)
{
	// 666 isolated cells = 3996 edges; one more isolated cell = 4002 > cap.
	Fixture f(104, 64);
	uint32_t n = 0;
	f.sel.BeginEdit();
	for (int q = 0; q < 104 && n < 666; q += 3)
	{
		for (int r = 0; r < 64 && n < 666; r += 2)
		{
			f.Select(q, r);
			++n;
		}
	}
	f.sel.EndEdit();
	f.outline.Update(f.sel);
	EXPECT_FALSE(f.outline.TooLarge());
	EXPECT_EQ(f.outline.EdgeCount(), 3996u);
	f.Select(102, 62);
	f.outline.Update(f.sel);
	EXPECT_TRUE(f.outline.TooLarge());
}

TEST(SelectionOutlineTest, RebuildsOnlyWhenVersionChanges)
{
	Fixture f;
	f.Select(3, 3);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.Rebuilds(), 1u);
	f.outline.Update(f.sel);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.Rebuilds(), 1u);
	f.Select(3, 4);
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.Rebuilds(), 2u);
	EXPECT_EQ(f.outline.EdgeCount(), 10u);
}

TEST(SelectionOutlineTest, RebuildsWhenCellSizeDiffersEvenIfVersionMatches)
{
	Fixture f;
	f.Select(1, 1);
	f.outline.Update(f.sel);
	const uint32_t before = f.outline.Rebuilds();

	Dia::Graphics::CellGridDesc other = MakeGrid(8, 8, 20.0f);
	CellSelection sel2;
	sel2.Reset(&other);
	sel2.Set(9, true);	// version 1, same as f.sel
	ASSERT_EQ(sel2.Version(), f.sel.Version());
	f.outline.Update(sel2);
	EXPECT_GT(f.outline.Rebuilds(), before);
}

TEST(SelectionOutlineTest, DrawEmitsOneLinePerEdge)
{
	Fixture f;
	f.Select(3, 3);
	f.Select(4, 3);
	f.outline.Update(f.sel);
	FakeDebugDraw draw;
	f.outline.Draw(draw);
	EXPECT_EQ(draw.lines.size(), size_t(f.outline.EdgeCount()));
	EXPECT_EQ(draw.lines.size(), 10u);
}

TEST(SelectionOutlineTest, EmptySelectionNoEdgesNoDraw)
{
	Fixture f;
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 0u);
	EXPECT_FALSE(f.outline.TooLarge());
	FakeDebugDraw draw;
	f.outline.Draw(draw);
	EXPECT_TRUE(draw.lines.empty());
}

TEST(SelectionOutlineTest, ClearingSelectionRemovesEdges)
{
	Fixture f;
	f.Select(3, 3);
	f.outline.Update(f.sel);
	f.sel.Clear();
	f.outline.Update(f.sel);
	EXPECT_EQ(f.outline.EdgeCount(), 0u);
}

TEST(SelectionOutlineTest, FullGridRebuildWithinFrameBudget)
{
	Fixture f(104, 64);
	f.sel.BeginEdit();
	for (uint32_t i = 0; i < f.sel.CellCount(); ++i)
	{
		f.sel.Set(i, true);
	}
	f.sel.EndEdit();
	const auto t0 = std::chrono::steady_clock::now();
	f.outline.Update(f.sel);
	const auto t1 = std::chrono::steady_clock::now();
	const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	printf("[ perf ] full-grid outline rebuild: %.3f ms, %u edges\n", ms, f.outline.EdgeCount());
	EXPECT_EQ(f.outline.Rebuilds(), 1u);
	EXPECT_LT(ms, 5.0);
}
