#include <gtest/gtest.h>
#include <DiaEditor/Project/ProjectContextController.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/MVC/IEditorContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdlib>
#include <cstring>

using namespace Dia::Editor;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Stub IEditorContext
// ---------------------------------------------------------------------------

namespace
{
    struct HeadlessGuard
    {
        HeadlessGuard()  { _putenv_s("DIA_HEADLESS", "1"); }
        ~HeadlessGuard() { _putenv_s("DIA_HEADLESS", ""); }
    };

    struct StubEditorContext : IEditorContext
    {
        ProjectContext ctx{};
        bool loadResult = true;
        int loadCallCount = 0;
        int clearCallCount = 0;

        struct CbEntry { ProjectChangedCallback cb; void* ud; };
        CbEntry callbacks[8];
        unsigned int cbCount = 0;

        char recentPaths[5][512]{};
        unsigned int recentCount = 0;

        bool LoadDiagameProject(const char* path) override
        {
            ++loadCallCount;
            if (loadResult && path && path[0])
            {
                strncpy_s(ctx.diagamePath, ProjectContext::kMaxPath, path, _TRUNCATE);
                FireCallbacks();
                return true;
            }
            return false;
        }

        void ClearDiagameProject() override
        {
            ++clearCallCount;
            ctx = ProjectContext{};
            FireCallbacks();
        }

        const ProjectContext& GetDiagameProject() const override { return ctx; }

        void OnDiagameProjectChanged(ProjectChangedCallback callback, void* userData) override
        {
            if (callback && cbCount < 8)
                callbacks[cbCount++] = {callback, userData};
        }

        unsigned int GetRecentProjectCount() const override { return recentCount; }

        const char* GetRecentProject(unsigned int index) const override
        {
            if (index >= recentCount) return nullptr;
            return recentPaths[index];
        }

        void SetRecent(const char* p, unsigned int idx)
        {
            strncpy_s(recentPaths[idx], 512, p, _TRUNCATE);
            if (idx >= recentCount) recentCount = idx + 1;
        }

        void FireCallbacks()
        {
            for (unsigned int i = 0; i < cbCount; ++i)
                callbacks[i].cb(ctx, callbacks[i].ud);
        }
    };

    // Helper: initialise controller and invoke a named handler directly.
    Json::Value Dispatch(ProjectContextController& ctrl, WebUIBridge& bridge,
                         const char* handlerKey, const Json::Value& data)
    {
        return bridge.InvokeRequestHandler(StringCRC(handlerKey), data);
    }
}

// ---------------------------------------------------------------------------
// Construction / null-safety
// ---------------------------------------------------------------------------

TEST(ProjectContextController, ConstructsCleanly)
{
    ProjectContextController c;
}

TEST(ProjectContextController, InitializeWithNullBridge_NoCrash)
{
    StubEditorContext ctx;
    ProjectContextController c;
    c.Initialize(nullptr, &ctx);
    c.Shutdown();
}

TEST(ProjectContextController, InitializeWithNullContext_NoCrash)
{
    WebUIBridge bridge(nullptr);
    ProjectContextController c;
    c.Initialize(&bridge, nullptr);
    c.Shutdown();
}

TEST(ProjectContextController, InitializeWithBothNull_NoCrash)
{
    ProjectContextController c;
    c.Initialize(nullptr, nullptr);
    c.Shutdown();
}

TEST(ProjectContextController, DoubleShutdown_NoCrash)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController c;
    c.Initialize(&bridge, &ctx);
    c.Shutdown();
    c.Shutdown();
}

// ---------------------------------------------------------------------------
// Initialize registers observer on context
// ---------------------------------------------------------------------------

TEST(ProjectContextController, Initialize_RegistersObserverOnContext)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController c;
    c.Initialize(&bridge, &ctx);
    EXPECT_EQ(ctx.cbCount, 1u);
    c.Shutdown();
}

// ---------------------------------------------------------------------------
// Handler response shapes — project.open_path
// ---------------------------------------------------------------------------

TEST(ProjectContextController, OpenPath_ValidPath_ReturnsOkTrue)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value req;
    req["path"] = "game/test.diagame";
    Json::Value res = Dispatch(ctrl, bridge, "project.open_path", req);

    EXPECT_TRUE(res["ok"].asBool());
    EXPECT_STREQ(res["path"].asCString(), "game/test.diagame");

    ctrl.Shutdown();
}

TEST(ProjectContextController, OpenPath_LoadFails_ReturnsOkFalse)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ctx.loadResult = false;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value req;
    req["path"] = "bad/path.diagame";
    Json::Value res = Dispatch(ctrl, bridge, "project.open_path", req);

    EXPECT_FALSE(res["ok"].asBool());
    EXPECT_TRUE(res.isMember("error"));

    ctrl.Shutdown();
}

TEST(ProjectContextController, OpenPath_MissingPathField_ReturnsOkFalse)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value req;  // no "path" member
    Json::Value res = Dispatch(ctrl, bridge, "project.open_path", req);

    EXPECT_FALSE(res["ok"].asBool());
    EXPECT_TRUE(res.isMember("error"));

    ctrl.Shutdown();
}

TEST(ProjectContextController, OpenPath_EmptyPathString_ReturnsOkFalse)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value req;
    req["path"] = "";
    Json::Value res = Dispatch(ctrl, bridge, "project.open_path", req);

    // Empty path → LoadDiagameProject returns false.
    EXPECT_FALSE(res["ok"].asBool());

    ctrl.Shutdown();
}

TEST(ProjectContextController, OpenPath_CallsLoadDiagameProject)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value req;
    req["path"] = "proj/game.diagame";
    Dispatch(ctrl, bridge, "project.open_path", req);

    EXPECT_EQ(ctx.loadCallCount, 1);
    EXPECT_STREQ(ctx.ctx.diagamePath, "proj/game.diagame");

    ctrl.Shutdown();
}

// ---------------------------------------------------------------------------
// Handler response shapes — project.close
// ---------------------------------------------------------------------------

TEST(ProjectContextController, Close_ReturnsOkTrue)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value res = Dispatch(ctrl, bridge, "project.close", Json::Value{});

    EXPECT_TRUE(res["ok"].asBool());

    ctrl.Shutdown();
}

TEST(ProjectContextController, Close_CallsClearDiagameProject)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Dispatch(ctrl, bridge, "project.close", Json::Value{});
    EXPECT_EQ(ctx.clearCallCount, 1);

    ctrl.Shutdown();
}

// ---------------------------------------------------------------------------
// Handler response shapes — project.get_recent
// ---------------------------------------------------------------------------

TEST(ProjectContextController, GetRecent_EmptyList_ReturnsEmptyArray)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value res = Dispatch(ctrl, bridge, "project.get_recent", Json::Value{});

    ASSERT_TRUE(res.isMember("paths"));
    EXPECT_TRUE(res["paths"].isArray());
    EXPECT_EQ(res["paths"].size(), 0u);

    ctrl.Shutdown();
}

TEST(ProjectContextController, GetRecent_PopulatedList_ReturnsAllPaths)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ctx.SetRecent("proj/a.diagame", 0);
    ctx.SetRecent("proj/b.diagame", 1);
    ctx.SetRecent("proj/c.diagame", 2);

    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value res = Dispatch(ctrl, bridge, "project.get_recent", Json::Value{});

    ASSERT_TRUE(res["paths"].isArray());
    ASSERT_EQ(res["paths"].size(), 3u);
    EXPECT_STREQ(res["paths"][0].asCString(), "proj/a.diagame");
    EXPECT_STREQ(res["paths"][1].asCString(), "proj/b.diagame");
    EXPECT_STREQ(res["paths"][2].asCString(), "proj/c.diagame");

    ctrl.Shutdown();
}

// ---------------------------------------------------------------------------
// Handler response shapes — project.open (placeholder)
// ---------------------------------------------------------------------------

TEST(ProjectContextController, Open_Headless_ReturnsCancelled)
{
    HeadlessGuard guard;
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);

    Json::Value res = Dispatch(ctrl, bridge, "project.open", Json::Value{});

    EXPECT_FALSE(res["ok"].asBool());
    EXPECT_TRUE(res["cancelled"].asBool());

    ctrl.Shutdown();
}

// ---------------------------------------------------------------------------
// Shutdown unregisters handlers
// ---------------------------------------------------------------------------

TEST(ProjectContextController, Shutdown_UnregistersHandlers)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);
    ctrl.Shutdown();

    // After shutdown handlers are gone — dispatch returns empty value.
    Json::Value req;
    req["path"] = "x.diagame";
    Json::Value res = bridge.InvokeRequestHandler(StringCRC("project.open_path"), req);
    EXPECT_TRUE(res.isNull());
}

TEST(ProjectContextController, Shutdown_PostCallbackNoCrash)
{
    WebUIBridge bridge(nullptr);
    StubEditorContext ctx;
    ProjectContextController ctrl;
    ctrl.Initialize(&bridge, &ctx);
    ctrl.Shutdown();

    // Firing context callbacks after shutdown should not crash.
    ctx.ClearDiagameProject();
    EXPECT_FALSE(ctx.ctx.IsValid());
}
