// Integration tests for DiaApplicationFlowEditorPlugin.
//
// Uses a real WebUIBridge(nullptr) + EditorModel — same pattern as all other
// editor plugin integration tests. No mocking required; null UISystem is safe.
//
// Scope: lifecycle (OnLoad/OnUnload), handler registration, and the no-manifest
// state returned by manifest.getState. Full preload round-trips (loading a real
// .diaapp from disk) are covered by TestManifestLoader.cpp + the pipeline tests.

#include <gtest/gtest.h>
#include <DiaApplicationEditor/DiaApplicationFlowEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
// ===========================================================================

class AppFlowEditorPluginTest : public ::testing::Test
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

	DiaApplicationFlowEditorPlugin  mPlugin;
	WebUIBridge*                    mBridge = nullptr;
	EditorModel*                    mModel  = nullptr;
};

// ===========================================================================
// Lifecycle — null inputs don't crash
// ===========================================================================

TEST(AppFlowEditorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
	DiaApplicationFlowEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge   = nullptr;
	ctx.mModel    = nullptr;
	ctx.mServices = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

TEST(AppFlowEditorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
	WebUIBridge bridge(nullptr);
	DiaApplicationFlowEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge   = &bridge;
	ctx.mModel    = nullptr;
	ctx.mServices = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Lifecycle — all handlers registered on OnLoad
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_ManifestLoad)
{
	EXPECT_FALSE(Invoke("manifest.load").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_ManifestSave)
{
	EXPECT_FALSE(Invoke("manifest.save").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_ManifestGetState)
{
	EXPECT_FALSE(Invoke("manifest.getState").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_ManifestApplyCommand)
{
	EXPECT_FALSE(Invoke("manifest.applyCommand").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_HistoryUndo)
{
	EXPECT_FALSE(Invoke("history.undo").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_HistoryRedo)
{
	EXPECT_FALSE(Invoke("history.redo").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_HistoryGetState)
{
	EXPECT_FALSE(Invoke("history.getState").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_ValidationRun)
{
	EXPECT_FALSE(Invoke("validation.run").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_TypesGet)
{
	EXPECT_FALSE(Invoke("types.get").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_TypesRefresh)
{
	EXPECT_FALSE(Invoke("types.refresh").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_RiskCheck)
{
	EXPECT_FALSE(Invoke("risk.check").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_RiskConfirm)
{
	EXPECT_FALSE(Invoke("risk.confirm").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_LiveConnect)
{
	EXPECT_FALSE(Invoke("live.connect").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_LiveDisconnect)
{
	EXPECT_FALSE(Invoke("live.disconnect").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_LiveGetStatus)
{
	EXPECT_FALSE(Invoke("live.getStatus").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_LiveTransitionTo)
{
	EXPECT_FALSE(Invoke("live.transitionTo").isNull());
}

TEST_F(AppFlowEditorPluginTest, OnLoad_HandlerRegistered_LiveShutdown)
{
	EXPECT_FALSE(Invoke("live.shutdown").isNull());
}

// ===========================================================================
// Lifecycle — OnUnload removes all handlers
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, OnUnload_RemovesAllHandlers)
{
	mPlugin.OnUnload();

	EXPECT_TRUE(Invoke("manifest.load").isNull());
	EXPECT_TRUE(Invoke("manifest.save").isNull());
	EXPECT_TRUE(Invoke("manifest.getState").isNull());
	EXPECT_TRUE(Invoke("manifest.applyCommand").isNull());
	EXPECT_TRUE(Invoke("history.undo").isNull());
	EXPECT_TRUE(Invoke("history.redo").isNull());
	EXPECT_TRUE(Invoke("history.getState").isNull());
	EXPECT_TRUE(Invoke("validation.run").isNull());
	EXPECT_TRUE(Invoke("types.get").isNull());
	EXPECT_TRUE(Invoke("types.refresh").isNull());
	EXPECT_TRUE(Invoke("risk.check").isNull());
	EXPECT_TRUE(Invoke("risk.confirm").isNull());
	EXPECT_TRUE(Invoke("live.connect").isNull());
	EXPECT_TRUE(Invoke("live.disconnect").isNull());
	EXPECT_TRUE(Invoke("live.getStatus").isNull());
	EXPECT_TRUE(Invoke("live.transitionTo").isNull());
	EXPECT_TRUE(Invoke("live.shutdown").isNull());

	// Re-load so TearDown doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge   = mBridge;
	ctx.mModel    = mModel;
	ctx.mServices = nullptr;
	mPlugin.OnLoad(ctx);
}

// ===========================================================================
// manifest.getState — no manifest loaded
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, ManifestGetState_NoManifestLoaded_ReturnsOkFalse)
{
	Json::Value r = Invoke("manifest.getState");
	EXPECT_FALSE(r["ok"].asBool());
}

TEST_F(AppFlowEditorPluginTest, ManifestGetState_NoManifestLoaded_HasErrorField)
{
	Json::Value r = Invoke("manifest.getState");
	EXPECT_TRUE(r.isMember("error"));
	EXPECT_FALSE(r["error"].asString().empty());
}

// ===========================================================================
// manifest.load — missing path returns error, not null
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, ManifestLoad_MissingPath_ReturnsError)
{
	Json::Value r = Invoke("manifest.load");
	EXPECT_FALSE(r.isNull());
	EXPECT_FALSE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("error"));
}

TEST_F(AppFlowEditorPluginTest, ManifestLoad_EmptyPath_ReturnsError)
{
	Json::Value data;
	data["path"] = "";
	Json::Value r = Invoke("manifest.load", data);
	EXPECT_FALSE(r["ok"].asBool());
}

TEST_F(AppFlowEditorPluginTest, ManifestLoad_NonexistentFile_ReturnsError)
{
	Json::Value data;
	data["path"] = "nonexistent_xyz.diaapp";
	Json::Value r = Invoke("manifest.load", data);
	EXPECT_FALSE(r["ok"].asBool());
}

// ===========================================================================
// history.getState — returns valid shape with no manifest loaded
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, HistoryGetState_NoManifest_ReturnsShape)
{
	Json::Value r = Invoke("history.getState");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("canUndo"));
	EXPECT_TRUE(r.isMember("canRedo"));
	EXPECT_FALSE(r["canUndo"].asBool());
	EXPECT_FALSE(r["canRedo"].asBool());
}

// ===========================================================================
// live.getStatus — returns shape with no connection
// ===========================================================================

TEST_F(AppFlowEditorPluginTest, LiveGetStatus_NoConnection_ReturnsShape)
{
	Json::Value r = Invoke("live.getStatus");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("connected"));
	EXPECT_FALSE(r["connected"].asBool());
}
