#include <gtest/gtest.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <limits>
#include <random>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();
	const float kInf = std::numeric_limits<float>::infinity();

	Dia::Graphics::CellGridDesc Grid(uint16_t w, uint16_t h, float size = 1.0f)
	{
		Dia::Graphics::CellGridDesc g;
		g.width = w;
		g.height = h;
		g.cellSize = size;
		return g;
	}

	CellPickerView View(float cx, float cy, float zoom, float vw = 1280.0f, float vh = 720.0f)
	{
		CellPickerView v;
		v.viewportWidth = vw;
		v.viewportHeight = vh;
		v.cameraX = cx;
		v.cameraY = cy;
		v.zoom = zoom;
		return v;
	}

	PickedCell PickCentre(const CellPickerView& v, const Dia::Graphics::CellGridDesc& g, int q, int r)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, g.cellSize);
		float sx, sy;
		WorldToScreen(v, c.X(), c.Y(), sx, sy);
		return CellPicker::Pick(v, sx, sy, g);
	}
}

TEST(CellPickerEdge, DefaultPickedCellIsInvalidSentinel)
{
	const PickedCell p;
	EXPECT_FALSE(p.valid);
	EXPECT_EQ(p.q, -1);
	EXPECT_EQ(p.r, -1);
	EXPECT_EQ(p.index, 0xFFFFFFFFu);
}

TEST(CellPickerEdge, OneByOneGridOnlyCentreCellPicks)
{
	const auto g = Grid(1, 1);
	const CellPickerView v = View(0, 0, 10.0f);
	const PickedCell p = PickCentre(v, g, 0, 0);
	ASSERT_TRUE(p.valid);
	EXPECT_EQ(p.index, 0u);
	EXPECT_FALSE(PickCentre(v, g, 1, 0).valid);
	EXPECT_FALSE(PickCentre(v, g, 0, 1).valid);
	EXPECT_FALSE(PickCentre(v, g, -1, 0).valid);
}

TEST(CellPickerEdge, SingleRowAndSingleColumnGrids)
{
	const auto row = Grid(40, 1);
	const auto col = Grid(1, 40);
	const CellPickerView v = View(30, 30, 6.0f);
	for (int q = 0; q < 40; ++q) { const PickedCell p = PickCentre(v, row, q, 0); ASSERT_TRUE(p.valid); EXPECT_EQ(p.index, static_cast<uint32_t>(q)); }
	for (int r = 0; r < 40; ++r) { const PickedCell p = PickCentre(v, col, 0, r); ASSERT_TRUE(p.valid); EXPECT_EQ(p.index, static_cast<uint32_t>(r)); }
	EXPECT_FALSE(PickCentre(v, row, 0, 1).valid);
	EXPECT_FALSE(PickCentre(v, col, 1, 0).valid);
}

TEST(CellPickerEdge, NonSquareGridsIndexIsRowMajor)
{
	const auto g = Grid(7, 3);
	const CellPickerView v = View(5, 4, 12.0f);
	for (int r = 0; r < 3; ++r)
	{
		for (int q = 0; q < 7; ++q)
		{
			const PickedCell p = PickCentre(v, g, q, r);
			ASSERT_TRUE(p.valid);
			EXPECT_EQ(p.index, static_cast<uint32_t>(r * 7 + q));
		}
	}
}

TEST(CellPickerEdge, CellSizeScalesWorldToCell)
{
	const auto g = Grid(20, 20, 2.5f);
	const CellPickerView v = View(20, 20, 5.0f);
	for (int r = 0; r < 20; r += 3) { for (int q = 0; q < 20; q += 3) { const PickedCell p = PickCentre(v, g, q, r); ASSERT_TRUE(p.valid); EXPECT_EQ(p.q, q); EXPECT_EQ(p.r, r); } }
}

TEST(CellPickerEdge, PointsJustInsideEdgesAndCornersPickThatCell)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(40, 30, 20.0f);
	const float h = 0.8660254f;	// flat-top hex half height at size 1
	const Dia::Maths::Vector2D offsets[] = { {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.5f, h}, {-0.5f, h}, {0.5f, -h}, {-0.5f, -h}, {0.0f, h}, {0.0f, -h} };
	for (int r = 0; r < 42; r += 5)
	{
		for (int q = 0; q < 66; q += 5)
		{
			const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, 1.0f);
			for (const auto& o : offsets)
			{
				float sx, sy;
				WorldToScreen(v, c.X() + o.X() * 0.99f, c.Y() + o.Y() * 0.99f, sx, sy);
				const PickedCell p = CellPicker::Pick(v, sx, sy, g);
				ASSERT_TRUE(p.valid);
				ASSERT_EQ(p.q, q);
				ASSERT_EQ(p.r, r);
			}
		}
	}
}

TEST(CellPickerEdge, ExactVertexPicksAnAdjacentCellWithinCircumradius)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(40, 30, 20.0f);
	const float h = 0.8660254f;
	const Dia::Maths::Vector2D offsets[] = { {1.0f, 0.0f}, {0.5f, h}, {-0.5f, -h} };
	for (int r = 2; r < 40; r += 7)
	{
		for (int q = 2; q < 64; q += 7)
		{
			const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, 1.0f);
			for (const auto& o : offsets)
			{
				float sx, sy;
				WorldToScreen(v, c.X() + o.X(), c.Y() + o.Y(), sx, sy);
				const PickedCell p = CellPicker::Pick(v, sx, sy, g);
				ASSERT_TRUE(p.valid);
				const Dia::Maths::Vector2D pc = HexTopology::CellCenter(CellCoord{ p.q, p.r }, 1.0f);
				const float dx = pc.X() - (c.X() + o.X()), dy = pc.Y() - (c.Y() + o.Y());
				EXPECT_LE(dx * dx + dy * dy, 1.0001f);
			}
		}
	}
}

TEST(CellPickerEdge, GridOuterBoundaryFirstAndLastCells)
{
	const auto g = Grid(104, 64);
	const CellPickerView v = View(80, 50, 6.0f);
	EXPECT_EQ(PickCentre(v, g, 0, 0).index, 0u);
	EXPECT_EQ(PickCentre(v, g, 103, 63).index, 104u * 64u - 1u);
	EXPECT_EQ(PickCentre(v, g, 103, 0).index, 103u);
	EXPECT_EQ(PickCentre(v, g, 0, 63).index, 63u * 104u);
	EXPECT_FALSE(PickCentre(v, g, 104, 63).valid);
	EXPECT_FALSE(PickCentre(v, g, 103, 64).valid);
}

TEST(CellPickerEdge, TinyZoomStillPicksAtScreenCentre)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(10, 10, 1.0e-6f);
	const PickedCell p = CellPicker::Pick(v, 640, 360, g);
	ASSERT_TRUE(p.valid);
	const CellCoord expect = HexTopology::CellFromPoint(Dia::Maths::Vector2D(10, 10), 1.0f);
	EXPECT_EQ(p.q, expect.x);
	EXPECT_EQ(p.r, expect.y);
}

TEST(CellPickerEdge, TinyZoomOffCentreIsBeyondWorldLimitAndInvalid)
{
	const CellPickerView v = View(10, 10, 1.0e-6f);
	EXPECT_FALSE(CellPicker::Pick(v, 1000, 360, Grid(66, 42)).valid);	// 360px / 1e-6 = 3.6e8
}

TEST(CellPickerEdge, HugeZoomOnePixelStaysInCell)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(10, 10, 1.0e6f);
	const PickedCell centre = CellPicker::Pick(v, 640, 360, g);
	const PickedCell moved = CellPicker::Pick(v, 641, 361, g);
	ASSERT_TRUE(centre.valid);
	ASSERT_TRUE(moved.valid);
	EXPECT_EQ(centre.index, moved.index);
}

TEST(CellPickerEdge, InfiniteZoomMapsEverythingToCamera)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(10, 10, kInf);
	float wx, wy;
	CellPicker::ScreenToWorld(v, 1.0f, 2.0f, wx, wy);
	EXPECT_FLOAT_EQ(wx, 10.0f);
	EXPECT_FLOAT_EQ(wy, 10.0f);
	EXPECT_TRUE(CellPicker::Pick(v, 5, 5, g).valid);
}

TEST(CellPickerEdge, NegativeAndNaNZoomTreatedAsOne)
{
	const auto g = Grid(66, 42);
	const PickedCell ref = CellPicker::Pick(View(30, 20, 1.0f), 700, 400, g);
	const PickedCell neg = CellPicker::Pick(View(30, 20, -4.0f), 700, 400, g);
	const PickedCell nan = CellPicker::Pick(View(30, 20, kNaN), 700, 400, g);
	EXPECT_EQ(ref.valid, neg.valid);
	EXPECT_EQ(ref.index, neg.index);
	EXPECT_EQ(ref.valid, nan.valid);
	EXPECT_EQ(ref.index, nan.index);
}

TEST(CellPickerEdge, CameraFarAwayReturnsNoCell)
{
	const auto g = Grid(66, 42);
	EXPECT_FALSE(CellPicker::Pick(View(1.0e8f, 0, 1.0f), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(0, -1.0e8f, 1.0f), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(2.0e7f, 2.0e7f, 1.0f), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(5000, 5000, 1.0f), 640, 360, g).valid);
}

TEST(CellPickerEdge, NonFiniteCameraOrViewportReturnsNoCell)
{
	const auto g = Grid(66, 42);
	EXPECT_FALSE(CellPicker::Pick(View(kNaN, 0, 1.0f), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(0, kInf, 1.0f), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(0, 0, 1.0f, kNaN, 720), 640, 360, g).valid);
	EXPECT_FALSE(CellPicker::Pick(View(0, 0, 1.0f, 1280, kInf), 640, 360, g).valid);
}

TEST(CellPickerEdge, NaNGridCellSizeReturnsNoCell)
{
	EXPECT_FALSE(CellPicker::Pick(View(0, 0, 1.0f), 640, 360, Grid(10, 10, kNaN)).valid);
	EXPECT_FALSE(CellPicker::Pick(View(0, 0, 1.0f), 640, 360, Grid(10, 10, -1.0f)).valid);
}

TEST(CellPickerEdge, ZeroSizeCanvasCentreIsOrigin)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(10, 10, 1.0f, 0.0f, 0.0f);
	float wx, wy;
	CellPicker::ScreenToWorld(v, 0.0f, 0.0f, wx, wy);
	EXPECT_FLOAT_EQ(wx, 10.0f);
	EXPECT_FLOAT_EQ(wy, 10.0f);
	EXPECT_TRUE(CellPicker::Pick(v, 0, 0, g).valid);
}

TEST(CellPickerEdge, DefaultConstructedViewIsUsable)
{
	const CellPickerView v;
	float wx, wy;
	CellPicker::ScreenToWorld(v, 3.0f, 4.0f, wx, wy);
	EXPECT_FLOAT_EQ(wx, 3.0f);
	EXPECT_FLOAT_EQ(wy, 4.0f);
}

TEST(CellPickerEdge, NegativeScreenCoordinatesOutsideCanvasStillMapToWorld)
{
	const auto g = Grid(66, 42);
	const CellPickerView v = View(30, 20, 10.0f);
	float wx, wy;
	CellPicker::ScreenToWorld(v, -100.0f, -50.0f, wx, wy);
	EXPECT_NEAR(wx, 30.0f + (-100.0f - 640.0f) / 10.0f, 1e-4f);
	EXPECT_NEAR(wy, 20.0f + (-50.0f - 360.0f) / 10.0f, 1e-4f);
	EXPECT_FALSE(CellPicker::Pick(v, -100, -50, g).valid);
}

TEST(CellPickerInvariant, RandomCamerasRoundTripCellCentres)
{
	std::mt19937 rng(12345);
	const uint16_t dims[2][2] = { {66, 42}, {104, 64} };
	for (int iter = 0; iter < 1000; ++iter)
	{
		const auto& d = dims[iter & 1];
		const auto g = Grid(d[0], d[1]);
		std::uniform_real_distribution<float> cam(-20.0f, 140.0f), zoom(0.5f, 32.0f), vp(200.0f, 2500.0f);
		const CellPickerView v = View(cam(rng), cam(rng), zoom(rng), vp(rng), vp(rng));
		const int q = static_cast<int>(rng() % d[0]);
		const int r = static_cast<int>(rng() % d[1]);
		const PickedCell p = PickCentre(v, g, q, r);
		ASSERT_TRUE(p.valid) << iter;
		ASSERT_EQ(p.q, q) << iter;
		ASSERT_EQ(p.r, r) << iter;
		ASSERT_EQ(p.index, static_cast<uint32_t>(r * d[0] + q)) << iter;
	}
}

TEST(CellPickerInvariant, ScreenWorldRoundTrip)
{
	std::mt19937 rng(777);
	std::uniform_real_distribution<float> cam(-100.0f, 100.0f), zoom(0.1f, 50.0f), px(-500.0f, 2500.0f);
	for (int iter = 0; iter < 1000; ++iter)
	{
		const CellPickerView v = View(cam(rng), cam(rng), zoom(rng));
		const float sx = px(rng), sy = px(rng);
		float wx, wy, bx, by;
		CellPicker::ScreenToWorld(v, sx, sy, wx, wy);
		WorldToScreen(v, wx, wy, bx, by);
		ASSERT_NEAR(bx, sx, 0.05f) << iter;
		ASSERT_NEAR(by, sy, 0.05f) << iter;
	}
}

TEST(CellPickerInvariant, PickMatchesHexOracleForRandomPoints)
{
	std::mt19937 rng(2024);
	std::uniform_real_distribution<float> cam(0.0f, 100.0f), zoom(1.0f, 16.0f), px(0.0f, 1280.0f);
	const auto g = Grid(66, 42);
	const HexTopology topo(66, 42);
	for (int iter = 0; iter < 1000; ++iter)
	{
		const CellPickerView v = View(cam(rng), cam(rng), zoom(rng));
		const float sx = px(rng), sy = px(rng) * 0.5625f;
		float wx, wy;
		CellPicker::ScreenToWorld(v, sx, sy, wx, wy);
		const CellCoord c = HexTopology::CellFromPoint(Dia::Maths::Vector2D(wx, wy), 1.0f);
		const PickedCell p = CellPicker::Pick(v, sx, sy, g);
		ASSERT_EQ(p.valid, topo.IsInBounds(c)) << iter;
		if (p.valid) { ASSERT_EQ(p.q, c.x); ASSERT_EQ(p.r, c.y); ASSERT_EQ(p.index, topo.ToIndex(c)); }
		else { ASSERT_EQ(p.index, 0xFFFFFFFFu); }
	}
}
