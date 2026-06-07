// TestInspectorPlugin.cpp - Integration tests for DiaApplicationFlowInspectorPlugin.
//
// Uses a real WebUIBridge(nullptr) + EditorModel — same pattern as all other
// editor plugin integration tests. No mocking required; null UISystem is safe.
//
// Scope: lifecycle (OnLoad/OnUnload), handler registration, and null-connection
// responses for all 5 live.* handlers.

#include <gtest/gtest.h>
#include <DiaApplicationFlowInspector/DiaApplicationFlowInspectorPlugin.h>
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

class InspectorPluginTest : public ::testing::Test
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
		ctx.mServices     = nullptr;   // no GameConnectionManager → mGameConnection stays null

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

	DiaApplicationFlowInspectorPlugin  mPlugin;
	WebUIBridge*                       mBridge = nullptr;
	EditorModel*                       mModel  = nullptr;
};

// ===========================================================================
// Lifecycle — null inputs don't crash
// ===========================================================================

TEST(InspectorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
	DiaApplicationFlowInspectorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge   = nullptr;
	ctx.mModel    = nullptr;
	ctx.mServices = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

TEST(InspectorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
	WebUIBridge bridge(nullptr);
	DiaApplicationFlowInspectorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge   = &bridge;
	ctx.mModel    = nullptr;
	ctx.mServices = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Lifecycle — OnLoad registers all five live.* handlers
// ===========================================================================

TEST_F(InspectorPluginTest, OnLoad_RegistersFiveLiveHandlers_LiveConnect)
{
	Json::Value data;
	data["host"] = "localhost";
	data["port"] = 7000;
	Json::Value r = Invoke("live.connect", data);
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("ok"));
}

TEST_F(InspectorPluginTest, OnLoad_RegistersFiveLiveHandlers_LiveDisconnect)
{
	Json::Value r = Invoke("live.disconnect");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("ok"));
}

TEST_F(InspectorPluginTest, OnLoad_RegistersFiveLiveHandlers_LiveGetStatus)
{
	Json::Value r = Invoke("live.getStatus");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("ok"));
}

TEST_F(InspectorPluginTest, OnLoad_RegistersFiveLiveHandlers_LiveTransitionTo)
{
	Json::Value data;
	data["stageName"] = "Boot";
	Json::Value r = Invoke("live.transitionTo", data);
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("ok"));
}

TEST_F(InspectorPluginTest, OnLoad_RegistersFiveLiveHandlers_LiveShutdown)
{
	Json::Value r = Invoke("live.shutdown");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("ok"));
}

// ===========================================================================
// Handler responses — null game connection (ctx.mServices == nullptr)
// ===========================================================================

TEST_F(InspectorPluginTest, LiveConnect_NoGameConnection_ReturnsOkFalse)
{
	Json::Value data;
	data["host"] = "localhost";
	data["port"] = 7000;
	Json::Value r = Invoke("live.connect", data);
	EXPECT_FALSE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("error"));
	EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(InspectorPluginTest, LiveDisconnect_NotConnected_ReturnsOkFalse)
{
	Json::Value r = Invoke("live.disconnect");
	EXPECT_FALSE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("error"));
}

TEST_F(InspectorPluginTest, LiveGetStatus_NoConnection_ReturnsOkTrueWithShape)
{
	Json::Value r = Invoke("live.getStatus");
	EXPECT_TRUE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("connected"));
	EXPECT_FALSE(r["connected"].asBool());
	EXPECT_TRUE(r.isMember("liveActive"));
	EXPECT_FALSE(r["liveActive"].asBool());
}

TEST_F(InspectorPluginTest, LiveTransitionTo_NotConnected_ReturnsOkFalse)
{
	Json::Value data;
	data["stageName"] = "Boot";
	Json::Value r = Invoke("live.transitionTo", data);
	EXPECT_FALSE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("error"));
}

TEST_F(InspectorPluginTest, LiveShutdown_NotConnected_ReturnsOkFalse)
{
	Json::Value r = Invoke("live.shutdown");
	EXPECT_FALSE(r["ok"].asBool());
	EXPECT_TRUE(r.isMember("error"));
}

// ===========================================================================
// Lifecycle — OnUnload + OnLoad completes cleanly
// ===========================================================================

TEST_F(InspectorPluginTest, OnUnload_AfterLoad_DoesNotCrash)
{
	EXPECT_NO_FATAL_FAILURE(mPlugin.OnUnload());

	// Re-load so TearDown doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge   = mBridge;
	ctx.mModel    = mModel;
	ctx.mServices = nullptr;
	mPlugin.OnLoad(ctx);
}
