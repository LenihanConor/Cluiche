#include <gtest/gtest.h>
#include <DiaEditor/AppEditor/AppEditorController.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/MVC/IEditorContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
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
        AppEditorController  ctrl;

        Fixture() { ctrl.Initialize(&bridge, &mockCtx); }
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
