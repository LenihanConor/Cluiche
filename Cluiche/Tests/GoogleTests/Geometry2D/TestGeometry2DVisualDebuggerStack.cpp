////////////////////////////////////////////////////////////////////////////////
// TestGeometry2DVisualDebuggerStack.cpp
// Tests for ShapeDrawer, SpatialGridDrawer, QuadtreeDrawer, BVHDrawer, HexGridDrawer
// from DiaGeometry2DVisualDebugger.
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#define DIA_DEBUG
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2DVisualDebugger/SpatialGridDrawer.h>
#include <DiaGeometry2DVisualDebugger/QuadtreeDrawer.h>
#include <DiaGeometry2DVisualDebugger/BVHDrawer.h>
#include <DiaGeometry2DVisualDebugger/HexGridDrawer.h>

#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Shapes/Triangle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Spatial/Quadtree.h>
#include <DiaGeometry2D/Spatial/BVH.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Testing/MockVisitors.h>

#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Geometry2DVisualDebugger;
using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using namespace Dia::Debug;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

// ---------------------------------------------------------------------------
// Helper: inspect FrameData via RecordingDebugVisitor
// ---------------------------------------------------------------------------
static RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

// ============================================================================
// ShapeDrawer tests
// ============================================================================

class ShapeDrawerFixture : public ::testing::Test
{
protected:
    Dia::Debug::DebugLayerManager manager;
    ShapeDrawer                   drawer{ manager };
    FrameData                     fd;
};

TEST_F(ShapeDrawerFixture, SubmitCircle_ThenDraw_DrawsCircle)
{
    drawer.SubmitCircle(Circle(1.0f, Vector2D(0.0f, 0.0f)),
                        DebugColourPalette::kActive);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.CircleCount(), 1);
}

TEST_F(ShapeDrawerFixture, SubmitLine_ThenDraw_DrawsLine)
{
    drawer.SubmitLine(Line(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f)),
                      DebugColourPalette::kActive);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 1);
}

TEST_F(ShapeDrawerFixture, SubmitRay_ThenDraw_DrawsRay)
{
    drawer.SubmitRay(Ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f)),
                     5.0f, DebugColourPalette::kGoal);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 1);
}

TEST_F(ShapeDrawerFixture, SubmitAARect_ThenDraw_DrawsRect)
{
    drawer.SubmitAARect(AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)),
                        DebugColourPalette::kWarning);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.RectCount(), 1);
}

TEST_F(ShapeDrawerFixture, SubmitTriangle_ThenDraw_DrawsTriangle)
{
    drawer.SubmitTriangle(Triangle(Vector2D(0.0f, 0.0f),
                                   Vector2D(1.0f, 0.0f),
                                   Vector2D(0.5f, 1.0f)),
                          DebugColourPalette::kHealthy);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.TriangleCount(), 1);
}

TEST_F(ShapeDrawerFixture, SubmitConvexPoly_FourVerts_DrawsFourLines)
{
    Vector2D verts[4] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f },
        { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };
    ConvexPolygon poly(verts, 4);
    drawer.SubmitConvexPoly(poly, DebugColourPalette::kActive);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 4);
}

TEST_F(ShapeDrawerFixture, Draw_ClearsBuffer_NoPrimitivesNextDraw)
{
    drawer.SubmitCircle(Circle(1.0f, Vector2D(0.0f, 0.0f)),
                        DebugColourPalette::kActive);
    drawer.Draw(fd); // first draw consumes the circle

    FrameData fd2;
    drawer.Draw(fd2); // second draw — buffer was cleared — nothing drawn
    auto v = Inspect(fd2);
    EXPECT_EQ(v.TotalCount(), 0);
}

TEST_F(ShapeDrawerFixture, Draw_Disabled_NoPrimitives)
{
    drawer.SetEnabled(false);
    drawer.SubmitCircle(Circle(1.0f, Vector2D(0.0f, 0.0f)),
                        DebugColourPalette::kActive);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.TotalCount(), 0);
}

TEST_F(ShapeDrawerFixture, LayerName_IsGeoShapes)
{
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kGeoShapes);
}

// ============================================================================
// SpatialGridDrawer tests
// ============================================================================

class SpatialGridFixture : public ::testing::Test
{
protected:
    using Grid = SpatialGrid<int>;

    Dia::Debug::DebugLayerManager manager;
    FrameData                     fd;

    // 10x10 world, 10 unit cells -> 1x1 = 1 cell? No: 10/10=1 per axis = 1 cell.
    // Use 100x100 world, 10 unit cells -> 10x10 = 100 cells.
    Grid* grid = nullptr;

    void SetUp() override
    {
        Grid::Def def;
        def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.cellSize    = 10.0f;
        grid = new Grid(def);
    }

    void TearDown() override
    {
        delete grid;
        grid = nullptr;
    }
};

TEST_F(SpatialGridFixture, Draw_SpatialGrid_DrawsCells)
{
    SpatialGridDrawer<int> drawer(*grid, manager);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    // 100x100 world, 10 unit cells = 10x10 = 100 cells, each drawn as a rect
    EXPECT_EQ(v.RectCount(), 100);
}

TEST_F(SpatialGridFixture, Draw_Colour_IsInactive)
{
    // Colour is kInactive — we can verify via InspectingDebugVisitor
    InspectingDebugVisitor iv;
    SpatialGridDrawer<int> drawer(*grid, manager);
    drawer.Draw(fd);
    static_cast<const DebugFrameData&>(fd).AcceptVisitor(iv);
    ASSERT_GT(iv.visitCount, 0);
    EXPECT_EQ(iv.lastPrimitive.type, DebugPrimitiveType::Rect2D);
    EXPECT_EQ(iv.lastPrimitive.rect2D.outlineColour, DebugColourPalette::kInactive);
}

TEST_F(SpatialGridFixture, LayerName_IsGeoSpatialGrid)
{
    SpatialGridDrawer<int> drawer(*grid, manager);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kGeoSpatialGrid);
}

// ============================================================================
// QuadtreeDrawer tests
// ============================================================================

class QuadtreeFixture : public ::testing::Test
{
protected:
    using Tree = Quadtree<int>;

    Dia::Debug::DebugLayerManager manager;
    FrameData                     fd;
    Tree* tree = nullptr;

    void SetUp() override
    {
        Tree::Def def;
        def.worldBounds    = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.splitThreshold = 2;
        def.maxDepth       = 4;
        tree = new Tree(def);
    }

    void TearDown() override
    {
        delete tree;
        tree = nullptr;
    }
};

TEST_F(QuadtreeFixture, Draw_Quadtree_DrawsNodes)
{
    // Insert enough objects to force a split (splitThreshold=2, insert 4)
    tree->Insert(1, AARect(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f)));
    tree->Insert(2, AARect(Vector2D(20.0f, 20.0f), Vector2D(30.0f, 30.0f)));
    tree->Insert(3, AARect(Vector2D(50.0f, 50.0f), Vector2D(60.0f, 60.0f)));
    tree->Insert(4, AARect(Vector2D(70.0f, 70.0f), Vector2D(80.0f, 80.0f)));

    QuadtreeDrawer<int> drawer(*tree, manager);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    // At least the root + 1 level of nodes
    EXPECT_GT(v.RectCount(), 1);
}

// ============================================================================
// BVHDrawer tests
// ============================================================================

class BVHFixture : public ::testing::Test
{
protected:
    using BVHT = BVH<int>;
    using BuildEntry = BVHT::BuildEntry;
    static constexpr unsigned int MaxObj = 64;
    using BVHSmall = BVH<int, MaxObj>;

    Dia::Debug::DebugLayerManager manager;
    FrameData                     fd;
};

TEST_F(BVHFixture, Draw_BVH_NotBuilt_NoPrimitives)
{
    BVHSmall::Def def;
    BVHSmall bvh(def);
    // Not built — no Draw
    BVHDrawer<int, MaxObj> drawer(bvh, manager);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_EQ(v.TotalCount(), 0);
}

TEST_F(BVHFixture, Draw_BVH_Built_DrawsNodes)
{
    BVHSmall::Def def;
    def.maxLeafObjects = 2;
    BVHSmall bvh(def);

    Dia::Core::Containers::DynamicArrayC<BVHSmall::BuildEntry, MaxObj> entries;
    for (int i = 0; i < 8; ++i)
    {
        BVHSmall::BuildEntry e;
        e.object = i;
        const float x = static_cast<float>(i * 10);
        e.bounds = AARect(Vector2D(x, 0.0f), Vector2D(x + 5.0f, 5.0f));
        entries.Add(e);
    }
    bvh.Build(entries);
    ASSERT_TRUE(bvh.IsBuilt());

    BVHDrawer<int, MaxObj> drawer(bvh, manager);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    EXPECT_GT(v.RectCount(), 0);
}

// ============================================================================
// HexGridDrawer tests  (smoke only — iteration requires valid grid)
// ============================================================================

class HexGridFixture : public ::testing::Test
{
protected:
    using HGrid = HexGrid<int>;

    Dia::Debug::DebugLayerManager manager;
    FrameData                     fd;
    HGrid* hgrid = nullptr;

    void SetUp() override
    {
        HGrid::Def def;
        def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.hexRadius   = 10.0f;
        hgrid = new HGrid(def);
    }

    void TearDown() override
    {
        delete hgrid;
        hgrid = nullptr;
    }
};

TEST_F(HexGridFixture, Draw_HexGrid_DrawsLines)
{
    HexGridDrawer<int> drawer(*hgrid, manager);
    drawer.Draw(fd);
    auto v = Inspect(fd);
    // Each valid hex cell contributes 6 line primitives; with non-zero cell count
    // we expect at least 6 lines.
    EXPECT_GT(v.LineCount(), 0);
    EXPECT_EQ(v.LineCount() % 6, 0); // always a multiple of 6
}
