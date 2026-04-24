// TestWebUIBridgeDispatch.cpp - Integration tests for WebUIBridge event and request dispatch
//
// Tests that registered handlers are correctly invoked when HandleEditorCall processes
// incoming JSON, covering fire-and-forget events and request-response cycles.
// Uses a mock IUISystem to capture JS calls.

#include <gtest/gtest.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaUI/IUISystem.h>
#include <DiaEditor/MVC/EditorViewController.h>
#include <DiaInput/EMouseButton.h>

#include <sstream>
#include <string>
#include <vector>

using namespace Dia::Editor;
using namespace Dia::Core;

// ==============================================================================
// Mock IUISystem
// ==============================================================================

class MockUISystem : public Dia::UI::IUISystem
{
public:
    struct Call
    {
        std::string functionName;
        std::string argsJson;
    };

    std::vector<Call> jsCalls;
    Dia::UI::IUISystem::JSHandler registeredHandler;
    std::string registeredHandlerName;

    void Initialize() override {}
    void Shutdown() override {}
    void LoadPage(Dia::UI::Page&) override {}
    void UnloadPage() override {}
    bool IsPageLoaded() const override { return false; }
    void Update() override {}
    void FetchUIDataBuffer(Dia::UI::UIDataBuffer&) const override {}
    Dia::UI::IPage* CreatePage(const char*, int, int) override { return nullptr; }
    void DestroyPage(Dia::UI::IPage*) override {}
    int GetPageCount() const override { return 0; }
    void InjectMouseMove(int, int) override {}
    void InjectMouseDown(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseUp(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseClick(Dia::Input::EMouseButton, int, int) override {}
    void InjectMouseWheel(int, int) override {}

    void RegisterJSHandler(const char* name, JSHandler handler) override
    {
        registeredHandlerName = name;
        registeredHandler = handler;
    }

    void CallJSFunction(const char* functionName, const char* argsJson) override
    {
        jsCalls.push_back({ functionName, argsJson ? argsJson : "" });
    }

    // Simulate JS calling into C++ via the registered handler
    std::string SimulateJSCall(const std::string& argsJson)
    {
        if (registeredHandler)
            return registeredHandler(argsJson);
        return "{}";
    }
};

// ==============================================================================
// Helpers: build call payloads as the JS side would
// ==============================================================================

static std::string MakeEventPayload(const char* eventType, const Json::Value& data = Json::Value())
{
    Json::Value msg;
    msg["type"] = eventType;
    msg["data"] = data;
    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    return Json::writeString(w, msg);
}

static std::string MakeRequestPayload(const char* eventType, const std::string& reqId,
    const Json::Value& data = Json::Value())
{
    Json::Value msg;
    msg["type"] = eventType;
    msg["reqId"] = reqId;
    msg["data"] = data;
    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    return Json::writeString(w, msg);
}

// ==============================================================================
// Test Fixture
// ==============================================================================

class WebUIBridgeDispatchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mUISystem = new MockUISystem();
        mBridge = new WebUIBridge(mUISystem);
        mController = new EditorViewController();
        mBridge->Initialize(mController);
    }

    void TearDown() override
    {
        delete mBridge;
        delete mController;
        delete mUISystem;
    }

    MockUISystem* mUISystem = nullptr;
    WebUIBridge* mBridge = nullptr;
    EditorViewController* mController = nullptr;
};

// ==============================================================================
// Fire-and-Forget Event Tests
// ==============================================================================

TEST_F(WebUIBridgeDispatchTest, Event_RegisteredHandler_Called)
{
    bool called = false;
    mBridge->RegisterEventHandler(StringCRC("my_event"), [&](const Json::Value&)
    {
        called = true;
    });

    mUISystem->SimulateJSCall(MakeEventPayload("my_event"));

    EXPECT_TRUE(called);
}

TEST_F(WebUIBridgeDispatchTest, Event_HandlerReceivesCorrectData)
{
    Json::Value received;
    mBridge->RegisterEventHandler(StringCRC("data_event"), [&](const Json::Value& data)
    {
        received = data;
    });

    Json::Value payload;
    payload["key"] = "hello";
    mUISystem->SimulateJSCall(MakeEventPayload("data_event", payload));

    EXPECT_STREQ(received.get("key", "").asCString(), "hello");
}

TEST_F(WebUIBridgeDispatchTest, Event_UnregisteredEvent_NoHandlerCalled)
{
    bool called = false;
    mBridge->RegisterEventHandler(StringCRC("other_event"), [&](const Json::Value&)
    {
        called = true;
    });

    mUISystem->SimulateJSCall(MakeEventPayload("unregistered_event"));

    EXPECT_FALSE(called);
}

TEST_F(WebUIBridgeDispatchTest, Event_MalformedJson_HandlerNotCalled)
{
    bool called = false;
    mBridge->RegisterEventHandler(StringCRC("my_event"), [&](const Json::Value&)
    {
        called = true;
    });

    mUISystem->SimulateJSCall("{ this is not valid json }");

    EXPECT_FALSE(called);
}

TEST_F(WebUIBridgeDispatchTest, Event_MalformedJson_NoCrash)
{
    mUISystem->SimulateJSCall("{ bad json ");
    // No assertion needed — test passes if no crash/exception
}

TEST_F(WebUIBridgeDispatchTest, Event_UnregisteredEvent_NoCrash)
{
    mUISystem->SimulateJSCall(MakeEventPayload("totally_unknown_event"));
    // No assertion needed — must not crash
}

TEST_F(WebUIBridgeDispatchTest, Event_UnregisterHandler_NotCalledAfterUnregister)
{
    bool called = false;
    mBridge->RegisterEventHandler(StringCRC("removable_event"), [&](const Json::Value&)
    {
        called = true;
    });

    mBridge->UnregisterEventHandler(StringCRC("removable_event"));
    mUISystem->SimulateJSCall(MakeEventPayload("removable_event"));

    EXPECT_FALSE(called);
}

// ==============================================================================
// Request-Response Tests
// ==============================================================================

TEST_F(WebUIBridgeDispatchTest, Request_RegisteredHandler_Called)
{
    bool called = false;
    mBridge->RegisterRequestHandler(StringCRC("my_request"), [&](const Json::Value&) -> Json::Value
    {
        called = true;
        return Json::Value();
    });

    mUISystem->SimulateJSCall(MakeRequestPayload("my_request", "req-1"));

    EXPECT_TRUE(called);
}

TEST_F(WebUIBridgeDispatchTest, Request_HandlerReceivesCorrectData)
{
    Json::Value received;
    mBridge->RegisterRequestHandler(StringCRC("req_with_data"), [&](const Json::Value& data) -> Json::Value
    {
        received = data;
        return Json::Value();
    });

    Json::Value payload;
    payload["count"] = 42;
    mUISystem->SimulateJSCall(MakeRequestPayload("req_with_data", "req-2", payload));

    EXPECT_EQ(received.get("count", 0).asInt(), 42);
}

TEST_F(WebUIBridgeDispatchTest, Request_ResponseSentToJS)
{
    mBridge->RegisterRequestHandler(StringCRC("get_data"), [](const Json::Value&) -> Json::Value
    {
        Json::Value result;
        result["answer"] = 99;
        return result;
    });

    mUISystem->SimulateJSCall(MakeRequestPayload("get_data", "req-3"));

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);
    EXPECT_STREQ(mUISystem->jsCalls[0].functionName.c_str(), "DiaEditor_onResponse");
}

TEST_F(WebUIBridgeDispatchTest, Request_ResponseContainsReqId)
{
    mBridge->RegisterRequestHandler(StringCRC("echo"), [](const Json::Value&) -> Json::Value
    {
        return Json::Value("pong");
    });

    mUISystem->SimulateJSCall(MakeRequestPayload("echo", "my-req-id-42"));

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);

    Json::Value responseEnvelope;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(mUISystem->jsCalls[0].argsJson);
    Json::parseFromStream(builder, stream, &responseEnvelope, &errors);

    EXPECT_STREQ(responseEnvelope.get("reqId", "").asCString(), "my-req-id-42");
}

TEST_F(WebUIBridgeDispatchTest, Request_ResponseContainsResult)
{
    mBridge->RegisterRequestHandler(StringCRC("get_value"), [](const Json::Value&) -> Json::Value
    {
        Json::Value result;
        result["value"] = "test_value";
        return result;
    });

    mUISystem->SimulateJSCall(MakeRequestPayload("get_value", "req-result-test"));

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);

    Json::Value responseEnvelope;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(mUISystem->jsCalls[0].argsJson);
    Json::parseFromStream(builder, stream, &responseEnvelope, &errors);

    EXPECT_STREQ(responseEnvelope["result"].get("value", "").asCString(), "test_value");
}

TEST_F(WebUIBridgeDispatchTest, Request_UnregisteredHandler_SendsEmptyResponse)
{
    mUISystem->SimulateJSCall(MakeRequestPayload("unknown_request", "req-unknown"));

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);
    EXPECT_STREQ(mUISystem->jsCalls[0].functionName.c_str(), "DiaEditor_onResponse");
}

TEST_F(WebUIBridgeDispatchTest, Request_UnregisterHandler_EmptyResponseAfterUnregister)
{
    int callCount = 0;
    mBridge->RegisterRequestHandler(StringCRC("removable_req"), [&](const Json::Value&) -> Json::Value
    {
        callCount++;
        return Json::Value();
    });

    mBridge->UnregisterRequestHandler(StringCRC("removable_req"));
    mUISystem->SimulateJSCall(MakeRequestPayload("removable_req", "req-removed"));

    EXPECT_EQ(callCount, 0);
}

// ==============================================================================
// NotifyUIDataChanged Tests
// ==============================================================================

TEST_F(WebUIBridgeDispatchTest, NotifyUIDataChanged_CallsJSFunction)
{
    Json::Value data;
    data["value"] = 1;
    mBridge->NotifyUIDataChanged("manifest_updated", data);

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);
    EXPECT_STREQ(mUISystem->jsCalls[0].functionName.c_str(), "DiaEditor_onDataChanged");
}

TEST_F(WebUIBridgeDispatchTest, NotifyUIDataChanged_EnvelopeContainsTopic)
{
    Json::Value data;
    mBridge->NotifyUIDataChanged("my_topic", data);

    ASSERT_EQ(mUISystem->jsCalls.size(), 1u);

    Json::Value envelope;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(mUISystem->jsCalls[0].argsJson);
    Json::parseFromStream(builder, stream, &envelope, &errors);

    EXPECT_STREQ(envelope.get("topic", "").asCString(), "my_topic");
}

TEST_F(WebUIBridgeDispatchTest, NotifyUIDataChanged_NullUISystem_NoCrash)
{
    WebUIBridge bridgeNoUI(nullptr);
    Json::Value data;
    bridgeNoUI.NotifyUIDataChanged("topic", data);
    // must not crash
}
