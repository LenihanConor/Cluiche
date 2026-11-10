#include <gtest/gtest.h>
#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/SelectionInput.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <string.h>
#include <string>
#include <vector>

// Task 10a exhaustive pass for Selection (Feature 2): SelectionInput state machine, group helpers, CellDetailRegistry.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
typedef LogCapture::Level Level;

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

	Px PixelOf(const CellPickerView& v, int q, int r)
	{
		const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, 1.0f);
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

	struct SelectionInputExhaustiveTest : ::testing::Test
	{
		CellPickerView view = TestView();
		SelectionInput input;
		Recorder rec;
		Dia::Graphics::CellGridDesc g = MakeGrid(66, 42);

		void Step(const SelectionInput::PointerFrame& f, const float* gv = nullptr, uint32_t gc = 0, StringCRC gid = StringCRC::kZero)
		{
			input.Process(f, view, &g, rec, gv, gc, gid);
		}
		Px At(int q, int r) const { return PixelOf(view, q, r); }
	};
}

// ---------------------------------------------------------------- gesture state machine

TEST_F(SelectionInputExhaustiveTest, ReleaseWithoutPressProducesNothing)
{
	Step(Frame(At(5, 5), false));
	Step(Frame(At(6, 6), false));
	EXPECT_TRUE(rec.calls.empty());
	EXPECT_FALSE(input.GestureActive());
}

TEST_F(SelectionInputExhaustiveTest, NoCommandWhileTheButtonIsStillHeld)
{
	Step(Frame(At(5, 5), true));
	Step(Frame(At(9, 9), true));
	Step(Frame(At(12, 12), true));
	EXPECT_TRUE(rec.calls.empty());
	EXPECT_TRUE(input.GestureActive());
	EXPECT_EQ(5, input.StartQ());
	EXPECT_EQ(5, input.StartR());
	EXPECT_EQ(12, input.CurrentQ());
	EXPECT_EQ(12, input.CurrentR());
}

TEST_F(SelectionInputExhaustiveTest, TwoSeparateClicksProduceTwoCommands)
{
	for (int i = 0; i < 2; ++i)
	{
		Step(Frame(At(3 + i, 4), true));
		Step(Frame(At(3 + i, 4), false));
	}
	ASSERT_EQ(2u, rec.calls.size());
	EXPECT_EQ(3, rec.calls[0].a);
	EXPECT_EQ(4, rec.calls[1].a);
	EXPECT_FALSE(input.GestureActive());
	EXPECT_EQ(0u, input.PathCount());
}

TEST_F(SelectionInputExhaustiveTest, GridCornersAreClickable)
{
	const int cells[][2] = { { 0, 0 }, { 65, 0 }, { 0, 41 }, { 65, 41 } };
	for (const auto& c : cells)
	{
		rec.calls.clear();
		Step(Frame(At(c[0], c[1]), true));
		Step(Frame(At(c[0], c[1]), false));
		ASSERT_EQ(1u, rec.calls.size()) << c[0] << "," << c[1];
		EXPECT_EQ(c[0], rec.calls[0].a);
		EXPECT_EQ(c[1], rec.calls[0].b);
	}
}

TEST_F(SelectionInputExhaustiveTest, JustOutsideTheGridEdgeDoesNotStartAGesture)
{
	const int cells[][2] = { { 66, 10 }, { 10, 42 }, { -1, 10 }, { 10, -1 } };
	for (const auto& c : cells)
	{
		Step(Frame(At(c[0], c[1]), true));
		EXPECT_FALSE(input.GestureActive()) << c[0] << "," << c[1];
		Step(Frame(At(c[0], c[1]), false));
	}
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, PressOutsideThenDragInAndReleaseInsideStillDoesNothing)
{
	Step(Frame(At(200, 200), true));
	Step(Frame(At(10, 10), true));		// no new press edge: the held button never starts a gesture
	EXPECT_FALSE(input.GestureActive());
	Step(Frame(At(12, 12), false));
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, ModifiersAreReadAtReleaseNotAtPress)
{
	Step(Frame(At(4, 4), true, true));			// shift at press
	Step(Frame(At(4, 4), false));				// not at release
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);

	rec.calls.clear();
	Step(Frame(At(4, 4), true));
	Step(Frame(At(4, 4), false, true));			// shift only at release
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Toggle, rec.calls[0].call);
}

TEST_F(SelectionInputExhaustiveTest, ModifierPriorityOnClickCtrlThenShiftThenPlain)
{
	std::vector<float> values(66 * 42, 2.0f);
	Step(Frame(At(4, 4), true));
	Step(Frame(At(4, 4), false, true, true), values.data(), uint32_t(values.size()), StringCRC("terrain"));	// ctrl+shift
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Category, rec.calls[0].call);
}

TEST_F(SelectionInputExhaustiveTest, AltOnAClickIsIgnored)
{
	Step(Frame(At(4, 4), true));
	Step(Frame(At(4, 4), false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
}

TEST_F(SelectionInputExhaustiveTest, ShiftOrCtrlDragIsStillABoxAndAltWinsOverShift)
{
	for (int mode = 0; mode < 3; ++mode)
	{
		rec.calls.clear();
		Step(Frame(At(10, 10), true));
		Step(Frame(At(14, 13), true));
		Step(Frame(At(14, 13), false, mode == 0 || mode == 2, mode == 1, mode == 2));
		ASSERT_EQ(1u, rec.calls.size()) << mode;
		EXPECT_EQ(mode == 2 ? Call::Poly : Call::Box, rec.calls[0].call) << mode;
	}
}

TEST_F(SelectionInputExhaustiveTest, DragBackToTheStartCellCountsAsAClick)
{
	Step(Frame(At(10, 10), true));
	Step(Frame(At(20, 20), true));
	Step(Frame(At(10, 10), false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
	EXPECT_EQ(10, rec.calls[0].a);
}

TEST_F(SelectionInputExhaustiveTest, DragToAdjacentCellIsABoxOfTwoCorners)
{
	Step(Frame(At(10, 10), true));
	Step(Frame(At(11, 10), false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Box, rec.calls[0].call);
	EXPECT_EQ(10, rec.calls[0].a);
	EXPECT_EQ(11, rec.calls[0].c);
}

TEST_F(SelectionInputExhaustiveTest, AltDragToAdjacentCellStillGivesAPolygonOfAtLeastThreeVertices)
{
	Step(Frame(At(10, 10), true));
	Step(Frame(At(11, 10), false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	EXPECT_GE(rec.calls[0].pts.size(), 3u);
}

TEST_F(SelectionInputExhaustiveTest, AltDragReleasedOutsideTheGridEndsAtTheReleasePosition)
{
	Step(Frame(At(60, 20), true));
	Step(Frame(At(64, 20), true));
	const Px outside = At(200, 20);
	Step(Frame(outside, true));
	Step(Frame(outside, false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	float ex, ey;
	CellPicker::ScreenToWorld(view, outside.x, outside.y, ex, ey);
	EXPECT_NEAR(ex, rec.calls[0].pts.back().x, 1e-3f);
	EXPECT_NEAR(ey, rec.calls[0].pts.back().y, 1e-3f);
}

TEST_F(SelectionInputExhaustiveTest, LassoPathIsCappedAtMaxPathPointsAndKeepsTheFinalPosition)
{
	Step(Frame(At(0, 10), true, false, false, true));
	for (int row = 10; row < 16; row += 2)
	{
		// serpentine across the full width: ~100 sampled cells per pass
		for (int q = 0; q <= 64; q += 4) { Step(Frame(At(q, row), true, false, false, true)); }
		for (int q = 64; q >= 0; q -= 4) { Step(Frame(At(q, row + 1), true, false, false, true)); }
	}
	EXPECT_EQ(SelectionInput::kMaxPathPoints, input.PathCount());
	const Px end = At(33, 30);
	Step(Frame(end, true, false, false, true));
	Step(Frame(end, false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	EXPECT_EQ(SelectionInput::kMaxPathPoints, rec.calls[0].pts.size());
	float ex, ey;
	CellPicker::ScreenToWorld(view, end.x, end.y, ex, ey);
	EXPECT_NEAR(ex, rec.calls[0].pts.back().x, 1e-3f);
	EXPECT_NEAR(ey, rec.calls[0].pts.back().y, 1e-3f);
	EXPECT_EQ(0u, input.PathCount());
}

TEST_F(SelectionInputExhaustiveTest, NewGestureAfterAPolygonStartsWithAFreshPath)
{
	Step(Frame(At(10, 10), true));
	Step(Frame(At(20, 14), true));
	Step(Frame(At(20, 14), false, false, false, true));
	ASSERT_EQ(Call::Poly, rec.calls[0].call);
	rec.calls.clear();
	Step(Frame(At(30, 30), true));
	EXPECT_EQ(1u, input.PathCount());
	Step(Frame(At(31, 30), false, false, false, true));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_LT(rec.calls[0].pts.size(), 6u);
}

// ---- Esc, tool changes, invalid frames, null grid

TEST_F(SelectionInputExhaustiveTest, EscMidGestureCancelsItAndTheLaterReleaseIsSilent)
{
	Step(Frame(At(10, 10), true));
	Step(Frame(At(14, 12), true));
	auto esc = Frame(At(14, 12), true);
	esc.escPressed = true;
	Step(esc);
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Clear, rec.calls[0].call);
	EXPECT_FALSE(input.GestureActive());
	EXPECT_EQ(0u, input.PathCount());
	Step(Frame(At(14, 12), true));			// still held: not a new press
	EXPECT_FALSE(input.GestureActive());
	Step(Frame(At(14, 12), false));
	EXPECT_EQ(1u, rec.calls.size());
	// a fresh press works again
	Step(Frame(At(2, 2), true));
	Step(Frame(At(2, 2), false));
	ASSERT_EQ(2u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[1].call);
}

TEST_F(SelectionInputExhaustiveTest, EscOnTheSameFrameAsAPressStartsNoGesture)
{
	auto f = Frame(At(10, 10), true);
	f.escPressed = true;
	Step(f);
	EXPECT_FALSE(input.GestureActive());
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Clear, rec.calls[0].call);
}

TEST_F(SelectionInputExhaustiveTest, EscWithoutKeyboardDoesNotCancelAGesture)
{
	Step(Frame(At(10, 10), true));
	auto f = Frame(At(10, 10), true);
	f.escPressed = true;
	f.hasKeyboard = false;
	Step(f);
	EXPECT_TRUE(input.GestureActive());
	Step(Frame(At(10, 10), false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
}

TEST_F(SelectionInputExhaustiveTest, EscStillClearsInThePanToolAndWithoutAGrid)
{
	input.SetTool(SelectionInput::Tool::Pan);
	auto f = Frame(At(1, 1), false);
	f.escPressed = true;
	Step(f);
	EXPECT_EQ(1u, rec.calls.size());
	input.Process(f, view, nullptr, rec, nullptr, 0, StringCRC::kZero);
	EXPECT_EQ(2u, rec.calls.size());
}

TEST_F(SelectionInputExhaustiveTest, NullGridCancelsTheGestureAndProducesNothing)
{
	Step(Frame(At(10, 10), true));
	ASSERT_TRUE(input.GestureActive());
	input.Process(Frame(At(10, 10), false), view, nullptr, rec, nullptr, 0, StringCRC::kZero);
	EXPECT_FALSE(input.GestureActive());
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, SwitchingToolMidGestureCancelsButSwitchingToTheSameToolDoesNot)
{
	Step(Frame(At(10, 10), true));
	input.SetTool(SelectionInput::Tool::Select);		// same tool: no-op
	EXPECT_TRUE(input.GestureActive());
	input.SetTool(SelectionInput::Tool::Pan);
	EXPECT_FALSE(input.GestureActive());
	input.SetTool(SelectionInput::Tool::Select);
	Step(Frame(At(10, 10), false));						// the cancelled gesture's release is silent
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, PanToolEdgesStillTrackSoLeavingPanDoesNotReplayAPress)
{
	input.SetTool(SelectionInput::Tool::Pan);
	Step(Frame(At(10, 10), true));			// button goes down while in Pan
	input.SetTool(SelectionInput::Tool::Select);
	Step(Frame(At(10, 10), true));			// still held: no press edge, so no gesture
	EXPECT_FALSE(input.GestureActive());
	Step(Frame(At(10, 10), false));
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, InvalidFrameMidGestureNeitherCancelsNorReleases)
{
	Step(Frame(At(10, 10), true));
	SelectionInput::PointerFrame junk = Frame(At(50, 30), false);	// button "up" but not valid
	junk.valid = false;
	Step(junk);
	EXPECT_TRUE(input.GestureActive());
	Step(Frame(At(10, 10), true));			// still a continuation, not a new press
	Step(Frame(At(10, 10), false));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_EQ(Call::Cell, rec.calls[0].call);
	EXPECT_EQ(10, rec.calls[0].a);
}

// ---- ctrl-click group values

TEST_F(SelectionInputExhaustiveTest, CtrlClickUsesTheValueAtTheCellIndexAndRespectsTheValueCount)
{
	std::vector<float> values(66 * 42);
	for (size_t i = 0; i < values.size(); ++i) { values[i] = float(i % 7); }
	const uint32_t last = 66u * 42u - 1u;

	// last cell with exact count: OK
	Step(Frame(At(65, 41), true));
	Step(Frame(At(65, 41), false, false, true), values.data(), last + 1, StringCRC("g"));
	ASSERT_EQ(1u, rec.calls.size());
	EXPECT_FLOAT_EQ(float(last % 7), rec.calls[0].value);

	// one value short: that cell has no value, so no command
	rec.calls.clear();
	Step(Frame(At(65, 41), true));
	Step(Frame(At(65, 41), false, false, true), values.data(), last, StringCRC("g"));
	EXPECT_TRUE(rec.calls.empty());

	// zero count and null values
	Step(Frame(At(0, 0), true));
	Step(Frame(At(0, 0), false, false, true), values.data(), 0, StringCRC("g"));
	EXPECT_TRUE(rec.calls.empty());
	Step(Frame(At(0, 0), true));
	Step(Frame(At(0, 0), false, false, true), nullptr, 100, StringCRC("g"));
	EXPECT_TRUE(rec.calls.empty());
}

TEST_F(SelectionInputExhaustiveTest, CtrlClickThenPlainClickBothWorkBackToBack)
{
	std::vector<float> values(66 * 42, 1.0f);
	Step(Frame(At(3, 3), true));
	Step(Frame(At(3, 3), false, false, true), values.data(), uint32_t(values.size()), StringCRC("g"));
	Step(Frame(At(3, 3), true));
	Step(Frame(At(3, 3), false));
	ASSERT_EQ(2u, rec.calls.size());
	EXPECT_EQ(Call::Category, rec.calls[0].call);
	EXPECT_EQ(Call::Cell, rec.calls[1].call);
}

// ---------------------------------------------------------------- selection group helpers

namespace
{
	struct GroupExhaustiveRig
	{
		TestGrid grid, other;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider a, b, c, d;
		CellLayerRegistry registry;

		GroupExhaustiveRig() : grid(8, 8), other(4, 4), registry(nullptr, &FakePuIdFn)
		{
			FakePuId() = 1;
			fx.Add(UploaderSchemaFixture::EnumEntry("first", 3));
			fx.Add(UploaderSchemaFixture::EnumEntry("second", 3));
			auto flagged = UploaderSchemaFixture::EnumEntry("third", 3);
			flagged.mUi["select_group"] = true;
			fx.Add(flagged);
			auto notBool = UploaderSchemaFixture::EnumEntry("fourth", 3);
			notBool.mUi["select_group"] = "yes";
			fx.Add(notBool);
			fx.Add(UploaderSchemaFixture::FloatEntry("flt", 0, 1));
			service = &fx.Publish();
			a.values.assign(64, 0.0f); b.values.assign(64, 0.0f); c.values.assign(64, 0.0f); d.values.assign(64, 0.0f);
		}
	};
}

TEST(SelectionExhaustiveGroups, NonBooleanSelectGroupFlagIsIgnored)
{
	GroupExhaustiveRig rig;
	rig.registry.Register(rig.a.MakeDesc("fourth", &rig.grid.desc, CellValueKind::Category, "cells/fourth"));
	rig.registry.Register(rig.b.MakeDesc("first", &rig.grid.desc, CellValueKind::Category, "cells/first"));
	EXPECT_EQ(StringCRC("fourth"), SelectionInput::DefaultGroup(rig.registry, rig.service, &rig.grid.desc));	// falls back to first category, not "flagged"
}

TEST(SelectionExhaustiveGroups, FlaggedLayerWinsRegardlessOfRegistrationOrder)
{
	GroupExhaustiveRig rig;
	rig.registry.Register(rig.a.MakeDesc("first", &rig.grid.desc, CellValueKind::Category, "cells/first"));
	rig.registry.Register(rig.b.MakeDesc("second", &rig.grid.desc, CellValueKind::Category, "cells/second"));
	rig.registry.Register(rig.c.MakeDesc("third", &rig.grid.desc, CellValueKind::Category, "cells/third"));
	EXPECT_EQ(StringCRC("third"), SelectionInput::DefaultGroup(rig.registry, rig.service, &rig.grid.desc));
}

TEST(SelectionExhaustiveGroups, GroupsArePerGridAndFloatLayersAreExcluded)
{
	GroupExhaustiveRig rig;
	rig.registry.Register(rig.a.MakeDesc("flt", &rig.grid.desc, CellValueKind::Float, "cells/flt"));
	rig.registry.Register(rig.b.MakeDesc("second", &rig.other.desc, CellValueKind::Category, "cells/second"));
	rig.registry.Register(rig.c.MakeDesc("first", &rig.grid.desc, CellValueKind::Category, "cells/first"));
	StringCRC ids[4];
	EXPECT_EQ(1u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.grid.desc, ids, 4));
	EXPECT_EQ(StringCRC("first"), ids[0]);
	EXPECT_EQ(1u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.other.desc, ids, 4));
	EXPECT_EQ(StringCRC("second"), ids[0]);
	EXPECT_EQ(0u, SelectionInput::ListGroups(rig.registry, rig.service, nullptr, ids, 4));
	EXPECT_EQ(StringCRC::kZero, SelectionInput::DefaultGroup(rig.registry, rig.service, nullptr));
}

TEST(SelectionExhaustiveGroups, ListGroupsWithZeroCapacityWritesNothing)
{
	GroupExhaustiveRig rig;
	rig.registry.Register(rig.a.MakeDesc("first", &rig.grid.desc, CellValueKind::Category, "cells/first"));
	rig.registry.Register(rig.b.MakeDesc("second", &rig.grid.desc, CellValueKind::Category, "cells/second"));
	StringCRC guard[1] = { StringCRC("untouched") };
	EXPECT_EQ(2u, SelectionInput::ListGroups(rig.registry, rig.service, &rig.grid.desc, guard, 0));
	EXPECT_EQ(StringCRC("untouched"), guard[0]);
}

TEST(SelectionExhaustiveGroups, IsGroupOfRejectsUnknownFloatWrongGridAndZero)
{
	GroupExhaustiveRig rig;
	rig.registry.Register(rig.a.MakeDesc("flt", &rig.grid.desc, CellValueKind::Float, "cells/flt"));
	rig.registry.Register(rig.b.MakeDesc("first", &rig.grid.desc, CellValueKind::Category, "cells/first"));
	EXPECT_TRUE(SelectionInput::IsGroupOf(rig.registry, &rig.grid.desc, StringCRC("first")));
	EXPECT_FALSE(SelectionInput::IsGroupOf(rig.registry, &rig.grid.desc, StringCRC("flt")));
	EXPECT_FALSE(SelectionInput::IsGroupOf(rig.registry, &rig.grid.desc, StringCRC("nope")));
	EXPECT_FALSE(SelectionInput::IsGroupOf(rig.registry, &rig.grid.desc, StringCRC::kZero));
	EXPECT_FALSE(SelectionInput::IsGroupOf(rig.registry, &rig.other.desc, StringCRC("first")));
	EXPECT_FALSE(SelectionInput::IsGroupOf(rig.registry, nullptr, StringCRC("first")));
}

// ---------------------------------------------------------------- CellDetailRegistry

namespace
{
	struct CellDetailExhaustiveTest : ::testing::Test
	{
		int mPuInit = (FakePuId() = 1, 0);
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc gridA = MakeGrid(4, 3);
		Dia::Graphics::CellGridDesc gridB = MakeGrid(2, 2);
		CellDetailRegistry reg{ &FakePuIdFn };
		CellDetailResult out;

		void SetUp() override
		{
			FakePuId() = 1;
			auto withDesc = UploaderSchemaFixture::FloatEntry("described", 0, 1);
			Dia::ValueSchema::SetText(withDesc.mLabel, "Described");
			Dia::ValueSchema::SetText(withDesc.mDescription, "Long description");
			fx.Add(withDesc);
			auto withHint = UploaderSchemaFixture::FloatEntry("hinted", 0, 1);
			Dia::ValueSchema::SetText(withHint.mDescription, "Long description");
			Dia::ValueSchema::SetText(withHint.mHint, "Short hint");
			fx.Add(withHint);
			service = &fx.Publish();
		}
	};

	struct SelfRemoving
	{
		CellDetailRegistry* reg = nullptr;
		const Dia::Graphics::CellGridDesc* grid = nullptr;
		uint32_t calls = 0;
		void Provide(uint32_t, CellDetailBuilder& b)
		{
			++calls;
			b.Row("self", "x");
			reg->Unregister(grid, "Self");
		}
	};
}

TEST_F(CellDetailExhaustiveTest, InvalidRegistrationsAreRejectedAndLoggedAsErrors)
{
	LogCapture log;
	StubDetailProvider p;
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(nullptr, "A", p.Delegate()));
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(&gridA, nullptr, p.Delegate()));
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(&gridA, "", p.Delegate()));
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(&gridA, "A", CellDetailProviderFn()));
	EXPECT_EQ(0u, reg.ProviderCount());
	EXPECT_EQ(4u, log.Count(Level::kError));
}

TEST_F(CellDetailExhaustiveTest, BuildWithNothingRegisteredClearsPreviousRowsAndReturnsZero)
{
	StubDetailProvider p; p.rowsToEmit = 3;
	reg.Register(&gridA, "A", p.Delegate());
	EXPECT_EQ(1u, reg.Build(&gridA, 0, service, out));
	ASSERT_EQ(4u, out.Count());
	EXPECT_EQ(0u, reg.Build(&gridB, 0, service, out));
	EXPECT_EQ(0u, out.Count());
	EXPECT_FALSE(out.Truncated());
	EXPECT_EQ(0u, reg.Build(nullptr, 0, service, out));
}

TEST_F(CellDetailExhaustiveTest, UnregisterAndUnregisterAllEdgeInputs)
{
	StubDetailProvider p;
	reg.Register(&gridA, "A", p.Delegate());
	EXPECT_FALSE(reg.Unregister(&gridA, nullptr));
	EXPECT_FALSE(reg.Unregister(&gridB, "A"));
	EXPECT_FALSE(reg.Unregister(&gridA, "a"));			// case-sensitive
	EXPECT_EQ(0u, reg.UnregisterAll(&gridB));
	EXPECT_EQ(0u, reg.UnregisterAll(nullptr));
	EXPECT_EQ(1u, reg.ProviderCount());
}

TEST_F(CellDetailExhaustiveTest, UnregisterKeepsRemainingProvidersInRegistrationOrder)
{
	StubDetailProvider a, b, c, d;
	a.rowsToEmit = b.rowsToEmit = c.rowsToEmit = d.rowsToEmit = 0;
	reg.Register(&gridA, "A", a.Delegate());
	reg.Register(&gridB, "X", a.Delegate());
	reg.Register(&gridA, "B", b.Delegate());
	reg.Register(&gridA, "C", c.Delegate());
	reg.Register(&gridB, "Y", d.Delegate());
	EXPECT_TRUE(reg.Unregister(&gridA, "B"));
	reg.Build(&gridA, 0, service, out);
	ASSERT_EQ(2u, out.Count());
	EXPECT_STREQ("A", out.At(0).label.AsCStr());
	EXPECT_STREQ("C", out.At(1).label.AsCStr());
	EXPECT_EQ(2u, reg.UnregisterAll(&gridA));
	EXPECT_EQ(2u, reg.ProviderCount());
	reg.Build(&gridB, 0, service, out);
	ASSERT_EQ(2u, out.Count());
	EXPECT_STREQ("X", out.At(0).label.AsCStr());
	EXPECT_STREQ("Y", out.At(1).label.AsCStr());
}

TEST_F(CellDetailExhaustiveTest, CapacityFreedByUnregisterAllowsRegistrationAgain)
{
	StubDetailProvider p;
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i)
	{
		std::string n = "p" + std::to_string(i);
		ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, n.c_str(), p.Delegate()));
	}
	EXPECT_EQ(CellDetailRegisterResult::CapacityReached, reg.Register(&gridB, "late", p.Delegate()));
	EXPECT_TRUE(reg.Unregister(&gridA, "p3"));
	EXPECT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridB, "late", p.Delegate()));
	EXPECT_EQ(CellDetailRegistry::kMaxProviders, reg.ProviderCount());
}

TEST_F(CellDetailExhaustiveTest, DuplicateCheckPrecedesCapacityCheck)
{
	StubDetailProvider p;
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i)
	{
		std::string n = "p" + std::to_string(i);
		reg.Register(&gridA, n.c_str(), p.Delegate());
	}
	EXPECT_EQ(CellDetailRegisterResult::Duplicate, reg.Register(&gridA, "p0", p.Delegate()));
}

TEST_F(CellDetailExhaustiveTest, WholeResultCapDropsRowsFlagsTruncationAndStillCallsEveryProvider)
{
	StubDetailProvider p[CellDetailRegistry::kMaxProviders];
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i)
	{
		p[i].rowsToEmit = CellDetailBuilder::kMaxRowsPerProvider;
		std::string n = "p" + std::to_string(i);
		ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, n.c_str(), p[i].Delegate()));
	}
	EXPECT_EQ(CellDetailRegistry::kMaxProviders, reg.Build(&gridA, 0, service, out));
	EXPECT_EQ(CellDetailResult::kMaxRows, out.Count());
	EXPECT_TRUE(out.Truncated());
	for (uint32_t i = 0; i < CellDetailRegistry::kMaxProviders; ++i) { EXPECT_EQ(1u, p[i].calls) << i; }
	// the next Build starts clean
	reg.Unregister(&gridA, "p0");
	StubDetailProvider small; small.rowsToEmit = 1;
	reg.UnregisterAll(&gridA);
	reg.Register(&gridA, "small", small.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_FALSE(out.Truncated());
	EXPECT_EQ(2u, out.Count());
}

TEST_F(CellDetailExhaustiveTest, ResultAddReturnsNullAtCapacityAndClearResetsIt)
{
	for (uint32_t i = 0; i < CellDetailResult::kMaxRows; ++i) { ASSERT_NE(nullptr, out.Add()); }
	EXPECT_FALSE(out.Truncated());
	EXPECT_EQ(nullptr, out.Add());
	EXPECT_TRUE(out.Truncated());
	EXPECT_EQ(CellDetailResult::kMaxRows, out.Count());
	out.Clear();
	EXPECT_EQ(0u, out.Count());
	EXPECT_FALSE(out.Truncated());
	CellDetailRow* r = out.Add();
	ASSERT_NE(nullptr, r);
	EXPECT_EQ(CellDetailRowKind::Text, r->kind);		// recycled rows are reset
	EXPECT_FALSE(r->resolved);
}

TEST_F(CellDetailExhaustiveTest, BuilderWithZeroRowBudgetDropsEverything)
{
	CellDetailBuilder b(out, service, 0);
	b.Section("s");
	b.Row("a", "b");
	b.RowValue(StringCRC("cells/hinted"), 1.0f);
	EXPECT_EQ(0u, b.Added());
	EXPECT_EQ(3u, b.Dropped());
	EXPECT_TRUE(b.Truncated());
	EXPECT_EQ(0u, out.Count());
}

TEST_F(CellDetailExhaustiveTest, TooltipPrefersHintThenFallsBackToDescription)
{
	CellDetailBuilder b(out, service);
	b.RowValue(StringCRC("cells/hinted"), 1.0f);
	b.RowValue(StringCRC("cells/described"), 2.0f);
	ASSERT_EQ(2u, out.Count());
	EXPECT_STREQ("Short hint", out.At(0).tooltip.AsCStr());
	EXPECT_STREQ("Long description", out.At(1).tooltip.AsCStr());
	EXPECT_STREQ("Described", out.At(1).label.AsCStr());
}

TEST_F(CellDetailExhaustiveTest, VeryLongLabelAndTextAreTruncatedNotOverrun)
{
	const std::string longText(500, 'x');
	CellDetailBuilder b(out, service);
	b.Section(longText.c_str());
	b.Row(longText.c_str(), longText.c_str());
	ASSERT_EQ(2u, out.Count());
	EXPECT_LT(strlen(out.At(0).label.AsCStr()), 200u);
	EXPECT_GT(strlen(out.At(0).label.AsCStr()), 0u);
	EXPECT_LT(strlen(out.At(1).text.AsCStr()), 200u);
}

TEST_F(CellDetailExhaustiveTest, ProviderSeesTheRequestedCellIndexEvenAtExtremes)
{
	StubDetailProvider p;
	reg.Register(&gridA, "A", p.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(0u, p.lastCell);
	reg.Build(&gridA, 0xFFFFFFFFu, service, out);		// no range check: the registry passes the index through
	EXPECT_EQ(0xFFFFFFFFu, p.lastCell);
}

TEST_F(CellDetailExhaustiveTest, ShouldRunProvidersLeavesIndexAloneWhenFalseAndFindsHighIndices)
{
	CellSelection sel;
	Dia::Graphics::CellGridDesc big = MakeGrid(66, 42);
	sel.Reset(&big);
	uint32_t cell = 1234;
	EXPECT_FALSE(ShouldRunProviders(&sel, true, cell));
	EXPECT_EQ(1234u, cell);
	for (uint32_t idx : { 0u, 63u, 64u, 2771u })
	{
		sel.Clear();
		sel.Set(idx, true);
		EXPECT_TRUE(ShouldRunProviders(&sel, true, cell));
		EXPECT_EQ(idx, cell);
	}
	EXPECT_FALSE(ShouldRunProviders(&sel, false, cell));
	EXPECT_FALSE(ShouldRunProviders(nullptr, false, cell));
}

// ---- documented contract vs behaviour

// Header: "Register / Unregister must be called from the owner PU (a call from any other PU is rejected ...)".
// FOUND_BUG (Task 10a): Unregister / UnregisterAll do not check the calling PU, so a foreign PU can remove providers.
TEST_F(CellDetailExhaustiveTest, UnregisterFromAnotherPuShouldBeRejected)
{
	StubDetailProvider p;
	ASSERT_EQ(CellDetailRegisterResult::Ok, reg.Register(&gridA, "A", p.Delegate()));
	FakePuId() = 2;
	const bool removed = reg.Unregister(&gridA, "A");
	const uint32_t removedAll = reg.UnregisterAll(&gridA);
	FakePuId() = 1;
	EXPECT_FALSE(removed);
	EXPECT_EQ(0u, removedAll);
	EXPECT_EQ(1u, reg.ProviderCount());
}

// FOUND_BUG (Task 10a): a provider that unregisters itself during Build shifts the array under the loop index, so the
// next provider is skipped for that Build.
TEST_F(CellDetailExhaustiveTest, ProviderUnregisteringItselfDoesNotSkipTheNextProvider)
{
	SelfRemoving self; self.reg = &reg; self.grid = &gridA;
	StubDetailProvider next; next.rowsToEmit = 1;
	reg.Register(&gridA, "Self", fastdelegate::MakeDelegate(&self, &SelfRemoving::Provide));
	reg.Register(&gridA, "Next", next.Delegate());
	reg.Build(&gridA, 0, service, out);
	EXPECT_EQ(1u, self.calls);
	EXPECT_EQ(1u, next.calls);
}
