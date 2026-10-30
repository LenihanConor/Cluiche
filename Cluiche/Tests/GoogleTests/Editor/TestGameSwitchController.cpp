#include <gtest/gtest.h>
#include <DiaEditor/Project/GameSwitchController.h>
#include <DiaEditor/Project/ProjectContextController.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/MVC/IEditorContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
    struct FakeContext : IEditorContext
    {
        ProjectContext ctx{};
        int loadCallCount = 0;
        int clearCallCount = 0;
        char lastLoaded[512]{};

        bool LoadDiagameProject(const char* path) override
        {
            ++loadCallCount;
            if (path && path[0])
            {
                strncpy_s(ctx.diagamePath, ProjectContext::kMaxPath, path, _TRUNCATE);
                strncpy_s(lastLoaded, sizeof(lastLoaded), path, _TRUNCATE);
                return true;
            }
            return false;
        }
        void ClearDiagameProject() override { ++clearCallCount; ctx = ProjectContext{}; }
        const ProjectContext& GetDiagameProject() const override { return ctx; }
        void OnDiagameProjectChanged(ProjectChangedCallback, void*) override {}
        unsigned int GetRecentProjectCount() const override { return 0; }
        const char* GetRecentProject(unsigned int) const override { return nullptr; }
    };

    struct FakeLoader : IPluginLoader
    {
        unsigned int dirtyCount = 0;
        bool saveResult = true;
        int saveCalls = 0;

        void LoadPlugin(const StringCRC&, const StringCRC&) override {}
        bool UnloadPlugin(const StringCRC&) override { return false; }
        bool IsPluginTypeLoaded(const StringCRC&) const override { return false; }
        bool IsPluginPinned(const StringCRC&) const override { return false; }

        unsigned int CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max) override
        {
            unsigned int n = dirtyCount < max ? dirtyCount : max;
            for (unsigned int i = 0; i < n; ++i)
            {
                out[i].typeId = StringCRC("CowPlugin");
                strncpy_s(out[i].name, sizeof(out[i].name), "Cow Plugin", _TRUNCATE);
                out[i].saved = false;
                out[i].saveFailed = false;
            }
            return n;
        }
        bool SaveLoadedPlugin(const StringCRC&) override { ++saveCalls; return saveResult; }
    };

    struct Fixture
    {
        WebUIBridge bridge{nullptr};
        FakeContext ctx;
        FakeLoader loader;
        GameSwitchController sw;

        Fixture() { sw.Initialize(&bridge, &ctx, &loader); }
        ~Fixture() { sw.Shutdown(); }

        Json::Value Call(const char* name, const Json::Value& data = Json::Value(Json::objectValue))
        {
            return bridge.InvokeRequestHandler(StringCRC(name), data);
        }
        Json::Value Respond(const char* choice)
        {
            Json::Value d;
            d["choice"] = choice;
            return Call("game_switch.respond", d);
        }
    };
}

TEST(GameSwitchController, State_Idle)
{
    Fixture f;
    Json::Value s = f.Call("game_switch.get_state");
    EXPECT_FALSE(s["pending"].asBool());
    EXPECT_STREQ(s["prompt"].asCString(), "none");
    EXPECT_STREQ(s["currentName"].asCString(), "");
    EXPECT_STREQ(s["lastOutcome"].asCString(), "none");
    EXPECT_TRUE(s["dirty"].isArray());
    EXPECT_EQ(s["dirty"].size(), 0u);
}

TEST(GameSwitchController, State_ConfirmSwitch)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    EXPECT_EQ(f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kLiveConnection), SwitchOutcome::kNone);

    Json::Value s = f.Call("game_switch.get_state");
    EXPECT_TRUE(s["pending"].asBool());
    EXPECT_STREQ(s["prompt"].asCString(), "confirm_switch");
    EXPECT_STREQ(s["targetName"].asCString(), "cow");
    EXPECT_STREQ(s["currentName"].asCString(), "dragon");
    EXPECT_STREQ(s["targetPath"].asCString(), "games/cow.diagame");
    EXPECT_FALSE(s["isClose"].asBool());
}

TEST(GameSwitchController, State_UnsavedListsDirty)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    EXPECT_EQ(f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kUser), SwitchOutcome::kNone);

    Json::Value s = f.Call("game_switch.get_state");
    EXPECT_STREQ(s["prompt"].asCString(), "unsaved");
    ASSERT_EQ(s["dirty"].size(), 1u);
    EXPECT_STREQ(s["dirty"][0]["typeId"].asCString(), "CowPlugin");
    EXPECT_STREQ(s["dirty"][0]["name"].asCString(), "Cow Plugin");
    EXPECT_FALSE(s["dirty"][0]["saved"].asBool());
    EXPECT_FALSE(s["dirty"][0]["saveFailed"].asBool());
}

TEST(GameSwitchController, State_SaveFailed)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.loader.saveResult = false;
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kUser);

    Json::Value r = f.Respond("save");
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_STREQ(r["state"]["prompt"].asCString(), "save_failed");
    EXPECT_TRUE(r["state"]["dirty"][0]["saveFailed"].asBool());
    EXPECT_EQ(f.ctx.loadCallCount, 1);   // only the initial dragon load
}

TEST(GameSwitchController, State_CloseFlag)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.sw.RequestClose(SwitchOrigin::kUser);
    Json::Value s = f.Call("game_switch.get_state");
    EXPECT_TRUE(s["isClose"].asBool());
    EXPECT_STREQ(s["prompt"].asCString(), "unsaved");
}

TEST(GameSwitchController, Respond_CancelAtConfirmDeclines)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kLiveConnection);

    Json::Value r = f.Respond("cancel");
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_STREQ(r["outcome"].asCString(), "declined");
    EXPECT_STREQ(r["state"]["lastOutcome"].asCString(), "declined");
    EXPECT_EQ(f.ctx.loadCallCount, 1);
    EXPECT_STREQ(f.ctx.ctx.diagamePath, "games/dragon.diagame");
}

TEST(GameSwitchController, Respond_ConfirmLoadsWhenClean)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kLiveConnection);

    Json::Value r = f.Respond("confirm");
    EXPECT_STREQ(r["outcome"].asCString(), "switched");
    EXPECT_EQ(f.ctx.loadCallCount, 2);
    EXPECT_STREQ(f.ctx.lastLoaded, "games/cow.diagame");
}

TEST(GameSwitchController, Respond_DiscardLoads)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kUser);

    Json::Value r = f.Respond("discard");
    EXPECT_STREQ(r["outcome"].asCString(), "switched");
    EXPECT_EQ(f.loader.saveCalls, 0);
    EXPECT_STREQ(f.ctx.lastLoaded, "games/cow.diagame");
}

TEST(GameSwitchController, Respond_SaveSavesThenLoads)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kUser);

    Json::Value r = f.Respond("save");
    EXPECT_STREQ(r["outcome"].asCString(), "switched");
    EXPECT_EQ(f.loader.saveCalls, 1);
    EXPECT_STREQ(f.ctx.lastLoaded, "games/cow.diagame");
}

TEST(GameSwitchController, Respond_CancelAtUnsavedKeepsGame)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.sw.RequestLoad("games/cow.diagame", SwitchOrigin::kUser);

    Json::Value r = f.Respond("cancel");
    EXPECT_STREQ(r["outcome"].asCString(), "cancelled");
    EXPECT_EQ(f.ctx.loadCallCount, 1);
    EXPECT_EQ(f.ctx.clearCallCount, 0);
}

TEST(GameSwitchController, Respond_CloseDiscardClearsGame)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;
    f.sw.RequestClose(SwitchOrigin::kUser);
    f.Respond("discard");
    EXPECT_EQ(f.ctx.clearCallCount, 1);
}

TEST(GameSwitchController, Respond_UnknownChoiceIsError)
{
    Fixture f;
    Json::Value r = f.Respond("bogus");
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("error"));
}

TEST(GameSwitchController, Respond_MissingChoiceIsError)
{
    Fixture f;
    Json::Value r = f.Call("game_switch.respond");
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_TRUE(r.isMember("error"));
}

TEST(GameSwitchController, Respond_WithNothingPendingNotOk)
{
    Fixture f;
    Json::Value r = f.Respond("confirm");
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_STREQ(r["outcome"].asCString(), "rejected");
}

TEST(GameSwitchController, OutcomeToResult_Mapping)
{
    Json::Value r;
    GameSwitchController::OutcomeToResult(SwitchOutcome::kNone, r);
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_TRUE(r["pending"].asBool());

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kSwitched, r);
    EXPECT_TRUE(r["ok"].asBool());
    EXPECT_FALSE(r.isMember("pending"));

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kAlreadyLoaded, r);
    EXPECT_TRUE(r["ok"].asBool());

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kLoadFailed, r);
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_STREQ(r["error"].asCString(), "failed to load project");

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kDeclined, r);
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_TRUE(r["cancelled"].asBool());

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kCancelled, r);
    EXPECT_TRUE(r["cancelled"].asBool());

    r = Json::Value();
    GameSwitchController::OutcomeToResult(SwitchOutcome::kRejected, r);
    EXPECT_FALSE(r["ok"].asBool());
    EXPECT_STREQ(r["error"].asCString(), "another game switch is pending");
}

TEST(GameSwitchController, Shutdown_UnregistersHandlers)
{
    Fixture f;
    f.sw.Shutdown();
    EXPECT_TRUE(f.Call("game_switch.get_state").isNull());
}

// ---------------------------------------------------------------------------
// ProjectContextController routed through the gate
// ---------------------------------------------------------------------------

TEST(GameSwitchProjectContext, SameGameUserOpenReloads)
{
    Fixture f;
    ProjectContextController pc;
    pc.Initialize(&f.bridge, &f.ctx);
    pc.SetGameSwitchController(&f.sw);
    f.ctx.LoadDiagameProject("games/cow.diagame");

    Json::Value req;
    req["path"] = "games/cow.diagame";
    Json::Value res = f.bridge.InvokeRequestHandler(StringCRC("project.open_path"), req);
    EXPECT_TRUE(res["ok"].asBool());
    EXPECT_FALSE(res.isMember("pending"));
    EXPECT_EQ(f.ctx.loadCallCount, 2);
    pc.Shutdown();
}

TEST(GameSwitchProjectContext, DifferentGameDirtyIsPendingUntilRespond)
{
    Fixture f;
    ProjectContextController pc;
    pc.Initialize(&f.bridge, &f.ctx);
    pc.SetGameSwitchController(&f.sw);
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;

    Json::Value req;
    req["path"] = "games/cow.diagame";
    Json::Value res = f.bridge.InvokeRequestHandler(StringCRC("project.open_path"), req);
    EXPECT_TRUE(res["ok"].asBool());
    EXPECT_TRUE(res["pending"].asBool());
    EXPECT_EQ(f.ctx.loadCallCount, 1);

    f.Respond("discard");
    EXPECT_EQ(f.ctx.loadCallCount, 2);
    EXPECT_STREQ(f.ctx.lastLoaded, "games/cow.diagame");
    pc.Shutdown();
}

TEST(GameSwitchProjectContext, CloseDirtyIsPending)
{
    Fixture f;
    ProjectContextController pc;
    pc.Initialize(&f.bridge, &f.ctx);
    pc.SetGameSwitchController(&f.sw);
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    f.loader.dirtyCount = 1;

    Json::Value res = f.bridge.InvokeRequestHandler(StringCRC("project.close"), Json::Value(Json::objectValue));
    EXPECT_TRUE(res["pending"].asBool());
    EXPECT_EQ(f.ctx.clearCallCount, 0);
    pc.Shutdown();
}

TEST(GameSwitchProjectContext, CloseWithNoGameStillOk)
{
    Fixture f;
    ProjectContextController pc;
    pc.Initialize(&f.bridge, &f.ctx);
    pc.SetGameSwitchController(&f.sw);

    Json::Value res = f.bridge.InvokeRequestHandler(StringCRC("project.close"), Json::Value(Json::objectValue));
    EXPECT_TRUE(res["ok"].asBool());
    pc.Shutdown();
}

// ---------------------------------------------------------------------------
// GameConnectionController live diagame_path
// ---------------------------------------------------------------------------

TEST(GameSwitchGameConnection, NoGameLoaded_LoadsDirectly)
{
    Fixture f;
    GameConnectionController gc;
    gc.SetEditorContext(&f.ctx);
    gc.SetGameSwitchController(&f.sw);
    gc.ApplyLiveDiagamePath("games/cow.diagame");
    EXPECT_EQ(f.ctx.loadCallCount, 1);
    EXPECT_STREQ(f.ctx.lastLoaded, "games/cow.diagame");
}

TEST(GameSwitchGameConnection, SameGame_NoPromptNoLoad)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/cow.diagame");
    GameConnectionController gc;
    gc.SetEditorContext(&f.ctx);
    gc.SetGameSwitchController(&f.sw);
    gc.ApplyLiveDiagamePath("games/cow.diagame");
    EXPECT_EQ(f.ctx.loadCallCount, 1);
    EXPECT_FALSE(f.sw.GetGate().IsPending());
}

TEST(GameSwitchGameConnection, DifferentGame_PromptsAndDeclineKeepsGame)
{
    Fixture f;
    f.ctx.LoadDiagameProject("games/dragon.diagame");
    GameConnectionController gc;
    gc.SetEditorContext(&f.ctx);
    gc.SetGameSwitchController(&f.sw);
    gc.ApplyLiveDiagamePath("games/cow.diagame");

    Json::Value s = f.Call("game_switch.get_state");
    EXPECT_STREQ(s["prompt"].asCString(), "confirm_switch");
    EXPECT_EQ(f.ctx.loadCallCount, 1);

    f.Respond("cancel");
    EXPECT_EQ(f.ctx.loadCallCount, 1);
    EXPECT_STREQ(f.ctx.ctx.diagamePath, "games/dragon.diagame");
}

TEST(GameSwitchGameConnection, NoController_LoadsDirectly)
{
    FakeContext ctx;
    GameConnectionController gc;
    gc.SetEditorContext(&ctx);
    gc.ApplyLiveDiagamePath("games/cow.diagame");
    EXPECT_EQ(ctx.loadCallCount, 1);
}
