////////////////////////////////////////////////////////////////////////////////
// TestCoord2DDrawers.cpp
// Tests for Coord2D overlay drawers — DiaVisualDebugger module
// Feature spec: docs/specs/features/dia/diavisualdebugger/coord2d-debug-overlay.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Coord2D/Coord2DOriginDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DAxesDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DGridDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DBoundsDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DCursorDrawer.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>

using namespace Dia::Debug;
using namespace Dia::Graphics;

// ============================================================================
// Helpers — count primitives by type
// ============================================================================

static int CountType(const Dia::Graphics::FrameData& frame, Dia::Graphics::DebugPrimitiveType type)
{
    int count = 0;
    for (uint32_t i = 0; i < frame.GetDebugPrimitiveCount(); ++i)
    {
        if (frame.GetDebugPrimitive(i).type == type)
            ++count;
    }
    return count;
}

static int CountText(const Dia::Graphics::FrameData& frame)
{
    return static_cast<int>(frame.GetTextPrimitiveCount());
}

// ============================================================================
// Test fixture — shared setup for all Coord2D drawer tests
// ============================================================================

class Coord2DDrawerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set viewport: camera at (0,0), zoom 1, window 800x600
        mManager.SetViewport(Dia::Graphics::Camera2D(), Dia::Maths::Vector2D(800.0f, 600.0f));
    }

    Dia::Debug::DebugLayerManager mManager;
    Dia::Graphics::FrameData mFrame;
};

// ============================================================================
// Coord2DOriginDrawer
// ============================================================================

TEST_F(Coord2DDrawerTest, OriginDrawer_GetLayerName_ReturnsCoord2DOrigin)
{
    Coord2DOriginDrawer drawer(mManager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kCoord2DOrigin);
}

TEST_F(Coord2DDrawerTest, OriginDrawer_Draw_EmitsLinePrimitives)
{
    Coord2DOriginDrawer drawer(mManager);
    drawer.Draw(mFrame);

    // Origin crosshair draws 2 line arms
    const int lineCount = CountType(mFrame, DebugPrimitiveType::Line2D);
    EXPECT_GE(lineCount, 2);
}

TEST_F(Coord2DDrawerTest, OriginDrawer_Draw_EmitsPointAtOrigin)
{
    Coord2DOriginDrawer drawer(mManager);
    drawer.Draw(mFrame);

    // Dot at origin
    const int pointCount = CountType(mFrame, DebugPrimitiveType::Point2D);
    EXPECT_GE(pointCount, 1);
}

// ============================================================================
// Coord2DAxesDrawer
// ============================================================================

TEST_F(Coord2DDrawerTest, AxesDrawer_GetLayerName_ReturnsCoord2DAxes)
{
    Coord2DAxesDrawer drawer(mManager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kCoord2DAxes);
}

TEST_F(Coord2DDrawerTest, AxesDrawer_Draw_OriginVisible_EmitsTwoLines)
{
    // Default viewport centres on (0,0) — both axes visible
    Coord2DAxesDrawer drawer(mManager);
    drawer.Draw(mFrame);

    const int lineCount = CountType(mFrame, DebugPrimitiveType::Line2D);
    EXPECT_EQ(lineCount, 2);
}

TEST_F(Coord2DDrawerTest, AxesDrawer_Draw_OriginOffScreen_NoLines)
{
    // Move camera far away so origin is outside the viewport
    mManager.SetViewport(
        Dia::Graphics::Camera2D(Dia::Maths::Vector2D(10000.0f, 10000.0f)),
        Dia::Maths::Vector2D(100.0f, 100.0f));

    Coord2DAxesDrawer drawer(mManager);
    drawer.Draw(mFrame);

    EXPECT_EQ(mFrame.GetDebugPrimitiveCount(), 0u);
}

// ============================================================================
// Coord2DGridDrawer
// ============================================================================

TEST_F(Coord2DDrawerTest, GridDrawer_GetLayerName_ReturnsCoord2DGrid)
{
    Coord2DGridDrawer drawer(mManager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kCoord2DGrid);
}

TEST_F(Coord2DDrawerTest, GridDrawer_Draw_EmitsLinesAndText)
{
    Coord2DGridDrawer drawer(mManager);
    drawer.Draw(mFrame);

    const int lineCount = CountType(mFrame, DebugPrimitiveType::Line2D);
    const int textCount = CountText(mFrame);

    EXPECT_GT(lineCount, 0);
    EXPECT_GT(textCount, 0);
}

TEST_F(Coord2DDrawerTest, GridDrawer_Draw_LineBudget_NotExceeded)
{
    Coord2DGridDrawer drawer(mManager);
    drawer.Draw(mFrame);

    // Implementation budget is 200 lines + text; geometry + text < 500
    const uint32_t total = mFrame.GetDebugPrimitiveCount() + mFrame.GetTextPrimitiveCount();
    EXPECT_LT(total, 500u);
}

// ============================================================================
// Coord2DBoundsDrawer
// ============================================================================

TEST_F(Coord2DDrawerTest, BoundsDrawer_GetLayerName_ReturnsCoord2DBounds)
{
    Coord2DBoundsDrawer drawer(mManager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kCoord2DBounds);
}

TEST_F(Coord2DDrawerTest, BoundsDrawer_Draw_EmitsFourTextPrimitives)
{
    Coord2DBoundsDrawer drawer(mManager);
    drawer.Draw(mFrame);

    EXPECT_EQ(CountText(mFrame), 4);
}

// ============================================================================
// Coord2DCursorDrawer
// ============================================================================

TEST_F(Coord2DDrawerTest, CursorDrawer_GetLayerName_ReturnsCoord2DCursor)
{
    Coord2DCursorDrawer drawer(mManager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kCoord2DCursor);
}

TEST_F(Coord2DDrawerTest, CursorDrawer_Draw_NoMouse_NoPrimitives)
{
    // Mouse pixel defaults to (0,0) — drawer treats this as "not set" and early-outs
    Coord2DCursorDrawer drawer(mManager);
    drawer.Draw(mFrame);

    EXPECT_EQ(mFrame.GetDebugPrimitiveCount(), 0u);
}

TEST_F(Coord2DDrawerTest, CursorDrawer_Draw_WithMouse_EmitsLinesAndText)
{
    mFrame.SetMousePixel(Dia::Maths::Vector2D(400.0f, 300.0f));

    Coord2DCursorDrawer drawer(mManager);
    drawer.Draw(mFrame);

    // Expects 2 crosshair lines + 1 text label
    EXPECT_EQ(CountType(mFrame, DebugPrimitiveType::Line2D), 2);
    EXPECT_EQ(CountText(mFrame), 1);
    EXPECT_EQ(mFrame.GetDebugPrimitiveCount(), 2u);
    EXPECT_EQ(mFrame.GetTextPrimitiveCount(),  1u);
}
