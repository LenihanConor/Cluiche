////////////////////////////////////////////////////////////////////////////////
// TestDebugLayerManager.cpp
// Tests for DebugLayerManager, DebugColourPalette — DiaVisualDebugger module
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-layer-manager.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#define DIA_DEBUG
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Debug;
using namespace Dia::Graphics;

// ============================================================================
// TestLayer — minimal IVisualDebugger stub used throughout all tests.
// Records the number of times Draw() has been called and the draw order.
// ============================================================================

static int sDrawSequence = 0;  // global counter reset per test via helper

struct TestLayer : public IVisualDebugger
{
    explicit TestLayer(const char* name, int* outOrder = nullptr)
        : mName(name)
        , mDrawCallCount(0)
        , mLastDrawOrder(-1)
        , mOutOrder(outOrder)
    {}

    Dia::Core::StringCRC GetLayerName() const override { return mName; }

    void Draw(Dia::Graphics::FrameData& /*frameData*/) override
    {
        ++mDrawCallCount;
        mLastDrawOrder = sDrawSequence++;
        if (mOutOrder)
            *mOutOrder = mLastDrawOrder;
    }

    Dia::Core::StringCRC mName;
    int  mDrawCallCount;
    int  mLastDrawOrder;
    int* mOutOrder;
};

static void ResetDrawSequence() { sDrawSequence = 0; }

// ============================================================================
// Registration suite
// ============================================================================

TEST(DebugLayerManager_Registration, Register_SingleLayer)
{
    DebugLayerManager mgr;
    TestLayer layer("test.single");
    mgr.Register(&layer);
    EXPECT_EQ(mgr.GetLayerCount(), 1);
}

TEST(DebugLayerManager_Registration, Register_MultipleLayersUniqueName)
{
    DebugLayerManager mgr;
    TestLayer a("test.a"), b("test.b"), c("test.c");
    mgr.Register(&a);
    mgr.Register(&b);
    mgr.Register(&c);
    EXPECT_EQ(mgr.GetLayerCount(), 3);
}

TEST(DebugLayerManager_Registration, Register_DuplicateNameAsserts)
{
    DebugLayerManager mgr;
    TestLayer a("test.dup");
    TestLayer b("test.dup");
    mgr.Register(&a);
    // Registering a second layer with the same name must fire DIA_ASSERT → death
    EXPECT_DEATH({ mgr.Register(&b); }, "");
}

TEST(DebugLayerManager_Registration, Register_NullptrAsserts)
{
    DebugLayerManager mgr;
    EXPECT_DEATH({ mgr.Register(nullptr); }, "");
}

TEST(DebugLayerManager_Registration, HasLayer_ReturnsTrueAfterRegister)
{
    DebugLayerManager mgr;
    TestLayer layer("test.present");
    mgr.Register(&layer);
    EXPECT_TRUE(mgr.HasLayer(Dia::Core::StringCRC("test.present")));
}

TEST(DebugLayerManager_Registration, HasLayer_ReturnsFalseBeforeRegister)
{
    DebugLayerManager mgr;
    EXPECT_FALSE(mgr.HasLayer(Dia::Core::StringCRC("test.absent")));
}

// ============================================================================
// Unregister suite
// ============================================================================

TEST(DebugLayerManager_Unregister, Unregister_RemovesLayer)
{
    DebugLayerManager mgr;
    TestLayer layer("test.unregister");
    mgr.Register(&layer);
    ASSERT_EQ(mgr.GetLayerCount(), 1);

    mgr.Unregister(Dia::Core::StringCRC("test.unregister"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Core::StringCRC("test.unregister")));
}

TEST(DebugLayerManager_Unregister, Unregister_UnknownNameNoOp)
{
    DebugLayerManager mgr;
    TestLayer layer("test.existing");
    mgr.Register(&layer);

    // Unregistering a non-existent name must not crash and count must be unchanged
    EXPECT_NO_FATAL_FAILURE(mgr.Unregister(Dia::Core::StringCRC("test.nonexistent")));
    EXPECT_EQ(mgr.GetLayerCount(), 1);
}

// ============================================================================
// Toggle suite
// ============================================================================

TEST(DebugLayerManager_Toggle, EnableLayer_DefaultEnabled)
{
    DebugLayerManager mgr;
    TestLayer layer("test.default_enabled");
    mgr.Register(&layer);
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("test.default_enabled")));
}

TEST(DebugLayerManager_Toggle, DisableLayer_LayerDisabled)
{
    DebugLayerManager mgr;
    TestLayer layer("test.disable");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("test.disable"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Core::StringCRC("test.disable")));
}

TEST(DebugLayerManager_Toggle, EnableLayer_ReEnables)
{
    DebugLayerManager mgr;
    TestLayer layer("test.reenable");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("test.reenable"));
    ASSERT_FALSE(mgr.IsLayerEnabled(Dia::Core::StringCRC("test.reenable")));
    mgr.EnableLayer(Dia::Core::StringCRC("test.reenable"));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("test.reenable")));
}

TEST(DebugLayerManager_Toggle, Toggle_UnknownNameNoOp)
{
    DebugLayerManager mgr;
    // EnableLayer/DisableLayer on an unregistered name must not crash
    EXPECT_NO_FATAL_FAILURE(mgr.EnableLayer(Dia::Core::StringCRC("test.unknown")));
    EXPECT_NO_FATAL_FAILURE(mgr.DisableLayer(Dia::Core::StringCRC("test.unknown")));
}

// ============================================================================
// Draw suite
// ============================================================================

TEST(DebugLayerManager_Draw, Draw_CallsEnabledLayer)
{
    DebugLayerManager mgr;
    TestLayer layer("test.draw_enabled");
    mgr.Register(&layer);

    FrameData fd;
    mgr.Draw(fd);

    EXPECT_EQ(layer.mDrawCallCount, 1);
}

TEST(DebugLayerManager_Draw, Draw_SkipsDisabledLayer)
{
    DebugLayerManager mgr;
    TestLayer layer("test.draw_disabled");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("test.draw_disabled"));

    FrameData fd;
    mgr.Draw(fd);

    EXPECT_EQ(layer.mDrawCallCount, 0);
}

TEST(DebugLayerManager_Draw, Draw_MultipleLayersAllEnabled)
{
    DebugLayerManager mgr;
    TestLayer a("test.multi.a"), b("test.multi.b"), c("test.multi.c");
    mgr.Register(&a);
    mgr.Register(&b);
    mgr.Register(&c);

    FrameData fd;
    mgr.Draw(fd);

    EXPECT_EQ(a.mDrawCallCount, 1);
    EXPECT_EQ(b.mDrawCallCount, 1);
    EXPECT_EQ(c.mDrawCallCount, 1);
}

TEST(DebugLayerManager_Draw, Draw_PriorityOrder)
{
    // Register priorities out of order: 20, 0, 10
    // Expected draw order: priority 0, then 10, then 20
    DebugLayerManager mgr;
    int orderA = -1, orderB = -1, orderC = -1;
    TestLayer layerA("test.prio.a", &orderA);  // priority 20
    TestLayer layerB("test.prio.b", &orderB);  // priority 0
    TestLayer layerC("test.prio.c", &orderC);  // priority 10

    mgr.Register(&layerA, 20);
    mgr.Register(&layerB,  0);
    mgr.Register(&layerC, 10);

    ResetDrawSequence();
    FrameData fd;
    mgr.Draw(fd);

    // B (priority 0) should be drawn first, C (10) second, A (20) third
    EXPECT_LT(orderB, orderC);
    EXPECT_LT(orderC, orderA);
}

TEST(DebugLayerManager_Draw, Draw_PriorityTieBreakByRegistration)
{
    // Two layers with the same priority — insertion sort is stable,
    // so the first-registered layer must be drawn first.
    DebugLayerManager mgr;
    int orderFirst = -1, orderSecond = -1;
    TestLayer first("test.tie.first",   &orderFirst);
    TestLayer second("test.tie.second", &orderSecond);

    mgr.Register(&first,  5);
    mgr.Register(&second, 5);

    ResetDrawSequence();
    FrameData fd;
    mgr.Draw(fd);

    EXPECT_LT(orderFirst, orderSecond);
}

TEST(DebugLayerManager_Draw, Draw_EmptyManagerNoOp)
{
    DebugLayerManager mgr;
    FrameData fd;
    EXPECT_NO_FATAL_FAILURE(mgr.Draw(fd));
}

// ============================================================================
// Scale suite
// ============================================================================

TEST(DebugLayerManager_Scale, DebugScale_DefaultIsOne)
{
    DebugLayerManager mgr;
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(DebugLayerManager_Scale, SetDebugScale_Persists)
{
    DebugLayerManager mgr;
    mgr.SetDebugScale(2.5f);
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

// ============================================================================
// EntityId suite
// ============================================================================

TEST(DebugLayerManager_EntityId, SelectedEntityId_DefaultZero)
{
    DebugLayerManager mgr;
    EXPECT_EQ(mgr.GetSelectedEntityId(), 0u);
}

TEST(DebugLayerManager_EntityId, SetSelectedEntityId_Persists)
{
    DebugLayerManager mgr;
    mgr.SetSelectedEntityId(42u);
    EXPECT_EQ(mgr.GetSelectedEntityId(), 42u);
}

// ============================================================================
// DebugColourPalette suite
// ============================================================================

TEST(DebugColourPalette_Values, DebugColourPalette_ActiveIsWhite)
{
    EXPECT_EQ(DebugColourPalette::kActive, Dia::Graphics::RGBA(255, 255, 255, 255));
}

TEST(DebugColourPalette_Values, DebugColourPalette_AllNineDistinct)
{
    // Collect all 9 constants and verify they are pairwise distinct.
    const Dia::Graphics::RGBA colours[9] = {
        DebugColourPalette::kActive,
        DebugColourPalette::kInactive,
        DebugColourPalette::kHealthy,
        DebugColourPalette::kWarning,
        DebugColourPalette::kError,
        DebugColourPalette::kGoal,
        DebugColourPalette::kPinned,
        DebugColourPalette::kCapped,
        DebugColourPalette::kDeepSleep
    };

    for (int i = 0; i < 9; ++i)
    {
        for (int j = i + 1; j < 9; ++j)
        {
            EXPECT_NE(colours[i], colours[j])
                << "Colour " << i << " equals colour " << j;
        }
    }
}
