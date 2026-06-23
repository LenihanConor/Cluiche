////////////////////////////////////////////////////////////////////////////////
// TestDebugPrimitive3D.cpp
// Tests for DebugFrameData 3D primitive extensions — debug-3d-primitives feature
// Spec: docs/specs/applications/dia/systems/diavisualdebugger/debug-3d-primitives.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGraphics/Misc/RGBA.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ============================================================================
// Suite: DebugPrimitive3DLayout
// Verifies struct sizing and enum values for the 5 new 3D primitive types.
// ============================================================================

TEST(DebugPrimitive3DLayout, StructSizesAreNonZero)
{
    EXPECT_GT(sizeof(DebugPrimitiveLine3D),   0u);
    EXPECT_GT(sizeof(DebugPrimitiveRay3D),    0u);
    EXPECT_GT(sizeof(DebugPrimitiveBox3D),    0u);
    EXPECT_GT(sizeof(DebugPrimitiveSphere3D), 0u);
    EXPECT_GT(sizeof(DebugPrimitiveArrow3D),  0u);
}

TEST(DebugPrimitive3DLayout, UnionFitsAllTypes)
{
    EXPECT_GE(sizeof(DebugPrimitive), sizeof(DebugPrimitiveLine3D));
    EXPECT_GE(sizeof(DebugPrimitive), sizeof(DebugPrimitiveRay3D));
    EXPECT_GE(sizeof(DebugPrimitive), sizeof(DebugPrimitiveBox3D));
    EXPECT_GE(sizeof(DebugPrimitive), sizeof(DebugPrimitiveSphere3D));
    EXPECT_GE(sizeof(DebugPrimitive), sizeof(DebugPrimitiveArrow3D));
}

TEST(DebugPrimitive3DLayout, EnumValuesAreDistinct)
{
    EXPECT_EQ(static_cast<int>(DebugPrimitiveType::Line3D),   7);
    EXPECT_EQ(static_cast<int>(DebugPrimitiveType::Ray3D),    8);
    EXPECT_EQ(static_cast<int>(DebugPrimitiveType::Box3D),    9);
    EXPECT_EQ(static_cast<int>(DebugPrimitiveType::Sphere3D), 10);
    EXPECT_EQ(static_cast<int>(DebugPrimitiveType::Arrow3D),  11);
}

// ============================================================================
// Suite: DebugFrameData3DSlotCounting
// Verifies that each RequestDraw*3D call increments the 3D primitive count
// by exactly 1, and that the 3D budget is independent of the 2D budget.
// ============================================================================

TEST(DebugFrameData3DSlotCounting, Line3DIncrements3DCount)
{
    DebugFrameData data;
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 0u);
    data.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(255, 0, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(data.GetDebugPrimitiveCount(),   0u);  // 2D budget untouched
}

TEST(DebugFrameData3DSlotCounting, Ray3DIncrements3DCount)
{
    DebugFrameData data;
    data.RequestDrawRay3D(Vector3D(0, 0, 0), Vector3D(0, 0, 1), 5.0f, RGBA(0, 255, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);
}

TEST(DebugFrameData3DSlotCounting, Box3DIncrements3DCount)
{
    DebugFrameData data;
    data.RequestDrawBox3D(Vector3D(-1, -1, -1), Vector3D(1, 1, 1), RGBA(0, 0, 255, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);  // one slot, not 24 vertices
}

TEST(DebugFrameData3DSlotCounting, Sphere3DIncrements3DCount)
{
    DebugFrameData data;
    data.RequestDrawSphere3D(Vector3D(0, 0, 0), 1.0f, RGBA(255, 255, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);  // one slot, not 144 vertices
}

TEST(DebugFrameData3DSlotCounting, Arrow3DIncrements3DCount)
{
    DebugFrameData data;
    data.RequestDrawArrow3D(Vector3D(0, 0, 0), Vector3D(0, 1, 0), 2.0f, 0.15f, RGBA(255, 0, 255, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);
}

TEST(DebugFrameData3DSlotCounting, BudgetIndependentOf2DBudget)
{
    DebugFrameData data;
    // Fill 2D budget partially
    data.RequestDraw(Vector2D(0, 0), Vector2D(1, 1), RGBA(255, 0, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 0u);
    // Now add one 3D primitive
    data.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(0, 255, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(data.GetDebugPrimitiveCount(),   1u);
}

TEST(DebugFrameData3DSlotCounting, ClearResetsAll3DPrimitives)
{
    DebugFrameData data;
    data.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(255, 0, 0, 255));
    data.RequestDrawBox3D(Vector3D(-1, -1, -1), Vector3D(1, 1, 1), RGBA(0, 0, 255, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 2u);
    data.ClearDebugBuffer();
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 0u);
    EXPECT_EQ(data.DroppedDebug3DCount(),      0u);
}

// ============================================================================
// Suite: DebugFrameData3DDropped
// Verifies that overflow beyond kDebug3DCapacity increments DroppedDebug3DCount
// and that Is3DOverCapacity() reflects the overflow state.
// ============================================================================

TEST(DebugFrameData3DDropped, DroppedCountStartsZero)
{
    DebugFrameData data;
    EXPECT_EQ(data.DroppedDebug3DCount(), 0u);
    EXPECT_FALSE(data.Is3DOverCapacity());
}

// slow — fills full kDebug3DCapacity budget
TEST(DebugFrameData3DDropped, DroppedCountIncrementsOnOverflow)
{
    DebugFrameData data;
    for (uint32_t i = 0; i <= DebugFrameData::kDebug3DCapacity; ++i)
        data.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(255, 0, 0, 255));
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), DebugFrameData::kDebug3DCapacity);
    EXPECT_EQ(data.DroppedDebug3DCount(),      1u);
    EXPECT_TRUE(data.Is3DOverCapacity());
}
