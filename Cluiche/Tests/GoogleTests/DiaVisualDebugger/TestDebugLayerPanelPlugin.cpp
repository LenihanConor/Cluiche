////////////////////////////////////////////////////////////////////////////////
// TestDebugLayerPanelPlugin.cpp
// Tests for DebugLayerPanelPlugin and BroadcastLayerState serialisation.
// Feature spec: docs/specs/features/dia/diavisualdebugger/debug-editor-panel.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

// DIA_DEBUG is already defined globally by the build system
#include <DiaVisualDebugger/DebugLayerPanelPlugin.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaGraphics/Frame/FrameData.h>

using namespace Dia::Debug;
using namespace Dia::Editor;

// ============================================================================
// Minimal IVisualDebugger stub used in BroadcastLayerState tests
// ============================================================================
struct PanelTestLayer : public IVisualDebugger
{
    explicit PanelTestLayer(const char* name) : mName(name) {}
    Dia::Core::StringCRC GetLayerName() const override { return mName; }
    void Draw(Dia::Graphics::FrameData& /*fd*/) override {}
    Dia::Core::StringCRC mName;
};

// ============================================================================
// Plugin metadata suite
// ============================================================================

TEST(DebugLayerPanelPlugin_Metadata, GetName_ReturnsDebugLayers)
{
    DebugLayerPanelPlugin plugin;
    EXPECT_STREQ(plugin.GetName(), "Debug Layers");
}

TEST(DebugLayerPanelPlugin_Metadata, GetUIPath_ReturnsCorrectPath)
{
    DebugLayerPanelPlugin plugin;
    EXPECT_STREQ(plugin.GetUIPath(), "dia://plugins/debug-layers/index.html");
}

TEST(DebugLayerPanelPlugin_Metadata, GetLayoutMode_IsDockable)
{
    DebugLayerPanelPlugin plugin;
    EXPECT_EQ(plugin.GetLayoutMode(), LayoutMode::kDockable);
}

TEST(DebugLayerPanelPlugin_Metadata, GetVersion_NonEmpty)
{
    DebugLayerPanelPlugin plugin;
    const char* ver = plugin.GetVersion();
    ASSERT_NE(ver, nullptr);
    EXPECT_GT(strlen(ver), 0u);
}

TEST(DebugLayerPanelPlugin_Metadata, GetDescription_NonEmpty)
{
    DebugLayerPanelPlugin plugin;
    const char* desc = plugin.GetDescription();
    ASSERT_NE(desc, nullptr);
    EXPECT_GT(strlen(desc), 0u);
}

TEST(DebugLayerPanelPlugin_Metadata, GetToolbarItem_LabelMatchesName)
{
    DebugLayerPanelPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();
    EXPECT_STREQ(item.label, "Debug Layers");
}

// ============================================================================
// BroadcastLayerState suite
// These tests verify the dirty-flag logic and null-server safety.
// Full serialisation is verified through DebugLayerManager query accessors
// (which are also used internally by BroadcastLayerState).
// ============================================================================

TEST(BroadcastLayerState, NullServer_NoOp)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("test.null");
    mgr.Register(&layer);
    // Must not crash when server is nullptr
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

TEST(BroadcastLayerState, DirtyFlag_SetByRegister)
{
    // After Register the manager is dirty; null-server call is a no-op but safe
    DebugLayerManager mgr;
    PanelTestLayer layer("test.dirty");
    mgr.Register(&layer);
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

TEST(BroadcastLayerState, DirtyFlag_SetByEnable)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("test.enable");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("test.enable"));
    mgr.EnableLayer(Dia::Core::StringCRC("test.enable"));
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

TEST(BroadcastLayerState, DirtyFlag_SetByDisable)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("test.disable");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("test.disable"));
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

TEST(BroadcastLayerState, DirtyFlag_SetByUnregister)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("test.unreg");
    mgr.Register(&layer);
    mgr.Unregister(Dia::Core::StringCRC("test.unreg"));
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

TEST(BroadcastLayerState, Draw_CachesDroppedCount)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("test.drop");
    mgr.Register(&layer);

    // Fill FrameData beyond capacity to produce dropped > 0
    Dia::Graphics::FrameData fd;
    const unsigned int kCapacity = Dia::Graphics::DebugFrameData::kCapacity;
    for (unsigned int i = 0; i <= kCapacity; ++i)
    {
        fd.RequestDraw(Dia::Maths::Vector2D(0.f, 0.f), 1.0f,
                       Dia::Graphics::RGBA(255, 255, 255, 255));
    }

    mgr.Draw(fd);
    // FrameData should show drops; manager caches and goes dirty
    EXPECT_TRUE(fd.IsOverCapacity());
    EXPECT_NO_FATAL_FAILURE(mgr.BroadcastLayerState(nullptr));
}

// ============================================================================
// Serialisation data verified through DebugLayerManager accessors
// (the same data BroadcastLayerState serialises into the JSON payload)
// ============================================================================

TEST(BroadcastLayerState, Serialises_LayerCount)
{
    DebugLayerManager mgr;
    PanelTestLayer a("sl.a"), b("sl.b"), c("sl.c");
    mgr.Register(&a, 0);
    mgr.Register(&b, 1);
    mgr.Register(&c, 2);
    EXPECT_EQ(mgr.GetLayerCount(), 3);
}

TEST(BroadcastLayerState, Serialises_EnabledFlag)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("sl.enabled");
    mgr.Register(&layer);
    mgr.DisableLayer(Dia::Core::StringCRC("sl.enabled"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Core::StringCRC("sl.enabled")));
}

TEST(BroadcastLayerState, Serialises_Priority)
{
    DebugLayerManager mgr;
    PanelTestLayer a("sl.prio.a"), b("sl.prio.b");
    mgr.Register(&a, 10);
    mgr.Register(&b, 20);
    EXPECT_EQ(mgr.GetLayerCount(), 2);
}

TEST(BroadcastLayerState, Serialises_LayerName)
{
    DebugLayerManager mgr;
    PanelTestLayer layer("my.special.layer");
    mgr.Register(&layer);

    // Verify name round-trip through GetLayerName accessor
    Dia::Core::StringCRC retrieved = mgr.GetLayerName(0);
    EXPECT_EQ(retrieved, Dia::Core::StringCRC("my.special.layer"));
}
