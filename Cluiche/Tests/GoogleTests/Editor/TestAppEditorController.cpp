#include <gtest/gtest.h>
#include <DiaEditor/AppEditor/AppEditorController.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/MVC/IEditorContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Editor;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Mock IEditorContext
// ---------------------------------------------------------------------------

namespace
{
    struct MockEditorContext : IEditorContext
    {
        ProjectContext ctx{};

        bool LoadDiagameProject(const char* path) override
        {
            if (path && path[0])
            {
                strncpy_s(ctx.diagamePath, ProjectContext::kMaxPath, path, _TRUNCATE);
                return true;
            }
            return false;
        }

        void ClearDiagameProject() override
        {
            ctx = ProjectContext{};
        }

        const ProjectContext& GetDiagameProject() const override { return ctx; }

        void OnDiagameProjectChanged(ProjectChangedCallback /*callback*/, void* /*userData*/) override {}

        unsigned int GetRecentProjectCount() const override { return 0; }

        const char* GetRecentProject(unsigned int /*index*/) const override { return nullptr; }
    };

    // -----------------------------------------------------------------------
    // Mock IPluginLoader
    // -----------------------------------------------------------------------

    struct MockPluginLoader : IPluginLoader
    {
        StringCRC loadedTypeId;

        void LoadPlugin(const StringCRC& /*typeId*/, const StringCRC& /*instanceId*/) override {}

        bool UnloadPlugin(const StringCRC& /*typeId*/) override { return true; }

        bool IsPluginTypeLoaded(const StringCRC& typeId) const override
        {
            return typeId == loadedTypeId;
        }

        bool IsPluginPinned(const StringCRC& /*typeId*/) const override { return false; }
    };
}

// ---------------------------------------------------------------------------
// Helper: build a controller with a bridge + mock context and call the handler
// ---------------------------------------------------------------------------

namespace
{
    struct Fixture
    {
        WebUIBridge          bridge{nullptr};
        MockEditorContext    mockCtx;
        MockPluginLoader     mockPluginLoader;
        AppEditorController  ctrl;

        Fixture() { ctrl.Initialize(&bridge, &mockCtx, &mockPluginLoader); }
        ~Fixture() { ctrl.Shutdown(); }

        Json::Value GetActiveContext()
        {
            return ctrl.HandleGetActiveContext(Json::Value{});
        }
    };
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(AppEditorController, InitialState)
{
    Fixture f;
    Json::Value result = f.GetActiveContext();

    EXPECT_TRUE(result["project"].isNull());
    EXPECT_TRUE(result["focus"].isNull());
    EXPECT_TRUE(result["edit_target"].isNull());
    EXPECT_TRUE(result["selection"].isNull());
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------

TEST(AppEditorController, SetFocus_PluginIdAndPanel)
{
    Fixture f;
    f.ctrl.SetFocus(StringCRC("scene_editor"), "scene_hierarchy");
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["focus"].isNull());
    EXPECT_STREQ(result["focus"]["plugin_id"].asCString(), "scene_editor");
    EXPECT_STREQ(result["focus"]["panel"].asCString(), "scene_hierarchy");
}

TEST(AppEditorController, ClearFocus_AfterSet_FocusIsNull)
{
    Fixture f;
    f.ctrl.SetFocus(StringCRC("scene_editor"), "scene_hierarchy");
    f.ctrl.ClearFocus();
    Json::Value result = f.GetActiveContext();

    EXPECT_TRUE(result["focus"].isNull());
}

// ---------------------------------------------------------------------------
// Edit target
// ---------------------------------------------------------------------------

TEST(AppEditorController, SetEditTarget_AllFields)
{
    Fixture f;
    f.ctrl.SetEditTarget(StringCRC("scene"), StringCRC("scene_test"), "SceneTest", false);
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["edit_target"].isNull());
    EXPECT_STREQ(result["edit_target"]["type"].asCString(), "scene");
    EXPECT_STREQ(result["edit_target"]["id"].asCString(), "scene_test");
    EXPECT_STREQ(result["edit_target"]["name"].asCString(), "SceneTest");
    EXPECT_FALSE(result["edit_target"]["dirty"].asBool());
}

TEST(AppEditorController, SetDirty_UpdatesDirtyFlag)
{
    Fixture f;
    f.ctrl.SetEditTarget(StringCRC("scene"), StringCRC("scene_test"), "SceneTest", false);
    f.ctrl.SetDirty(true);
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["edit_target"].isNull());
    EXPECT_TRUE(result["edit_target"]["dirty"].asBool());
}

TEST(AppEditorController, ClearEditTarget_AfterSet_EditTargetIsNull)
{
    Fixture f;
    f.ctrl.SetEditTarget(StringCRC("scene"), StringCRC("scene_test"), "SceneTest", false);
    f.ctrl.ClearEditTarget();
    Json::Value result = f.GetActiveContext();

    EXPECT_TRUE(result["edit_target"].isNull());
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

TEST(AppEditorController, SetSelection_AllFields)
{
    Fixture f;
    f.ctrl.SetSelection(StringCRC("entity"), StringCRC("entity_001"), "PlayerSpawn");
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["selection"].isNull());
    EXPECT_STREQ(result["selection"]["type"].asCString(), "entity");
    EXPECT_STREQ(result["selection"]["id"].asCString(), "entity_001");
    EXPECT_STREQ(result["selection"]["name"].asCString(), "PlayerSpawn");
}

TEST(AppEditorController, ClearSelection_AfterSet_SelectionIsNull)
{
    Fixture f;
    f.ctrl.SetSelection(StringCRC("entity"), StringCRC("entity_001"), "PlayerSpawn");
    f.ctrl.ClearSelection();
    Json::Value result = f.GetActiveContext();

    EXPECT_TRUE(result["selection"].isNull());
}

// ---------------------------------------------------------------------------
// Long string truncation
// ---------------------------------------------------------------------------

TEST(AppEditorController, LongStringTruncated_NoCrash)
{
    Fixture f;

    // 200-character panel name
    char longPanel[201];
    for (int i = 0; i < 200; ++i) longPanel[i] = 'a';
    longPanel[200] = '\0';

    f.ctrl.SetFocus(StringCRC("scene_editor"), longPanel);
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["focus"].isNull());
    const char* panel = result["focus"]["panel"].asCString();
    // Buffer is 128 bytes; strncpy_s with _TRUNCATE stores at most 127 chars + null.
    EXPECT_LE(static_cast<int>(strlen(panel)), 127);
}

// ---------------------------------------------------------------------------
// Project context
// ---------------------------------------------------------------------------

TEST(AppEditorController, ProjectContext_ValidPath_IdAndStateSet)
{
    Fixture f;
    f.mockCtx.LoadDiagameProject("C:/proj/cluichetest.diagame");
    Json::Value result = f.GetActiveContext();

    ASSERT_FALSE(result["project"].isNull());
    EXPECT_STREQ(result["project"]["id"].asCString(), "cluichetest");
    EXPECT_STREQ(result["project"]["state"].asCString(), "open");
}

TEST(AppEditorController, ProjectContext_InvalidPath_ProjectIsNull)
{
    Fixture f;
    // diagamePath[0] == '\0' — IsValid() returns false
    Json::Value result = f.GetActiveContext();

    EXPECT_TRUE(result["project"].isNull());
}

// ---------------------------------------------------------------------------
// HandleNavigateTo
// ---------------------------------------------------------------------------

TEST(AppEditorController, HandleNavigateTo_NoProject_ReturnsNoProjectOpen)
{
    // Initialize with no project loaded (context invalid)
    WebUIBridge         bridge{nullptr};
    MockEditorContext   ctx;
    MockPluginLoader    loader;
    AppEditorController ctrl;
    ctrl.Initialize(&bridge, &ctx, &loader);

    Json::Value result = ctrl.HandleNavigateTo(StringCRC("plugin"), StringCRC("x"));

    EXPECT_FALSE(result["success"].asBool());
    EXPECT_STREQ(result["reason"].asCString(), "no_project_open");

    ctrl.Shutdown();
}

TEST(AppEditorController, HandleNavigateTo_EmptyId_ReturnsNotFound)
{
    Fixture f;
    f.mockCtx.LoadDiagameProject("C:/proj/cluichetest.diagame");

    Json::Value result = f.ctrl.HandleNavigateTo(StringCRC("stage"), StringCRC());

    EXPECT_FALSE(result["success"].asBool());
    EXPECT_STREQ(result["reason"].asCString(), "not_found");
}

TEST(AppEditorController, HandleNavigateTo_AssetType_ReturnsNotImplemented)
{
    Fixture f;
    f.mockCtx.LoadDiagameProject("C:/proj/cluichetest.diagame");

    Json::Value result = f.ctrl.HandleNavigateTo(StringCRC("asset"), StringCRC("some_asset"));

    EXPECT_FALSE(result["success"].asBool());
    EXPECT_STREQ(result["reason"].asCString(), "not_implemented");
}

TEST(AppEditorController, HandleNavigateTo_ValidStage_ReturnsSuccess)
{
    Fixture f;
    f.mockCtx.LoadDiagameProject("C:/proj/cluichetest.diagame");

    // Stage validation is deferred to JS in Phase 1 — any non-empty id succeeds in C++
    Json::Value result = f.ctrl.HandleNavigateTo(StringCRC("stage"), StringCRC("boot"));

    EXPECT_TRUE(result["success"].asBool());
}

TEST(AppEditorController, HandleNavigateTo_ValidPlugin_NotLoaded_ReturnsPluginNotLoaded)
{
    // Use a controller with a null pluginLoader to exercise the nullptr path
    WebUIBridge         bridge{nullptr};
    MockEditorContext   ctx;
    AppEditorController ctrl;
    ctrl.Initialize(&bridge, &ctx, nullptr);

    ctx.LoadDiagameProject("C:/proj/cluichetest.diagame");

    Json::Value result = ctrl.HandleNavigateTo(StringCRC("plugin"), StringCRC("scene_editor"));

    EXPECT_FALSE(result["success"].asBool());
    EXPECT_STREQ(result["reason"].asCString(), "plugin_not_loaded");

    ctrl.Shutdown();
}
