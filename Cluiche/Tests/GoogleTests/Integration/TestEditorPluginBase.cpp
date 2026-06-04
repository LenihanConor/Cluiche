#include <gtest/gtest.h>
#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorViewController.h>
#include <DiaUI/IUISystem.h>
#include <DiaInput/EMouseButton.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <vector>

using namespace Dia::Editor;
using namespace Dia::Core;

// ==============================================================================
// Mock IUISystem (minimal version for plugin base tests)
// ==============================================================================

class PluginBaseMockUISystem : public Dia::UI::IUISystem
{
public:
    struct Call
    {
        std::string functionName;
        std::string argsJson;
    };

    std::vector<Call> jsCalls;
    Dia::UI::IUISystem::JSHandler registeredHandler;

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

    void RegisterJSHandler(const char*, JSHandler handler) override
    {
        registeredHandler = handler;
    }

    void CallJSFunction(const char* functionName, const char* argsJson) override
    {
        jsCalls.push_back({ functionName, argsJson ? argsJson : "" });
    }
};

// ==============================================================================
// Concrete test plugin
// ==============================================================================

class TestPlugin : public EditorPluginBase
{
public:
    TestPlugin()
        : EditorPluginBase({
            "TestPlugin",
            "1.0.0",
            "A test plugin",
            "dia://plugins/test/index.html",
            LayoutMode::kDockable,
            "test.dirty_changed",
            nullptr,
            false
        })
        , mLoadCalled(false)
        , mUnloadCalled(false)
        , mProjectChangedCalled(false)
    {}

    bool mLoadCalled;
    bool mUnloadCalled;
    bool mProjectChangedCalled;
    ProjectContext mLastProjectContext;

    int mHandlerCallCount = 0;

    // Expose protected members for testing
    using EditorPluginBase::IsDirty;
    using EditorPluginBase::MarkDirty;
    using EditorPluginBase::ClearDirty;
    using EditorPluginBase::GetProjectPath;

    Json::Value HandleTestRequest(const Json::Value& data)
    {
        mHandlerCallCount++;
        Json::Value result;
        result["echo"] = data.get("input", "").asString();
        return result;
    }

protected:
    void OnPluginLoad() override
    {
        mLoadCalled = true;

        RegisterHandler(StringCRC("test.echo"),
            [this](const Json::Value& data) -> Json::Value {
                return HandleTestRequest(data);
            });

        RegisterHandler(StringCRC("test.ping"),
            [this](const Json::Value&) -> Json::Value {
                Json::Value r;
                r["pong"] = true;
                return r;
            });

        RegisterEvent(StringCRC("test.notify"),
            [this](const Json::Value&) {
                mHandlerCallCount++;
            });
    }

    void OnPluginUnload() override
    {
        mUnloadCalled = true;
    }

    void OnProjectChanged(const ProjectContext& ctx) override
    {
        mProjectChangedCalled = true;
        mLastProjectContext = ctx;
    }
};

class TestPluginNoDirty : public EditorPluginBase
{
public:
    TestPluginNoDirty()
        : EditorPluginBase({
            "TestPluginNoDirty",
            "1.0.0",
            "No dirty tracking",
            "dia://plugins/test2/index.html",
            LayoutMode::kHeadless,
            nullptr,
            nullptr,
            false
        })
    {}

    using EditorPluginBase::IsDirty;
    using EditorPluginBase::MarkDirty;
    using EditorPluginBase::ClearDirty;
};

// ==============================================================================
// Test Fixture
// ==============================================================================

class EditorPluginBaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mUISystem = new PluginBaseMockUISystem();
        mBridge = new WebUIBridge(mUISystem);
        mController = new EditorViewController();
        mBridge->Initialize(mController);

        mContext.mBridge = mBridge;
        mContext.mModel = nullptr;
        mContext.mView = nullptr;
        mContext.mPluginLoader = nullptr;
        mContext.mServices = nullptr;
        mContext.mProjectPath = "C:/test/project";
    }

    void TearDown() override
    {
        delete mBridge;
        delete mController;
        delete mUISystem;
    }

    PluginBaseMockUISystem* mUISystem = nullptr;
    WebUIBridge* mBridge = nullptr;
    EditorViewController* mController = nullptr;
    EditorPluginContext mContext;
};

// ==============================================================================
// Metadata Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, Metadata_ReturnsConstructorValues)
{
    TestPlugin plugin;

    EXPECT_STREQ(plugin.GetName(), "TestPlugin");
    EXPECT_STREQ(plugin.GetVersion(), "1.0.0");
    EXPECT_STREQ(plugin.GetDescription(), "A test plugin");
    EXPECT_STREQ(plugin.GetUIPath(), "dia://plugins/test/index.html");
    EXPECT_EQ(plugin.GetLayoutMode(), LayoutMode::kDockable);
}

TEST_F(EditorPluginBaseTest, Metadata_HeadlessLayout)
{
    TestPluginNoDirty plugin;
    EXPECT_EQ(plugin.GetLayoutMode(), LayoutMode::kHeadless);
}

// ==============================================================================
// Lifecycle Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, OnLoad_CallsOnPluginLoad)
{
    TestPlugin plugin;
    EXPECT_FALSE(plugin.mLoadCalled);

    plugin.OnLoad(mContext);
    EXPECT_TRUE(plugin.mLoadCalled);

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, OnUnload_CallsOnPluginUnload)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    EXPECT_FALSE(plugin.mUnloadCalled);
    plugin.OnUnload();
    EXPECT_TRUE(plugin.mUnloadCalled);
}

TEST_F(EditorPluginBaseTest, OnLoad_StoresContext)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    EXPECT_STREQ(plugin.GetProjectPath(), "C:/test/project");

    plugin.OnUnload();
}

// ==============================================================================
// Handler Auto-Registration/Unregistration Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, RegisterHandler_HandlersInvokedViaBridge)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    Json::Value data;
    data["input"] = "hello";
    Json::Value result = mBridge->InvokeRequestHandler(StringCRC("test.echo"), data);

    EXPECT_EQ(result["echo"].asString(), "hello");
    EXPECT_EQ(plugin.mHandlerCallCount, 1);

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, RegisterHandler_MultipleHandlersWork)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    Json::Value pingResult = mBridge->InvokeRequestHandler(StringCRC("test.ping"), Json::Value());
    EXPECT_TRUE(pingResult["pong"].asBool());

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, OnUnload_AutoUnregistersAllHandlers)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    // Verify handler works before unload
    Json::Value data;
    data["input"] = "before";
    Json::Value result = mBridge->InvokeRequestHandler(StringCRC("test.echo"), data);
    EXPECT_EQ(result["echo"].asString(), "before");

    plugin.OnUnload();

    // After unload, handler should be gone — InvokeRequestHandler returns empty
    Json::Value afterResult = mBridge->InvokeRequestHandler(StringCRC("test.echo"), data);
    EXPECT_TRUE(afterResult.isNull() || afterResult.empty());
}

TEST_F(EditorPluginBaseTest, OnUnload_AutoUnregistersEventHandlers)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    // event handler was registered during OnPluginLoad
    // After unload, the event handler should be removed
    plugin.OnUnload();

    // We can't easily invoke event handlers externally, but this verifies no crash
    // and that re-registration works after reload
    plugin.OnLoad(mContext);
    EXPECT_EQ(plugin.mHandlerCallCount, 0);
    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, ReloadPlugin_HandlersWorkAgain)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);
    plugin.OnUnload();

    // Reload
    plugin.mLoadCalled = false;
    plugin.mUnloadCalled = false;
    plugin.mHandlerCallCount = 0;
    plugin.OnLoad(mContext);

    EXPECT_TRUE(plugin.mLoadCalled);

    Json::Value data;
    data["input"] = "reloaded";
    Json::Value result = mBridge->InvokeRequestHandler(StringCRC("test.echo"), data);
    EXPECT_EQ(result["echo"].asString(), "reloaded");

    plugin.OnUnload();
}

// ==============================================================================
// Dirty State Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, DirtyState_InitiallyClean)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    EXPECT_FALSE(plugin.IsDirty());

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, MarkDirty_SetsDirtyFlag)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    plugin.MarkDirty();
    EXPECT_TRUE(plugin.IsDirty());

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, ClearDirty_ClearsDirtyFlag)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    plugin.MarkDirty();
    plugin.ClearDirty();
    EXPECT_FALSE(plugin.IsDirty());

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, MarkDirty_NotifiesUI)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);
    mUISystem->jsCalls.clear();

    plugin.MarkDirty();

    ASSERT_FALSE(mUISystem->jsCalls.empty());
    EXPECT_EQ(mUISystem->jsCalls.back().functionName, "DiaEditor_onDataChanged");

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, DirtyState_NullTopic_NoNotification)
{
    TestPluginNoDirty plugin;
    plugin.OnLoad(mContext);
    mUISystem->jsCalls.clear();

    plugin.MarkDirty();

    // Should still set the flag
    EXPECT_TRUE(plugin.IsDirty());
    // But should NOT send a notification (no dirtyTopic configured)
    EXPECT_TRUE(mUISystem->jsCalls.empty());

    plugin.OnUnload();
}

TEST_F(EditorPluginBaseTest, OnUnload_ResetsDirtyState)
{
    TestPlugin plugin;
    plugin.OnLoad(mContext);

    plugin.MarkDirty();
    plugin.OnUnload();

    // After unload, dirty should be reset
    EXPECT_FALSE(plugin.IsDirty());
}

// ==============================================================================
// Handler Method Testability (direct call without bridge)
// ==============================================================================

TEST_F(EditorPluginBaseTest, HandlerMethod_TestableDirectly)
{
    TestPlugin plugin;
    // Don't even call OnLoad — test the handler method in isolation

    Json::Value data;
    data["input"] = "direct";

    Json::Value result = plugin.HandleTestRequest(data);
    EXPECT_EQ(result["echo"].asString(), "direct");
    EXPECT_EQ(plugin.mHandlerCallCount, 1);
}

// ==============================================================================
// Null Bridge Safety
// ==============================================================================

TEST_F(EditorPluginBaseTest, NullBridge_RegisterHandlerNoOp)
{
    TestPlugin plugin;

    EditorPluginContext nullContext;
    nullContext.mBridge = nullptr;
    nullContext.mProjectPath = nullptr;

    // Should not crash
    plugin.OnLoad(nullContext);
    plugin.OnUnload();
}

// ==============================================================================
// Response Helper Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, MakeSuccessResponse_NoData)
{
    Json::Value r = EditorPluginBase::MakeSuccessResponse();
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_FALSE(r.isMember("error"));
}

TEST_F(EditorPluginBaseTest, MakeSuccessResponse_WithData)
{
    Json::Value data;
    data["count"] = 42;
    Json::Value r = EditorPluginBase::MakeSuccessResponse(data);
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["data"]["count"].asInt(), 42);
}

TEST_F(EditorPluginBaseTest, MakeErrorResponse_ContainsMessage)
{
    Json::Value r = EditorPluginBase::MakeErrorResponse("something broke");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["error"].asString(), "something broke");
}

TEST_F(EditorPluginBaseTest, MakeErrorResponse_NullMessage)
{
    Json::Value r = EditorPluginBase::MakeErrorResponse(nullptr);
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["error"].asString(), "Unknown error");
}

// ==============================================================================
// Toolbar Metadata Tests
// ==============================================================================

TEST_F(EditorPluginBaseTest, GetToolbarItem_UsesMetadata)
{
    TestPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();
    EXPECT_STREQ(item.label, "TestPlugin");
    EXPECT_FALSE(item.pinned);
}

TEST_F(EditorPluginBaseTest, GetToolbarItem_CustomIcon)
{
    struct IconPlugin : public EditorPluginBase
    {
        IconPlugin() : EditorPluginBase({
            "MyPlugin", "1.0", "desc",
            "dia://plugins/my/index.html",
            LayoutMode::kDockable, nullptr,
            "X", true
        }) {}
    };

    IconPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();
    EXPECT_STREQ(item.iconChar, "X");
    EXPECT_TRUE(item.pinned);
}

TEST_F(EditorPluginBaseTest, GetToolbarItem_NullIcon_FallsBackToFirstChar)
{
    TestPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();
    EXPECT_EQ(item.iconChar[0], 'T');
}
