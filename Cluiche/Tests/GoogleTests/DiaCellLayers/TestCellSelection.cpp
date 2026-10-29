#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>

#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;

TEST(CellSelectionTest, EmptyOnConstructAndReset)
{
	CellSelection s;
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.CellCount(), 0u);
	EXPECT_EQ(s.Version(), 0u);
	EXPECT_FALSE(s.Contains(0));
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	EXPECT_EQ(s.Width(), 4u);
	EXPECT_EQ(s.Height(), 3u);
	EXPECT_EQ(s.CellCount(), 12u);
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), 0u);
}

TEST(CellSelectionTest, SetAddsCountsAndBumpsOnce)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	EXPECT_TRUE(s.Set(5, true));
	EXPECT_TRUE(s.Contains(5));
	EXPECT_EQ(s.Count(), 1u);
	EXPECT_EQ(s.Version(), 1u);
	EXPECT_TRUE(s.Set(5, false));
	EXPECT_FALSE(s.Contains(5));
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), 2u);
}

TEST(CellSelectionTest, SetSameValueIsNoChange)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	EXPECT_FALSE(s.Set(2, false));
	EXPECT_EQ(s.Version(), 0u);
	EXPECT_TRUE(s.Set(2, true));
	EXPECT_FALSE(s.Set(2, true));
	EXPECT_EQ(s.Count(), 1u);
	EXPECT_EQ(s.Version(), 1u);
}

TEST(CellSelectionTest, OutOfRangeIgnored)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	EXPECT_FALSE(s.Set(12, true));
	EXPECT_FALSE(s.Set(0xFFFFFFFFu, true));
	EXPECT_FALSE(s.Contains(12));
	EXPECT_FALSE(s.Contains(0xFFFFFFFFu));
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), 0u);
}

TEST(CellSelectionTest, ClearBumpsOnlyIfNonEmpty)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	EXPECT_FALSE(s.Clear());
	EXPECT_EQ(s.Version(), 0u);
	s.Set(1, true);
	s.Set(7, true);
	EXPECT_EQ(s.Version(), 2u);
	EXPECT_TRUE(s.Clear());
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), 3u);
	EXPECT_FALSE(s.Contains(1));
}

TEST(CellSelectionTest, ResetSameGridIsNoOp)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	s.Reset(&g);
	EXPECT_TRUE(s.Contains(3));
	EXPECT_EQ(s.Count(), 1u);
	EXPECT_EQ(s.Version(), v);
}

TEST(CellSelectionTest, ResetDifferentPointerClears)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	auto g2 = MakeGrid(4, 3);
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	s.Reset(&g2);
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), v + 1);
	// Empty selection: switching again does not bump.
	s.Reset(&g);
	EXPECT_EQ(s.Version(), v + 1);
}

TEST(CellSelectionTest, ResetDifferentWidthOrHeightClears)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	g.width = 5;	// same pointer, resized in place
	s.Reset(&g);
	EXPECT_EQ(s.Width(), 5u);
	EXPECT_EQ(s.CellCount(), 15u);
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), v + 1);
	s.Set(14, true);
	g.height = 4;
	s.Reset(&g);
	EXPECT_EQ(s.Height(), 4u);
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.Version(), v + 3);
}

TEST(CellSelectionTest, ResetDifferentTopologyIdClears)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	g.topology = Dia::Core::StringCRC("a");
	s.Reset(&g);
	s.Set(3, true);
	g.topology = Dia::Core::StringCRC("b");
	s.Reset(&g);
	EXPECT_EQ(s.Count(), 0u);
}

TEST(CellSelectionTest, ResetNullClears)
{
	CellSelection s;
	auto g = MakeGrid(4, 3);
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	s.Reset(nullptr);
	EXPECT_EQ(s.Count(), 0u);
	EXPECT_EQ(s.CellCount(), 0u);
	EXPECT_EQ(s.Version(), v + 1);
	EXPECT_FALSE(s.Set(0, true));
}

TEST(CellSelectionTest, ForEachAscending)
{
	CellSelection s;
	auto g = MakeGrid(10, 10);
	s.Reset(&g);
	for (uint32_t i : { 70u, 3u, 99u, 64u, 63u, 0u }) { s.Set(i, true); }
	const std::vector<uint32_t> expected = { 0, 3, 63, 64, 70, 99 };
	EXPECT_EQ(ToVector(s), expected);
}

TEST(CellSelectionTest, EditBatchBumpsOnce)
{
	CellSelection s;
	auto g = MakeGrid(10, 10);
	s.Reset(&g);
	s.BeginEdit();
	for (uint32_t i = 0; i < 20; ++i) { s.Set(i, true); }
	EXPECT_EQ(s.Version(), 0u);
	EXPECT_TRUE(s.EndEdit());
	EXPECT_EQ(s.Version(), 1u);
	EXPECT_EQ(s.Count(), 20u);
}

TEST(CellSelectionTest, EditNetNoChangeDoesNotBump)
{
	CellSelection s;
	auto g = MakeGrid(10, 10);
	s.Reset(&g);
	s.Set(4, true);
	const uint32_t v = s.Version();
	s.BeginEdit();
	s.Clear();
	s.Set(4, true);
	s.Set(8, true);
	s.Set(8, false);
	EXPECT_FALSE(s.EndEdit());
	EXPECT_EQ(s.Version(), v);
	EXPECT_EQ(s.Count(), 1u);
	EXPECT_TRUE(s.Contains(4));
}

TEST(CellSelectionTest, LargeGridLastCell)
{
	CellSelection s;
	auto g = MakeGrid(104, 64);
	s.Reset(&g);
	EXPECT_EQ(s.CellCount(), 6656u);
	EXPECT_TRUE(s.Set(6655, true));
	EXPECT_FALSE(s.Set(6656, true));
	EXPECT_TRUE(s.Contains(6655));
	EXPECT_EQ(s.Count(), 1u);
	EXPECT_EQ(ToVector(s), std::vector<uint32_t>{ 6655 });
}
