////////////////////////////////////////////////////////////////////////////////
// TestCellFieldFrameData.cpp
// Tests for CellField draw request carriage through FrameData — DiaGraphics
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>

using namespace Dia::Graphics;
using Dia::Core::CellFieldDrawRequest;

namespace
{
	CellFieldDrawRequest MakeRequest()
	{
		CellFieldDrawRequest r;
		r.layer.index = 3;
		r.layer.generation = 7;
		r.colourMap = 0xABCD1234u;
		r.rampMin = -2.0f;
		r.rampMax = 5.0f;
		r.inset = 0.25f;
		r.backdropColour = 0x11223344u;
		r.elevationLayer.index = 9;
		r.elevationLayer.generation = 2;
		r.lightAzimuthDeg = 270.0f;
		r.lightElevationDeg = 30.0f;
		r.hillshadeStrength = 0.5f;
		r.elevationScale = 2.0f;
		return r;
	}

	void ExpectEqual(const CellFieldDrawRequest& a, const CellFieldDrawRequest& b)
	{
		EXPECT_EQ(a.layer.index, b.layer.index);
		EXPECT_EQ(a.layer.generation, b.layer.generation);
		EXPECT_EQ(a.colourMap, b.colourMap);
		EXPECT_FLOAT_EQ(a.rampMin, b.rampMin);
		EXPECT_FLOAT_EQ(a.rampMax, b.rampMax);
		EXPECT_FLOAT_EQ(a.inset, b.inset);
		EXPECT_EQ(a.backdropColour, b.backdropColour);
		EXPECT_EQ(a.elevationLayer.index, b.elevationLayer.index);
		EXPECT_EQ(a.elevationLayer.generation, b.elevationLayer.generation);
		EXPECT_FLOAT_EQ(a.lightAzimuthDeg, b.lightAzimuthDeg);
		EXPECT_FLOAT_EQ(a.lightElevationDeg, b.lightElevationDeg);
		EXPECT_FLOAT_EQ(a.hillshadeStrength, b.hillshadeStrength);
		EXPECT_FLOAT_EQ(a.elevationScale, b.elevationScale);
	}
}

TEST(CellFieldFrameData, RequestIsStored)
{
	FrameData frame;
	Dia::Core::IDebugDraw& draw = frame;
	draw.RequestDrawCellField(MakeRequest());

	ASSERT_EQ(1u, frame.GetCellFieldFrameData().Size());
	ExpectEqual(MakeRequest(), frame.GetCellFieldFrameData()[0]);
}

TEST(CellFieldFrameData, CopyPreservesRequest)
{
	FrameData src;
	src.RequestDrawCellField(MakeRequest());

	FrameData dst;
	dst.Copy(src);

	ASSERT_EQ(1u, dst.GetCellFieldFrameData().Size());
	ExpectEqual(MakeRequest(), dst.GetCellFieldFrameData()[0]);
}

TEST(CellFieldFrameData, ClearRemovesRequest)
{
	FrameData frame;
	frame.RequestDrawCellField(MakeRequest());
	frame.Clear();

	EXPECT_EQ(0u, frame.GetCellFieldFrameData().Size());
	EXPECT_EQ(0u, frame.CellFieldDroppedCount());
}

TEST(CellFieldFrameData, ConsumesNoPrimitiveBudget)
{
	FrameData frame;
	frame.RequestDrawCellField(MakeRequest());

	EXPECT_EQ(0u, frame.DroppedCount());
	EXPECT_EQ(0u, frame.CellFieldDroppedCount());
	EXPECT_FALSE(frame.IsOverCapacity());
}

TEST(CellFieldFrameData, RequestBeyondStep1CapIsDroppedAndCounted)
{
	FrameData frame;
	frame.RequestDrawCellField(MakeRequest());
	frame.RequestDrawCellField(MakeRequest());

	EXPECT_EQ(1u, frame.GetCellFieldFrameData().Size());
	EXPECT_EQ(1u, frame.CellFieldDroppedCount());
	EXPECT_EQ(0u, frame.DroppedCount());
}
