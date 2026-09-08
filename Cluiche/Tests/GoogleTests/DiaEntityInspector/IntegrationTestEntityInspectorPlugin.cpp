// Integration tests for DiaEntityInspectorPlugin and EntityWatchListController.
//
// Uses a real WebUIBridge(nullptr) — null UISystem is safe for all handler
// operations (Register/Invoke/Unregister). NotifyUIDataChanged is a no-op
// with null UISystem. No mocking required.
//
// The plugin only registers entity_inspector.get_connection_state directly.
// Watch handlers (watch_add, watch_remove, watch_get) are owned by
// EntityWatchListController and tested via that controller's interface.

#include <gtest/gtest.h>
#include <DiaEntityInspector/DiaEntityInspectorPlugin.h>
#include <DiaEntityInspector/EntityWatchListController.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::EntityInspector;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture — plugin-level tests
// ===========================================================================

class EntityInspectorPluginTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mBridge = new WebUIBridge(nullptr);  // null UISystem — safe for handler tests
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

    DiaEntityInspectorPlugin  mPlugin;
    WebUIBridge*              mBridge = nullptr;
    EditorModel*              mModel  = nullptr;
};

// ===========================================================================
// Fixture — watch list controller tests
// Uses the bridge directly after activating the controller.
// ===========================================================================

class WatchListControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mBridge = new WebUIBridge(nullptr);
        mController.Activate(mBridge);
        mController.RegisterHandlers();
    }

    void TearDown() override
    {
        mController.UnregisterHandlers();
        mController.Deactivate();
        delete mBridge;
    }

    Json::Value Invoke(const char* command,
                       const Json::Value& data = Json::Value(Json::objectValue))
    {
        return mBridge->InvokeRequestHandler(StringCRC(command), data);
    }

    Json::Value MakeWatchEntry(const char* entity, const char* component, const char* field)
    {
        Json::Value data;
        data["entity"]    = entity;
        data["component"] = component;
        data["field"]     = field;
        return data;
    }

    EntityWatchListController  mController;
    WebUIBridge*               mBridge = nullptr;
};

// ===========================================================================
// Test 1: OnLoad_HandlerRegistered_GetConnectionState
// Invoking get_connection_state must return a non-null value with "connected".
// ===========================================================================

TEST_F(EntityInspectorPluginTest, OnLoad_HandlerRegistered_GetConnectionState)
{
    Json::Value r = Invoke("entity_inspector.get_connection_state");
    EXPECT_FALSE(r.isNull());
    EXPECT_TRUE(r.isMember("connected"));
}

// ===========================================================================
// Test 2: OnLoad_GetConnectionState_NotConnected
// Without a manager, connected must be false.
// ===========================================================================

TEST_F(EntityInspectorPluginTest, OnLoad_GetConnectionState_NotConnected)
{
    Json::Value r = Invoke("entity_inspector.get_connection_state");
    EXPECT_FALSE(r["connected"].asBool());
}

// ===========================================================================
// Test 3: OnLoad_HandlerRegistered_WatchAdd
// The watch_add handler must be registered and return a non-null value.
// ===========================================================================

TEST_F(WatchListControllerTest, OnLoad_HandlerRegistered_WatchAdd)
{
    // Invoke with missing fields — expect a response (not null), not a crash.
    Json::Value r = Invoke("entity_inspector.watch_add");
    EXPECT_FALSE(r.isNull());
    EXPECT_TRUE(r.isMember("success"));
}

// ===========================================================================
// Test 4: OnLoad_HandlerRegistered_WatchRemove
// The watch_remove handler must be registered and return a non-null value.
// ===========================================================================

TEST_F(WatchListControllerTest, OnLoad_HandlerRegistered_WatchRemove)
{
    Json::Value r = Invoke("entity_inspector.watch_remove");
    EXPECT_FALSE(r.isNull());
    EXPECT_TRUE(r.isMember("success"));
}

// ===========================================================================
// Test 5: OnLoad_HandlerRegistered_WatchGet
// The watch_get handler must return success=true and a watchList array.
// ===========================================================================

TEST_F(WatchListControllerTest, OnLoad_HandlerRegistered_WatchGet)
{
    Json::Value r = Invoke("entity_inspector.watch_get");
    EXPECT_FALSE(r.isNull());
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(r.isMember("watchList"));
    EXPECT_TRUE(r["watchList"].isArray());
}

// ===========================================================================
// Test 6: OnUnload_RemovesGetConnectionStateHandler
// After OnUnload, get_connection_state returns null (no handler).
// ===========================================================================

TEST_F(EntityInspectorPluginTest, OnUnload_RemovesGetConnectionStateHandler)
{
    mPlugin.OnUnload();
    EXPECT_TRUE(Invoke("entity_inspector.get_connection_state").isNull());
    // Re-load so TearDown's OnUnload doesn't double-unregister
    EditorPluginContext ctx;
    ctx.mBridge = mBridge;
    ctx.mModel  = mModel;
    mPlugin.OnLoad(ctx);
}

// ===========================================================================
// Test 7: OnUnload_RemovesWatchHandlers
// After UnregisterHandlers, all watch handlers return null.
// ===========================================================================

TEST_F(WatchListControllerTest, OnUnload_RemovesWatchHandlers)
{
    mController.UnregisterHandlers();

    EXPECT_TRUE(Invoke("entity_inspector.watch_add").isNull());
    EXPECT_TRUE(Invoke("entity_inspector.watch_remove").isNull());
    EXPECT_TRUE(Invoke("entity_inspector.watch_get").isNull());

    // Re-register so TearDown's UnregisterHandlers doesn't double-unregister
    mController.RegisterHandlers();
}

// ===========================================================================
// Test 8: OnLoad_NullBridge_DoesNotCrash
// Passing a null bridge to OnLoad must not crash.
// ===========================================================================

TEST(EntityInspectorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
    DiaEntityInspectorPlugin plugin;
    EditorPluginContext ctx;
    ctx.mBridge = nullptr;
    ctx.mModel  = nullptr;
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Test 9: OnLoad_NullModel_DoesNotCrash
// A valid bridge with null model must not crash on load.
// ===========================================================================

TEST(EntityInspectorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
    WebUIBridge bridge(nullptr);
    DiaEntityInspectorPlugin plugin;
    EditorPluginContext ctx;
    ctx.mBridge = &bridge;
    ctx.mModel  = nullptr;
    EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
    EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Test 10: WatchAdd_MissingFields_ReturnsError
// Missing entity/component/field → success=false with an error string.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchAdd_MissingFields_ReturnsError)
{
    // No fields at all
    Json::Value r = Invoke("entity_inspector.watch_add");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(WatchListControllerTest, WatchAdd_MissingComponent_ReturnsError)
{
    Json::Value data;
    data["entity"] = "player";
    data["field"]  = "x";
    // missing "component"
    Json::Value r = Invoke("entity_inspector.watch_add", data);
    EXPECT_FALSE(r["success"].asBool());
}

TEST_F(WatchListControllerTest, WatchAdd_MissingEntity_ReturnsError)
{
    Json::Value data;
    data["component"] = "Transform2D";
    data["field"]     = "x";
    // missing "entity"
    Json::Value r = Invoke("entity_inspector.watch_add", data);
    EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Test 11: WatchAdd_ValidEntry_AddsToList
// A complete entry must be added and visible via watch_get.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchAdd_ValidEntry_AddsToList)
{
    Json::Value r = Invoke("entity_inspector.watch_add",
                           MakeWatchEntry("player", "Transform2D", "x"));
    EXPECT_TRUE(r["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_TRUE(list["success"].asBool());
    ASSERT_EQ(list["watchList"].size(), 1u);
    EXPECT_EQ(list["watchList"][0]["entity"].asString(),    "player");
    EXPECT_EQ(list["watchList"][0]["component"].asString(), "Transform2D");
    EXPECT_EQ(list["watchList"][0]["field"].asString(),     "x");
}

// ===========================================================================
// Test 12: WatchAdd_MultipleEntries
// Adding 3 distinct entries → watch_get shows all 3 in insertion order.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchAdd_MultipleEntries)
{
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("player",  "Transform2D", "x"))["success"].asBool());
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("enemy",   "Health",      "current_hp"))["success"].asBool());
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("camera",  "Camera2D",    "zoom"))["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_TRUE(list["success"].asBool());
    ASSERT_EQ(list["watchList"].size(), 3u);
    EXPECT_EQ(list["watchList"][0]["entity"].asString(), "player");
    EXPECT_EQ(list["watchList"][1]["entity"].asString(), "enemy");
    EXPECT_EQ(list["watchList"][2]["entity"].asString(), "camera");
}

// ===========================================================================
// Test 13: WatchRemove_ValidIndex_RemovesEntry
// Add 2 entries, remove index 0 → watch_get shows exactly 1 remaining.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchRemove_ValidIndex_RemovesEntry)
{
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("player", "Transform2D", "x"))["success"].asBool());
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("enemy",  "Health",      "current_hp"))["success"].asBool());

    Json::Value removeData;
    removeData["index"] = 0;
    Json::Value r = Invoke("entity_inspector.watch_remove", removeData);
    EXPECT_TRUE(r["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_EQ(list["watchList"].size(), 1u);
    EXPECT_EQ(list["watchList"][0]["entity"].asString(), "enemy");
}

// ===========================================================================
// Test 14: WatchRemove_InvalidIndex_ReturnsError
// Out-of-range index → success=false.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchRemove_InvalidIndex_ReturnsError)
{
    // Empty list — any index is out of range
    Json::Value removeData;
    removeData["index"] = 99;
    Json::Value r = Invoke("entity_inspector.watch_remove", removeData);
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(WatchListControllerTest, WatchRemove_NegativeIndex_ReturnsError)
{
    Json::Value removeData;
    removeData["index"] = -1;
    Json::Value r = Invoke("entity_inspector.watch_remove", removeData);
    EXPECT_FALSE(r["success"].asBool());
}

TEST_F(WatchListControllerTest, WatchRemove_MissingIndex_ReturnsError)
{
    // No "index" field at all
    Json::Value r = Invoke("entity_inspector.watch_remove");
    EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Test 15: WatchGet_EmptyList_ReturnsEmptyArray
// Fresh controller → watchList is an empty array.
// ===========================================================================

TEST_F(WatchListControllerTest, WatchGet_EmptyList_ReturnsEmptyArray)
{
    Json::Value r = Invoke("entity_inspector.watch_get");
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["watchList"].size(), 0u);
}

// ===========================================================================
// Additional coverage: remove last element leaves empty list
// ===========================================================================

TEST_F(WatchListControllerTest, WatchRemove_LastEntry_LeavesEmptyList)
{
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("player", "Transform2D", "x"))["success"].asBool());

    Json::Value removeData;
    removeData["index"] = 0;
    ASSERT_TRUE(Invoke("entity_inspector.watch_remove", removeData)["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    EXPECT_EQ(list["watchList"].size(), 0u);
}

// ===========================================================================
// Additional coverage: remove middle element preserves order of others
// ===========================================================================

TEST_F(WatchListControllerTest, WatchRemove_MiddleEntry_PreservesOrder)
{
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("a", "CA", "f1"))["success"].asBool());
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("b", "CB", "f2"))["success"].asBool());
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("c", "CC", "f3"))["success"].asBool());

    Json::Value removeData;
    removeData["index"] = 1;  // remove "b"
    ASSERT_TRUE(Invoke("entity_inspector.watch_remove", removeData)["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_EQ(list["watchList"].size(), 2u);
    EXPECT_EQ(list["watchList"][0]["entity"].asString(), "a");
    EXPECT_EQ(list["watchList"][1]["entity"].asString(), "c");
}

// ===========================================================================
// Additional coverage: new entry has null value (not yet populated)
// ===========================================================================

TEST_F(WatchListControllerTest, WatchAdd_NewEntry_HasNullValue)
{
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("player", "Transform2D", "x"))["success"].asBool());

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_EQ(list["watchList"].size(), 1u);
    EXPECT_TRUE(list["watchList"][0]["value"].isNull());
}

// ===========================================================================
// Additional coverage: WatchListController with null bridge does not crash
// ===========================================================================

TEST(WatchListControllerLifecycle, NullBridge_DoesNotCrash)
{
    EntityWatchListController controller;
    EXPECT_NO_FATAL_FAILURE(controller.Activate(nullptr));
    EXPECT_NO_FATAL_FAILURE(controller.RegisterHandlers());
    EXPECT_NO_FATAL_FAILURE(controller.UnregisterHandlers());
    EXPECT_NO_FATAL_FAILURE(controller.Deactivate());
}

// ===========================================================================
// Additional coverage: OnConnectionStateChanged clears values
// ===========================================================================

TEST_F(WatchListControllerTest, OnConnectionStateChanged_ClearsWatchValues)
{
    // Add an entry
    ASSERT_TRUE(Invoke("entity_inspector.watch_add",
                       MakeWatchEntry("player", "Transform2D", "x"))["success"].asBool());

    // Simulate reconnect — values should be nulled
    mController.OnConnectionStateChanged(true);

    Json::Value list = Invoke("entity_inspector.watch_get");
    ASSERT_EQ(list["watchList"].size(), 1u);
    EXPECT_TRUE(list["watchList"][0]["value"].isNull());
}
