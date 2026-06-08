#include <gtest/gtest.h>
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorViewController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaUI/IUISystem.h>
#include <DiaInput/EMouseButton.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <vector>

using namespace Dia::Editor;
using namespace Dia::Core;

// ==============================================================================
// Mock IUISystem — captures JS calls for assertion
// ==============================================================================

class LivePluginMockUISystem : public Dia::UI::IUISystem
{
public:
    struct JSCall { std::string fn; std::string args; };
    std::vector<JSCall> jsCalls;
    JSHandler registeredHandler;

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
    void RegisterJSHandler(const char*, JSHandler handler) override { registeredHandler = handler; }
    void CallJSFunction(const char* fn, const char* args) override
    {
        jsCalls.push_back({ fn, args ? args : "" });
    }
};

// ==============================================================================
// FakeGameConnectionManager — subclass of the real type (same kUniqueId).
// IsConnected/Subscribe/Unsubscribe are all virtual so we can override them.
// ==============================================================================

class FakeGameConnectionManager : public GameConnectionManager
{
public:
    struct SubscribeCall   { Dia::Core::StringCRC topic; };
    struct UnsubscribeCall { Dia::Core::StringCRC topic; };

    std::vector<SubscribeCall>   subscribeCalls;
    std::vector<UnsubscribeCall> unsubscribeCalls;

    bool mFakeConnected = false;

    bool IsConnected() const override { return mFakeConnected; }

    void Subscribe(const Dia::Core::StringCRC& topic,
                   GameConnectionManager::DataCallback /*cb*/) override
    {
        subscribeCalls.push_back({ topic });
    }

    void Unsubscribe(const Dia::Core::StringCRC& topic) override
    {
        unsubscribeCalls.push_back({ topic });
    }

    bool HasSubscribed(const Dia::Core::StringCRC& topic) const
    {
        for (auto& s : subscribeCalls)
            if (s.topic == topic) return true;
        return false;
    }

    bool HasUnsubscribed(const Dia::Core::StringCRC& topic) const
    {
        for (auto& u : unsubscribeCalls)
            if (u.topic == topic) return true;
        return false;
    }
};

// ==============================================================================
// Concrete test plugin — minimal, records each virtual call
// ==============================================================================

class TestLivePlugin : public LiveConnectionPluginBase
{
public:
    TestLivePlugin()
        : LiveConnectionPluginBase({
            "TestLivePlugin", "1.0", "desc",
            "dia://plugins/test/index.html",
            LayoutMode::kDockable,
            nullptr, nullptr, false
          }, "test_plugin")
    {}

    int liveLoadCount     = 0;
    int liveUnloadCount   = 0;
    int connectedCount    = 0;
    int disconnectedCount = 0;
    int liveUpdateCount   = 0;
    float lastDelta       = 0.0f;

    void RegisterTopic(const Dia::Core::StringCRC& topic,
                       GameConnectionManager::DataCallback cb)
    {
        RegisterGameTopic(topic, cb);
    }

    using LiveConnectionPluginBase::IsGameConnected;
    using LiveConnectionPluginBase::GetGameConnection;

protected:
    void OnLivePluginLoad()   override { ++liveLoadCount; }
    void OnLivePluginUnload() override { ++liveUnloadCount; }
    void OnGameConnected()    override { ++connectedCount; }
    void OnGameDisconnected() override { ++disconnectedCount; }
    void OnLiveUpdate(float dt) override { ++liveUpdateCount; lastDelta = dt; }
};

// ==============================================================================
// Test Fixture
// ==============================================================================

class LiveConnectionPluginBaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mUISystem   = new LivePluginMockUISystem();
        mBridge     = new WebUIBridge(mUISystem);
        mController = new EditorViewController();
        mBridge->Initialize(mController);

        mServices.RegisterService<GameConnectionManager>(&mFake);

        mContext.mBridge      = mBridge;
        mContext.mModel       = nullptr;
        mContext.mView        = nullptr;
        mContext.mPluginLoader = nullptr;
        mContext.mServices    = &mServices;
        mContext.mProjectPath = "C:/test";
    }

    void TearDown() override
    {
        delete mBridge;
        delete mController;
        delete mUISystem;
    }

    // OnUpdate is private on LiveConnectionPluginBase; call via the public IEditorPlugin interface.
    void Tick(float dt = 0.016f) { static_cast<IEditorPlugin&>(mPlugin).OnUpdate(dt); }

    bool JsCallContains(const std::string& fn, const std::string& substr) const
    {
        for (auto& c : mUISystem->jsCalls)
            if (c.fn == fn && c.args.find(substr) != std::string::npos)
                return true;
        return false;
    }

    LivePluginMockUISystem* mUISystem   = nullptr;
    WebUIBridge*            mBridge     = nullptr;
    EditorViewController*   mController = nullptr;
    FakeGameConnectionManager mFake;
    PluginServiceLocator    mServices;
    EditorPluginContext     mContext;
    TestLivePlugin          mPlugin;
};

// ==============================================================================
// 1. Lifecycle delegation
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, OnLoad_CallsOnLivePluginLoad)
{
    mPlugin.OnLoad(mContext);
    EXPECT_EQ(mPlugin.liveLoadCount, 1);
    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUnload_CallsOnLivePluginUnload)
{
    mPlugin.OnLoad(mContext);
    mPlugin.OnUnload();
    EXPECT_EQ(mPlugin.liveUnloadCount, 1);
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_CallsOnLiveUpdate)
{
    mPlugin.OnLoad(mContext);
    Tick(0.032f);
    EXPECT_EQ(mPlugin.liveUpdateCount, 1);
    EXPECT_FLOAT_EQ(mPlugin.lastDelta, 0.032f);
    mPlugin.OnUnload();
}

// ==============================================================================
// 2. Fire-if-connected at load
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, OnLoad_AlreadyConnected_CallsOnGameConnected)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    EXPECT_EQ(mPlugin.connectedCount, 1);
    EXPECT_EQ(mPlugin.disconnectedCount, 0);

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnLoad_AlreadyConnected_SubscribesRegisteredTopics)
{
    static const Dia::Core::StringCRC kTopic("my.topic");

    struct TopicPlugin : TestLivePlugin {
        void OnLivePluginLoad() override {
            TestLivePlugin::OnLivePluginLoad();
            RegisterGameTopic(Dia::Core::StringCRC("my.topic"), [](const Json::Value&) {});
        }
    } plugin;

    mFake.mFakeConnected = true;
    plugin.OnLoad(mContext);

    EXPECT_TRUE(mFake.HasSubscribed(kTopic))
        << "Should Subscribe(my.topic) when already connected at load";

    plugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnLoad_NotConnected_DoesNotCallOnGameConnected)
{
    mFake.mFakeConnected = false;
    mPlugin.OnLoad(mContext);

    EXPECT_EQ(mPlugin.connectedCount, 0);

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnLoad_AlreadyConnected_PushesConnectionStateTrue)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "test_plugin.connection_state"))
        << "Expected connection_state notification on load-while-connected";
    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "true"));

    mPlugin.OnUnload();
}

// ==============================================================================
// 3. OnUpdate polling — connect transition
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_ConnectTransition_CallsOnGameConnected)
{
    mPlugin.OnLoad(mContext);
    EXPECT_EQ(mPlugin.connectedCount, 0);

    mFake.mFakeConnected = true;
    Tick();

    EXPECT_EQ(mPlugin.connectedCount, 1);
    EXPECT_EQ(mPlugin.disconnectedCount, 0);

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_ConnectTransition_SubscribesAllTopics)
{
    static const Dia::Core::StringCRC kT("t.data");
    mPlugin.OnLoad(mContext);
    mPlugin.RegisterTopic(kT, [](const Json::Value&) {});

    mFake.mFakeConnected = true;
    Tick();

    EXPECT_TRUE(mFake.HasSubscribed(kT));

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_ConnectTransition_PushesConnectionStateTrue)
{
    mPlugin.OnLoad(mContext);
    mUISystem->jsCalls.clear();

    mFake.mFakeConnected = true;
    Tick();

    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "test_plugin.connection_state"));
    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "true"));

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_NoTransition_OnGameConnectedNotCalledAgain)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);
    int afterLoad = mPlugin.connectedCount;

    Tick(); Tick(); Tick();

    EXPECT_EQ(mPlugin.connectedCount, afterLoad);
    EXPECT_EQ(mPlugin.disconnectedCount, 0);

    mPlugin.OnUnload();
}

// ==============================================================================
// 4. OnUpdate polling — disconnect transition
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_DisconnectTransition_CallsOnGameDisconnected)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    mFake.mFakeConnected = false;
    Tick();

    EXPECT_EQ(mPlugin.disconnectedCount, 1);

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_DisconnectTransition_UnsubscribesAllTopics)
{
    static const Dia::Core::StringCRC kT("t.data");
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);
    mPlugin.RegisterTopic(kT, [](const Json::Value&) {});

    mFake.mFakeConnected = false;
    mFake.unsubscribeCalls.clear();
    Tick();

    EXPECT_TRUE(mFake.HasUnsubscribed(kT));

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_DisconnectTransition_PushesConnectionStateFalse)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);
    mUISystem->jsCalls.clear();

    mFake.mFakeConnected = false;
    Tick();

    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "test_plugin.connection_state"));
    EXPECT_TRUE(JsCallContains("DiaEditor_onDataChanged", "false"));

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, OnUpdate_MultipleDisconnectTicks_OnlyOneDisconnectCallback)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    mFake.mFakeConnected = false;
    Tick(); Tick(); Tick();

    EXPECT_EQ(mPlugin.disconnectedCount, 1);

    mPlugin.OnUnload();
}

// ==============================================================================
// 5. RegisterGameTopic
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, RegisterGameTopic_WhileDisconnected_BufferedNotSubscribed)
{
    mPlugin.OnLoad(mContext);
    mPlugin.RegisterTopic(Dia::Core::StringCRC("t.foo"), [](const Json::Value&) {});

    EXPECT_TRUE(mFake.subscribeCalls.empty())
        << "Should not subscribe while disconnected";

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, RegisterGameTopic_WhileConnected_SubscribesImmediately)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    static const Dia::Core::StringCRC kT("t.immediate");
    mFake.subscribeCalls.clear();
    mPlugin.RegisterTopic(kT, [](const Json::Value&) {});

    EXPECT_TRUE(mFake.HasSubscribed(kT))
        << "Should subscribe immediately when already connected";

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, RegisterGameTopic_BeforeConnect_SubscribedOnConnectTransition)
{
    mPlugin.OnLoad(mContext);

    static const Dia::Core::StringCRC kT("t.deferred");
    mPlugin.RegisterTopic(kT, [](const Json::Value&) {});
    EXPECT_TRUE(mFake.subscribeCalls.empty());

    mFake.mFakeConnected = true;
    Tick();

    EXPECT_TRUE(mFake.HasSubscribed(kT))
        << "Buffered topic should subscribe on connect transition";

    mPlugin.OnUnload();
}

// ==============================================================================
// 6. get_connection_state handler
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, GetConnectionState_ReturnsFalseWhenDisconnected)
{
    mPlugin.OnLoad(mContext);

    Json::Value result = mBridge->InvokeRequestHandler(
        StringCRC("test_plugin.get_connection_state"), Json::Value());

    EXPECT_FALSE(result["connected"].asBool());

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, GetConnectionState_ReturnsTrueWhenConnected)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);

    Json::Value result = mBridge->InvokeRequestHandler(
        StringCRC("test_plugin.get_connection_state"), Json::Value());

    EXPECT_TRUE(result["connected"].asBool());

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, GetConnectionState_WrongPrefixNotFound)
{
    mPlugin.OnLoad(mContext);

    Json::Value wrong = mBridge->InvokeRequestHandler(
        StringCRC("other_plugin.get_connection_state"), Json::Value());

    EXPECT_TRUE(wrong.isNull() || wrong.empty());

    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, GetConnectionState_ReflectsRuntimeChange)
{
    mPlugin.OnLoad(mContext);

    Json::Value before = mBridge->InvokeRequestHandler(
        StringCRC("test_plugin.get_connection_state"), Json::Value());
    EXPECT_FALSE(before["connected"].asBool());

    mFake.mFakeConnected = true;
    Tick();

    Json::Value after = mBridge->InvokeRequestHandler(
        StringCRC("test_plugin.get_connection_state"), Json::Value());
    EXPECT_TRUE(after["connected"].asBool());

    mPlugin.OnUnload();
}

// ==============================================================================
// 7. Null / missing service — graceful degradation
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, NullServices_LoadDoesNotCrash)
{
    EditorPluginContext noServices = mContext;
    noServices.mServices = nullptr;

    mPlugin.OnLoad(noServices);
    Tick();
    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, NullServices_IsGameConnectedReturnsFalse)
{
    EditorPluginContext noServices = mContext;
    noServices.mServices = nullptr;

    mPlugin.OnLoad(noServices);
    EXPECT_FALSE(mPlugin.IsGameConnected());
    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, NullServices_OnLivePluginLoadStillCalled)
{
    EditorPluginContext noServices = mContext;
    noServices.mServices = nullptr;

    mPlugin.OnLoad(noServices);
    EXPECT_EQ(mPlugin.liveLoadCount, 1);
    mPlugin.OnUnload();
}

// ==============================================================================
// 8. Reload — second load/unload cycle
// ==============================================================================

TEST_F(LiveConnectionPluginBaseTest, Reload_LifecycleCallsRepeat)
{
    mPlugin.OnLoad(mContext);
    mPlugin.OnUnload();

    mPlugin.liveLoadCount = 0;
    mPlugin.liveUnloadCount = 0;

    mPlugin.OnLoad(mContext);
    EXPECT_EQ(mPlugin.liveLoadCount, 1);
    mPlugin.OnUnload();
    EXPECT_EQ(mPlugin.liveUnloadCount, 1);
}

TEST_F(LiveConnectionPluginBaseTest, Reload_AfterConnectedUnload_NoGhostConnectedCallback)
{
    mFake.mFakeConnected = true;
    mPlugin.OnLoad(mContext);
    EXPECT_EQ(mPlugin.connectedCount, 1);
    mPlugin.OnUnload();

    // Reload while disconnected — should not fire OnGameConnected again.
    mFake.mFakeConnected = false;
    mPlugin.connectedCount = 0;
    mPlugin.OnLoad(mContext);
    Tick();

    EXPECT_EQ(mPlugin.connectedCount, 0);
    mPlugin.OnUnload();
}

TEST_F(LiveConnectionPluginBaseTest, Reload_HandlersRewiredAfterCycle)
{
    mPlugin.OnLoad(mContext);
    mPlugin.OnUnload();

    // After reload the get_connection_state handler should be re-registered.
    mPlugin.OnLoad(mContext);

    Json::Value result = mBridge->InvokeRequestHandler(
        StringCRC("test_plugin.get_connection_state"), Json::Value());
    EXPECT_FALSE(result.isNull());

    mPlugin.OnUnload();
}
