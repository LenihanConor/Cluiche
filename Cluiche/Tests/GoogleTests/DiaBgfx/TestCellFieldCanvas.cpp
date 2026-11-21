// TestCellFieldCanvas.cpp - Canvas wiring of the CellField renderer (GPU-free parts) and the
// bgfx::setViewOrder remap for views 4..8.
#include <gtest/gtest.h>

#include <DiaBgfx/Canvas.h>
#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaBgfx/Renderers/CellFieldViewOrder.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>

#include <algorithm>

using namespace Dia::Bgfx;

TEST(CellFieldCanvasViewOrder, RemapCoversViews4To8)
{
    const auto remap = BuildCellFieldViewRemap(4, kCellFieldViewId, 5, 6, 7);
    EXPECT_EQ(kCellFieldViewOrderStart, 4);
    ASSERT_EQ(remap.size(), kCellFieldViewOrderCount);
    const unsigned short expected[5] = { 4, 8, 5, 6, 7 };
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(remap[i], expected[i]);
    EXPECT_TRUE(IsViewOrderPermutation(remap.data(), remap.size(), kCellFieldViewOrderStart));
}

TEST(CellFieldCanvasViewOrder, PermutationCheckRejectsDuplicatesAndOutOfRange)
{
    const unsigned short dup[5] = { 4, 4, 5, 6, 7 };
    EXPECT_FALSE(IsViewOrderPermutation(dup, 5, 4));
    const unsigned short outside[5] = { 3, 8, 5, 6, 7 };   // would disturb Canvas3D's view 3
    EXPECT_FALSE(IsViewOrderPermutation(outside, 5, 4));
}

TEST(CellFieldCanvas, NoRegistryMeansNoRenderer)
{
    Canvas canvas;
    EXPECT_FALSE(canvas.HasCellFieldRenderer());
    CellFieldRendererStats stats;
    stats.drawCalls = 99;
    EXPECT_FALSE(canvas.GetCellFieldRendererStats(stats));
    EXPECT_EQ(stats.drawCalls, 0u);
}

TEST(CellFieldCanvas, RegistryIsInjectedBeforeInit)
{
    Canvas canvas;
    Dia::Graphics::CellFieldRegistry registry;
    EXPECT_EQ(canvas.GetCellFieldRegistry(), nullptr);
    canvas.SetCellFieldRegistry(&registry);
    EXPECT_EQ(canvas.GetCellFieldRegistry(), &registry);
    EXPECT_FALSE(canvas.HasCellFieldRenderer());   // renderer only exists after DeferredInit (GPU)
}

TEST(CellFieldCanvas, ShutdownBeforeInitIsSafe)
{
    Canvas canvas;
    canvas.Shutdown();
    SUCCEED();
}
