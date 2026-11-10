#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>

#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
namespace Tools = Dia::CellLayers::SelectionTools;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	std::vector<uint32_t> Rect(uint32_t w, uint32_t q0, uint32_t r0, uint32_t q1, uint32_t r1)
	{
		std::vector<uint32_t> v;
		for (uint32_t r = r0; r <= r1; ++r)
			for (uint32_t q = q0; q <= q1; ++q)
				v.push_back(r * w + q);
		return v;
	}
}

TEST(SelectionToolsTest, SelectCellReplacesPriorSelection)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		s.Set(1, true); s.Set(2, true); s.Set(300, true);
		uint32_t v = s.Version();
		auto res = Tools::SelectCell(s, 5, 7);
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 1u);
		EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ 7u * d.w + 5u });
		EXPECT_EQ(s.Version(), v + 1);
	}
}

TEST(SelectionToolsTest, SelectCellOnSoleSelectedNoBump)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		Tools::SelectCell(s, 3, 4);
		uint32_t v = s.Version();
		auto res = Tools::SelectCell(s, 3, 4);
		EXPECT_TRUE(res.ok);
		EXPECT_FALSE(res.changed);
		EXPECT_EQ(res.count, 1u);
		EXPECT_EQ(s.Version(), v);
	}
}

TEST(SelectionToolsTest, SelectCellOutOfBoundsIgnored)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		Tools::SelectCell(s, 1, 1);
		uint32_t v = s.Version();
		const int32_t bad[][2] = { { -1, 0 }, { 0, -1 }, { d.w, 0 }, { 0, d.h } };
		for (const auto& b : bad)
		{
			auto res = Tools::SelectCell(s, b[0], b[1]);
			EXPECT_TRUE(res.ok);
			EXPECT_FALSE(res.changed);
			EXPECT_EQ(res.count, 1u);
		}
		EXPECT_EQ(s.Version(), v);
		EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ 1u * d.w + 1u });
	}
}

TEST(SelectionToolsTest, ToggleOnOff)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto on = Tools::ToggleCell(s, 2, 3);
		EXPECT_TRUE(on.ok);
		EXPECT_TRUE(on.changed);
		EXPECT_EQ(on.count, 1u);
		EXPECT_TRUE(s.Contains(3u * d.w + 2u));
		EXPECT_EQ(s.Version(), 1u);
		Tools::ToggleCell(s, 4, 4);
		EXPECT_EQ(s.Count(), 2u);
		auto off = Tools::ToggleCell(s, 2, 3);
		EXPECT_TRUE(off.changed);
		EXPECT_EQ(off.count, 1u);
		EXPECT_FALSE(s.Contains(3u * d.w + 2u));
		EXPECT_EQ(s.Version(), 3u);
	}
}

TEST(SelectionToolsTest, ToggleOutOfBoundsUnchanged)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		Tools::ToggleCell(s, 0, 0);
		uint32_t v = s.Version();
		EXPECT_FALSE(Tools::ToggleCell(s, d.w, 0).changed);
		EXPECT_FALSE(Tools::ToggleCell(s, 0, d.h).changed);
		EXPECT_FALSE(Tools::ToggleCell(s, -1, -1).changed);
		EXPECT_TRUE(Tools::ToggleCell(s, -1, -1).ok);
		EXPECT_EQ(s.Count(), 1u);
		EXPECT_EQ(s.Version(), v);
	}
}

TEST(SelectionToolsTest, BoxSelectsInclusiveRectangle)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto res = Tools::SelectBox(s, 2, 3, 4, 5);
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 9u);
		EXPECT_EQ(ToVector(s), Rect(d.w, 2, 3, 4, 5));
	}
}

TEST(SelectionToolsTest, BoxReversedCornersSameSet)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection a, b, c;
		a.Reset(&g); b.Reset(&g); c.Reset(&g);
		Tools::SelectBox(a, 2, 3, 4, 5);
		Tools::SelectBox(b, 4, 5, 2, 3);
		Tools::SelectBox(c, 4, 3, 2, 5);
		EXPECT_EQ(ToVector(a), ToVector(b));
		EXPECT_EQ(ToVector(a), ToVector(c));
		EXPECT_EQ(a.Count(), 9u);
	}
}

TEST(SelectionToolsTest, BoxReplacesPriorSelection)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		Tools::SelectCell(s, 0, 0);
		Tools::SelectBox(s, 10, 10, 11, 11);
		EXPECT_EQ(ToVector(s), Rect(d.w, 10, 10, 11, 11));
		EXPECT_FALSE(s.Contains(0));
	}
}

TEST(SelectionToolsTest, BoxWholeGridAndCornersPinned)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto res = Tools::SelectBox(s, 0, 0, d.w - 1, d.h - 1);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, uint32_t(d.w) * d.h);
		EXPECT_TRUE(s.Contains(0));
		EXPECT_TRUE(s.Contains(uint32_t(d.w) * d.h - 1));

		Tools::SelectBox(s, 0, 0, 0, 0);
		EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ 0u });
		Tools::SelectBox(s, d.w - 1, d.h - 1, d.w - 1, d.h - 1);
		EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ uint32_t(d.w) * d.h - 1 });
	}
}

TEST(SelectionToolsTest, BoxPartlyOutsideIsClamped)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto res = Tools::SelectBox(s, -5, -5, 1, 1);
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(res.count, 4u);
		EXPECT_EQ(ToVector(s), Rect(d.w, 0, 0, 1, 1));

		Tools::SelectBox(s, d.w - 2, d.h - 2, d.w + 10, d.h + 10);
		EXPECT_EQ(ToVector(s), Rect(d.w, d.w - 2u, d.h - 2u, d.w - 1u, d.h - 1u));
	}
}

TEST(SelectionToolsTest, BoxFullyOutsideUnchanged)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		Tools::SelectCell(s, 5, 5);
		uint32_t v = s.Version();
		EXPECT_FALSE(Tools::SelectBox(s, d.w, 0, d.w + 5, 5).changed);
		EXPECT_FALSE(Tools::SelectBox(s, -9, -9, -1, -1).changed);
		EXPECT_FALSE(Tools::SelectBox(s, 0, d.h, 3, d.h + 3).changed);
		EXPECT_TRUE(Tools::SelectBox(s, -9, -9, -1, -1).ok);
		EXPECT_EQ(s.Version(), v);
		EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ 5u * d.w + 5u });
	}
}

TEST(SelectionToolsTest, BoxBumpsVersionExactlyOnce)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto res = Tools::SelectBox(s, 0, 0, 19, 9);	// 200 cells
		EXPECT_EQ(res.count, 200u);
		EXPECT_EQ(s.Version(), 1u);
		// Same box again: no net change, no bump.
		EXPECT_FALSE(Tools::SelectBox(s, 0, 0, 19, 9).changed);
		EXPECT_EQ(s.Version(), 1u);
		EXPECT_FALSE(s.IsEditing());
	}
}

TEST(SelectionToolsTest, ClearBumpsOnlyWhenNonEmpty)
{
	for (const Dim& d : kDims)
	{
		auto g = MakeGrid(d.w, d.h);
		CellSelection s;
		s.Reset(&g);
		auto empty = Tools::ClearSelection(s);
		EXPECT_TRUE(empty.ok);
		EXPECT_FALSE(empty.changed);
		EXPECT_EQ(s.Version(), 0u);
		Tools::SelectBox(s, 0, 0, 9, 9);
		uint32_t v = s.Version();
		auto res = Tools::ClearSelection(s);
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(res.count, 0u);
		EXPECT_EQ(s.Count(), 0u);
		EXPECT_EQ(s.Version(), v + 1);
	}
}

TEST(SelectionToolsTest, NoGridIsNoOp)
{
	CellSelection s;
	for (auto res : { Tools::SelectCell(s, 0, 0), Tools::ToggleCell(s, 0, 0), Tools::SelectBox(s, 0, 0, 3, 3), Tools::ClearSelection(s) })
	{
		EXPECT_TRUE(res.ok);
		EXPECT_FALSE(res.changed);
		EXPECT_EQ(res.count, 0u);
	}
	EXPECT_EQ(s.Version(), 0u);
}
