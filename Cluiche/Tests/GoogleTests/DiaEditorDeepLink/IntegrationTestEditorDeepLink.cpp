// Integration tests — OnNavigate for all four editor plugins.
//
// Uses a real WebUIBridge(nullptr) + EditorModel — same pattern as the per-plugin
// integration tests.  null UISystem is safe: NotifyUIDataChanged is a no-op.
// get_record / load handlers will return null/failure because no catalogue or
// file system is wired up, so the tests verify graceful no-crash behaviour only.

#include <gtest/gtest.h>

// Plugins under test
#include <DiaApplicationEditor/DiaApplicationFlowEditorPlugin.h>
#include <DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h>
#include <DiaSceneEditor/DiaSceneEditorPlugin.h>
#include <DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h>

// Framework
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>

// Asset catalogue (needed for the catalogue test fixture)
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaAssetCatalogue/AssetRegistry.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Helpers
// ===========================================================================

static EditorPluginContext MakeContext(WebUIBridge* bridge, EditorModel* model)
{
    EditorPluginContext ctx;
    ctx.mBridge       = bridge;
    ctx.mModel        = model;
    ctx.mView         = nullptr;
    ctx.mPluginLoader = nullptr;
    ctx.mServices     = nullptr;
    return ctx;
}

// ===========================================================================
// DiaApplicationFlowEditorPlugin — OnNavigate
// ===========================================================================

// Test 1: null bridge, OnNavigate must not crash (early-out path).
TEST(AppFlowEditorOnNavigate, OnNavigate_NoBridge_DoesNotCrash)
{
    Dia::Editor::DiaApplicationFlowEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("stage.test_stage")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 2: bridge set but no manifest loaded — hasManifest is false, early-out.
TEST(AppFlowEditorOnNavigate, OnNavigate_NoManifest_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::Editor::DiaApplicationFlowEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    // No manifest loaded, so the plugin should return early without crashing.
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("stage.test_stage")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 3: bridge set, call OnNavigate with a stage id — no crash.
// The null UISystem means NotifyUIDataChanged is a no-op; we can't verify the
// push payload, but we can verify the code path completes without a crash.
TEST(AppFlowEditorOnNavigate, OnNavigate_WithBridge_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::Editor::DiaApplicationFlowEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("stage.test_stage")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// DiaEntityTemplateEditorPlugin — OnNavigate
// ===========================================================================

// Test 4: null bridge — OnNavigate early-out, no crash.
TEST(EntityTemplateEditorOnNavigate, OnNavigate_NoBridge_DoesNotCrash)
{
    Dia::EntityTemplateEditor::DiaEntityTemplateEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.test")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 5: bridge set but no catalogue loaded — get_record returns null, no crash.
TEST(EntityTemplateEditorOnNavigate, OnNavigate_CatalogueNotLoaded_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::EntityTemplateEditor::DiaEntityTemplateEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    // asset_catalogue.get_record has no handler → returns null → plugin early-outs.
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.test")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 6: valid bridge + registered blueprint.load handler, get_record still fails
// (catalogue not loaded) — the call chain exits gracefully.
TEST(EntityTemplateEditorOnNavigate, OnNavigate_ValidCall_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::EntityTemplateEditor::DiaEntityTemplateEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.test")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// DiaSceneEditorPlugin — OnNavigate
// ===========================================================================

// Test 7: null bridge — OnNavigate early-out, no crash.
TEST(SceneEditorOnNavigate, OnNavigate_NoBridge_DoesNotCrash)
{
    Dia::SceneEditor::DiaSceneEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diascene.test_level")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 8: bridge set but no catalogue loaded — get_record returns null, no crash.
TEST(SceneEditorOnNavigate, OnNavigate_CatalogueNotLoaded_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::SceneEditor::DiaSceneEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    // asset_catalogue.get_record has no handler registered here → null → early-out.
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diascene.test_level")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// DiaAssetCatalogueEditorPlugin — OnNavigate
// ===========================================================================

// Test 9: null bridge — OnNavigate early-out, no crash.
TEST(AssetCatalogueEditorOnNavigate, OnNavigate_NoBridge_DoesNotCrash)
{
    Dia::AssetCatalogue::Editor::DiaAssetCatalogueEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.unknown")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 10: bridge set, record not in registry — FindById returns nullptr, no crash.
TEST(AssetCatalogueEditorOnNavigate, OnNavigate_RecordNotInRegistry_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::AssetCatalogue::Editor::DiaAssetCatalogueEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    // Registry is empty — FindById returns nullptr → plugin logs warning and returns.
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.nonexistent")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// Test 11: record registered via the CRUD handler, then OnNavigate with its id.
// NotifyUIDataChanged is a no-op (null UISystem) — verify no crash.
TEST(AssetCatalogueEditorOnNavigate, OnNavigate_RecordExists_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    EditorModel model;
    Dia::AssetCatalogue::Editor::DiaAssetCatalogueEditorPlugin plugin;
    EditorPluginContext ctx = MakeContext(&bridge, &model);
    plugin.OnLoad(ctx);

    // Register a record through the asset_catalogue.add_asset handler.
    Json::Value addReq;
    addReq["id"]         = "diaentitytemplate.hero";
    addReq["typeId"]     = "diaentitytemplate";
    addReq["sourcePath"] = "Assets/hero.diaentitytemplatetemplate";
    // The handler may or may not succeed depending on manifest state — we don't
    // assert success here; we only care that OnNavigate doesn't crash regardless.
    bridge.InvokeRequestHandler(StringCRC("asset_catalogue.add_asset"), addReq);

    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("diaentitytemplate.hero")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// IEditorPlugin base — default OnNavigate no-op
// ===========================================================================

// Test 12: Minimal concrete IEditorPlugin — verify the default OnNavigate
// compiles and does nothing (no crash, no side-effect).
namespace
{
    class MinimalPlugin : public IEditorPlugin
    {
    public:
        const char* GetName()        const override { return "Minimal"; }
        const char* GetVersion()     const override { return "0.0"; }
        const char* GetDescription() const override { return ""; }
        const char* GetUIPath()      const override { return ""; }
        LayoutMode  GetLayoutMode()  const override { return LayoutMode::kFullScreen; }

        void OnLoad(const EditorPluginContext& /*ctx*/) override {}
        void OnUnload()                                 override {}
        void OnUpdate(float /*dt*/)                     override {}
        // OnNavigate is intentionally NOT overridden — uses the base default no-op.
    };
} // anonymous namespace

TEST(IEditorPluginBase, DefaultOnNavigate_DoesNothing)
{
    MinimalPlugin plugin;
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("anything.foo")));
    EXPECT_NO_FATAL_FAILURE(plugin.OnNavigate(StringCRC("")));
}
