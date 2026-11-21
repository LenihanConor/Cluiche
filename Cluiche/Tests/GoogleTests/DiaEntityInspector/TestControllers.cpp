// Direct unit tests for EntityInspectorController, QueryBrowserController,
// and MailboxMonitorController.
//
// Uses a SpyUISystem to capture the last topic+payload pushed via
// NotifyUIDataChanged, since WebUIBridge is a no-op with a null UISystem.

#include <gtest/gtest.h>
#include <DiaEntityInspector/EntityInspectorController.h>
#include <DiaEntityInspector/QueryBrowserController.h>
#include <DiaEntityInspector/MailboxMonitorController.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaUI/IUISystem.h>
#include <DiaCore/Json/external/json/json.h>
#include <string>
#include <vector>

using namespace Dia::EntityInspector;

// ===========================================================================
// SpyUISystem — captures the most-recent CallJSFunction arguments
// ===========================================================================

class SpyUISystem : public Dia::UI::IUISystem
{
public:
    std::string lastFunctionName;
    std::string lastArgsJson;
    int         callCount = 0;

    void CallJSFunction(const char* functionName, const char* argsJson) override
    {
        lastFunctionName = functionName ? functionName : "";
        lastArgsJson     = argsJson     ? argsJson     : "";
        ++callCount;
    }

    // Minimal no-op overrides for pure virtuals not needed by these tests
    void Initialize() override {}
    void Shutdown()   override {}
    void LoadPage(Dia::UI::Page&) override {}
    void UnloadPage() override {}
    bool IsPageLoaded() const override { return false; }
    void Update() override {}
    void FetchUIDataBuffer(Dia::UI::UIDataBuffer&) const override {}
    Dia::UI::IPage* CreatePage(const char*, int, int) override { return nullptr; }
    void DestroyPage(Dia::UI::IPage*) override {}
    int  GetPageCount() const override { return 0; }
    void InjectMouseMove(int, int) override {}
    void InjectMouseDown(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseUp(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseClick(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseWheel(int, int) override {}

    // Parse the last captured JSON envelope and return its "topic" field.
    std::string LastTopic() const
    {
        if (lastArgsJson.empty()) return "";
        Json::Value v;
        Json::CharReaderBuilder b;
        std::string err;
        std::istringstream ss(lastArgsJson);
        if (!Json::parseFromStream(b, ss, &v, &err)) return "";
        return v.get("topic", "").asString();
    }

    // Parse the last captured JSON envelope and return its "data" field.
    Json::Value LastData() const
    {
        if (lastArgsJson.empty()) return Json::Value{};
        Json::Value v;
        Json::CharReaderBuilder b;
        std::string err;
        std::istringstream ss(lastArgsJson);
        if (!Json::parseFromStream(b, ss, &v, &err)) return Json::Value{};
        return v["data"];
    }
};

// ===========================================================================
// Shared fixture for all three controllers
// ===========================================================================

class ControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mBridge = new Dia::Editor::WebUIBridge(&mSpy);
    }
    void TearDown() override
    {
        delete mBridge;
    }

    // Build a minimal entity.inspect payload
    static Json::Value MakeInspectPayload(bool includeQueries   = false,
                                          bool includeMailbox   = false)
    {
        Json::Value p;
        p["entity"]["index"]      = 1;
        p["entity"]["gen"]        = 2;
        p["entity"]["debug_name"] = "Player";
        p["components"]           = Json::Value(Json::arrayValue);
        p["hierarchy"]["parent_index"] = -1;
        p["hierarchy"]["parent_gen"]   = 0;
        p["hierarchy"]["child_count"]  = 0;
        if (includeQueries)
        {
            Json::Value q;
            q["index"]        = 0;
            q["entity_count"] = 5;
            p["queries"].append(q);
        }
        else
        {
            p["queries"] = Json::Value(Json::arrayValue);
        }
        if (includeMailbox)
        {
            Json::Value m;
            m["frame"]         = 100;
            m["msg_type_name"] = "DamageMsg";
            p["mailbox_log"].append(m);
        }
        else
        {
            p["mailbox_log"] = Json::Value(Json::arrayValue);
        }
        return p;
    }

    SpyUISystem              mSpy;
    Dia::Editor::WebUIBridge* mBridge = nullptr;
};

// ===========================================================================
// EntityInspectorController
// ===========================================================================

TEST_F(ControllerTest, EntityInspector_OnInspectPayload_PushesInspectData)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload());

    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.inspect_data");
    EXPECT_EQ(mSpy.callCount,   1);
}

TEST_F(ControllerTest, EntityInspector_OnInspectPayload_DataContainsEntitySection)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload());

    Json::Value data = mSpy.LastData();
    EXPECT_TRUE(data.isMember("entity"));
    EXPECT_EQ(data["entity"]["debug_name"].asString(), "Player");
}

TEST_F(ControllerTest, EntityInspector_NullPayload_DoesNotNotify)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(Json::Value{});  // null

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, EntityInspector_NullBridge_DoesNotCrash)
{
    EntityInspectorController ctrl;
    ctrl.Activate(nullptr);
    EXPECT_NO_FATAL_FAILURE(ctrl.OnInspectPayload(MakeInspectPayload()));
}

TEST_F(ControllerTest, EntityInspector_AfterDeactivate_DoesNotNotify)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    ctrl.Deactivate();

    ctrl.OnInspectPayload(MakeInspectPayload());

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, EntityInspector_MultiplePayloads_NotifiesEachTime)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload());
    ctrl.OnInspectPayload(MakeInspectPayload());
    ctrl.OnInspectPayload(MakeInspectPayload());

    EXPECT_EQ(mSpy.callCount, 3);
}

// ===========================================================================
// QueryBrowserController
// ===========================================================================

TEST_F(ControllerTest, QueryBrowser_OnInspectPayload_WithQueries_PushesQueryData)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(/*includeQueries=*/true));

    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.query_data");
    EXPECT_EQ(mSpy.callCount,   1);
}

TEST_F(ControllerTest, QueryBrowser_OnInspectPayload_QueryDataHasQueriesArray)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(true));

    Json::Value data = mSpy.LastData();
    ASSERT_TRUE(data.isMember("queries"));
    EXPECT_TRUE(data["queries"].isArray());
    EXPECT_EQ(data["queries"].size(), 1u);
}

TEST_F(ControllerTest, QueryBrowser_OnInspectPayload_EmptyQueries_StillNotifies)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);

    // Payload has queries key but empty array
    ctrl.OnInspectPayload(MakeInspectPayload(/*includeQueries=*/false));

    // queries key is present (empty array) → should still notify
    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.query_data");
}

TEST_F(ControllerTest, QueryBrowser_OnInspectPayload_MissingQueriesKey_DoesNotNotify)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);

    Json::Value payload;
    payload["entity"]["index"] = 1;
    // no "queries" key at all

    ctrl.OnInspectPayload(payload);

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, QueryBrowser_NullPayload_DoesNotNotify)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(Json::Value{});

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, QueryBrowser_NullBridge_DoesNotCrash)
{
    QueryBrowserController ctrl;
    ctrl.Activate(nullptr);
    EXPECT_NO_FATAL_FAILURE(ctrl.OnInspectPayload(MakeInspectPayload(true)));
}

TEST_F(ControllerTest, QueryBrowser_AfterDeactivate_DoesNotNotify)
{
    QueryBrowserController ctrl;
    ctrl.Activate(mBridge);
    ctrl.Deactivate();

    ctrl.OnInspectPayload(MakeInspectPayload(true));

    EXPECT_EQ(mSpy.callCount, 0);
}

// ===========================================================================
// MailboxMonitorController
// ===========================================================================

TEST_F(ControllerTest, Mailbox_OnInspectPayload_WithEntries_PushesMailboxData)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(false, /*includeMailbox=*/true));

    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.mailbox_data");
    EXPECT_EQ(mSpy.callCount,   1);
}

TEST_F(ControllerTest, Mailbox_OnInspectPayload_DataHasLogArray)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(false, true));

    Json::Value data = mSpy.LastData();
    ASSERT_TRUE(data.isMember("log"));
    EXPECT_TRUE(data["log"].isArray());
    EXPECT_EQ(data["log"].size(), 1u);
}

TEST_F(ControllerTest, Mailbox_OnInspectPayload_AccumulatesAcrossPayloads)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(false, true));  // 1 entry
    ctrl.OnInspectPayload(MakeInspectPayload(false, true));  // 1 more

    Json::Value data = mSpy.LastData();
    EXPECT_EQ(data["log"].size(), 2u);
}

TEST_F(ControllerTest, Mailbox_RingBufferEvictsOldestWhenFull)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    // Push kRingBufferSize+1 entries one at a time
    for (unsigned int i = 0; i <= MailboxMonitorController::kRingBufferSize; ++i)
    {
        Json::Value payload = MakeInspectPayload(false, false);
        Json::Value entry;
        entry["frame"]         = static_cast<int>(i);
        entry["msg_type_name"] = "Msg";
        payload["mailbox_log"].append(entry);
        ctrl.OnInspectPayload(payload);
    }

    Json::Value data = mSpy.LastData();
    EXPECT_EQ(data["log"].size(), MailboxMonitorController::kRingBufferSize);
    // Oldest entry (frame 0) should have been evicted; first entry is now frame 1
    EXPECT_EQ(data["log"][0]["frame"].asInt(), 1);
}

TEST_F(ControllerTest, Mailbox_Clear_EmptiesLog)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(MakeInspectPayload(false, true));
    ASSERT_EQ(mSpy.LastData()["log"].size(), 1u);

    mSpy.callCount = 0;  // reset counter
    ctrl.Clear();

    EXPECT_EQ(mSpy.LastTopic(),              "entity_inspector.mailbox_data");
    EXPECT_EQ(mSpy.LastData()["log"].size(), 0u);
    EXPECT_EQ(mSpy.callCount,                1);
}

TEST_F(ControllerTest, Mailbox_Clear_WithNullBridge_DoesNotCrash)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(nullptr);
    EXPECT_NO_FATAL_FAILURE(ctrl.Clear());
}

TEST_F(ControllerTest, Mailbox_NullPayload_DoesNotNotify)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    ctrl.OnInspectPayload(Json::Value{});

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, Mailbox_MissingMailboxLogKey_DoesNotNotify)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    Json::Value payload;
    payload["entity"]["index"] = 1;
    // no "mailbox_log" key

    ctrl.OnInspectPayload(payload);

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, Mailbox_AfterDeactivate_DoesNotNotify)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);
    ctrl.Deactivate();

    ctrl.OnInspectPayload(MakeInspectPayload(false, true));

    EXPECT_EQ(mSpy.callCount, 0);
}

TEST_F(ControllerTest, Mailbox_NullBridge_DoesNotCrash)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(nullptr);
    EXPECT_NO_FATAL_FAILURE(ctrl.OnInspectPayload(MakeInspectPayload(false, true)));
}

TEST_F(ControllerTest, Mailbox_EmptyMailboxLog_StillNotifies)
{
    MailboxMonitorController ctrl;
    ctrl.Activate(mBridge);

    // mailbox_log key present but empty array — controller should still push (log stays empty)
    ctrl.OnInspectPayload(MakeInspectPayload(false, false));

    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.mailbox_data");
    EXPECT_EQ(mSpy.LastData()["log"].size(), 0u);
}

// ===========================================================================
// EntityInspectorController: entity_inspector.schema_get forwards valueschema.get
// ===========================================================================

namespace {
struct FakeGameLink
{
    std::string                                              lastCommand;
    Json::Value                                              lastArgs;
    std::function<void(bool, const Json::Value&)>            lastCallback;
    int                                                      sendCount = 0;
    bool                                                     accept = true;

    EntityInspectorController::SchemaSendFn Fn()
    {
        return [this](const char* command, const Json::Value& args,
                      std::function<void(bool, const Json::Value&)> cb) -> bool
        {
            ++sendCount;
            if (!accept) return false;
            lastCommand  = command;
            lastArgs     = args;
            lastCallback = cb;
            return true;
        };
    }
};
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_ForwardsValueSchemaGetWithPrefix)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    ctrl.RegisterHandlers(link.Fn());

    Json::Value req; req["prefix"] = "foo-component/";
    Json::Value res = mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), req);

    EXPECT_TRUE(res["success"].asBool());
    EXPECT_EQ(link.lastCommand, "valueschema.get");
    EXPECT_EQ(link.lastArgs["prefix"].asString(), "foo-component/");
    EXPECT_EQ(mSpy.callCount, 0);   // reply is asynchronous
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_ReplyIsPushedToUiWithPrefixAndEntries)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    ctrl.RegisterHandlers(link.Fn());

    Json::Value req; req["prefix"] = "foo-component/";
    mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), req);

    Json::Value payload;
    payload["version"] = 1;
    payload["entries"].append(Json::Value(Json::objectValue));
    payload["entries"][0]["key"] = "speed";
    link.lastCallback(true, payload);

    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.schema_data");
    Json::Value data = mSpy.LastData();
    EXPECT_EQ(data["prefix"].asString(), "foo-component/");
    EXPECT_TRUE(data["success"].asBool());
    ASSERT_EQ(data["entries"].size(), 1u);
    EXPECT_EQ(data["entries"][0]["key"].asString(), "speed");
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_FailedReplyPushesSuccessFalse)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    ctrl.RegisterHandlers(link.Fn());

    Json::Value req; req["prefix"] = "foo-component/";
    mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), req);
    link.lastCallback(false, Json::Value(Json::objectValue));

    Json::Value data = mSpy.LastData();
    EXPECT_FALSE(data["success"].asBool());
    EXPECT_EQ(data["entries"].size(), 0u);
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_NoLinkPushesFailureImmediately)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    link.accept = false;   // game not connected
    ctrl.RegisterHandlers(link.Fn());

    Json::Value req; req["prefix"] = "foo-component/";
    Json::Value res = mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), req);

    EXPECT_FALSE(res["success"].asBool());
    EXPECT_EQ(mSpy.LastTopic(), "entity_inspector.schema_data");
    EXPECT_FALSE(mSpy.LastData()["success"].asBool());
    EXPECT_EQ(mSpy.LastData()["prefix"].asString(), "foo-component/");
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_MissingPrefixIsRejectedWithoutSending)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    ctrl.RegisterHandlers(link.Fn());

    Json::Value res = mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), Json::Value(Json::objectValue));

    EXPECT_FALSE(res["success"].asBool());
    EXPECT_EQ(link.sendCount, 0);
}

TEST_F(ControllerTest, EntityInspector_SchemaGet_AfterUnregisterHandlerIsGone)
{
    EntityInspectorController ctrl;
    ctrl.Activate(mBridge);
    FakeGameLink link;
    ctrl.RegisterHandlers(link.Fn());
    ctrl.UnregisterHandlers();

    Json::Value req; req["prefix"] = "foo-component/";
    mBridge->InvokeRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"), req);

    EXPECT_EQ(link.sendCount, 0);
}
