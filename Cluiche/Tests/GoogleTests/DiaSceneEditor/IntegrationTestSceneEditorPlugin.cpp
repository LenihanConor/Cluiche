// Integration tests for DiaSceneEditorPlugin.
//
// Uses a real WebUIBridge(nullptr) + EditorModel — same pattern as
// IntegrationTestEntityTemplateEditorPlugin. No mocking required.

#include <gtest/gtest.h>
#include <DiaSceneEditor/DiaSceneEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <cstdio>

using namespace Dia::SceneEditor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
// ===========================================================================

class SceneEditorPluginTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mBridge = new WebUIBridge(nullptr);
		mModel  = new EditorModel();

		EditorPluginContext ctx;
		ctx.mBridge       = mBridge;
		ctx.mModel        = mModel;
		ctx.mView         = nullptr;
		ctx.mPluginLoader = nullptr;
		ctx.mServices     = nullptr;

		mPlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mPlugin.OnUnload();
		delete mModel;
		delete mBridge;
	}

	Json::Value Invoke(const char* command,
	                   const Json::Value& data = Json::Value(Json::objectValue))
	{
		return mBridge->InvokeRequestHandler(StringCRC(command), data);
	}

	DiaSceneEditorPlugin  mPlugin;
	WebUIBridge*          mBridge = nullptr;
	EditorModel*          mModel  = nullptr;
};

// ===========================================================================
// Lifecycle — all handlers registered on OnLoad
// ===========================================================================

TEST_F(SceneEditorPluginTest, OnLoad_HandlerRegistered_GetProjectState)
{
	Json::Value r = Invoke("scene_editor.get_project_state");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("isValid"));
}

TEST_F(SceneEditorPluginTest, OnLoad_HandlerRegistered_GetStageList)
{
	Json::Value r = Invoke("scene_editor.get_stage_list");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

// ===========================================================================
// OnUnload removes all handlers
// ===========================================================================

TEST_F(SceneEditorPluginTest, OnUnload_RemovesGetProjectStateHandler)
{
	mPlugin.OnUnload();
	EXPECT_TRUE(Invoke("scene_editor.get_project_state").isNull());
	// Re-load so TearDown doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge = mBridge;
	ctx.mModel  = mModel;
	mPlugin.OnLoad(ctx);
}

// ===========================================================================
// Null inputs — no crash
// ===========================================================================

TEST(SceneEditorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
	DiaSceneEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = nullptr;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

TEST(SceneEditorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
	WebUIBridge bridge(nullptr);
	DiaSceneEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// get_project_state — no project
// ===========================================================================

TEST_F(SceneEditorPluginTest, GetProjectState_NoProjectLoaded_IsValidFalse)
{
	Json::Value r = Invoke("scene_editor.get_project_state");
	EXPECT_FALSE(r["isValid"].asBool());
}

TEST_F(SceneEditorPluginTest, GetProjectState_NoProjectLoaded_DiagamePathEmpty)
{
	Json::Value r = Invoke("scene_editor.get_project_state");
	EXPECT_EQ(r["diagamePath"].asString(), "");
}

// ===========================================================================
// get_project_state — after project cleared
// ===========================================================================

TEST_F(SceneEditorPluginTest, GetProjectState_AfterClear_IsValidFalse)
{
	mModel->ClearDiagameProject();

	Json::Value r = Invoke("scene_editor.get_project_state");
	EXPECT_FALSE(r["isValid"].asBool());
	EXPECT_EQ(r["diagamePath"].asString(), "");
}

// ===========================================================================
// get_project_state — payload has required fields for UI overlay wiring
// ===========================================================================

TEST_F(SceneEditorPluginTest, GetProjectState_AlwaysHasBothFields)
{
	Json::Value r = Invoke("scene_editor.get_project_state");
	EXPECT_TRUE(r.isMember("isValid"));
	EXPECT_TRUE(r.isMember("diagamePath"));
}

// ===========================================================================
// get_stage_list — returns success and stages array
// ===========================================================================

TEST_F(SceneEditorPluginTest, GetStageList_ReturnsSuccessAndStagesArray)
{
	Json::Value r = Invoke("scene_editor.get_stage_list");
	EXPECT_TRUE(r["success"].asBool());
	// stages is either an array (empty or populated) or null when no project loaded
	EXPECT_TRUE(r["stages"].isArray() || r["stages"].isNull());
}

// ===========================================================================
// Regression: project loaded before OnLoad — get_project_state is valid immediately
//
// Before the fix, RegisterRequestHandlers() was called before mDiagamePath was
// populated from the model. If the UI polled get_project_state before the first
// OnProjectChanged callback fired, it saw isValid=false and showed the overlay.
// ===========================================================================

namespace
{
	// Write a minimal valid .diagame to disk; caller removes it.
	const char* WriteDiagame(const char* path)
	{
		std::ofstream f(path);
		f << "{\"name\":\"Test\",\"version\":\"1.0\",\"imports\":[],\"config\":{}}\n";
		return path;
	}
}

TEST(SceneEditorPluginProjectState, OnLoad_WithPreloadedProject_GetProjectState_IsValidTrue)
{
	// Simulate the common case: user selects .diagame, THEN opens the Scene Editor panel.
	// The model already has a valid context when OnLoad is called.
	const char* path = "test_sceneeditor_preloaded.diagame";
	WriteDiagame(path);

	WebUIBridge bridge(nullptr);
	EditorModel model;
	model.LoadDiagameProject(path);
	ASSERT_TRUE(model.GetDiagameProject().IsValid());

	DiaSceneEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = &model;

	plugin.OnLoad(ctx);

	Json::Value r = bridge.InvokeRequestHandler(StringCRC("scene_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_TRUE(r["isValid"].asBool());
	EXPECT_STREQ(r["diagamePath"].asCString(), path);

	plugin.OnUnload();
	std::remove(path);
}

TEST(SceneEditorPluginProjectState, OnLoad_ProjectLoadedAfterOnLoad_GetProjectState_UpdatesViaCallback)
{
	// Complementary case: panel opened first, project loaded after.
	// The OnProjectChanged callback must update state so a subsequent poll returns valid.
	const char* path = "test_sceneeditor_postload.diagame";
	WriteDiagame(path);

	WebUIBridge bridge(nullptr);
	EditorModel model;

	DiaSceneEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = &model;
	plugin.OnLoad(ctx);

	// No project yet — overlay should show.
	Json::Value before = bridge.InvokeRequestHandler(StringCRC("scene_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_FALSE(before["isValid"].asBool());

	// Project loaded after OnLoad — callback fires, mDiagamePath updated.
	model.LoadDiagameProject(path);

	Json::Value after = bridge.InvokeRequestHandler(StringCRC("scene_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_TRUE(after["isValid"].asBool());
	EXPECT_STREQ(after["diagamePath"].asCString(), path);

	plugin.OnUnload();
	std::remove(path);
}
