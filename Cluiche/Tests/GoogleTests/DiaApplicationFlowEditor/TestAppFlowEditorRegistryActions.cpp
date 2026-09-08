// Tests for the 10 DiaEditorAPI actions added to DiaApplicationFlowEditorPlugin:
//   manifest.load, manifest.save, manifest.getState, manifest.applyCommand
//   history.undo, history.redo, history.getState
//   validation.run, types.get, risk.check
//
// These actions are registered via DualRegisterActions() into EditorActionRegistry —
// they are NOT WebUIBridge handlers. Tests call ExecuteAction() on the registry.
// WebUIBridge handler tests are in IntegrationTestApplicationFlowEditorPlugin.cpp.

#include <gtest/gtest.h>
#include <DiaApplicationFlowEditor/DiaApplicationFlowEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
//
// Wires a real EditorActionRegistry so DualRegisterActions() can register
// the 10 actions. Tests call ExecuteAction() instead of InvokeRequestHandler().
// ===========================================================================

class AppFlowEditorRegistryActionsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mBridge   = new WebUIBridge(nullptr);
        mModel    = new EditorModel();
        mRegistry = new EditorActionRegistry();
        mRegistry->Initialize();
        mRegSvc   = new EditorActionRegistryService(mRegistry);
        mServices = new PluginServiceLocator();
        mServices->RegisterService<EditorActionRegistryService>(mRegSvc);

        EditorPluginContext ctx;
        ctx.mBridge       = mBridge;
        ctx.mModel        = mModel;
        ctx.mView         = nullptr;
        ctx.mPluginLoader = nullptr;
        ctx.mServices     = mServices;

        mPlugin.OnLoad(ctx);
    }

    void TearDown() override
    {
        mPlugin.OnUnload();
        mRegistry->Shutdown();
        delete mServices;
        delete mRegSvc;
        delete mRegistry;
        delete mModel;
        delete mBridge;
    }

    Json::Value Execute(const char* action,
                        const Json::Value& data = Json::Value(Json::objectValue))
    {
        return mRegistry->ExecuteAction(Dia::Core::StringCRC(action), data);
    }

    DiaApplicationFlowEditorPlugin  mPlugin;
    WebUIBridge*                    mBridge   = nullptr;
    EditorModel*                    mModel    = nullptr;
    EditorActionRegistry*           mRegistry = nullptr;
    EditorActionRegistryService*    mRegSvc   = nullptr;
    PluginServiceLocator*           mServices = nullptr;
};

// ===========================================================================
// Registration — all 10 actions reachable via ExecuteAction (not unknown_action)
// ===========================================================================

TEST_F(AppFlowEditorRegistryActionsTest, ManifestLoad_Registered)
{
    Json::Value r = Execute("manifest.load");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, ManifestSave_Registered)
{
    Json::Value r = Execute("manifest.save");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, ManifestGetState_Registered)
{
    Json::Value r = Execute("manifest.getState");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, ManifestApplyCommand_Registered)
{
    Json::Value r = Execute("manifest.applyCommand");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryUndo_Registered)
{
    Json::Value r = Execute("history.undo");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryRedo_Registered)
{
    Json::Value r = Execute("history.redo");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryGetState_Registered)
{
    Json::Value r = Execute("history.getState");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, ValidationRun_Registered)
{
    Json::Value r = Execute("validation.run");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, TypesGet_Registered)
{
    Json::Value r = Execute("types.get");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

TEST_F(AppFlowEditorRegistryActionsTest, RiskCheck_Registered)
{
    Json::Value r = Execute("risk.check");
    EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

// ===========================================================================
// Deregistration — OnUnload removes all 10 from the registry
// ===========================================================================

TEST_F(AppFlowEditorRegistryActionsTest, OnUnload_RemovesAllRegistryActions)
{
    mPlugin.OnUnload();

    static const char* kActions[] = {
        "manifest.load", "manifest.save", "manifest.getState", "manifest.applyCommand",
        "history.undo", "history.redo", "history.getState",
        "validation.run", "types.get", "risk.check"
    };
    for (const char* action : kActions)
    {
        Json::Value r = Execute(action);
        EXPECT_EQ(r.get("reason", "").asString(), "unknown_action")
            << action << " still registered after OnUnload";
    }

    // Re-load so TearDown doesn't double-unregister
    EditorPluginContext ctx;
    ctx.mBridge   = mBridge;
    ctx.mModel    = mModel;
    ctx.mServices = mServices;
    mPlugin.OnLoad(ctx);
}

// ===========================================================================
// Response shapes — no-manifest-loaded state returns correct structure
// ===========================================================================

TEST_F(AppFlowEditorRegistryActionsTest, ManifestGetState_NoManifest_ReturnsOkFalse)
{
    Json::Value r = Execute("manifest.getState");
    EXPECT_TRUE(r.isMember("ok"));
    EXPECT_FALSE(r["ok"].asBool());
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryGetState_ReturnsRequiredShape)
{
    Json::Value r = Execute("history.getState");
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("canUndo"));
    EXPECT_TRUE(r.isMember("canRedo"));
    EXPECT_TRUE(r.isMember("count"));
    EXPECT_TRUE(r.isMember("isDirty"));
    EXPECT_FALSE(r["canUndo"].asBool());
    EXPECT_FALSE(r["canRedo"].asBool());
}

TEST_F(AppFlowEditorRegistryActionsTest, ValidationRun_NoManifest_ReturnsOkFalse)
{
    Json::Value r = Execute("validation.run");
    EXPECT_TRUE(r.isMember("ok"));
    EXPECT_FALSE(r["ok"].asBool());
}

TEST_F(AppFlowEditorRegistryActionsTest, TypesGet_ReturnsRequiredShape)
{
    Json::Value r = Execute("types.get");
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("moduleTypes"));
    EXPECT_TRUE(r.isMember("puTypes"));
    EXPECT_TRUE(r["moduleTypes"].isArray());
    EXPECT_TRUE(r["puTypes"].isArray());
}

TEST_F(AppFlowEditorRegistryActionsTest, RiskCheck_NotLiveConnected_HasRiskFalse)
{
    Json::Value data;
    data["commandType"] = "RemovePU";
    Json::Value r = Execute("risk.check", data);
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("hasRisk"));
    EXPECT_FALSE(r["hasRisk"].asBool());
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryUndo_NothingToUndo_ReturnsOkFalse)
{
    Json::Value r = Execute("history.undo");
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("canUndo"));
    EXPECT_FALSE(r["canUndo"].asBool());
}

TEST_F(AppFlowEditorRegistryActionsTest, HistoryRedo_NothingToRedo_ReturnsOkFalse)
{
    Json::Value r = Execute("history.redo");
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("canRedo"));
    EXPECT_FALSE(r["canRedo"].asBool());
}
