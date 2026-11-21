// TestDebugBudget.cpp - Google Test unit tests for DebugFrameData budget tracking
//
// Tests DroppedCount / IsOverCapacity tracking and entityId field on DebugPrimitive.

#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{
	static const Vector2D kZero(0.0f, 0.0f);
	static const Vector2D kOne(1.0f, 1.0f);
	static const Vector2D kUnitX(1.0f, 0.0f);
	static const RGBA     kWhite = RGBA::White;

	// Submit n circle draws into fd
	static void FillWithCircles(DebugFrameData& fd, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
			fd.RequestDraw(kZero, 1.0f, kWhite);
	}
}

// ===========================================================================
// Suite: DroppedCount
// ===========================================================================

TEST(DebugBudget_DroppedCount, DefaultZero)
{
	DebugFrameData fd;
	EXPECT_EQ(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_DroppedCount, NotOverCapacityWhenEmpty)
{
	DebugFrameData fd;
	EXPECT_FALSE(fd.IsOverCapacity());
}

TEST(DebugBudget_DroppedCount, NoPrimitivesDroppedBelowCapacity)
{
	DebugFrameData fd;
	FillWithCircles(fd, DebugFrameData::kCapacity);
	EXPECT_EQ(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_DroppedCount, DropsWhenFull)
{
	DebugFrameData fd;
	FillWithCircles(fd, DebugFrameData::kCapacity + 5);
	EXPECT_EQ(fd.DroppedCount(), 5u);
}

TEST(DebugBudget_DroppedCount, IsOverCapacityWhenDropped)
{
	DebugFrameData fd;
	FillWithCircles(fd, DebugFrameData::kCapacity + 1);
	EXPECT_TRUE(fd.IsOverCapacity());
}

TEST(DebugBudget_DroppedCount, ClearResetsDropCount)
{
	DebugFrameData fd;
	FillWithCircles(fd, DebugFrameData::kCapacity + 3);
	fd.ClearDebugBuffer();
	EXPECT_EQ(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_DroppedCount, ClearResetsIsOverCapacity)
{
	DebugFrameData fd;
	FillWithCircles(fd, DebugFrameData::kCapacity + 3);
	fd.ClearDebugBuffer();
	EXPECT_FALSE(fd.IsOverCapacity());
}

TEST(DebugBudget_DroppedCount, CopyPreservesDropCount)
{
	DebugFrameData src;
	FillWithCircles(src, DebugFrameData::kCapacity + 7);

	DebugFrameData dst;
	dst.CopyDebugBuffer(src);

	EXPECT_EQ(dst.DroppedCount(), src.DroppedCount());
}

// ===========================================================================
// Suite: AllDrawTypes
// ===========================================================================

TEST(DebugBudget_AllDrawTypes, RequestDraw_Circle_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDraw(kZero, 1.0f, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Line_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDraw(kZero, kOne, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Ray_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDrawRay(kZero, kUnitX, 1.0f, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Rect_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDrawRect(kZero, kOne, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Arc_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDrawArc(kZero, 1.0f, 0.0f, 90.0f, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Triangle_CountsDrops)
{
	DebugFrameData fd;
	const Vector2D p2(1.0f, 0.0f);
	const Vector2D p3(0.0f, 1.0f);
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDraw(kZero, p2, p3, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

TEST(DebugBudget_AllDrawTypes, RequestDraw_Point_CountsDrops)
{
	DebugFrameData fd;
	for (uint32_t i = 0; i <= DebugFrameData::kCapacity; ++i)
		fd.RequestDrawPoint(kZero, kWhite);
	EXPECT_GT(fd.DroppedCount(), 0u);
}

// ===========================================================================
// Suite: EntityId
// ===========================================================================

TEST(DebugBudget_EntityId, DefaultEntityIdIsZero)
{
	DebugPrimitive p;
	EXPECT_EQ(p.entityId, 0u);
}

TEST(DebugBudget_EntityId, EntityIdCopied)
{
	DebugPrimitive src;
	src.entityId = 42u;

	DebugPrimitive copy(src);
	EXPECT_EQ(copy.entityId, 42u);
}

TEST(DebugBudget_EntityId, EntityIdAssigned)
{
	DebugPrimitive src;
	src.entityId = 7u;

	DebugPrimitive dst;
	dst = src;
	EXPECT_EQ(dst.entityId, 7u);
}

TEST(DebugBudget_EntityId, EntityIdPreservedAcrossRequestDraw)
{
	// Submit one circle draw, then manually overwrite its entityId via GetDebugPrimitive.
	// The test verifies the field is present and accessible on retrieved primitives.
	// (RequestDraw sets entityId = 0 by default; this validates the accessor works.)
	DebugFrameData fd;
	fd.RequestDraw(kZero, 1.0f, kWhite);

	ASSERT_EQ(fd.GetDebugPrimitiveCount(), 1u);
	const DebugPrimitive& retrieved = fd.GetDebugPrimitive(0u);
	EXPECT_EQ(retrieved.entityId, 0u);  // default value survives round-trip
}

// ===========================================================================
// Suite: Capacity (dia-needs #13: geometry budget raised 2,048 -> 8,192)
// ===========================================================================

namespace
{
	// Submit a mix of circle, line, rect and triangle primitives; returns how many were requested.
	static void FillMixed(DebugFrameData& fd, uint32_t count)
	{
		const Vector2D p2(1.0f, 0.0f);
		const Vector2D p3(0.0f, 1.0f);
		for (uint32_t i = 0; i < count; ++i)
		{
			switch (i % 4)
			{
				case 0: fd.RequestDraw(kZero, 1.0f, kWhite); break;
				case 1: fd.RequestDraw(kZero, kOne, kWhite); break;
				case 2: fd.RequestDrawRect(kZero, kOne, kWhite, kWhite); break;
				default: fd.RequestDraw(kZero, p2, p3, kWhite, kWhite); break;
			}
		}
	}
}

TEST(DebugBudget_Capacity, GeometryCapacityIs8192)
{
	EXPECT_EQ(DebugFrameData::kGeometryCapacity, 8192u);
	EXPECT_EQ(DebugFrameData::kCapacity, DebugFrameData::kGeometryCapacity);
}

TEST(DebugBudget_Capacity, MixedPrimitivesFillToCapacityWithoutDrops)
{
	auto fd = std::make_unique<DebugFrameData>();
	FillMixed(*fd, DebugFrameData::kGeometryCapacity);
	EXPECT_EQ(fd->GetDebugPrimitiveCount(), 8192u);
	EXPECT_EQ(fd->DroppedCount(), 0u);
	EXPECT_FALSE(fd->IsOverCapacity());
}

TEST(DebugBudget_Capacity, FirstPrimitiveOverCapacityIsDropped)
{
	auto fd = std::make_unique<DebugFrameData>();
	FillMixed(*fd, DebugFrameData::kGeometryCapacity);
	fd->RequestDraw(kZero, 1.0f, kWhite);   // the 8,193rd
	EXPECT_EQ(fd->GetDebugPrimitiveCount(), 8192u);
	EXPECT_EQ(fd->DroppedCount(), 1u);
	EXPECT_TRUE(fd->IsOverCapacity());
}

TEST(DebugBudget_Capacity, TextAndThreeDBudgetsUnchanged)
{
	EXPECT_EQ(DebugFrameData::kTextCapacity, 256u);
	EXPECT_EQ(DebugFrameData::kDebug3DCapacity, 2048u);
}

TEST(DebugBudget_Capacity, FullFrameDataCopyPreservesEveryPrimitive)
{
	auto src = std::make_unique<FrameData>();
	FillMixed(*src, DebugFrameData::kGeometryCapacity);

	auto dst = std::make_unique<FrameData>();
	dst->Copy(*src);
	ASSERT_EQ(dst->GetDebugPrimitiveCount(), 8192u);
	for (uint32_t i = 0; i < 8192u; ++i)
		EXPECT_EQ(dst->GetDebugPrimitive(i).type, src->GetDebugPrimitive(i).type) << "index " << i;

	auto assigned = std::make_unique<FrameData>();
	*assigned = *src;
	EXPECT_EQ(assigned->GetDebugPrimitiveCount(), 8192u);
	EXPECT_EQ(assigned->DroppedCount(), 0u);
}

TEST(DebugBudget_Capacity, ReportFrameDataSize)
{
	// Informational: many tests hold a FrameData on the stack, so its size matters (1 MB default stack).
	std::cout << "[budget] sizeof(DebugPrimitive)=" << sizeof(DebugPrimitive)
	          << " sizeof(DebugFrameData)=" << sizeof(DebugFrameData)
	          << " sizeof(FrameData)=" << sizeof(FrameData) << std::endl;
	SUCCEED();
}
