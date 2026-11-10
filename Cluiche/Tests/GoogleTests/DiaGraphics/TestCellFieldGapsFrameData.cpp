////////////////////////////////////////////////////////////////////////////////
// TestCellFieldGapsFrameData.cpp
// Task 26 gap-audit tests: CellFieldFrameData + FrameData Copy/Clear - DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>

using namespace Dia::Graphics;
using Dia::Core::CellFieldDrawRequest;

namespace
{
	CellFieldDrawRequest Req(uint16_t index, uint32_t colourMap = 0)
	{
		CellFieldDrawRequest r;
		r.layer.index = index;
		r.layer.generation = 1;
		r.colourMap = colourMap;
		return r;
	}
}

TEST(CellFieldGapsFrameData, DefaultIsEmptyWithZeroDropped)
{
	CellFieldFrameData d;
	EXPECT_EQ(0u, d.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, d.CellFieldDroppedCount());
	FrameData f;
	EXPECT_EQ(0u, f.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, f.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, FirstRequestKeptAndEveryExtraDroppedAndCounted)
{
	CellFieldFrameData d;
	d.RequestDrawCellFieldData(Req(1, 111));
	for (int i = 0; i < 20; ++i) { d.RequestDrawCellFieldData(Req(2, 222)); }
	ASSERT_EQ(1u, d.GetCellFieldFrameData().Size());
	EXPECT_EQ(1u, d.GetCellFieldFrameData()[0].layer.index);	// the first one wins, never overwritten
	EXPECT_EQ(111u, d.GetCellFieldFrameData()[0].colourMap);
	EXPECT_EQ(20u, d.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, StoredByValue)
{
	CellFieldFrameData d;
	CellFieldDrawRequest r = Req(4, 44);
	d.RequestDrawCellFieldData(r);
	r.layer.index = 99;
	r.colourMap = 99;
	EXPECT_EQ(4u, d.GetCellFieldFrameData()[0].layer.index);
	EXPECT_EQ(44u, d.GetCellFieldFrameData()[0].colourMap);
}

TEST(CellFieldGapsFrameData, ClearResetsDroppedCountAndAllowsReuse)
{
	CellFieldFrameData d;
	d.RequestDrawCellFieldData(Req(1));
	d.RequestDrawCellFieldData(Req(2));
	d.RequestDrawCellFieldData(Req(3));
	ASSERT_EQ(2u, d.CellFieldDroppedCount());
	d.ClearCellFieldBuffer();
	EXPECT_EQ(0u, d.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, d.CellFieldDroppedCount());

	d.RequestDrawCellFieldData(Req(7));
	ASSERT_EQ(1u, d.GetCellFieldFrameData().Size());
	EXPECT_EQ(7u, d.GetCellFieldFrameData()[0].layer.index);
	EXPECT_EQ(0u, d.CellFieldDroppedCount());

	d.ClearCellFieldBuffer();
	d.ClearCellFieldBuffer();	// clearing an empty buffer is harmless
	EXPECT_EQ(0u, d.GetCellFieldFrameData().Size());
}

TEST(CellFieldGapsFrameData, CopyCarriesDroppedCount)
{
	CellFieldFrameData src;
	src.RequestDrawCellFieldData(Req(1));
	src.RequestDrawCellFieldData(Req(2));
	src.RequestDrawCellFieldData(Req(3));
	CellFieldFrameData dst;
	dst.CopyCellFieldBuffer(src);
	ASSERT_EQ(1u, dst.GetCellFieldFrameData().Size());
	EXPECT_EQ(1u, dst.GetCellFieldFrameData()[0].layer.index);
	EXPECT_EQ(2u, dst.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, CopyReplacesDestinationContentAndDroppedCount)
{
	CellFieldFrameData dst;
	dst.RequestDrawCellFieldData(Req(9));
	dst.RequestDrawCellFieldData(Req(9));
	dst.RequestDrawCellFieldData(Req(9));
	ASSERT_EQ(2u, dst.CellFieldDroppedCount());

	CellFieldFrameData emptySrc;
	dst.CopyCellFieldBuffer(emptySrc);
	EXPECT_EQ(0u, dst.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, dst.CellFieldDroppedCount());

	CellFieldFrameData src;
	src.RequestDrawCellFieldData(Req(5));
	dst.CopyCellFieldBuffer(src);
	ASSERT_EQ(1u, dst.GetCellFieldFrameData().Size());
	EXPECT_EQ(5u, dst.GetCellFieldFrameData()[0].layer.index);
}

TEST(CellFieldGapsFrameData, CopyOfCopyIsStable)
{
	CellFieldFrameData a, b, c;
	a.RequestDrawCellFieldData(Req(3, 33));
	a.RequestDrawCellFieldData(Req(4));
	b.CopyCellFieldBuffer(a);
	c.CopyCellFieldBuffer(b);
	ASSERT_EQ(1u, c.GetCellFieldFrameData().Size());
	EXPECT_EQ(33u, c.GetCellFieldFrameData()[0].colourMap);
	EXPECT_EQ(1u, c.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, SelfCopyKeepsRequestAndDroppedCount)
{
	CellFieldFrameData d;
	d.RequestDrawCellFieldData(Req(6, 66));
	d.RequestDrawCellFieldData(Req(7));
	d.CopyCellFieldBuffer(d);
	ASSERT_EQ(1u, d.GetCellFieldFrameData().Size());
	EXPECT_EQ(66u, d.GetCellFieldFrameData()[0].colourMap);
	EXPECT_EQ(1u, d.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, FrameDataSelfAssignmentKeepsRequest)
{
	FrameData f;
	f.RequestDrawCellField(Req(6, 66));
	f.SetWindowSize(Dia::Maths::Vector2D(640.0f, 480.0f));
	FrameData& alias = f;
	f = alias;
	ASSERT_EQ(1u, f.GetCellFieldFrameData().Size());
	EXPECT_EQ(66u, f.GetCellFieldFrameData()[0].colourMap);
	EXPECT_FLOAT_EQ(640.0f, f.GetWindowSize().x);
}

TEST(CellFieldGapsFrameData, FrameDataSelfCopyKeepsRequest)
{
	FrameData f;
	f.RequestDrawCellField(Req(8, 88));
	f.Copy(f);
	ASSERT_EQ(1u, f.GetCellFieldFrameData().Size());
	EXPECT_EQ(88u, f.GetCellFieldFrameData()[0].colourMap);
}

TEST(CellFieldGapsFrameData, FrameDataAssignmentCopiesRequestAndDroppedCount)
{
	FrameData src;
	src.RequestDrawCellField(Req(2, 22));
	src.RequestDrawCellField(Req(3));
	FrameData dst;
	dst = src;
	ASSERT_EQ(1u, dst.GetCellFieldFrameData().Size());
	EXPECT_EQ(22u, dst.GetCellFieldFrameData()[0].colourMap);
	EXPECT_EQ(1u, dst.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, FrameDataClearThenReuse)
{
	FrameData f;
	f.RequestDrawCellField(Req(1));
	f.RequestDrawCellField(Req(2));
	f.Clear();
	EXPECT_EQ(0u, f.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, f.CellFieldDroppedCount());
	f.RequestDrawCellField(Req(3));
	ASSERT_EQ(1u, f.GetCellFieldFrameData().Size());
	EXPECT_EQ(3u, f.GetCellFieldFrameData()[0].layer.index);
	EXPECT_EQ(0u, f.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, FrameDataCopyOfEmptyClearsDestinationRequests)
{
	FrameData dst;
	dst.RequestDrawCellField(Req(1));
	FrameData empty;
	dst.Copy(empty);
	EXPECT_EQ(0u, dst.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, dst.CellFieldDroppedCount());
}

TEST(CellFieldGapsFrameData, CapacityConstantsMatchStep1Contract)
{
	EXPECT_EQ(1u, CellFieldFrameData::kStep1Limit);
	EXPECT_GE(CellFieldFrameData::kCapacity, CellFieldFrameData::kStep1Limit);
}
