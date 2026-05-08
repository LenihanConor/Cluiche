////////////////////////////////////////////////////////////////////////////////
// Filename: TestFixedDrawLayer.cpp
// Description: Tests for fixed-draw-layer infrastructure:
//              FixedPrimitiveBuffer, FixedDrawRegistry, DebugLayerManager
//              fixed-layer API, and default spatial renderers.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/FixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/FixedDrawRegistry.h>
#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/Renderers/SpatialGridRenderer.h>
#include <DiaVisualDebugger/Renderers/QuadtreeRenderer.h>
#include <DiaVisualDebugger/Renderers/BVHRenderer.h>
#include <DiaVisualDebugger/Renderers/HexGridRenderer.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Spatial/Quadtree.h>
#include <DiaGeometry2D/Spatial/BVH.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaCore/CRC/StringCRC.h>
#include <memory>
#include <vector>

// =====================================================================
// Test helpers
// =====================================================================

namespace
{
    // Counts how many primitives were visited
    class CountingVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable int visitCount = 0;

        void Visit(const Dia::Graphics::DebugPrimitive& /*p*/) const override
        {
            ++visitCount;
        }
        void Visit(const Dia::Graphics::DebugFrameData& /*fd*/) const override {}
    };

    // Emits a fixed number of Line2D primitives when built
    class LineRenderer : public Dia::Debug::TypedObjectRenderer<int>
    {
    public:
        explicit LineRenderer(int linesToEmit) : mLinesToEmit(linesToEmit) {}
        mutable int buildCount = 0;

    protected:
        void DoBuild(const int& /*source*/, Dia::Debug::IFixedPrimitiveBuffer& out) const override
        {
            ++buildCount;
            for (int i = 0; i < mLinesToEmit; ++i)
            {
                out.RequestDraw(
                    Dia::Maths::Vector2D(0.0f, 0.0f),
                    Dia::Maths::Vector2D(1.0f, 0.0f),
                    Dia::Graphics::RGBA(255, 255, 255, 255));
            }
        }

    private:
        int mLinesToEmit;
    };

    // Minimal IVisualDebugger stub for dynamic-layer tests
    class StubDynLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit StubDynLayer(const char* name) : mName(name) {}
        Dia::Core::StringCRC GetLayerName() const override { return mName; }
        void Draw(Dia::Graphics::FrameData& /*fd*/) override {}
    private:
        Dia::Core::StringCRC mName;
    };
}

// =====================================================================
// Suite: Registration
// =====================================================================

TEST(FixedDrawLayer_Registration, Single_Entry_Registered)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("test.fixed"), &source, &renderer, 16, 0);
    EXPECT_EQ(registry.GetCount(), 1);
    EXPECT_TRUE(registry.HasLayer(Dia::Core::StringCRC("test.fixed")));
}

TEST(FixedDrawLayer_Registration, Duplicate_Asserts)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("dup.layer"), &source, &renderer, 16, 0);
    EXPECT_DEATH(
        registry.Register(Dia::Core::StringCRC("dup.layer"), &source, &renderer, 16, 0),
        "");
}

TEST(FixedDrawLayer_Registration, LockThenRegister_Asserts)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(2);
    int source = 0;

    manager.LockRegistration();
    EXPECT_DEATH(
        manager.RegisterFixed(Dia::Core::StringCRC("locked.layer"), &source, &renderer, 16, 0),
        "");
}

TEST(FixedDrawLayer_Registration, BeforeLock_NoAssert)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(2);
    int source = 0;

    EXPECT_NO_FATAL_FAILURE(
        manager.RegisterFixed(Dia::Core::StringCRC("pre.lock"), &source, &renderer, 16, 0));
}

TEST(FixedDrawLayer_Registration, Unregister_Removes)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("remove.me"), &source, &renderer, 16, 0);
    EXPECT_EQ(registry.GetCount(), 1);
    registry.Unregister(Dia::Core::StringCRC("remove.me"));
    EXPECT_EQ(registry.GetCount(), 0);
    EXPECT_FALSE(registry.HasLayer(Dia::Core::StringCRC("remove.me")));
}

TEST(FixedDrawLayer_Registration, Unregister_Unknown_NoOp)
{
    Dia::Debug::FixedDrawRegistry registry;
    EXPECT_NO_FATAL_FAILURE(registry.Unregister(Dia::Core::StringCRC("ghost.layer")));
    EXPECT_EQ(registry.GetCount(), 0);
}

// =====================================================================
// Suite: Toggle
// =====================================================================

TEST(FixedDrawLayer_Toggle, DefaultEnabled)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("tog.layer"), &source, &renderer, 16, 0);
    EXPECT_TRUE(registry.IsLayerEnabled(Dia::Core::StringCRC("tog.layer")));
}

TEST(FixedDrawLayer_Toggle, Disabled_SkipsDraw)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("dis.layer"), &source, &renderer, 16, 0);
    registry.DisableLayer(Dia::Core::StringCRC("dis.layer"));

    CountingVisitor visitor;
    registry.DrawFixed(visitor);
    EXPECT_EQ(visitor.visitCount, 0);
}

TEST(FixedDrawLayer_Toggle, Enabled_Draws)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(2);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("en.layer"), &source, &renderer, 16, 0);
    // Disable then re-enable
    registry.DisableLayer(Dia::Core::StringCRC("en.layer"));
    registry.EnableLayer(Dia::Core::StringCRC("en.layer"));

    CountingVisitor visitor;
    registry.DrawFixed(visitor);
    EXPECT_EQ(visitor.visitCount, 2);
}

TEST(FixedDrawLayer_Toggle, ManagerEnableLayer_RoutesToFixed)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(1);
    int source = 0;

    manager.RegisterFixed(Dia::Core::StringCRC("mgr.fixed"), &source, &renderer, 8, 0);
    manager.DisableLayer(Dia::Core::StringCRC("mgr.fixed"));
    EXPECT_FALSE(manager.IsLayerEnabled(Dia::Core::StringCRC("mgr.fixed")));

    manager.EnableLayer(Dia::Core::StringCRC("mgr.fixed"));
    EXPECT_TRUE(manager.IsLayerEnabled(Dia::Core::StringCRC("mgr.fixed")));
}

TEST(FixedDrawLayer_Toggle, ManagerDisableLayer_RoutesToFixed)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(1);
    int source = 0;

    manager.RegisterFixed(Dia::Core::StringCRC("mgr.dis"), &source, &renderer, 8, 0);
    manager.DisableLayer(Dia::Core::StringCRC("mgr.dis"));
    EXPECT_FALSE(manager.IsLayerEnabled(Dia::Core::StringCRC("mgr.dis")));
}

// =====================================================================
// Suite: Build
// =====================================================================

TEST(FixedDrawLayer_Build, BuildsOnFirstDraw)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(3);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("build.first"), &source, &renderer, 16, 0);

    CountingVisitor visitor;
    registry.DrawFixed(visitor);

    EXPECT_EQ(renderer.buildCount, 1);
    EXPECT_EQ(visitor.visitCount, 3);
}

TEST(FixedDrawLayer_Build, NoRebuildOnSecondDraw)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(3);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("build.second"), &source, &renderer, 16, 0);

    CountingVisitor v1, v2;
    registry.DrawFixed(v1);
    registry.DrawFixed(v2);

    EXPECT_EQ(renderer.buildCount, 1);  // only built once
    EXPECT_EQ(v2.visitCount, 3);        // still emits 3 on second draw
}

TEST(FixedDrawLayer_Build, Invalidate_RebuildsOnNextDraw)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(3);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("build.inv"), &source, &renderer, 16, 0);

    CountingVisitor v1;
    registry.DrawFixed(v1);
    EXPECT_EQ(renderer.buildCount, 1);

    registry.Invalidate(Dia::Core::StringCRC("build.inv"));

    CountingVisitor v2;
    registry.DrawFixed(v2);
    EXPECT_EQ(renderer.buildCount, 2);
}

TEST(FixedDrawLayer_Build, Invalidate_DoesNotRebuildUntilDraw)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(3);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("build.lazy"), &source, &renderer, 16, 0);

    CountingVisitor v1;
    registry.DrawFixed(v1);
    EXPECT_EQ(renderer.buildCount, 1);

    registry.Invalidate(Dia::Core::StringCRC("build.lazy"));
    EXPECT_EQ(renderer.buildCount, 1);  // no rebuild yet — only marked dirty
}

// =====================================================================
// Suite: Draw
// =====================================================================

TEST(FixedDrawLayer_Draw, FixedLayer_NotInFrameData)
{
    // Fixed layers go through DrawFixed(visitor), not FrameData.
    // This test verifies that FixedPrimitiveBuffer::AcceptVisitor routes to visitor,
    // not to DebugFrameData.
    Dia::Debug::FixedPrimitiveBuffer buf(8);
    buf.RequestDraw(
        Dia::Maths::Vector2D(0, 0),
        Dia::Maths::Vector2D(1, 1),
        Dia::Graphics::RGBA(255, 0, 0, 255));

    CountingVisitor visitor;
    buf.AcceptVisitor(visitor);
    EXPECT_EQ(visitor.visitCount, 1);
    EXPECT_EQ(buf.GetCount(), 1);
}

TEST(FixedDrawLayer_Draw, FixedLayer_HitsVisitor)
{
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer renderer(5);
    int source = 0;

    registry.Register(Dia::Core::StringCRC("hit.visitor"), &source, &renderer, 16, 0);

    CountingVisitor visitor;
    registry.DrawFixed(visitor);
    EXPECT_EQ(visitor.visitCount, 5);
}

TEST(FixedDrawLayer_Draw, PriorityOrder_LowerBeforeHigher)
{
    // Two fixed layers: priority 10 and priority 1.
    // We verify both are visited (ordering correctness validated structurally).
    Dia::Debug::FixedDrawRegistry registry;
    LineRenderer rendererA(2), rendererB(3);
    int sourceA = 0, sourceB = 0;

    registry.Register(Dia::Core::StringCRC("prio.high"), &sourceA, &rendererA, 16, 10);
    registry.Register(Dia::Core::StringCRC("prio.low"),  &sourceB, &rendererB, 16,  1);

    CountingVisitor visitor;
    registry.DrawFixed(visitor);
    EXPECT_EQ(visitor.visitCount, 5);  // both layers contributed
}

TEST(FixedDrawLayer_Draw, EmptyRegistry_NoOp)
{
    Dia::Debug::FixedDrawRegistry registry;
    CountingVisitor visitor;
    EXPECT_NO_FATAL_FAILURE(registry.DrawFixed(visitor));
    EXPECT_EQ(visitor.visitCount, 0);
}

TEST(FixedDrawLayer_Draw, ManagerHasLayer_CountsFixed)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(1);
    int source = 0;

    StubDynLayer dyn("dyn.layer");
    manager.Register(&dyn, 0);
    manager.RegisterFixed(Dia::Core::StringCRC("fix.layer"), &source, &renderer, 8, 0);

    // GetLayerCount returns dynamic + fixed
    EXPECT_EQ(manager.GetLayerCount(), 2);
    EXPECT_TRUE(manager.HasLayer(Dia::Core::StringCRC("fix.layer")));
    EXPECT_TRUE(manager.HasLayer(Dia::Core::StringCRC("dyn.layer")));
}

// =====================================================================
// Suite: Buffer
// =====================================================================

TEST(FixedDrawLayer_Buffer, CapacityRespected)
{
    Dia::Debug::FixedPrimitiveBuffer buf(3);

    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));
    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));
    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));
    // 4th add should be silently dropped
    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));

    EXPECT_EQ(buf.GetCount(), 3u);
    EXPECT_TRUE(buf.IsFull());
}

TEST(FixedDrawLayer_Buffer, Clear_ResetsCount)
{
    Dia::Debug::FixedPrimitiveBuffer buf(8);

    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));
    buf.RequestDraw(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1,1), Dia::Graphics::RGBA(255,255,255,255));
    EXPECT_EQ(buf.GetCount(), 2u);

    buf.Clear();
    EXPECT_EQ(buf.GetCount(), 0u);
    EXPECT_FALSE(buf.IsFull());
}

// =====================================================================
// Suite: CrossTypeCollision
// =====================================================================

TEST(FixedDrawLayer_CrossType, DynamicDuplicateOfFixed_Asserts)
{
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(1);
    int source = 0;

    manager.RegisterFixed(Dia::Core::StringCRC("clash.name"), &source, &renderer, 8, 0);

    StubDynLayer dyn("clash.name");
    EXPECT_DEATH(manager.Register(&dyn, 0), "");
}

TEST(FixedDrawLayer_CrossType, FixedDuplicateOfDynamic_Asserts)
{
    Dia::Debug::DebugLayerManager manager;
    StubDynLayer dyn("clash.dyn");
    manager.Register(&dyn, 0);

    LineRenderer renderer(1);
    int source = 0;
    EXPECT_DEATH(
        manager.RegisterFixed(Dia::Core::StringCRC("clash.dyn"), &source, &renderer, 8, 0),
        "");
}

// =====================================================================
// Suite: BufferVariants
// =====================================================================

TEST(FixedDrawLayer_BufferVariants, RequestDrawRect_EmitsRect2D)
{
    Dia::Debug::FixedPrimitiveBuffer buf(4);
    buf.RequestDrawRect(
        Dia::Maths::Vector2D(0.0f, 0.0f),
        Dia::Maths::Vector2D(10.0f, 5.0f),
        Dia::Graphics::RGBA(128, 128, 128, 255));

    EXPECT_EQ(buf.GetCount(), 1u);

    // Verify the stored primitive is Rect2D via a collecting visitor
    class TypeCheckVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable Dia::Graphics::DebugPrimitiveType lastType = Dia::Graphics::DebugPrimitiveType::Circle2D;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override { lastType = p.type; }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    TypeCheckVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_EQ(v.lastType, Dia::Graphics::DebugPrimitiveType::Rect2D);
}

TEST(FixedDrawLayer_BufferVariants, RequestDrawCircle_EmitsCircle2D)
{
    Dia::Debug::FixedPrimitiveBuffer buf(4);
    buf.RequestDrawCircle(
        Dia::Maths::Vector2D(3.0f, 3.0f),
        2.5f,
        Dia::Graphics::RGBA(0, 255, 0, 255));

    EXPECT_EQ(buf.GetCount(), 1u);

    class TypeCheckVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable Dia::Graphics::DebugPrimitiveType lastType = Dia::Graphics::DebugPrimitiveType::Line2D;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override { lastType = p.type; }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    TypeCheckVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_EQ(v.lastType, Dia::Graphics::DebugPrimitiveType::Circle2D);
}

TEST(FixedDrawLayer_BufferVariants, MixedPrimitives_AllStoredInOrder)
{
    Dia::Debug::FixedPrimitiveBuffer buf(4);
    buf.RequestDraw(
        Dia::Maths::Vector2D(0, 0), Dia::Maths::Vector2D(1, 0),
        Dia::Graphics::RGBA(255, 255, 255, 255));
    buf.RequestDrawRect(
        Dia::Maths::Vector2D(0, 0), Dia::Maths::Vector2D(1, 1),
        Dia::Graphics::RGBA(128, 128, 128, 255));
    buf.RequestDrawCircle(
        Dia::Maths::Vector2D(0, 0), 1.0f,
        Dia::Graphics::RGBA(0, 255, 0, 255));

    EXPECT_EQ(buf.GetCount(), 3u);

    class OrderedVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable std::vector<Dia::Graphics::DebugPrimitiveType> types;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override { types.push_back(p.type); }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    OrderedVisitor v;
    buf.AcceptVisitor(v);
    ASSERT_EQ(v.types.size(), 3u);
    EXPECT_EQ(v.types[0], Dia::Graphics::DebugPrimitiveType::Line2D);
    EXPECT_EQ(v.types[1], Dia::Graphics::DebugPrimitiveType::Rect2D);
    EXPECT_EQ(v.types[2], Dia::Graphics::DebugPrimitiveType::Circle2D);
}

// =====================================================================
// Suite: PriorityOrder (draw order verification)
// =====================================================================

namespace
{
    // Records the first primitive from each DrawFixed visit round as a sentinel.
    // Two layers: low-priority emits 1 Line2D (colour R=1), high-priority emits 1 Line2D (colour R=2).
    class PriorityOrderVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable std::vector<unsigned char> visitedR;  // collect colour.r in visit order
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override
        {
            if (p.type == Dia::Graphics::DebugPrimitiveType::Line2D)
                visitedR.push_back(p.line2D.colour.R());
        }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    class TaggedLineRenderer : public Dia::Debug::TypedObjectRenderer<int>
    {
    public:
        explicit TaggedLineRenderer(unsigned char tag) : mTag(tag) {}
    protected:
        void DoBuild(const int& /*src*/, Dia::Debug::IFixedPrimitiveBuffer& out) const override
        {
            out.RequestDraw(
                Dia::Maths::Vector2D(0, 0),
                Dia::Maths::Vector2D(1, 0),
                Dia::Graphics::RGBA(mTag, 0, 0, 255));
        }
    private:
        unsigned char mTag;
    };
}

TEST(FixedDrawLayer_PriorityOrder, LowerPriority_VisitedFirst)
{
    Dia::Debug::FixedDrawRegistry registry;
    TaggedLineRenderer rendererLow(1);
    TaggedLineRenderer rendererHigh(2);
    int sourceA = 0, sourceB = 0;

    // priority 1 = low, priority 10 = high
    registry.Register(Dia::Core::StringCRC("prio.high2"), &sourceA, &rendererHigh, 8, 10);
    registry.Register(Dia::Core::StringCRC("prio.low2"),  &sourceB, &rendererLow,  8, 1);

    PriorityOrderVisitor visitor;
    registry.DrawFixed(visitor);

    ASSERT_EQ(visitor.visitedR.size(), 2u);
    EXPECT_EQ(visitor.visitedR[0], 1u);  // low-priority (tag=1) visited first
    EXPECT_EQ(visitor.visitedR[1], 2u);  // high-priority (tag=2) visited second
}

TEST(FixedDrawLayer_PriorityOrder, EqualPriority_BothVisited)
{
    Dia::Debug::FixedDrawRegistry registry;
    TaggedLineRenderer rendererA(10), rendererB(20);
    int sourceA = 0, sourceB = 0;

    registry.Register(Dia::Core::StringCRC("prio.eq.a"), &sourceA, &rendererA, 8, 5);
    registry.Register(Dia::Core::StringCRC("prio.eq.b"), &sourceB, &rendererB, 8, 5);

    PriorityOrderVisitor visitor;
    registry.DrawFixed(visitor);

    EXPECT_EQ(visitor.visitedR.size(), 2u);
}

// =====================================================================
// Suite: Renderers (default spatial structure renderers)
// =====================================================================

TEST(FixedDrawLayer_Renderers, SpatialGrid_EmitsCellCountRects)
{
    // 2x3 grid over [0,20]x[0,30], cellSize=10
    Dia::Geometry2D::SpatialGrid<int>::Def def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(0.0f,  0.0f),
        Dia::Maths::Vector2D(20.0f, 30.0f));
    def.cellSize = 10.0f;
    auto grid = std::make_unique<Dia::Geometry2D::SpatialGrid<int>>(def);

    Dia::Debug::SpatialGridRenderer<int> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(64);
    renderer.BuildPrimitives(grid.get(), buf);

    const int expectedCells = grid->GetCellCountX() * grid->GetCellCountY();
    EXPECT_EQ(static_cast<int>(buf.GetCount()), expectedCells);
}

TEST(FixedDrawLayer_Renderers, SpatialGrid_EmitsRect2D_InInactiveColour)
{
    Dia::Geometry2D::SpatialGrid<int>::Def def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(0.0f, 0.0f),
        Dia::Maths::Vector2D(10.0f, 10.0f));
    def.cellSize = 5.0f;
    auto grid = std::make_unique<Dia::Geometry2D::SpatialGrid<int>>(def);

    Dia::Debug::SpatialGridRenderer<int> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(16);
    renderer.BuildPrimitives(grid.get(), buf);

    ASSERT_GT(buf.GetCount(), 0u);

    class RectColourVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable Dia::Graphics::RGBA firstColour = Dia::Graphics::RGBA(0,0,0,0);
        mutable Dia::Graphics::DebugPrimitiveType firstType = Dia::Graphics::DebugPrimitiveType::Line2D;
        mutable bool captured = false;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override
        {
            if (!captured) {
                firstType   = p.type;
                firstColour = p.rect2D.outlineColour;
                captured    = true;
            }
        }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    RectColourVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_EQ(v.firstType,   Dia::Graphics::DebugPrimitiveType::Rect2D);
    EXPECT_EQ(v.firstColour, Dia::Debug::DebugColourPalette::kInactive);
}

TEST(FixedDrawLayer_Renderers, Quadtree_EmitsNodeRects_LeafVsInternal)
{
    // Single-node quadtree (no splits): root is a leaf → 1 Rect2D at kActive
    Dia::Geometry2D::Quadtree<int>::Def def;
    def.worldBounds     = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(-50.0f, -50.0f),
        Dia::Maths::Vector2D( 50.0f,  50.0f));
    def.splitThreshold  = 16;
    def.maxDepth        = 8;
    auto tree = std::make_unique<Dia::Geometry2D::Quadtree<int>>(def);

    Dia::Debug::QuadtreeRenderer<int> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(128);
    renderer.BuildPrimitives(tree.get(), buf);

    // An empty tree has exactly 1 node (the root, which is a leaf).
    EXPECT_EQ(buf.GetCount(), 1u);

    class FirstRectVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable Dia::Graphics::RGBA colour = Dia::Graphics::RGBA(0,0,0,0);
        mutable bool captured = false;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override
        {
            if (!captured && p.type == Dia::Graphics::DebugPrimitiveType::Rect2D) {
                colour   = p.rect2D.outlineColour;
                captured = true;
            }
        }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    FirstRectVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_TRUE(v.captured);
    EXPECT_EQ(v.colour, Dia::Debug::DebugColourPalette::kActive);  // root is leaf → kActive
}

TEST(FixedDrawLayer_Renderers, BVH_Unbuilt_EmitsZeroPrimitives)
{
    Dia::Geometry2D::BVH<int, 64>::Def def;
    def.maxLeafObjects = 4;
    Dia::Geometry2D::BVH<int, 64> bvh(def);

    EXPECT_FALSE(bvh.IsBuilt());

    Dia::Debug::BVHRenderer<int, 64> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(64);
    renderer.BuildPrimitives(&bvh, buf);

    EXPECT_EQ(buf.GetCount(), 0u);
}

TEST(FixedDrawLayer_Renderers, BVH_Built_EmitsNodeRects)
{
    Dia::Geometry2D::BVH<int, 64>::Def def;
    def.maxLeafObjects = 1;  // force splits: each leaf holds 1 object
    Dia::Geometry2D::BVH<int, 64> bvh(def);

    // Build with 4 objects so the tree has internal nodes
    Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::BVH<int, 64>::BuildEntry, 64> entries;
    for (int i = 0; i < 4; ++i)
    {
        Dia::Geometry2D::BVH<int, 64>::BuildEntry e;
        e.object = i;
        const float x = static_cast<float>(i) * 10.0f;
        e.bounds = Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(x,        0.0f),
            Dia::Maths::Vector2D(x + 5.0f, 5.0f));
        entries.Add(e);
    }
    bvh.Build(entries);
    EXPECT_TRUE(bvh.IsBuilt());
    EXPECT_GT(bvh.GetNodeCount(), 0);

    Dia::Debug::BVHRenderer<int, 64> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(128);
    renderer.BuildPrimitives(&bvh, buf);

    EXPECT_EQ(buf.GetCount(), static_cast<unsigned int>(bvh.GetNodeCount()));
}

TEST(FixedDrawLayer_Renderers, HexGrid_EmitsSixLinesPerValidHex)
{
    // Small hex grid: radius=10, bounds [0,0]→[40,40] → a handful of valid hexes
    Dia::Geometry2D::HexGrid<int>::Def def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D( 0.0f,  0.0f),
        Dia::Maths::Vector2D(40.0f, 40.0f));
    def.hexRadius = 10.0f;
    auto grid = std::make_unique<Dia::Geometry2D::HexGrid<int>>(def);

    // Count valid hexes manually using the grid API
    int validHexCount = 0;
    const int minQ = grid->GetMinQ();
    const int minR = grid->GetMinR();
    const int colCount = grid->GetColCount();
    const int rowCount = grid->GetRowCount();
    for (int r = 0; r < rowCount; ++r)
        for (int q = 0; q < colCount; ++q)
            if (grid->IsValidHex({ minQ + q, minR + r }))
                ++validHexCount;

    Dia::Debug::HexGridRenderer<int> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(1024);
    renderer.BuildPrimitives(grid.get(), buf);

    EXPECT_EQ(buf.GetCount(), static_cast<unsigned int>(validHexCount * 6));
}

TEST(FixedDrawLayer_Renderers, HexGrid_EmitsLine2D_InInactiveColour)
{
    Dia::Geometry2D::HexGrid<int>::Def def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(0.0f,  0.0f),
        Dia::Maths::Vector2D(20.0f, 20.0f));
    def.hexRadius = 8.0f;
    auto grid = std::make_unique<Dia::Geometry2D::HexGrid<int>>(def);

    Dia::Debug::HexGridRenderer<int> renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(256);
    renderer.BuildPrimitives(grid.get(), buf);

    ASSERT_GT(buf.GetCount(), 0u);

    class FirstLineVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable Dia::Graphics::RGBA colour = Dia::Graphics::RGBA(0,0,0,0);
        mutable bool captured = false;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override
        {
            if (!captured && p.type == Dia::Graphics::DebugPrimitiveType::Line2D) {
                colour   = p.line2D.colour;
                captured = true;
            }
        }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };

    FirstLineVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_TRUE(v.captured);
    EXPECT_EQ(v.colour, Dia::Debug::DebugColourPalette::kInactive);
}

// =====================================================================
// Suite: GetLayerNameGap
// =====================================================================

TEST(FixedDrawLayer_GetLayerNameGap, IndexBeyondDynamic_ReturnsKZero)
{
    // GetLayerName(i) only indexes the dynamic registry.
    // If a fixed layer is present, GetLayerCount() > dynamic count, but
    // GetLayerName(dynamicCount) returns kZero (not the fixed layer name).
    Dia::Debug::DebugLayerManager manager;
    LineRenderer renderer(1);
    int source = 0;

    StubDynLayer dyn("dyn.only");
    manager.Register(&dyn, 0);
    manager.RegisterFixed(Dia::Core::StringCRC("fix.only"), &source, &renderer, 8, 0);

    // GetLayerCount() == 2 (1 dynamic + 1 fixed)
    EXPECT_EQ(manager.GetLayerCount(), 2);

    // GetLayerName(0) returns the dynamic layer
    EXPECT_EQ(manager.GetLayerName(0), Dia::Core::StringCRC("dyn.only"));

    // GetLayerName(1) is beyond the dynamic registry — returns kZero
    EXPECT_EQ(manager.GetLayerName(1), Dia::Core::StringCRC::kZero);
}

TEST(FixedDrawLayer_GetLayerNameGap, EmptyManager_IndexZero_ReturnsKZero)
{
    Dia::Debug::DebugLayerManager manager;
    EXPECT_EQ(manager.GetLayerName(0), Dia::Core::StringCRC::kZero);
}

#endif // DIA_DEBUG
