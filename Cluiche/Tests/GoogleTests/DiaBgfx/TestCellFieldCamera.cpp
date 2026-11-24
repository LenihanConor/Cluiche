// TestCellFieldCamera.cpp - shared Ortho2D helper (golden vs DebugRenderer's original matrix),
// CellField/debug camera parity (AC8) and the CellField view id/order. GPU-free.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaBgfx/Renderers/CellFieldViewOrder.h>
#include <DiaBgfx/Renderers/Ortho2D.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <math.h>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;
using Dia::Maths::Vector2D;

namespace
{
    struct Setting { float px, py, zoom; };
    const Setting kSettings[3] = { { 0.0f, 0.0f, 1.0f }, { 100.0f, 50.0f, 2.0f }, { -50.0f, 25.0f, 0.5f } };

    Camera2D MakeCamera(const Setting& s)
    {
        Camera2D c;
        c.SetPosition(Vector2D(s.px, s.py));
        c.SetZoom(s.zoom);
        return c;
    }

    // Original DebugRenderer::Draw inputs, canvas 800x600, near 0 far 1000, non-homogeneous depth.
    // Expected values derived by hand from bx::mtxOrtho(l=cx-halfW, r=cx+halfW, b=cy+halfH, t=cy-halfH).
    struct Golden { float aa, bb, dd, ee; };
    const Golden kGolden[3] = {
        { 0.0025f, -1.0f / 300.0f, 0.0f, 0.0f },
        { 0.005f, -2.0f / 300.0f, -0.5f, 1.0f / 3.0f },
        { 0.00125f, -1.0f / 600.0f, 0.0625f, 1.0f / 24.0f },
    };
}

TEST(CellFieldCamera, OrthoMatchesOriginalDebugRendererMatrix)
{
    for (int i = 0; i < 3; ++i)
    {
        float m[16];
        BuildOrtho2D(MakeCamera(kSettings[i]), Vector2D(800.0f, 600.0f), m);
        EXPECT_NEAR(m[0], kGolden[i].aa, 1e-6f) << i;
        EXPECT_NEAR(m[5], kGolden[i].bb, 1e-6f) << i;
        EXPECT_NEAR(m[10], 0.001f, 1e-9f) << i;
        EXPECT_NEAR(m[12], kGolden[i].dd, 1e-6f) << i;
        EXPECT_NEAR(m[13], kGolden[i].ee, 1e-6f) << i;
        EXPECT_NEAR(m[14], 0.0f, 1e-9f) << i;
        EXPECT_EQ(m[15], 1.0f) << i;
        for (int k : { 1, 2, 3, 4, 6, 7, 8, 9, 11 })
            EXPECT_EQ(m[k], 0.0f) << i << " idx " << k;
    }
}

TEST(CellFieldCamera, OrthoHomogeneousDepthAndZeroZoom)
{
    float m[16];
    BuildOrtho2D(MakeCamera(kSettings[0]), Vector2D(800.0f, 600.0f), m, true);
    EXPECT_NEAR(m[10], 0.002f, 1e-9f);
    EXPECT_NEAR(m[14], -1.0f, 1e-6f);

    Setting zero = { 0.0f, 0.0f, 0.0f };   // zoom <= 0 behaves as 1
    float z[16], one[16];
    BuildOrtho2D(MakeCamera(zero), Vector2D(800.0f, 600.0f), z);
    BuildOrtho2D(MakeCamera(kSettings[0]), Vector2D(800.0f, 600.0f), one);
    for (int k = 0; k < 16; ++k)
        EXPECT_EQ(z[k], one[k]);
}

TEST(CellFieldCamera, WorldToPixelIsYDownAndCentredOnCamera)
{
    const Vector2D canvas(800.0f, 600.0f);
    const Camera2D cam = MakeCamera(kSettings[1]);
    Vector2D c = WorldToPixel(cam, canvas, Vector2D(100.0f, 50.0f));
    EXPECT_NEAR(c.X(), 400.0f, 0.01f);
    EXPECT_NEAR(c.Y(), 300.0f, 0.01f);
    // +world Y is further down the screen; zoom 2 => 2 px per unit.
    Vector2D p = WorldToPixel(cam, canvas, Vector2D(110.0f, 60.0f));
    EXPECT_NEAR(p.X(), 420.0f, 0.01f);
    EXPECT_NEAR(p.Y(), 320.0f, 0.01f);
}

TEST(CellFieldCamera, CellFieldAndDebugPathsAgreeWithinHalfPixel)
{
    const Vector2D canvas(800.0f, 600.0f);
    const Vector2D cellCentre(123.5f, -42.25f);   // world point shared by a cell centre and a debug circle
    for (int i = 0; i < 3; ++i)
    {
        CellFieldRegistry registry;
        CellFieldRenderer renderer(kCellFieldViewId, nullptr, &registry);
        const Camera2D cam = MakeCamera(kSettings[i]);
        renderer.SetCamera(cam);
        renderer.OnCanvasSizeChanged(canvas);

        float cfMatrix[16];
        renderer.GetViewMatrix(cfMatrix);
        const Vector2D cfPixel = MatrixWorldToPixel(cfMatrix, canvas, cellCentre);
        const Vector2D dbgPixel = WorldToPixel(cam, canvas, cellCentre);   // path DebugRenderer::Draw uses
        EXPECT_NEAR(cfPixel.X(), dbgPixel.X(), 0.5f) << i;
        EXPECT_NEAR(cfPixel.Y(), dbgPixel.Y(), 0.5f) << i;
    }
}

TEST(CellFieldCamera, ViewIdAndOrder)
{
    EXPECT_EQ(kCellFieldViewId, 8);
    const auto order = BuildCellFieldViewOrder(4, kCellFieldViewId, 5, 6, 7);
    ASSERT_EQ(order.size(), 5u);
    EXPECT_EQ(order[0], 4);
    EXPECT_EQ(order[1], 8);
    EXPECT_EQ(order[2], 5);
    EXPECT_EQ(order[3], 6);
    EXPECT_EQ(order[4], 7);
}
