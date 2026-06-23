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

// slow — fills full kDebug3DCapacity budget then clears
TEST(DebugFrameData3DDropped, ClearResetsDroppedCount)
{
    DebugFrameData data;
    for (uint32_t i = 0; i <= DebugFrameData::kDebug3DCapacity; ++i)
        data.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(255, 0, 0, 255));
    ASSERT_TRUE(data.Is3DOverCapacity());

    data.ClearDebugBuffer();

    EXPECT_EQ(data.DroppedDebug3DCount(),      0u);
    EXPECT_FALSE(data.Is3DOverCapacity());
    EXPECT_EQ(data.GetDebug3DPrimitiveCount(), 0u);
}

// ============================================================================
// Suite: DebugPrimitive3DRoundTrip
// Verifies that each RequestDraw*3D call stores the correct type tag and data
// fields — exercises the 5 new union members and operator= cases.
// ============================================================================

TEST(DebugPrimitive3DRoundTrip, Line3DStoresTypeAndFields)
{
    DebugFrameData data;
    data.RequestDrawLine3D(Vector3D(1, 2, 3), Vector3D(4, 5, 6), RGBA(100, 150, 200, 255));
    const DebugPrimitive& p = data.GetDebug3DPrimitive(0);
    EXPECT_EQ(p.type, DebugPrimitiveType::Line3D);
    EXPECT_FLOAT_EQ(p.line3D.from.X(), 1.0f);
    EXPECT_FLOAT_EQ(p.line3D.from.Y(), 2.0f);
    EXPECT_FLOAT_EQ(p.line3D.from.Z(), 3.0f);
    EXPECT_FLOAT_EQ(p.line3D.to.X(),   4.0f);
    EXPECT_FLOAT_EQ(p.line3D.to.Y(),   5.0f);
    EXPECT_FLOAT_EQ(p.line3D.to.Z(),   6.0f);
    EXPECT_EQ(p.line3D.colour.R(), 100u);
}

TEST(DebugPrimitive3DRoundTrip, Ray3DStoresTypeAndFields)
{
    DebugFrameData data;
    data.RequestDrawRay3D(Vector3D(1, 2, 3), Vector3D(0, 0, 1), 7.5f, RGBA(0, 200, 50, 255));
    const DebugPrimitive& p = data.GetDebug3DPrimitive(0);
    EXPECT_EQ(p.type, DebugPrimitiveType::Ray3D);
    EXPECT_FLOAT_EQ(p.ray3D.origin.X(), 1.0f);
    EXPECT_FLOAT_EQ(p.ray3D.direction.Z(), 1.0f);
    EXPECT_FLOAT_EQ(p.ray3D.length, 7.5f);
    EXPECT_EQ(p.ray3D.colour.G(), 200u);
}

TEST(DebugPrimitive3DRoundTrip, Box3DStoresTypeAndFields)
{
    DebugFrameData data;
    data.RequestDrawBox3D(Vector3D(-1, -2, -3), Vector3D(4, 5, 6), RGBA(50, 100, 150, 255));
    const DebugPrimitive& p = data.GetDebug3DPrimitive(0);
    EXPECT_EQ(p.type, DebugPrimitiveType::Box3D);
    EXPECT_FLOAT_EQ(p.box3D.min.X(), -1.0f);
    EXPECT_FLOAT_EQ(p.box3D.min.Y(), -2.0f);
    EXPECT_FLOAT_EQ(p.box3D.min.Z(), -3.0f);
    EXPECT_FLOAT_EQ(p.box3D.max.X(),  4.0f);
    EXPECT_FLOAT_EQ(p.box3D.max.Y(),  5.0f);
    EXPECT_FLOAT_EQ(p.box3D.max.Z(),  6.0f);
    EXPECT_EQ(p.box3D.colour.B(), 150u);
}

TEST(DebugPrimitive3DRoundTrip, Sphere3DStoresTypeAndFields)
{
    DebugFrameData data;
    data.RequestDrawSphere3D(Vector3D(10, 20, 30), 5.5f, RGBA(255, 128, 0, 255));
    const DebugPrimitive& p = data.GetDebug3DPrimitive(0);
    EXPECT_EQ(p.type, DebugPrimitiveType::Sphere3D);
    EXPECT_FLOAT_EQ(p.sphere3D.center.X(), 10.0f);
    EXPECT_FLOAT_EQ(p.sphere3D.center.Y(), 20.0f);
    EXPECT_FLOAT_EQ(p.sphere3D.center.Z(), 30.0f);
    EXPECT_FLOAT_EQ(p.sphere3D.radius,      5.5f);
    EXPECT_EQ(p.sphere3D.colour.R(), 255u);
}

TEST(DebugPrimitive3DRoundTrip, Arrow3DStoresTypeAndFields)
{
    DebugFrameData data;
    data.RequestDrawArrow3D(Vector3D(1, 2, 3), Vector3D(1, 0, 0), 3.0f, 0.2f, RGBA(0, 100, 200, 255));
    const DebugPrimitive& p = data.GetDebug3DPrimitive(0);
    EXPECT_EQ(p.type, DebugPrimitiveType::Arrow3D);
    EXPECT_FLOAT_EQ(p.arrow3D.origin.X(),    1.0f);
    EXPECT_FLOAT_EQ(p.arrow3D.direction.X(), 1.0f);
    EXPECT_FLOAT_EQ(p.arrow3D.length,        3.0f);
    EXPECT_FLOAT_EQ(p.arrow3D.headSize,      0.2f);
    EXPECT_EQ(p.arrow3D.colour.B(), 200u);
}

// ============================================================================
// Suite: DebugFrameData3DCopy
// Verifies CopyDebugBuffer correctly copies the 3D primitive buffer and
// dropped count, and that the source is not affected by the copy.
// ============================================================================

TEST(DebugFrameData3DCopy, CopiesCountAndTypes)
{
    DebugFrameData src;
    src.RequestDrawLine3D(Vector3D(0, 0, 0), Vector3D(1, 0, 0), RGBA(255, 0, 0, 255));
    src.RequestDrawBox3D(Vector3D(-1, -1, -1), Vector3D(1, 1, 1), RGBA(0, 255, 0, 255));

    DebugFrameData dst;
    dst.CopyDebugBuffer(src);

    EXPECT_EQ(dst.GetDebug3DPrimitiveCount(), 2u);
    EXPECT_EQ(dst.GetDebug3DPrimitive(0).type, DebugPrimitiveType::Line3D);
    EXPECT_EQ(dst.GetDebug3DPrimitive(1).type, DebugPrimitiveType::Box3D);
    EXPECT_FLOAT_EQ(dst.GetDebug3DPrimitive(0).line3D.to.X(), 1.0f);
}

TEST(DebugFrameData3DCopy, DoesNotMutateSource)
{
    DebugFrameData src;
    src.RequestDrawSphere3D(Vector3D(5, 5, 5), 2.0f, RGBA(0, 0, 255, 255));

    DebugFrameData dst;
    dst.CopyDebugBuffer(src);

    // Mutate dst — src must be unaffected
    dst.ClearDebugBuffer();
    EXPECT_EQ(src.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dst.GetDebug3DPrimitiveCount(), 0u);
}
