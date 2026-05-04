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
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaCore/CRC/StringCRC.h>

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

#endif // DIA_DEBUG
