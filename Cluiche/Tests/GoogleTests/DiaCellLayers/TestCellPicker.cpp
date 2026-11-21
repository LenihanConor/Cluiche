#include <gtest/gtest.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <limits>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;

namespace
{
	Dia::Graphics::CellGridDesc MakeGrid(uint16_t w, uint16_t h, float size)
	{
		Dia::Graphics::CellGridDesc g;
		g.width = w;
		g.height = h;
		g.cellSize = size;
		return g;
	}

	CellPickerView View(float cx, float cy, float zoom)
	{
		CellPickerView v;
		v.viewportWidth = 1280;
		v.viewportHeight = 720;
		v.cameraX = cx;
		v.cameraY = cy;
		v.zoom = zoom;
		return v;
	}

	void RoundTripAll(uint16_t w, uint16_t h)
	{
		const float size = 1.0f;
		const Dia::Graphics::CellGridDesc grid = MakeGrid(w, h, size);
		const CellPickerView views[] = { View(0, 0, 1.0f), View(50, 40, 8.0f), View(-30, 25, 0.5f), View(90, 60, 20.0f), View(10, 10, 3.0f) };
		for (const CellPickerView& v : views)
		{
			for (int r = 0; r < h; ++r)
			{
				for (int q = 0; q < w; ++q)
				{
					const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, size);
					float sx, sy;
					WorldToScreen(v, c.X(), c.Y(), sx, sy);
					const PickedCell p = CellPicker::Pick(v, sx, sy, grid);
					ASSERT_TRUE(p.valid) << q << "," << r;
					ASSERT_EQ(p.q, q);
					ASSERT_EQ(p.r, r);
					ASSERT_EQ(p.index, static_cast<uint32_t>(r * w + q));
				}
			}
		}
	}
}

TEST(CellPickerTest, EveryCentreRoundTrips66x42) { RoundTripAll(66, 42); }
TEST(CellPickerTest, EveryCentreRoundTrips104x64) { RoundTripAll(104, 64); }

TEST(CellPickerTest, ScreenCentreMapsToCameraPosition)
{
	float wx, wy;
	CellPicker::ScreenToWorld(View(12, -7, 4.0f), 640, 360, wx, wy);
	EXPECT_FLOAT_EQ(wx, 12.0f);
	EXPECT_FLOAT_EQ(wy, -7.0f);
	CellPicker::ScreenToWorld(View(0, 0, 2.0f), 740, 460, wx, wy);	// +100px, +100px at zoom 2
	EXPECT_FLOAT_EQ(wx, 50.0f);
	EXPECT_FLOAT_EQ(wy, 50.0f);
}

TEST(CellPickerTest, WorldYGrowsDownTheScreen)
{
	const Dia::Graphics::CellGridDesc grid = MakeGrid(10, 10, 1.0f);
	const CellPickerView v = View(0, 0, 10.0f);
	float sx, sy;
	const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ 2, 3 }, 1.0f);
	WorldToScreen(v, c.X(), c.Y(), sx, sy);
	EXPECT_EQ(CellPicker::Pick(v, sx, sy, grid).r, 3);
	// the row above is higher on screen (smaller y)
	WorldToScreen(v, c.X(), c.Y() - 1.7320508f, sx, sy);
	EXPECT_EQ(CellPicker::Pick(v, sx, sy, grid).r, 2);
}

TEST(CellPickerTest, OutsideGridReturnsNoCell)
{
	const Dia::Graphics::CellGridDesc grid = MakeGrid(66, 42, 1.0f);
	const CellPickerView v = View(50, 40, 8.0f);
	const CellCoord outside[] = { {-1, 0}, {0, -1}, {66, 0}, {65, 42}, {66, 42}, {-1, -1}, {70, 50} };
	for (const CellCoord& o : outside)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(o, 1.0f);
		float sx, sy;
		WorldToScreen(v, c.X(), c.Y(), sx, sy);
		EXPECT_FALSE(CellPicker::Pick(v, sx, sy, grid).valid) << o.x << "," << o.y;
	}
}

TEST(CellPickerTest, PathologicalPointsReturnNoCell)
{
	const Dia::Graphics::CellGridDesc grid = MakeGrid(66, 42, 1.0f);
	const CellPickerView v = View(0, 0, 1.0f);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(CellPicker::Pick(v, nan, 10, grid).valid);
	EXPECT_FALSE(CellPicker::Pick(v, 10, nan, grid).valid);
	EXPECT_FALSE(CellPicker::Pick(v, inf, 10, grid).valid);
	EXPECT_FALSE(CellPicker::Pick(v, -inf, -inf, grid).valid);
	EXPECT_FALSE(CellPicker::Pick(v, -1.0e6f, -1.0e6f, grid).valid);
	EXPECT_FALSE(CellPicker::Pick(v, 3.0e38f, 3.0e38f, grid).valid);
}

TEST(CellPickerTest, EmptyOrInvalidGridReturnsNoCell)
{
	const CellPickerView v = View(0, 0, 1.0f);
	EXPECT_FALSE(CellPicker::Pick(v, 640, 360, MakeGrid(0, 5, 1.0f)).valid);
	EXPECT_FALSE(CellPicker::Pick(v, 640, 360, MakeGrid(5, 5, 0.0f)).valid);
}

TEST(CellPickerTest, NudgedPointsInsideACellPickThatCell)
{
	const Dia::Graphics::CellGridDesc grid = MakeGrid(66, 42, 1.0f);
	const CellPickerView v = View(50, 40, 8.0f);
	const CellCoord cells[] = { {0, 0}, {1, 0}, {33, 20}, {65, 41}, {64, 10} };
	const float dx[] = { 0.45f, -0.45f, 0.0f, 0.0f };
	const float dy[] = { 0.0f, 0.0f, 0.7f, -0.7f };
	for (const CellCoord& cc : cells)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(cc, 1.0f);
		for (int i = 0; i < 4; ++i)
		{
			float sx, sy;
			WorldToScreen(v, c.X() + dx[i], c.Y() + dy[i], sx, sy);
			const PickedCell p = CellPicker::Pick(v, sx, sy, grid);
			ASSERT_TRUE(p.valid);
			EXPECT_EQ(p.q, cc.x);
			EXPECT_EQ(p.r, cc.y);
		}
	}
}

TEST(CellPickerTest, ZeroZoomTreatedAsOne)
{
	const Dia::Graphics::CellGridDesc grid = MakeGrid(10, 10, 1.0f);
	const PickedCell a = CellPicker::Pick(View(5, 5, 0.0f), 700, 400, grid);
	const PickedCell b = CellPicker::Pick(View(5, 5, 1.0f), 700, 400, grid);
	EXPECT_EQ(a.valid, b.valid);
	EXPECT_EQ(a.index, b.index);
}
