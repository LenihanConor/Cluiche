#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/SelectionInput.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaPathfinding/HexTopology.h>

#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;

namespace
{
	enum class Call { Cell, Toggle, Box, Poly, Category, Clear };

	struct Rec
	{
		Call					call;
		int32_t					a = 0, b = 0, c = 0, d = 0;
		StringCRC				layer;
		float					value = 0.0f;
		std::vector<SelectionTools::PolyPoint>	pts;
	};

	struct Recorder : ISelectionCommands
	{
		std::vector<Rec> calls;
		bool SelectCell(int32_t q, int32_t r) override { Rec x; x.call = Call::Cell; x.a = q; x.b = r; calls.push_back(x); return true; }
		bool ToggleCell(int32_t q, int32_t r) override { Rec x; x.call = Call::Toggle; x.a = q; x.b = r; calls.push_back(x); return true; }
		bool SelectBox(int32_t q0, int32_t r0, int32_t q1, int32_t r1) override { Rec x; x.call = Call::Box; x.a = q0; x.b = r0; x.c = q1; x.d = r1; calls.push_back(x); return true; }
		bool SelectPolygon(const SelectionTools::PolyPoint* p, uint32_t n) override { Rec x; x.call = Call::Poly; x.pts.assign(p, p + n); calls.push_back(x); return true; }
		bool SelectCategory(StringCRC id, float v) override { Rec x; x.call = Call::Category; x.layer = id; x.value = v; calls.push_back(x); return true; }
		bool Clear() override { Rec x; x.call = Call::Clear; calls.push_back(x); return true; }
	};

	CellPickerView TestView()
	{
		CellPickerView v;
		v.viewportWidth = 1280;
		v.viewportHeight = 720;
		v.cameraX = 50;
		v.cameraY = 40;
		v.zoom = 8.0f;
		return v;
	}

	struct Px { float x, y; };

	Px PixelOf(const CellPickerView& v, int q, int r, float size = 1.0f)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, size);
		Px p;
		WorldToScreen(v, c.X(), c.Y(), p.x, p.y);
		return p;
	}

	SelectionInput::PointerFrame Frame(Px p, bool left, bool shift = false, bool ctrl = false, bool alt = false)
	{
		SelectionInput::PointerFrame f;
		f.x = p.x; f.y = p.y; f.left = left; f.shift = shift; f.ctrl = ctrl; f.alt = alt; f.valid = true;
		return f;
	}

	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	struct SelectionInputTest : ::testing::Test
	{
		CellPickerView view = TestView();
		SelectionInput input;
		Recorder rec;

		void Step(const Dia::Graphics::CellGridDesc& g, const SelectionInput::PointerFrame& f, const float* gv = nullptr, uint32_t gc = 0, StringCRC gid = StringCRC::kZero)
		{
			input.Process(f, view, &g, rec, gv, gc, gid);
		}
		void Click(const Dia::Graphics::CellGridDesc& g, int q, int r, bool shift = false, bool ctrl = false, const float* gv = nullptr, uint32_t gc = 0, StringCRC gid = StringCRC::kZero)
		{
			const Px p = PixelOf(view, q, r);
			Step(g, Frame(p, true), gv, gc, gid);
			Step(g, Frame(p, false, shift, ctrl), gv, gc, gid);
		}
	};
}

TEST_F(SelectionInputTest, ClickSelectsCellUnderCursor)
{
	for (const Dim& d : kDims)
	{
		rec.calls.clear();
		const auto g = MakeGrid(d.w, d.h);
		Click(g, 7, 9);
		ASSERT_EQ(1u, rec.calls.size());
		EXPECT_EQ(Call::Cell, rec.calls[0].call);
		EXPECT_EQ(7, rec.calls[0].a);
		EXPECT_EQ(9, rec.calls[0].b);
	}
}

TEST_F(SelectionInputTest, ShiftClickToggles)
{
	const auto g = MakeGrid(66, 42);
	Click(g, 3, 4, true);
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Toggle, rec.calls[0].call);
	EXPECT_EQ(3, rec.calls[0].a);
	EXPECT_EQ(4, rec.calls[0].b);
}

TEST_F(SelectionInputTest, CtrlClickSelectsCategoryOfCellValue)
{
	const auto g = MakeGrid(66, 42);
	std::vector<float> values(66 * 42);
	for (size_t i = 0; i < values.size(); ++i) { values[i] = static_cast<float>(i % 4); }
	Click(g, 5, 2, false, true, values.data(), static_cast<uint32_t>(values.size()), StringCRC("terrain"));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Category, rec.calls[0].call);
	EXPECT_EQ(StringCRC("terrain"), rec.calls[0].layer);
	EXPECT_FLOAT_EQ(static_cast<float>((2 * 66 + 5) % 4), rec.calls[0].value);
}

TEST_F(SelectionInputTest, CtrlClickWithoutGroupDoesNothing)
{
	const auto g = MakeGrid(66, 42);
	Click(g, 5, 2, false, true, nullptr, 0, StringCRC::kZero);
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputTest, DragSelectsBoxWithStartAndEndCorners)
{
	for (const Dim& d : kDims)
	{
		rec.calls.clear();
		const auto g = MakeGrid(d.w, d.h);
		Step(g, Frame(PixelOf(view, 10, 12), true));
		EXPECT_TRUE(input.GestureActive());
		Step(g, Frame(PixelOf(view, 13, 14), true));
		Step(g, Frame(PixelOf(view, 15, 11), false));
		ASSERT_EQ(1u, rec.calls.size());
		EXPECT_EQ(Call::Box, rec.calls[0].call);
		EXPECT_EQ(10, rec.calls[0].a);
		EXPECT_EQ(12, rec.calls[0].b);
		EXPECT_EQ(15, rec.calls[0].c);
		EXPECT_EQ(11, rec.calls[0].d);
		EXPECT_FALSE(input.GestureActive());
	}
}

TEST_F(SelectionInputTest, AltDragSelectsPolygonIncludingStartAndEnd)
{
	const auto g = MakeGrid(66, 42);
	const Px a = PixelOf(view, 10, 10), b = PixelOf(view, 12, 11), c = PixelOf(view, 14, 10), e = PixelOf(view, 16, 12);
	Step(g, Frame(a, true));
	Step(g, Frame(b, true));
	Step(g, Frame(c, true));
	Step(g, Frame(e, false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	const auto& pts = rec.calls[0].pts;
	ASSERT_GE(pts.size(), 3u);
	float sx, sy, ex, ey;
	CellPicker::ScreenToWorld(view, a.x, a.y, sx, sy);
	CellPicker::ScreenToWorld(view, e.x, e.y, ex, ey);
	EXPECT_NEAR(sx, pts.front().x, 1e-3f);
	EXPECT_NEAR(sy, pts.front().y, 1e-3f);
	EXPECT_NEAR(ex, pts.back().x, 1e-3f);
	EXPECT_NEAR(ey, pts.back().y, 1e-3f);
}

TEST_F(SelectionInputTest, FastMouseKeepsFinalPositionAndInterpolates)
{
	const auto g = MakeGrid(66, 42);
	const Px a = PixelOf(view, 2, 2), e = PixelOf(view, 40, 30);
	Step(g, Frame(a, true));
	Step(g, Frame(e, true));	// one frame, ~40 cells
	Step(g, Frame(e, false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	const auto& pts = rec.calls[0].pts;
	EXPECT_GT(pts.size(), 10u);	// interpolated, not just two points
	float ex, ey;
	CellPicker::ScreenToWorld(view, e.x, e.y, ex, ey);
	EXPECT_NEAR(ex, pts.back().x, 1e-3f);
	EXPECT_NEAR(ey, pts.back().y, 1e-3f);
}

TEST_F(SelectionInputTest, ClickOutsideGridDoesNothing)
{
	const auto g = MakeGrid(66, 42);
	const Px out = PixelOf(view, 200, 200);
	Step(g, Frame(out, true));
	EXPECT_FALSE(input.GestureActive());
	Step(g, Frame(out, false));
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputTest, DragStartingOutsideDoesNothing)
{
	const auto g = MakeGrid(66, 42);
	Step(g, Frame(PixelOf(view, 200, 200), true));
	Step(g, Frame(PixelOf(view, 10, 10), true));
	Step(g, Frame(PixelOf(view, 12, 12), false));
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputTest, DragEndingOutsideUsesLastValidCell)
{
	const auto g = MakeGrid(66, 42);
	Step(g, Frame(PixelOf(view, 60, 20), true));
	Step(g, Frame(PixelOf(view, 64, 20), true));
	Step(g, Frame(PixelOf(view, 200, 20), true));
	Step(g, Frame(PixelOf(view, 200, 20), false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Box, rec.calls[0].call);
	EXPECT_EQ(60, rec.calls[0].a);
	EXPECT_EQ(65, rec.calls[0].c);	// interpolation walks to the edge of the grid
	EXPECT_EQ(20, rec.calls[0].b);
}

TEST_F(SelectionInputTest, EscClears)
{
	const auto g = MakeGrid(66, 42);
	auto f = Frame(PixelOf(view, 1, 1), false);
	f.escPressed = true;
	Step(g, f);
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Clear, rec.calls[0].call);
}

TEST_F(SelectionInputTest, EscIgnoredWithoutKeyboard)
{
	const auto g = MakeGrid(66, 42);
	auto f = Frame(PixelOf(view, 1, 1), false);
	f.escPressed = true;
	f.hasKeyboard = false;
	Step(g, f);
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputTest, ModifiersIgnoredWithoutKeyboard)
{
	const auto g = MakeGrid(66, 42);
	const Px p = PixelOf(view, 4, 4);
	auto down = Frame(p, true);
	auto up = Frame(p, false, true, false, false);	// shift held, but no keyboard
	down.hasKeyboard = up.hasKeyboard = false;
	Step(g, down);
	Step(g, up);
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
}

TEST_F(SelectionInputTest, MiddleAndRightButtonsProduceNothing)
{
	const auto g = MakeGrid(66, 42);
	for (int which = 0; which < 2; ++which)
	{
		auto f = Frame(PixelOf(view, 5, 5), false);
		(which == 0 ? f.middle : f.right) = true;
		Step(g, f);
		f.x = PixelOf(view, 9, 9).x;
		Step(g, f);
		f.middle = f.right = false;
		Step(g, f);
	}
	EXPECT_TRUE(rec.calls.empty());
	EXPECT_FALSE(input.GestureActive());
}

TEST_F(SelectionInputTest, MiddleDragDoesNotDisturbLeftGesture)
{
	const auto g = MakeGrid(66, 42);
	const Px a = PixelOf(view, 5, 5);
	Step(g, Frame(a, true));
	auto mid = Frame(a, true);
	mid.middle = true;
	Step(g, mid);
	auto right = Frame(a, true);
	right.right = true;
	Step(g, right);
	EXPECT_TRUE(input.GestureActive());
	Step(g, Frame(a, false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
}

TEST_F(SelectionInputTest, PanToolLeftDragProducesNothing)
{
	const auto g = MakeGrid(66, 42);
	input.SetTool(SelectionInput::Tool::Pan);
	EXPECT_TRUE(input.PanTool());
	Step(g, Frame(PixelOf(view, 5, 5), true));
	Step(g, Frame(PixelOf(view, 9, 9), true));
	Step(g, Frame(PixelOf(view, 9, 9), false));
	EXPECT_TRUE(rec.calls.empty());
	EXPECT_FALSE(input.GestureActive());
	input.SetTool(SelectionInput::Tool::Select);
	EXPECT_FALSE(input.PanTool());
}

TEST_F(SelectionInputTest, InvalidFrameDoesNothing)
{
	const auto g = MakeGrid(66, 42);
	SelectionInput::PointerFrame f = Frame(PixelOf(view, 5, 5), true);
	f.valid = false;
	f.escPressed = true;
	Step(g, f);
	f.left = false;
	Step(g, f);
	EXPECT_TRUE(rec.calls.empty());
	EXPECT_FALSE(input.GestureActive());
}

// ---- selection group ----

namespace
{
	struct GroupRig
	{
		TestGrid grid;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, biome, temp;
		CellLayerRegistry registry;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f, bool flagBiome)
		{
			f.Add(UploaderSchemaFixture::EnumEntry("terrain", 4));
			auto biome = UploaderSchemaFixture::EnumEntry("biome", 4);
			if (flagBiome) { biome.mUi["select_group"] = true; }
			f.Add(biome);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			return &f.Publish();
		}

		GroupRig(bool flagBiome, bool withCategories = true)
			: grid(66, 42)
			, service(Build(fx, flagBiome))
			, registry(nullptr, &FakePuIdFn)
		{
			FakePuId() = 1;
			terrain.values.assign(66 * 42, 0.0f);
			biome.values.assign(66 * 42, 0.0f);
			temp.values.assign(66 * 42, 1.0f);
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			if (withCategories)
			{
				registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
				registry.Register(biome.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome"));
			}
		}
	};
}

TEST(SelectionInputGroupTest, ListGroupsListsCategoryLayersInOrder)
{
	GroupRig rig(false);
	StringCRC ids[4];
	EXPECT_EQ(2u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.grid.desc, ids, 4));
	EXPECT_EQ(StringCRC("terrain"), ids[0]);
	EXPECT_EQ(StringCRC("biome"), ids[1]);
	StringCRC one[1];
	EXPECT_EQ(2u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.grid.desc, one, 1));	// total returned, capacity respected
	EXPECT_EQ(StringCRC("terrain"), one[0]);
}

TEST(SelectionInputGroupTest, DefaultIsFirstWithSelectGroupFlag)
{
	GroupRig rig(true);
	EXPECT_EQ(StringCRC("biome"), SelectionInput::DefaultGroup(rig.registry, rig.service, &rig.grid.desc));
}

TEST(SelectionInputGroupTest, DefaultFallsBackToFirstCategoryLayer)
{
	GroupRig rig(false);
	EXPECT_EQ(StringCRC("terrain"), SelectionInput::DefaultGroup(rig.registry, rig.service, &rig.grid.desc));
	EXPECT_EQ(StringCRC("terrain"), SelectionInput::DefaultGroup(rig.registry, nullptr, &rig.grid.desc));
}

TEST(SelectionInputGroupTest, DefaultIsNoneWithoutCategoryLayers)
{
	GroupRig rig(true, false);
	EXPECT_EQ(StringCRC::kZero, SelectionInput::DefaultGroup(rig.registry, rig.service, &rig.grid.desc));
	EXPECT_EQ(0u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.grid.desc, nullptr, 0));
}

TEST(SelectionInputGroupTest, GroupIsSettable)
{
	SelectionInput in;
	in.SetSelectionGroup(StringCRC("biome"));
	EXPECT_EQ(StringCRC("biome"), in.SelectionGroup());
}

// ---- domain integration ----

#ifdef DIA_DEBUG
namespace
{
	double Commands()
	{
		auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC("dia.celllayers.viewer.select_commands"));
		return c ? static_cast<double>(c->Value()) : -1.0;
	}
}

TEST(SelectionInputDomainTest, SubmittedPressReleaseSelectsCellAndCountsCommand)
{
	GroupRig rig(true);
	Dia::Graphics::CellFieldRegistry cellField;
	LayerUploader uploader(cellField, nullptr);
	CellLayerViewerDomain domain(rig.registry, uploader, rig.service);
	FakeDebugDraw draw;
	domain.Update(draw);
	ASSERT_NE(nullptr, domain.ActiveSelectionView());
	const double before = Commands();

	const CellPickerView view = TestView();
	const Px p = PixelOf(view, 12, 8);
	domain.SubmitPointerFrame(Frame(p, true), view);
	domain.Update(draw);
	EXPECT_EQ(0u, domain.ActiveSelectionView()->Count());
	domain.SubmitPointerFrame(Frame(p, false), view);
	domain.Update(draw);

	const ICellSelectionView* sel = domain.ActiveSelectionView();
	ASSERT_NE(nullptr, sel);
	EXPECT_EQ(1u, sel->Count());
	EXPECT_TRUE(sel->Contains(8u * 66u + 12u));
	EXPECT_EQ(before + 1.0, Commands());
	EXPECT_EQ(StringCRC("biome"), domain.Input().SelectionGroup());	// default resolved from ui.select_group

	// No submit -> no processing.
	domain.Update(draw);
	EXPECT_EQ(before + 1.0, Commands());
}
#endif
