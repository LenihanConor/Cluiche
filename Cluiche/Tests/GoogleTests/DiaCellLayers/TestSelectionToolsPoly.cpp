#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <math.h>
#include <limits>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
namespace Tools = Dia::CellLayers::SelectionTools;
using Tools::PolyPoint;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };
	const float kScales[] = { 1.0f, 8.0f };

	typedef std::vector<std::pair<int, int>> CellList;

	// Polygons are authored for cellSize 1 and scaled by the grid's cellSize (cells scale with it).
	std::vector<PolyPoint> Poly(std::initializer_list<PolyPoint> pts, float scale)
	{
		std::vector<PolyPoint> v;
		for (const PolyPoint& p : pts)
		{
			v.push_back({ p.x * scale, p.y * scale });
		}
		return v;
	}

	std::vector<uint32_t> Indices(const CellList& cells, uint32_t width)
	{
		std::vector<uint32_t> v;
		for (const auto& c : cells)
		{
			v.push_back(uint32_t(c.second) * width + uint32_t(c.first));
		}
		std::sort(v.begin(), v.end());
		return v;
	}

	// Independent oracle for axis-aligned rectangles: centre strictly inside (no edge cases are placed on the boundary).
	CellList CellsInRect(const Dim& d, float scale, double x0, double y0, double x1, double y1)
	{
		CellList out;
		for (int r = 0; r < d.h; ++r)
		{
			for (int q = 0; q < d.w; ++q)
			{
				const auto c = Dia::Pathfinding::HexTopology::CellCenter({ q, r }, scale);
				if (c.x > x0 * scale && c.x < x1 * scale && c.y > y0 * scale && c.y < y1 * scale)
				{
					out.push_back({ q, r });
				}
			}
		}
		return out;
	}

	template <typename Fn>
	void ForEachGrid(Fn fn)
	{
		for (const Dim& d : kDims)
		{
			for (float s : kScales)
			{
				auto g = MakeGrid(d.w, d.h, s);
				CellSelection sel;
				sel.Reset(&g);
				fn(d, s, sel);
			}
		}
	}
}

TEST(SelectionToolsPolyTest, RectangleSelectsHandComputedCells)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		// Centres: even col y = sqrt3*r; odd col y = sqrt3*(r+0.5). x = 1.5*q.
		auto poly = Poly({ { 2.5f, 1.6f }, { 6.5f, 1.6f }, { 6.5f, 3.6f }, { 2.5f, 3.6f } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 5u);
		EXPECT_EQ(ToVector(sel), Indices({ { 2, 1 }, { 2, 2 }, { 3, 1 }, { 4, 1 }, { 4, 2 } }, d.w));
	});
}

TEST(SelectionToolsPolyTest, TriangleSelectsHandComputedCells)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		auto poly = Poly({ { -0.5f, -0.5f }, { 10.5f, -0.5f }, { -0.5f, 6.5f } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(res.count, 16u);
		const CellList expected = {
			{ 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 },
			{ 1, 0 }, { 1, 1 }, { 1, 2 },
			{ 2, 0 }, { 2, 1 }, { 2, 2 },
			{ 3, 0 }, { 3, 1 },
			{ 4, 0 }, { 4, 1 },
			{ 5, 0 },
			{ 6, 0 } };
		EXPECT_EQ(ToVector(sel), Indices(expected, d.w));
	});
}

TEST(SelectionToolsPolyTest, HexagonSelectsCentreAndSixNeighbours)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		// Hexagon (corners on the x axis, circumradius 2.2) around the centre of odd-q cell (5,4) = (7.5, 7.794).
		const auto c = Dia::Pathfinding::HexTopology::CellCenter({ 5, 4 }, 1.0f);
		const float cx = c.x, cy = c.y, R = 2.2f, hy = 1.9052559f;	// hy = R * sqrt(3) / 2
		auto poly = Poly({ { cx + R, cy }, { cx + R / 2, cy + hy }, { cx - R / 2, cy + hy },
			{ cx - R, cy }, { cx - R / 2, cy - hy }, { cx + R / 2, cy - hy } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(res.count, 7u);
		// Neighbours of an odd column: N/S in the column, and two each in cols 4 and 6 (rows 4 and 5).
		const CellList expected = { { 5, 4 }, { 5, 3 }, { 5, 5 }, { 4, 4 }, { 4, 5 }, { 6, 4 }, { 6, 5 } };
		EXPECT_EQ(ToVector(sel), Indices(expected, d.w));
	});
}

TEST(SelectionToolsPolyTest, ConcaveCShapeExcludesNotch)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		auto poly = Poly({ { 2.5f, 1.5f }, { 10.0f, 1.5f }, { 10.0f, 3.9f }, { 6.2f, 3.9f },
			{ 6.2f, 6.5f }, { 10.0f, 6.5f }, { 10.0f, 9.0f }, { 2.5f, 9.0f } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);

		const CellList outer = CellsInRect(d, s, 2.5, 1.5, 10.0, 9.0);
		const CellList notch = CellsInRect(d, s, 6.2, 3.9, 10.0, 6.5);
		EXPECT_FALSE(notch.empty());
		std::vector<uint32_t> expected;
		const auto notchIdx = Indices(notch, d.w);
		for (uint32_t i : Indices(outer, d.w))
		{
			if (!std::binary_search(notchIdx.begin(), notchIdx.end(), i))
			{
				expected.push_back(i);
			}
		}
		EXPECT_EQ(ToVector(sel), expected);
		for (uint32_t i : notchIdx)
		{
			EXPECT_FALSE(sel.Contains(i));
		}
	});
}

TEST(SelectionToolsPolyTest, BowTieSelectsBothLobesOnly)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		auto poly = Poly({ { 0.0f, 0.0f }, { 10.0f, 10.0f }, { 10.0f, 0.0f }, { 0.0f, 10.0f } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(sel.Contains(3u * d.w + 6u));	// col 6 (x=9), r3 (y=5.196): inside the right lobe
		EXPECT_TRUE(sel.Contains(2u * d.w + 1u));	// col 1 (x=1.5), r2 (y=4.33): inside the left lobe
		EXPECT_FALSE(sel.Contains(0u * d.w + 3u));	// col 3 (x=4.5), r0 (y=0.866): above the waist
		EXPECT_FALSE(sel.Contains(4u * d.w + 3u));	// col 3 (x=4.5), r4 (y=7.8): below the waist
	});
}

TEST(SelectionToolsPolyTest, PentagramFollowsEvenOdd)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		// {5/2} star, circumradius 6, around the centre of cell (5,4). The inner pentagon has winding 2 -> excluded.
		const auto c = Dia::Pathfinding::HexTopology::CellCenter({ 5, 4 }, 1.0f);
		std::vector<PolyPoint> poly;
		for (int k = 0; k < 5; ++k)
		{
			const double a = (-90.0 + 72.0 * ((k * 2) % 5)) * 3.14159265358979323846 / 180.0;
			poly.push_back({ float(c.x + 6.0 * cos(a)) * s, float(c.y + 6.0 * sin(a)) * s });
		}
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_FALSE(sel.Contains(4u * d.w + 5u));	// centre: winding 2
		EXPECT_FALSE(sel.Contains(3u * d.w + 5u));	// N neighbour (1.73 away): still inside the inner pentagon
		EXPECT_TRUE(sel.Contains(1u * d.w + 5u));	// 5.2 up the top arm: winding 1
		EXPECT_GT(res.count, 0u);
	});
}

TEST(SelectionToolsPolyTest, PolygonOutsideGridSelectsNothingButIsOk)
{
	ForEachGrid([](const Dim&, float s, CellSelection& sel)
	{
		sel.Set(10, true);
		sel.Set(11, true);
		const uint32_t v = sel.Version();
		auto poly = Poly({ { 1000.0f, 1000.0f }, { 1010.0f, 1000.0f }, { 1005.0f, 1010.0f } }, s);
		auto res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 0u);
		EXPECT_EQ(sel.Count(), 0u);
		EXPECT_EQ(sel.Version(), v + 1);

		// Repeat on an already-empty selection: no change, no bump.
		res = Tools::SelectPolygon(sel, poly.data(), uint32_t(poly.size()));
		EXPECT_TRUE(res.ok);
		EXPECT_FALSE(res.changed);
		EXPECT_EQ(sel.Version(), v + 1);
	});
}

TEST(SelectionToolsPolyTest, TooFewVerticesIsErrorAndUnchanged)
{
	ForEachGrid([](const Dim&, float s, CellSelection& sel)
	{
		sel.Set(5, true);
		const uint32_t v = sel.Version();
		auto two = Poly({ { 0.0f, 0.0f }, { 10.0f, 10.0f } }, s);
		auto res = Tools::SelectPolygon(sel, two.data(), 2);
		EXPECT_FALSE(res.ok);
		EXPECT_FALSE(res.changed);
		res = Tools::SelectPolygon(sel, nullptr, 0);
		EXPECT_FALSE(res.ok);
		res = Tools::SelectPolygon(sel, nullptr, 5);
		EXPECT_FALSE(res.ok);
		EXPECT_EQ(sel.Count(), 1u);
		EXPECT_TRUE(sel.Contains(5));
		EXPECT_EQ(sel.Version(), v);
	});
}

TEST(SelectionToolsPolyTest, NonFiniteVertexIsErrorAndUnchanged)
{
	ForEachGrid([](const Dim&, float s, CellSelection& sel)
	{
		sel.Set(5, true);
		const uint32_t v = sel.Version();
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float inf = std::numeric_limits<float>::infinity();
		auto a = Poly({ { 0.0f, 0.0f }, { 10.0f, 0.0f }, { nan, 10.0f } }, s);
		auto b = Poly({ { 0.0f, 0.0f }, { 10.0f, inf }, { 5.0f, 10.0f } }, s);
		EXPECT_FALSE(Tools::SelectPolygon(sel, a.data(), 3).ok);
		EXPECT_FALSE(Tools::SelectPolygon(sel, b.data(), 3).ok);
		EXPECT_EQ(sel.Count(), 1u);
		EXPECT_EQ(sel.Version(), v);
	});
}

TEST(SelectionToolsPolyTest, EdgeRuleMinInclusiveMaxExclusive)
{
	ForEachGrid([](const Dim& d, float s, CellSelection& sel)
	{
		// Centres of col 2 (x=3), col 4 (x=6), r0 (y=0) lie exactly on the rectangle's min-x and max-x edges.
		auto xr = Poly({ { 3.0f, -0.5f }, { 6.0f, -0.5f }, { 6.0f, 0.5f }, { 3.0f, 0.5f } }, s);
		auto res = Tools::SelectPolygon(sel, xr.data(), 4);
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(ToVector(sel), std::vector<uint32_t>{ 2u });
		(void)d;

		// y = 0 (r0 of col 0, x = 0) on the min-y edge is inside; on the max-y edge it is outside.
		auto yMin = Poly({ { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.7f }, { -1.0f, 1.7f } }, s);
		Tools::SelectPolygon(sel, yMin.data(), 4);
		EXPECT_TRUE(sel.Contains(0));
		auto yMax = Poly({ { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f } }, s);
		Tools::SelectPolygon(sel, yMax.data(), 4);
		EXPECT_FALSE(sel.Contains(0));
		EXPECT_EQ(sel.Count(), 0u);
	});
}

TEST(SelectionToolsPolyTest, PolygonBumpsVersionOnce)
{
	ForEachGrid([](const Dim&, float s, CellSelection& sel)
	{
		sel.Set(1, true);
		const uint32_t v = sel.Version();
		auto poly = Poly({ { 2.5f, 1.6f }, { 6.5f, 1.6f }, { 6.5f, 3.6f }, { 2.5f, 3.6f } }, s);
		Tools::SelectPolygon(sel, poly.data(), 4);
		EXPECT_EQ(sel.Version(), v + 1);
		const uint32_t v2 = sel.Version();
		auto res = Tools::SelectPolygon(sel, poly.data(), 4);
		EXPECT_FALSE(res.changed);
		EXPECT_EQ(sel.Version(), v2);
	});
}

// ---- SelectCells ----

TEST(SelectionToolsPolyTest, CellsSimpleListWithAcceptedOutput)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection sel;
		sel.Reset(&g);
		const int32_t qr[] = { 3, 4, 0, 0, 10, 2 };
		uint32_t out[6] = {};
		auto res = Tools::SelectCells(sel, qr, 3, out, 3);
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 3u);
		EXPECT_EQ(res.requested, 3u);
		EXPECT_EQ(res.accepted, 3u);
		EXPECT_FALSE(res.overCap);
		EXPECT_EQ(ToVector(sel), Indices({ { 0, 0 }, { 10, 2 }, { 3, 4 } }, d.w));
		const uint32_t expectedOut[] = { 3, 4, 0, 0, 10, 2 };
		for (int i = 0; i < 6; ++i)
		{
			EXPECT_EQ(out[i], expectedOut[i]);
		}
	}
}

TEST(SelectionToolsPolyTest, CellsOutOfBoundsAndDuplicatesIgnored)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection sel;
		sel.Reset(&g);
		const int32_t qr[] = { 1, 1, -1, 0, 0, -1, int32_t(d.w), 0, 0, int32_t(d.h), 1, 1, 2, 2, 2, 2 };
		uint32_t out[16] = {};
		auto res = Tools::SelectCells(sel, qr, 8, out, 8);
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(res.requested, 8u);
		EXPECT_EQ(res.accepted, 2u);
		EXPECT_EQ(res.count, 2u);
		EXPECT_EQ(ToVector(sel), Indices({ { 1, 1 }, { 2, 2 } }, d.w));
		EXPECT_EQ(out[0], 1u); EXPECT_EQ(out[1], 1u);
		EXPECT_EQ(out[2], 2u); EXPECT_EQ(out[3], 2u);
		EXPECT_EQ(out[4], 0u);	// nothing written past the accepted pairs
	}
}

TEST(SelectionToolsPolyTest, CellsAcceptedCapacityLimitsWritesNotCount)
{
	auto g = MakeGrid(66, 42);
	CellSelection sel;
	sel.Reset(&g);
	const int32_t qr[] = { 1, 1, 2, 2, 3, 3 };
	uint32_t out[6] = {};
	auto res = Tools::SelectCells(sel, qr, 3, out, 2);
	EXPECT_EQ(res.accepted, 3u);
	EXPECT_EQ(out[2], 2u); EXPECT_EQ(out[3], 2u);
	EXPECT_EQ(out[4], 0u); EXPECT_EQ(out[5], 0u);
	EXPECT_EQ(sel.Count(), 3u);
}

TEST(SelectionToolsPolyTest, CellsCapIgnoresExtras)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection sel;
		sel.Reset(&g);
		// 2,100 distinct valid cells in row-major order: only the first 2,048 may be selected.
		std::vector<int32_t> qr;
		for (uint32_t i = 0; i < 2100; ++i)
		{
			qr.push_back(int32_t(i % d.w));
			qr.push_back(int32_t(i / d.w));
		}
		auto res = Tools::SelectCells(sel, qr.data(), 2100, nullptr, 0);
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.overCap);
		EXPECT_EQ(res.requested, 2100u);
		EXPECT_EQ(res.accepted, 2048u);
		EXPECT_EQ(res.count, 2048u);
		EXPECT_TRUE(sel.Contains(2047));
		EXPECT_FALSE(sel.Contains(2048));	// valid cell, but beyond the cap
		EXPECT_FALSE(sel.Contains(2099));

		// Exactly at the cap is not over.
		auto res2 = Tools::SelectCells(sel, qr.data(), 2048, nullptr, 0);
		EXPECT_FALSE(res2.overCap);
		EXPECT_FALSE(res2.changed);
	}
}

TEST(SelectionToolsPolyTest, CellsEmptyListClearsSelection)
{
	auto g = MakeGrid(66, 42);
	CellSelection sel;
	sel.Reset(&g);
	sel.Set(3, true);
	sel.Set(4, true);
	const uint32_t v = sel.Version();
	auto res = Tools::SelectCells(sel, nullptr, 0);
	EXPECT_TRUE(res.ok);
	EXPECT_TRUE(res.changed);
	EXPECT_EQ(res.count, 0u);
	EXPECT_EQ(res.accepted, 0u);
	EXPECT_EQ(sel.Version(), v + 1);

	// All-invalid list on a non-empty selection also clears it.
	sel.Set(3, true);
	const int32_t bad[] = { -1, -1, 9999, 9999 };
	res = Tools::SelectCells(sel, bad, 2);
	EXPECT_TRUE(res.ok);
	EXPECT_EQ(res.requested, 2u);
	EXPECT_EQ(res.accepted, 0u);
	EXPECT_EQ(sel.Count(), 0u);
}

TEST(SelectionToolsPolyTest, CellsReplacesPreviousSelectionWithSingleBump)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection sel;
		sel.Reset(&g);
		sel.Set(100, true);
		sel.Set(101, true);
		const uint32_t v = sel.Version();
		const int32_t qr[] = { 1, 0, 2, 0, 3, 0 };
		auto res = Tools::SelectCells(sel, qr, 3);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(ToVector(sel), (std::vector<uint32_t>{ 1u, 2u, 3u }));
		EXPECT_EQ(sel.Version(), v + 1);

		// Same list again: no net change, no bump.
		res = Tools::SelectCells(sel, qr, 3);
		EXPECT_TRUE(res.ok);
		EXPECT_FALSE(res.changed);
		EXPECT_EQ(sel.Version(), v + 1);
	}
}
