#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionOutline.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaPathfinding/HexTopology.h>

#include <limits>
#include <math.h>
#include <vector>

// Task 10a exhaustive pass for Selection (Feature 2): CellSelection, SelectionTools, SelectionOutline boundaries.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
namespace Tools = Dia::CellLayers::SelectionTools;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	const int32_t kIntMin = std::numeric_limits<int32_t>::min();
	const int32_t kIntMax = std::numeric_limits<int32_t>::max();

	struct Bound
	{
		Dia::Graphics::CellGridDesc grid;
		CellSelection sel;
		explicit Bound(uint16_t w, uint16_t h, float size = 1.0f) : grid(MakeGrid(w, h, size)) { sel.Reset(&grid); }
		uint32_t Idx(int q, int r) const { return uint32_t(r) * sel.Width() + uint32_t(q); }
	};

	uint32_t CountVisited(const CellSelection& s)
	{
		uint32_t n = 0;
		s.ForEach([](uint32_t, void* u) { ++*static_cast<uint32_t*>(u); }, &n);
		return n;
	}
}

// ---------------------------------------------------------------- CellSelection

TEST(SelectionExhaustiveCellSelection, UnboundSelectionIsInert)
{
	CellSelection s;
	EXPECT_EQ(0u, s.Count());
	EXPECT_EQ(0u, s.Version());
	EXPECT_EQ(0u, s.CellCount());
	EXPECT_FALSE(s.Contains(0));
	EXPECT_FALSE(s.Set(0, true));
	EXPECT_FALSE(s.Set(0, false));
	EXPECT_FALSE(s.Clear());
	EXPECT_EQ(0u, CountVisited(s));
	s.BeginEdit();
	EXPECT_FALSE(s.EndEdit());
	EXPECT_EQ(0u, s.Version());
}

TEST(SelectionExhaustiveCellSelection, ZeroSizedGridsHaveNoCells)
{
	for (const Dim& d : { Dim{ 0, 5 }, Dim{ 5, 0 }, Dim{ 0, 0 } })
	{
		Bound b(d.w, d.h);
		EXPECT_EQ(0u, b.sel.CellCount());
		EXPECT_FALSE(b.sel.Set(0, true));
		EXPECT_FALSE(b.sel.Contains(0));
		EXPECT_FALSE(b.sel.Clear());
		b.sel.BeginEdit();
		EXPECT_FALSE(b.sel.EndEdit());
		EXPECT_EQ(0u, b.sel.Version());
	}
}

TEST(SelectionExhaustiveCellSelection, SizesAroundTheWordBoundary)
{
	const Dim dims[] = { { 1, 1 }, { 63, 1 }, { 64, 1 }, { 65, 1 }, { 1, 64 }, { 7, 9 }, { 8, 8 }, { 5, 13 }, { 66, 42 }, { 104, 64 } };
	for (const Dim& d : dims)
	{
		Bound b(d.w, d.h);
		const uint32_t n = b.sel.CellCount();
		ASSERT_EQ(uint32_t(d.w) * d.h, n);
		EXPECT_TRUE(b.sel.Set(0, true));
		EXPECT_EQ(n > 1, b.sel.Set(n - 1, true));
		EXPECT_FALSE(b.sel.Set(n, true)) << d.w << "x" << d.h;
		EXPECT_FALSE(b.sel.Contains(n));
		EXPECT_EQ(n == 1 ? 1u : 2u, b.sel.Count());
		for (uint32_t i = 0; i < n; ++i) { b.sel.Set(i, true); }
		EXPECT_EQ(n, b.sel.Count());
		EXPECT_EQ(n, CountVisited(b.sel));
		EXPECT_TRUE(b.sel.Clear());
		EXPECT_EQ(0u, b.sel.Count());
		EXPECT_EQ(0u, CountVisited(b.sel));
	}
}

TEST(SelectionExhaustiveCellSelection, FarOutOfRangeIndicesAreIgnoredWithoutBump)
{
	Bound b(8, 8);
	const uint32_t v = b.sel.Version();
	EXPECT_FALSE(b.sel.Set(64, true));
	EXPECT_FALSE(b.sel.Set(0xFFFFFFFFu, true));
	EXPECT_FALSE(b.sel.Set(0xFFFFFFFFu, false));
	EXPECT_FALSE(b.sel.Contains(0xFFFFFFFFu));
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_EQ(0u, b.sel.Count());
}

TEST(SelectionExhaustiveCellSelection, UnsetOfAbsentCellIsNoChange)
{
	Bound b(8, 8);
	const uint32_t v = b.sel.Version();
	EXPECT_FALSE(b.sel.Set(5, false));
	EXPECT_EQ(v, b.sel.Version());
	b.sel.Set(5, true);
	EXPECT_TRUE(b.sel.Set(5, false));
	EXPECT_EQ(v + 2, b.sel.Version());
	EXPECT_EQ(0u, b.sel.Count());
}

TEST(SelectionExhaustiveCellSelection, ResetNullOnEmptyDoesNotBumpAndOnNonEmptyBumpsOnce)
{
	Bound b(8, 8);
	b.sel.Reset(nullptr);
	EXPECT_EQ(0u, b.sel.Version());
	EXPECT_EQ(0u, b.sel.CellCount());
	EXPECT_EQ(0.0f, b.sel.CellSize());
	b.sel.Reset(&b.grid);
	b.sel.Set(3, true);
	const uint32_t v = b.sel.Version();
	b.sel.Reset(nullptr);
	EXPECT_EQ(v + 1, b.sel.Version());
	b.sel.Reset(nullptr);	// again: idempotent
	EXPECT_EQ(v + 1, b.sel.Version());
	EXPECT_FALSE(b.sel.Set(3, true));
}

TEST(SelectionExhaustiveCellSelection, RebindToSameCellCountDifferentShapeLeavesNoStaleBits)
{
	Dia::Graphics::CellGridDesc a = MakeGrid(8, 8);
	Dia::Graphics::CellGridDesc b = MakeGrid(4, 16);
	CellSelection s;
	s.Reset(&a);
	for (uint32_t i = 0; i < 64; ++i) { s.Set(i, true); }
	ASSERT_EQ(64u, s.Count());
	s.Reset(&b);
	EXPECT_EQ(0u, s.Count());
	for (uint32_t i = 0; i < 64; ++i) { EXPECT_FALSE(s.Contains(i)) << i; }
	EXPECT_EQ(0u, CountVisited(s));
	// grow, then shrink, then back: still clean
	Dia::Graphics::CellGridDesc big = MakeGrid(66, 42);
	Dia::Graphics::CellGridDesc tiny = MakeGrid(2, 2);
	s.Set(10, true);
	s.Reset(&big);
	for (uint32_t i = 0; i < big.width * big.height; ++i) { ASSERT_FALSE(s.Contains(i)); }
	s.Set(2771, true);
	s.Reset(&tiny);
	s.Reset(&big);
	EXPECT_EQ(0u, s.Count());
	EXPECT_FALSE(s.Contains(2771));
}

TEST(SelectionExhaustiveCellSelection, RebindToEmptySelectionDoesNotBump)
{
	Dia::Graphics::CellGridDesc a = MakeGrid(8, 8);
	Dia::Graphics::CellGridDesc b = MakeGrid(9, 9);
	CellSelection s;
	s.Reset(&a);
	const uint32_t v = s.Version();
	s.Reset(&b);
	EXPECT_EQ(v, s.Version());
	EXPECT_EQ(81u, s.CellCount());
}

TEST(SelectionExhaustiveCellSelection, NestedBeginEditIsIgnoredAndKeepsTheFirstSnapshot)
{
	Bound b(8, 8);
	const uint32_t v = b.sel.Version();
	b.sel.BeginEdit();
	b.sel.Set(1, true);
	b.sel.BeginEdit();		// ignored: must not re-snapshot
	b.sel.Set(1, false);
	EXPECT_TRUE(b.sel.IsEditing());
	EXPECT_FALSE(b.sel.EndEdit());
	EXPECT_FALSE(b.sel.IsEditing());
	EXPECT_EQ(v, b.sel.Version());
}

TEST(SelectionExhaustiveCellSelection, EndEditWithoutBeginIsNoOp)
{
	Bound b(8, 8);
	b.sel.Set(1, true);
	const uint32_t v = b.sel.Version();
	EXPECT_FALSE(b.sel.EndEdit());
	EXPECT_EQ(v, b.sel.Version());
	b.sel.BeginEdit();
	EXPECT_FALSE(b.sel.EndEdit());
	EXPECT_FALSE(b.sel.EndEdit());	// second End: no-op
	EXPECT_EQ(v, b.sel.Version());
}

TEST(SelectionExhaustiveCellSelection, ClearThenRestoreInsideEditIsNetNoChange)
{
	Bound b(8, 8);
	b.sel.Set(3, true);
	b.sel.Set(9, true);
	const uint32_t v = b.sel.Version();
	b.sel.BeginEdit();
	EXPECT_TRUE(b.sel.Clear());
	EXPECT_EQ(0u, b.sel.Count());
	b.sel.Set(9, true);
	b.sel.Set(3, true);
	EXPECT_FALSE(b.sel.EndEdit());
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_EQ(2u, b.sel.Count());
}

TEST(SelectionExhaustiveCellSelection, SetsInsideEditDoNotBumpUntilEnd)
{
	Bound b(8, 8);
	const uint32_t v = b.sel.Version();
	b.sel.BeginEdit();
	b.sel.Set(1, true);
	b.sel.Set(2, true);
	b.sel.Clear();
	b.sel.Set(4, true);
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_TRUE(b.sel.EndEdit());
	EXPECT_EQ(v + 1, b.sel.Version());
	EXPECT_EQ(1u, b.sel.Count());
}

TEST(SelectionExhaustiveCellSelection, ResetDuringEditCancelsItAndBumpsOnce)
{
	Dia::Graphics::CellGridDesc a = MakeGrid(8, 8);
	Dia::Graphics::CellGridDesc b = MakeGrid(4, 4);
	CellSelection s;
	s.Reset(&a);
	const uint32_t v = s.Version();
	s.BeginEdit();
	s.Set(1, true);
	s.Reset(&b);
	EXPECT_FALSE(s.IsEditing());
	EXPECT_EQ(v + 1, s.Version());
	EXPECT_EQ(0u, s.Count());
	EXPECT_FALSE(s.EndEdit());
	EXPECT_EQ(v + 1, s.Version());
	EXPECT_TRUE(s.Set(2, true));				// edits bump normally again
	EXPECT_EQ(v + 2, s.Version());
}

TEST(SelectionExhaustiveCellSelection, ForEachNullCallbackIsSafeAndUserPointerIsPassedThrough)
{
	Bound b(8, 8);
	b.sel.Set(5, true);
	b.sel.ForEach(nullptr, nullptr);
	const void* seen = nullptr;
	b.sel.ForEach([](uint32_t, void* u) { *static_cast<const void**>(u) = u; }, &seen);
	EXPECT_EQ(static_cast<const void*>(&seen), seen);
}

TEST(SelectionExhaustiveCellSelection, ForEachVisitsAcrossWordBoundariesInOrder)
{
	Bound b(66, 42);
	const uint32_t cells[] = { 0, 63, 64, 65, 127, 128, 2771 };
	for (uint32_t c : cells) { b.sel.Set(c, true); }
	const std::vector<uint32_t> v = ToVector(b.sel);
	ASSERT_EQ(7u, v.size());
	for (size_t i = 0; i < v.size(); ++i) { EXPECT_EQ(cells[i], v[i]); }
}

TEST(SelectionExhaustiveCellSelection, CellSizeFollowsResetToNullAndToGrid)
{
	Dia::Graphics::CellGridDesc g = MakeGrid(4, 4, 2.5f);
	CellSelection s;
	s.Reset(&g);
	EXPECT_FLOAT_EQ(2.5f, s.CellSize());
	s.Reset(nullptr);
	EXPECT_FLOAT_EQ(0.0f, s.CellSize());
}

// ---------------------------------------------------------------- SelectionTools

TEST(SelectionExhaustiveTools, SelectCellCornersAndJustOutsideBothGridSizes)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h);
		EXPECT_TRUE(Tools::SelectCell(b.sel, 0, 0).changed);
		EXPECT_TRUE(b.sel.Contains(0));
		EXPECT_TRUE(Tools::SelectCell(b.sel, d.w - 1, d.h - 1).changed);
		EXPECT_EQ(std::vector<uint32_t>{ uint32_t(d.w) * d.h - 1 }, ToVector(b.sel));
		const uint32_t v = b.sel.Version();
		const int32_t bad[][2] = { { d.w, 0 }, { 0, d.h }, { d.w, d.h }, { -1, 0 }, { 0, -1 }, { kIntMin, kIntMin }, { kIntMax, kIntMax }, { kIntMin, 0 } };
		for (const auto& p : bad)
		{
			const auto r = Tools::SelectCell(b.sel, p[0], p[1]);
			EXPECT_TRUE(r.ok);
			EXPECT_FALSE(r.changed);
			EXPECT_EQ(1u, r.count);
		}
		EXPECT_EQ(v, b.sel.Version());
		EXPECT_EQ(1u, b.sel.Count());
	}
}

TEST(SelectionExhaustiveTools, SelectCellSameCellTwiceIsIdempotent)
{
	Bound b(66, 42);
	const auto first = Tools::SelectCell(b.sel, 5, 5);
	const uint32_t v = b.sel.Version();
	const auto second = Tools::SelectCell(b.sel, 5, 5);
	EXPECT_TRUE(first.changed);
	EXPECT_FALSE(second.changed);
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_EQ(1u, second.count);
}

TEST(SelectionExhaustiveTools, SelectCellReplacesMultiCellSelectionWithOneBump)
{
	Bound b(66, 42);
	Tools::SelectBox(b.sel, 0, 0, 9, 9);
	const uint32_t v = b.sel.Version();
	const auto r = Tools::SelectCell(b.sel, 3, 3);
	EXPECT_TRUE(r.changed);
	EXPECT_EQ(v + 1, b.sel.Version());
	EXPECT_EQ(std::vector<uint32_t>{ b.Idx(3, 3) }, ToVector(b.sel));
}

TEST(SelectionExhaustiveTools, ToggleBoundariesAndVersionPerFlip)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h);
		const uint32_t v = b.sel.Version();
		auto r = Tools::ToggleCell(b.sel, d.w - 1, d.h - 1);
		EXPECT_TRUE(r.changed);
		EXPECT_EQ(1u, r.count);
		r = Tools::ToggleCell(b.sel, d.w - 1, d.h - 1);
		EXPECT_TRUE(r.changed);
		EXPECT_EQ(0u, r.count);
		EXPECT_EQ(v + 2, b.sel.Version());
		r = Tools::ToggleCell(b.sel, d.w, 0);
		EXPECT_TRUE(r.ok);
		EXPECT_FALSE(r.changed);
		r = Tools::ToggleCell(b.sel, kIntMin, kIntMax);
		EXPECT_TRUE(r.ok);
		EXPECT_FALSE(r.changed);
		EXPECT_EQ(v + 2, b.sel.Version());
	}
}

TEST(SelectionExhaustiveTools, ToggleDoesNotDisturbOtherCells)
{
	Bound b(66, 42);
	Tools::SelectBox(b.sel, 0, 0, 2, 0);
	Tools::ToggleCell(b.sel, 1, 0);
	EXPECT_EQ((std::vector<uint32_t>{ 0, 2 }), ToVector(b.sel));
	Tools::ToggleCell(b.sel, 40, 20);
	EXPECT_EQ(3u, b.sel.Count());
}

TEST(SelectionExhaustiveTools, BoxExtremeCornersCoverWholeGrid)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h);
		const auto r = Tools::SelectBox(b.sel, kIntMin, kIntMin, kIntMax, kIntMax);
		EXPECT_TRUE(r.ok);
		EXPECT_TRUE(r.changed);
		EXPECT_EQ(uint32_t(d.w) * d.h, r.count);
		const auto again = Tools::SelectBox(b.sel, kIntMax, kIntMax, kIntMin, kIntMin);
		EXPECT_FALSE(again.changed);
	}
}

TEST(SelectionExhaustiveTools, BoxSingleCellRowColumnAndLastCell)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h);
		EXPECT_EQ(1u, Tools::SelectBox(b.sel, 4, 4, 4, 4).count);
		EXPECT_EQ(std::vector<uint32_t>{ b.Idx(4, 4) }, ToVector(b.sel));
		EXPECT_EQ(uint32_t(d.w), Tools::SelectBox(b.sel, 0, 3, d.w - 1, 3).count);
		EXPECT_EQ(uint32_t(d.h), Tools::SelectBox(b.sel, 7, 0, 7, d.h - 1).count);
		EXPECT_EQ(1u, Tools::SelectBox(b.sel, d.w - 1, d.h - 1, d.w - 1, d.h - 1).count);
		EXPECT_TRUE(b.sel.Contains(uint32_t(d.w) * d.h - 1));
	}
}

TEST(SelectionExhaustiveTools, BoxJustOutsideEachSideIsUnchangedAndReplacesNothing)
{
	Bound b(66, 42);
	Tools::SelectCell(b.sel, 10, 10);
	const uint32_t v = b.sel.Version();
	const int32_t outside[][4] = {
		{ 66, 0, 70, 5 }, { 0, 42, 5, 50 }, { -5, -5, -1, 5 }, { 0, -5, 5, -1 }, { 66, 42, 70, 50 },
	};
	for (const auto& o : outside)
	{
		const auto r = Tools::SelectBox(b.sel, o[0], o[1], o[2], o[3]);
		EXPECT_TRUE(r.ok);
		EXPECT_FALSE(r.changed);
		EXPECT_EQ(1u, r.count);
	}
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_TRUE(b.sel.Contains(b.Idx(10, 10)));
}

TEST(SelectionExhaustiveTools, BoxTouchingGridCornerFromOutsideClamps)
{
	Bound b(66, 42);
	EXPECT_EQ(1u, Tools::SelectBox(b.sel, -10, -10, 0, 0).count);
	EXPECT_TRUE(b.sel.Contains(0));
	EXPECT_EQ(1u, Tools::SelectBox(b.sel, 65, 41, 99, 99).count);
	EXPECT_TRUE(b.sel.Contains(b.Idx(65, 41)));
}

TEST(SelectionExhaustiveTools, BoxOnOneByOneGrid)
{
	Bound b(1, 1);
	EXPECT_EQ(1u, Tools::SelectBox(b.sel, 0, 0, 0, 0).count);
	EXPECT_FALSE(Tools::SelectBox(b.sel, 0, 0, 0, 0).changed);
	EXPECT_EQ(1u, Tools::SelectBox(b.sel, -3, -3, 3, 3).count);
	EXPECT_EQ(0u, Tools::ClearSelection(b.sel).count);
}

TEST(SelectionExhaustiveTools, BoxShrinkingSelectionDropsOldCellsOutsideTheNewBox)
{
	Bound b(66, 42);
	Tools::SelectBox(b.sel, 0, 0, 9, 9);
	const auto r = Tools::SelectBox(b.sel, 20, 20, 21, 21);
	EXPECT_TRUE(r.changed);
	EXPECT_EQ(4u, r.count);
	EXPECT_FALSE(b.sel.Contains(0));
}

TEST(SelectionExhaustiveTools, ClearTwiceBumpsOnce)
{
	Bound b(66, 42);
	Tools::SelectCell(b.sel, 1, 1);
	const uint32_t v = b.sel.Version();
	EXPECT_TRUE(Tools::ClearSelection(b.sel).changed);
	EXPECT_FALSE(Tools::ClearSelection(b.sel).changed);
	EXPECT_EQ(v + 1, b.sel.Version());
}

TEST(SelectionExhaustiveTools, EveryToolOnUnboundSelectionIsInertAndSafe)
{
	CellSelection s;
	EXPECT_FALSE(Tools::SelectCell(s, 0, 0).changed);
	EXPECT_FALSE(Tools::ToggleCell(s, 0, 0).changed);
	EXPECT_FALSE(Tools::SelectBox(s, 0, 0, 3, 3).changed);
	EXPECT_FALSE(Tools::ClearSelection(s).changed);
	const Tools::PolyPoint tri[] = { { 0, 0 }, { 5, 0 }, { 0, 5 } };
	EXPECT_FALSE(Tools::SelectPolygon(s, tri, 3).ok);
	const int32_t qr[] = { 0, 0 };
	EXPECT_FALSE(Tools::SelectCells(s, qr, 1).changed);
	EXPECT_EQ(0u, s.Version());
}

// ---- polygon

TEST(SelectionExhaustiveTools, PolygonNullVerticesWithCountIsErrorNotCrash)
{
	Bound b(66, 42);
	Tools::SelectCell(b.sel, 1, 1);
	const uint32_t v = b.sel.Version();
	const auto r = Tools::SelectPolygon(b.sel, nullptr, 5);
	EXPECT_FALSE(r.ok);
	EXPECT_EQ(1u, r.count);
	EXPECT_EQ(v, b.sel.Version());
}

TEST(SelectionExhaustiveTools, PolygonNeedsAtLeastThreeVerticesBoundary)
{
	Bound b(66, 42, 10.0f);
	const Tools::PolyPoint pts[] = { { -50, -50 }, { 5000, -50 }, { 5000, 5000 } };
	EXPECT_FALSE(Tools::SelectPolygon(b.sel, pts, 0).ok);
	EXPECT_FALSE(Tools::SelectPolygon(b.sel, pts, 1).ok);
	EXPECT_FALSE(Tools::SelectPolygon(b.sel, pts, 2).ok);
	EXPECT_TRUE(Tools::SelectPolygon(b.sel, pts, 3).ok);
}

TEST(SelectionExhaustiveTools, PolygonBadGridCellSizeIsErrorAndUnchanged)
{
	const float sizes[] = { 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN() };
	const Tools::PolyPoint tri[] = { { -50, -50 }, { 5000, -50 }, { 5000, 5000 } };
	for (float size : sizes)
	{
		Bound b(8, 8, size);
		const auto r = Tools::SelectPolygon(b.sel, tri, 3);
		EXPECT_FALSE(r.ok);
		EXPECT_EQ(0u, b.sel.Version());
	}
}

TEST(SelectionExhaustiveTools, PolygonInfiniteOrNaNVertexAnywhereIsErrorAndUnchanged)
{
	Bound b(66, 42, 10.0f);
	Tools::SelectCell(b.sel, 2, 2);
	const uint32_t v = b.sel.Version();
	const float inf = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	for (int slot = 0; slot < 4; ++slot)
	{
		Tools::PolyPoint pts[4] = { { 0, 0 }, { 500, 0 }, { 500, 500 }, { 0, 500 } };
		pts[slot].x = (slot & 1) ? inf : nan;
		EXPECT_FALSE(Tools::SelectPolygon(b.sel, pts, 4).ok) << slot;
		pts[slot].x = 0;
		pts[slot].y = (slot & 1) ? nan : -inf;
		EXPECT_FALSE(Tools::SelectPolygon(b.sel, pts, 4).ok) << slot;
	}
	EXPECT_EQ(v, b.sel.Version());
	EXPECT_EQ(1u, b.sel.Count());
}

TEST(SelectionExhaustiveTools, DegeneratePolygonsSelectNothingButReplaceThePriorSelection)
{
	Bound b(66, 42, 10.0f);
	const Tools::PolyPoint collinear[] = { { 0, 0 }, { 100, 100 }, { 200, 200 } };
	const Tools::PolyPoint identical[] = { { 55, 55 }, { 55, 55 }, { 55, 55 } };
	const Tools::PolyPoint horizontal[] = { { 0, 20 }, { 100, 20 }, { 200, 20 } };
	for (const auto* poly : { collinear, identical, horizontal })
	{
		Tools::SelectCell(b.sel, 3, 3);
		const auto r = Tools::SelectPolygon(b.sel, poly, 3);
		EXPECT_TRUE(r.ok);
		EXPECT_TRUE(r.changed);
		EXPECT_EQ(0u, r.count);
		EXPECT_EQ(0u, b.sel.Count());
	}
}

TEST(SelectionExhaustiveTools, PolygonWindingDirectionAndDuplicateVerticesDoNotChangeTheSet)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h, 10.0f);
		const Tools::PolyPoint cw[] = { { 40, 40 }, { 160, 40 }, { 160, 160 }, { 40, 160 } };
		const Tools::PolyPoint ccw[] = { { 40, 40 }, { 40, 160 }, { 160, 160 }, { 160, 40 } };
		const Tools::PolyPoint dup[] = { { 40, 40 }, { 40, 40 }, { 160, 40 }, { 160, 40 }, { 160, 160 }, { 40, 160 }, { 40, 160 } };
		Tools::SelectPolygon(b.sel, cw, 4);
		const auto expected = ToVector(b.sel);
		ASSERT_FALSE(expected.empty());
		EXPECT_FALSE(Tools::SelectPolygon(b.sel, ccw, 4).changed);
		EXPECT_EQ(expected, ToVector(b.sel));
		EXPECT_FALSE(Tools::SelectPolygon(b.sel, dup, 7).changed);
		EXPECT_EQ(expected, ToVector(b.sel));
	}
}

TEST(SelectionExhaustiveTools, PolygonAroundOneCellCentreSelectsExactlyThatCell)
{
	Bound b(66, 42, 10.0f);
	const auto c = HexTopology::CellCenter(CellCoord{ 4, 5 }, 10.0f);
	const Tools::PolyPoint tri[] = { { c.X() - 3, c.Y() - 3 }, { c.X() + 4, c.Y() - 3 }, { c.X(), c.Y() + 4 } };
	const auto r = Tools::SelectPolygon(b.sel, tri, 3);
	EXPECT_TRUE(r.ok);
	EXPECT_EQ(std::vector<uint32_t>{ b.Idx(4, 5) }, ToVector(b.sel));
}

TEST(SelectionExhaustiveTools, PolygonCoveringWholeGridSelectsEveryCellOnBothSizes)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h, 10.0f);
		const Tools::PolyPoint big[] = { { -1000, -1000 }, { 5000, -1000 }, { 5000, 5000 }, { -1000, 5000 } };
		const auto r = Tools::SelectPolygon(b.sel, big, 4);
		EXPECT_TRUE(r.ok);
		EXPECT_EQ(uint32_t(d.w) * d.h, r.count);
		EXPECT_FALSE(Tools::SelectPolygon(b.sel, big, 4).changed);
	}
}

TEST(SelectionExhaustiveTools, PolygonVeryLargeCoordinatesDoNotHang)
{
	Bound b(66, 42, 10.0f);
	const Tools::PolyPoint huge[] = { { -1e30f, -1e30f }, { 1e30f, -1e30f }, { 1e30f, 1e30f }, { -1e30f, 1e30f } };
	const auto r = Tools::SelectPolygon(b.sel, huge, 4);
	EXPECT_TRUE(r.ok);
	EXPECT_EQ(66u * 42u, r.count);
}

TEST(SelectionExhaustiveTools, PolygonWithManyVerticesApproximatingACircle)
{
	Bound b(104, 64, 10.0f);
	const auto c = HexTopology::CellCenter(CellCoord{ 50, 30 }, 10.0f);
	std::vector<Tools::PolyPoint> ring;
	for (int i = 0; i < 1000; ++i)
	{
		const float a = float(i) * 6.2831853f / 1000.0f;
		ring.push_back({ c.X() + 25.0f * cosf(a), c.Y() + 25.0f * sinf(a) });
	}
	const auto r = Tools::SelectPolygon(b.sel, ring.data(), 1000);
	EXPECT_TRUE(r.ok);
	EXPECT_TRUE(b.sel.Contains(b.Idx(50, 30)));
	EXPECT_GT(r.count, 1u);
	EXPECT_LT(r.count, 40u);
}

// ---- cell list

TEST(SelectionExhaustiveTools, CellsNullListSemantics)
{
	Bound b(66, 42);
	Tools::SelectCell(b.sel, 1, 1);
	const uint32_t v = b.sel.Version();
	const auto bad = Tools::SelectCells(b.sel, nullptr, 3);
	EXPECT_FALSE(bad.ok);
	EXPECT_EQ(3u, bad.requested);
	EXPECT_EQ(1u, bad.count);
	EXPECT_EQ(v, b.sel.Version());
	const auto empty = Tools::SelectCells(b.sel, nullptr, 0);
	EXPECT_TRUE(empty.ok);
	EXPECT_TRUE(empty.changed);
	EXPECT_EQ(0u, empty.count);
}

TEST(SelectionExhaustiveTools, CellsExtremeAndBoundaryCoordinates)
{
	for (const Dim& d : kDims)
	{
		Bound b(d.w, d.h);
		const int32_t qr[] = { 0, 0, d.w - 1, d.h - 1, d.w, 0, 0, d.h, -1, -1, kIntMin, kIntMin, kIntMax, kIntMax, 0, kIntMax };
		uint32_t acc[16] = {};
		const auto r = Tools::SelectCells(b.sel, qr, 8, acc, 8);
		EXPECT_TRUE(r.ok);
		EXPECT_EQ(8u, r.requested);
		EXPECT_EQ(2u, r.accepted);
		EXPECT_FALSE(r.overCap);
		EXPECT_EQ(2u, r.count);
		EXPECT_EQ(0u, acc[0]); EXPECT_EQ(0u, acc[1]);
		EXPECT_EQ(uint32_t(d.w - 1), acc[2]); EXPECT_EQ(uint32_t(d.h - 1), acc[3]);
	}
}

TEST(SelectionExhaustiveTools, CellsExactlyAtCapIsNotOverCapAndOneMoreIs)
{
	Bound b(104, 64);
	std::vector<int32_t> qr;
	for (uint32_t i = 0; i < Tools::kMaxCellsPerCall + 1; ++i)
	{
		qr.push_back(int32_t(i % 100));
		qr.push_back(int32_t(i / 100));
	}
	const auto atCap = Tools::SelectCells(b.sel, qr.data(), Tools::kMaxCellsPerCall);
	EXPECT_FALSE(atCap.overCap);
	EXPECT_EQ(Tools::kMaxCellsPerCall, atCap.accepted);
	EXPECT_EQ(Tools::kMaxCellsPerCall, b.sel.Count());

	const auto over = Tools::SelectCells(b.sel, qr.data(), Tools::kMaxCellsPerCall + 1);
	EXPECT_TRUE(over.overCap);
	EXPECT_EQ(Tools::kMaxCellsPerCall + 1, over.requested);
	EXPECT_EQ(Tools::kMaxCellsPerCall, over.accepted);
	EXPECT_EQ(Tools::kMaxCellsPerCall, b.sel.Count());
	EXPECT_FALSE(over.changed);		// the extra pair is ignored, the set is identical
	EXPECT_FALSE(b.sel.Contains(uint32_t(Tools::kMaxCellsPerCall / 100) * 104 + Tools::kMaxCellsPerCall % 100));
}

TEST(SelectionExhaustiveTools, CellsDuplicatesStillCountTowardsTheCap)
{
	Bound b(104, 64);
	std::vector<int32_t> qr;
	for (uint32_t i = 0; i < Tools::kMaxCellsPerCall; ++i) { qr.push_back(1); qr.push_back(1); }
	qr.push_back(2);	// pair number 2049: beyond the cap, ignored
	qr.push_back(2);
	const auto r = Tools::SelectCells(b.sel, qr.data(), Tools::kMaxCellsPerCall + 1);
	EXPECT_TRUE(r.overCap);
	EXPECT_EQ(1u, r.accepted);
	EXPECT_EQ(1u, b.sel.Count());
	EXPECT_FALSE(b.sel.Contains(b.Idx(2, 2)));
}

TEST(SelectionExhaustiveTools, CellsSameListTwiceAndReorderedIsNoChange)
{
	Bound b(66, 42);
	const int32_t a[] = { 1, 1, 5, 5, 9, 9 };
	const int32_t reordered[] = { 9, 9, 1, 1, 5, 5 };
	EXPECT_TRUE(Tools::SelectCells(b.sel, a, 3).changed);
	const uint32_t v = b.sel.Version();
	EXPECT_FALSE(Tools::SelectCells(b.sel, a, 3).changed);
	EXPECT_FALSE(Tools::SelectCells(b.sel, reordered, 3).changed);
	EXPECT_EQ(v, b.sel.Version());
}

TEST(SelectionExhaustiveTools, CellsAcceptedBufferNullAndZeroCapacityStillCount)
{
	Bound b(66, 42);
	const int32_t qr[] = { 1, 1, 2, 2, 3, 3 };
	EXPECT_EQ(3u, Tools::SelectCells(b.sel, qr, 3, nullptr, 0).accepted);
	uint32_t guard[2] = { 0xAAAAAAAAu, 0xBBBBBBBBu };
	EXPECT_EQ(3u, Tools::SelectCells(b.sel, qr, 3, guard, 0).accepted);
	EXPECT_EQ(0xAAAAAAAAu, guard[0]);	// capacity 0: nothing written
	EXPECT_EQ(0xBBBBBBBBu, guard[1]);
}

TEST(SelectionExhaustiveTools, CellsAllInvalidListClearsPriorSelection)
{
	Bound b(66, 42);
	Tools::SelectCell(b.sel, 1, 1);
	const int32_t qr[] = { -1, 0, 66, 0 };
	const auto r = Tools::SelectCells(b.sel, qr, 2);
	EXPECT_TRUE(r.ok);
	EXPECT_TRUE(r.changed);
	EXPECT_EQ(0u, r.accepted);
	EXPECT_EQ(0u, b.sel.Count());
}

// ---- category

namespace
{
	struct CategoryRig
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc grid = MakeGrid(8, 4);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider provider;
		CellSelection sel;

		// enum ids: "3" (index 0), "0" (index 1), "grass" (index 2)
		CategoryRig()
		{
			FakePuId() = 1;
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "numeric");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			t.mEnumValues.Reserve(3);
			for (const char* id : { "3", "0", "grass" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			fx.Add(t);
			service = &fx.Publish();
			provider.values.assign(32, 0.0f);
			CellLayerDesc d = MakeCountingLayer("layer", &grid, provider, CellValueKind::Category);
			d.schemaKey = StringCRC("cells/numeric");
			registry.Register(d);
			sel.Reset(&grid);
		}
		Tools::CategoryResult Run(const char* value, const Dia::ValueSchema::ValueSchemaService* schema)
		{
			LayerReader reader(registry);
			return Tools::SelectCategory(sel, reader, registry, schema, StringCRC("layer"), value);
		}
	};
}

TEST(SelectionExhaustiveTools, CategoryEnumIdWinsOverNumericInterpretation)
{
	CategoryRig rig;
	for (uint32_t i = 0; i < 32; ++i) { rig.provider.values[i] = float(i % 3); }	// indices 0,1,2
	// "3" is an enum id (index 0): wins over the number 3.
	auto r = rig.Run("3", rig.service);
	EXPECT_TRUE(r.ok);
	EXPECT_EQ(11u, r.count);	// cells with value 0: i%3==0 over 0..31
	// "0" is an enum id (index 1): selects value-1 cells, not value-0 cells.
	r = rig.Run("0", rig.service);
	EXPECT_EQ(11u, r.count);
	EXPECT_TRUE(rig.sel.Contains(1));
	EXPECT_FALSE(rig.sel.Contains(0));
	// without a schema the same string is just a number
	r = rig.Run("0", nullptr);
	EXPECT_TRUE(rig.sel.Contains(0));
	EXPECT_FALSE(rig.sel.Contains(1));
}

TEST(SelectionExhaustiveTools, CategoryValueStringParsing)
{
	CategoryRig rig;
	for (uint32_t i = 0; i < 32; ++i) { rig.provider.values[i] = float(i % 4); }
	struct Case { const char* value; bool ok; Tools::CategoryError err; };
	const Case cases[] = {
		{ nullptr, false, Tools::CategoryError::UnknownValue },
		{ "", false, Tools::CategoryError::UnknownValue },
		{ "2abc", false, Tools::CategoryError::UnknownValue },
		{ "1 ", false, Tools::CategoryError::UnknownValue },
		{ "nan", false, Tools::CategoryError::UnknownValue },
		{ "inf", false, Tools::CategoryError::UnknownValue },
		{ "-inf", false, Tools::CategoryError::UnknownValue },
		{ "1e999", false, Tools::CategoryError::UnknownValue },
		{ "GRASS", false, Tools::CategoryError::UnknownValue },
		{ "2", true, Tools::CategoryError::None },
		{ "2.0", true, Tools::CategoryError::None },
		{ "2e0", true, Tools::CategoryError::None },
		{ "+2", true, Tools::CategoryError::None },
		{ "-0", true, Tools::CategoryError::None },
	};
	for (const Case& c : cases)
	{
		rig.sel.Clear();
		rig.sel.Set(31, true);
		const uint32_t v = rig.sel.Version();
		const auto r = rig.Run(c.value, nullptr);
		EXPECT_EQ(c.ok, r.ok) << (c.value ? c.value : "(null)");
		EXPECT_EQ(c.err, r.error) << (c.value ? c.value : "(null)");
		if (!c.ok)
		{
			EXPECT_EQ(v, rig.sel.Version()) << (c.value ? c.value : "(null)");
			EXPECT_TRUE(rig.sel.Contains(31));
		}
	}
}

TEST(SelectionExhaustiveTools, CategoryCellValuesRoundToNearestIntegerLikeTheViewerHover)
{
	CategoryRig rig;
	rig.provider.values.assign(32, 0.0f);
	const float vals[] = { 0.49f, 0.5f, 1.49f, -0.5f, -0.51f, 1.5f };
	for (int i = 0; i < 6; ++i) { rig.provider.values[i] = vals[i]; }
	rig.provider.values[6] = std::numeric_limits<float>::infinity();
	rig.provider.values[7] = -std::numeric_limits<float>::infinity();
	rig.Run("1", nullptr);
	// floor(v + 0.5): 0.5 -> 1, 1.49 -> 1
	EXPECT_EQ((std::vector<uint32_t>{ 1, 2 }), ToVector(rig.sel));
	rig.Run("-1", nullptr);
	EXPECT_EQ((std::vector<uint32_t>{ 4 }), ToVector(rig.sel));
	rig.Run("0", nullptr);
	EXPECT_TRUE(rig.sel.Contains(0));
	EXPECT_TRUE(rig.sel.Contains(3));		// -0.5 -> floor(0.0) = 0
	EXPECT_FALSE(rig.sel.Contains(6));		// +inf never matches a finite target
	EXPECT_FALSE(rig.sel.Contains(7));
}

TEST(SelectionExhaustiveTools, CategoryErrorsLeaveSelectionAndVersionUntouched)
{
	CategoryRig rig;
	rig.sel.Set(5, true);
	const uint32_t v = rig.sel.Version();
	LayerReader reader(rig.registry);
	EXPECT_EQ(Tools::CategoryError::UnknownLayer, Tools::SelectCategory(rig.sel, reader, rig.registry, rig.service, StringCRC("nope"), "1").error);
	EXPECT_EQ(Tools::CategoryError::UnknownLayer, Tools::SelectCategory(rig.sel, reader, rig.registry, rig.service, StringCRC::kZero, "1").error);
	// selection bound to a different-sized grid than the layer's
	Dia::Graphics::CellGridDesc other = MakeGrid(3, 3);
	CellSelection wrong;
	wrong.Reset(&other);
	wrong.Set(1, true);
	const auto r = Tools::SelectCategory(wrong, reader, rig.registry, rig.service, StringCRC("layer"), "0");
	EXPECT_EQ(Tools::CategoryError::GridMismatch, r.error);
	EXPECT_FALSE(r.ok);
	EXPECT_EQ(1u, r.count);
	// unbound selection
	CellSelection unbound;
	EXPECT_EQ(Tools::CategoryError::GridMismatch, Tools::SelectCategory(unbound, reader, rig.registry, rig.service, StringCRC("layer"), "0").error);
	EXPECT_EQ(v, rig.sel.Version());
	EXPECT_TRUE(rig.sel.Contains(5));
}

TEST(SelectionExhaustiveTools, CategoryRepeatedSelectionIsIdempotentAndUsesTheCache)
{
	CategoryRig rig;
	for (uint32_t i = 0; i < 32; ++i) { rig.provider.values[i] = float(i % 2); }
	LayerReader reader(rig.registry);
	const auto first = Tools::SelectCategory(rig.sel, reader, rig.registry, nullptr, StringCRC("layer"), "1");
	const uint32_t v = rig.sel.Version();
	const auto second = Tools::SelectCategory(rig.sel, reader, rig.registry, nullptr, StringCRC("layer"), "1");
	EXPECT_TRUE(first.changed);
	EXPECT_FALSE(second.changed);
	EXPECT_EQ(v, rig.sel.Version());
	EXPECT_EQ(1u, reader.Reads());
}

// ---------------------------------------------------------------- SelectionOutline

namespace
{
	struct OutlineRig
	{
		Dia::Graphics::CellGridDesc grid;
		CellSelection sel;
		SelectionOutline outline;
		explicit OutlineRig(uint16_t w, uint16_t h, float size = 10.0f) : grid(MakeGrid(w, h, size)) { sel.Reset(&grid); }
	};
}

TEST(SelectionExhaustiveOutline, FreshOutlineIsEmptyAndDrawsNothing)
{
	SelectionOutline o;
	EXPECT_EQ(0u, o.EdgeCount());
	EXPECT_FALSE(o.TooLarge());
	EXPECT_EQ(0u, o.Rebuilds());
	FakeDebugDraw draw;
	o.Draw(draw);
	EXPECT_TRUE(draw.lines.empty());
}

TEST(SelectionExhaustiveOutline, CellsAtLastIndexAndAllFourGridCornersHaveSixEdgesEach)
{
	for (const Dim& d : kDims)
	{
		OutlineRig f(d.w, d.h);
		const int cells[][2] = { { 0, 0 }, { d.w - 1, 0 }, { 0, d.h - 1 }, { d.w - 1, d.h - 1 } };
		for (const auto& c : cells)
		{
			f.sel.Clear();
			f.sel.Set(uint32_t(c[1]) * d.w + uint32_t(c[0]), true);
			f.outline.Update(f.sel);
			EXPECT_EQ(6u, f.outline.EdgeCount()) << c[0] << "," << c[1];
		}
	}
}

TEST(SelectionExhaustiveOutline, ColumnOfNCellsHasFourNPlusTwoEdges)
{
	OutlineRig f(66, 42);
	for (uint32_t n = 1; n <= 8; ++n)
	{
		f.sel.Clear();
		for (uint32_t r = 0; r < n; ++r) { f.sel.Set(r * 66 + 10, true); }
		f.outline.Update(f.sel);
		EXPECT_EQ(4 * n + 2, f.outline.EdgeCount()) << n;
	}
}

TEST(SelectionExhaustiveOutline, FullSmallGridPerimeterIsBelowCapAndMatchesBorderCount)
{
	OutlineRig f(66, 42);
	for (uint32_t i = 0; i < 66 * 42; ++i) { f.sel.Set(i, true); }
	f.outline.Update(f.sel);
	EXPECT_FALSE(f.outline.TooLarge());
	EXPECT_GT(f.outline.EdgeCount(), 0u);
	EXPECT_LE(f.outline.EdgeCount(), SelectionOutline::kMaxEdges);
	FakeDebugDraw draw;
	f.outline.Draw(draw);
	EXPECT_EQ(f.outline.EdgeCount(), draw.lines.size());
}

TEST(SelectionExhaustiveOutline, ClearedSelectionOnAlreadyBuiltOutlineRebuildsToEmpty)
{
	OutlineRig f(66, 42);
	f.sel.Set(100, true);
	f.outline.Update(f.sel);
	ASSERT_EQ(6u, f.outline.EdgeCount());
	f.sel.Clear();
	f.outline.Update(f.sel);
	EXPECT_EQ(0u, f.outline.EdgeCount());
	EXPECT_EQ(0u, f.outline.SelectedCountForBanner());
	EXPECT_FALSE(f.outline.TooLarge());
	FakeDebugDraw draw;
	f.outline.Draw(draw);
	EXPECT_TRUE(draw.lines.empty());
}

TEST(SelectionExhaustiveOutline, TooLargeThenShrinkingRecoversAndBannerCountTracksSelection)
{
	OutlineRig f(104, 64);
	// a checkerboard has no shared sides, so 6 edges per cell: 700 cells > 4000 edges
	for (uint32_t r = 0; r < 64; ++r)
	{
		for (uint32_t q = 0; q < 104; q += 2) { f.sel.Set(r * 104 + q, true); }
	}
	f.outline.Update(f.sel);
	EXPECT_TRUE(f.outline.TooLarge());
	EXPECT_EQ(0u, f.outline.EdgeCount());
	EXPECT_EQ(f.sel.Count(), f.outline.SelectedCountForBanner());
	f.sel.Clear();
	f.sel.Set(5, true);
	f.outline.Update(f.sel);
	EXPECT_FALSE(f.outline.TooLarge());
	EXPECT_EQ(6u, f.outline.EdgeCount());
	EXPECT_EQ(1u, f.outline.SelectedCountForBanner());
}

TEST(SelectionExhaustiveOutline, ReboundSelectionToZeroSizedGridBuildsNothing)
{
	OutlineRig f(8, 8);
	f.sel.Set(3, true);
	f.outline.Update(f.sel);
	ASSERT_EQ(6u, f.outline.EdgeCount());
	f.sel.Reset(nullptr);
	f.outline.Update(f.sel);
	EXPECT_EQ(0u, f.outline.EdgeCount());
	EXPECT_FALSE(f.outline.TooLarge());
}

TEST(SelectionExhaustiveOutline, DrawUsesTheGivenColour)
{
	OutlineRig f(8, 8);
	f.sel.Set(3, true);
	f.outline.Update(f.sel);
	FakeDebugDraw draw;
	const Dia::Core::RGBA red(255, 0, 0, 255);
	f.outline.Draw(draw, red);
	ASSERT_EQ(6u, draw.lines.size());
	EXPECT_TRUE(draw.lines[0].colour == red);
	FakeDebugDraw def;
	f.outline.Draw(def);
	EXPECT_TRUE(def.lines[0].colour == SelectionOutline::DefaultColour());
}

TEST(SelectionExhaustiveOutline, UpdateIsIdempotentWithoutChange)
{
	OutlineRig f(8, 8);
	f.sel.Set(3, true);
	f.outline.Update(f.sel);
	f.outline.Update(f.sel);
	f.outline.Update(f.sel);
	EXPECT_EQ(1u, f.outline.Rebuilds());
	f.sel.Set(3, true);		// no change
	f.outline.Update(f.sel);
	EXPECT_EQ(1u, f.outline.Rebuilds());
	f.sel.Set(4, true);
	f.outline.Update(f.sel);
	EXPECT_EQ(2u, f.outline.Rebuilds());
}
