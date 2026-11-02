// Headless integration tests for game-scoped editor plugins (spec: game-scoped-plugins.md).
//
// Wires the real EditorModel + EditorPluginRegistry + PluginLoaderModule + PluginBrowserEditorPlugin +
// GameSwitchController + GameConnectionController against the test-only stub plugins and fixture
// .diagame/.diaapp files from GameScopedPluginFixtures.h. No UI, no Application.
//
// Known headless limit: PluginLoaderModule registers its game-scope listener inside DoStart(), which
// needs the full editor application. The fixture therefore installs a listener that performs the same
// "close every out-of-scope plugin through UnloadPlugin" step (see OnScopeChanged). AC4 is covered via the
// asset-type route registry (asset_catalogue.register_type_editor / unregister_type_editor); the ShellExecuteExW
// fallback itself is not exercised, only that no editor route exists.

#include <gtest/gtest.h>
#include "GameScopedPluginFixtures.h"

#include <DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h>
#include <ApplicationFlow/Modules/PluginLoaderModule.h>
#include <ApplicationFlow/Modules/EditorViewModule.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Memory/EditorMemory.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/PluginBrowserEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Project/GameSwitchController.h>
#include <DiaEditor/Project/GameScopeDiscovery.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>

#include <atomic>
#include <string>

using namespace Dia::Editor;
using namespace Dia::Core;
using namespace GameScopedFixtures;

// ApplicationFlow/Modules/EditorViewModule.cpp pulls in the CEF UI stack and is not linked into the test
// executable. PluginLoaderModule only needs the type id (for a ModuleRef that never resolves headlessly).
namespace Cluiche { namespace Editor { const Dia::Core::StringCRC EditorViewModule::kTypeId("EditorViewModule"); } }

namespace
{
    std::atomic<int> gAssertCount{0};
    void CountingAssertHandler(const char*, const char*, int, const char*, ...) { gAssertCount.fetch_add(1); }
}

class GameScopedPluginsIntegration : public ::testing::Test
{
protected:
    GameScopedPluginsIntegration()
        : mBridge(nullptr)
        , mSwitchBridge(nullptr)
        , mLoader(StringCRC("gsp_integration_loader"))
    {}

    void SetUp() override
    {
        EnsureStubsRegistered();
        ResetEditorRegistry();
        GameCounters().Reset();
        GenericCounters().Reset();
        mFiles = WriteFixtureFiles();

        mSwitch.Initialize(&mSwitchBridge, &mModel, &mLoader);
        StubGamePlugin::RouteBridgeOverride() = &mBridge;
        EditorPluginRegistry::Instance().AddGameScopeListener(&GameScopedPluginsIntegration::OnScopeChanged, this);
    }

    void TearDown() override
    {
        EditorPluginRegistry::Instance().RemoveGameScopeListener(&GameScopedPluginsIntegration::OnScopeChanged, this);
        mSwitch.Shutdown();

        // The browser is pinned (cannot go through UnloadPlugin), so it is hosted directly; unload it
        // here so its palette commands and scope listener are removed.
        if (mBrowserLoaded)
            mBrowser.OnUnload();
        mBrowserLoaded = false;

        const char* typesFirst[] = { kStubGameType, kStubGenericType };
        for (const char* t : typesFirst)
            mLoader.UnloadPlugin(StringCRC(t));   // game plugin unregisters its route while the catalogue is still up
        if (mCatalogueLoaded)
            mCatalogue.OnUnload();
        mCatalogueLoaded = false;
        StubGamePlugin::RouteBridgeOverride() = nullptr;

        const char* types[] = { kStubGameType, kStubGenericType };
        for (const char* t : types)
            mLoader.UnloadPlugin(StringCRC(t));

        ResetEditorRegistry();
        RemoveFixtureFiles(mFiles);
    }

    // Stand-in for PluginLoaderModule::OnGameScopeChanged (private, registered in DoStart).
    static void OnScopeChanged(void* ud, const StringCRC&, const StringCRC&)
    {
        auto* self = static_cast<GameScopedPluginsIntegration*>(ud);
        const char* types[] = { kStubGameType, kStubGenericType };
        for (const char* t : types)
        {
            const StringCRC id(t);
            if (EditorPluginRegistry::Instance().IsOutOfGameScope(id) && self->mLoader.IsPluginTypeLoaded(id))
                self->mLoader.UnloadPlugin(id);
        }
    }

    void LoadBrowser()
    {
        EditorPluginContext ctx;
        ctx.mBridge = &mBridge;
        ctx.mPluginLoader = &mLoader;   // the real PluginLoaderModule
        mBrowser.OnLoad(ctx);
        mBrowserLoaded = true;
    }

    void LoadCatalogue()
    {
        EditorPluginContext ctx;
        ctx.mBridge = &mBridge;
        mCatalogue.OnLoad(ctx);
        mCatalogueLoaded = true;
    }

    // Probe: registering a route for the stub asset type only succeeds when none exists (the probe is then removed).
    bool StubRouteExists()
    {
        Json::Value req(Json::objectValue);
        req["assetType"] = kStubAssetType;
        req["editorPluginType"] = "ProbeEditor";
        Json::Value r = mBridge.InvokeRequestHandler(StringCRC("asset_catalogue.register_type_editor"), req);
        if (r["registered"].asBool())
        {
            mBridge.InvokeRequestHandler(StringCRC("asset_catalogue.unregister_type_editor"), req);
            return false;
        }
        return true;
    }

    bool Listed(const char* typeId)
    {
        Json::Value result = mBridge.InvokeRequestHandler(StringCRC("plugin_browser.get_available"), Json::Value());
        const Json::Value& plugins = result["plugins"];
        for (Json::ArrayIndex i = 0; i < plugins.size(); ++i)
        {
            if (plugins[i]["typeId"].asString() == typeId)
                return true;
        }
        return false;
    }

    Json::Value BrowserLoad(const char* typeId)
    {
        Json::Value data;
        data["typeId"] = typeId;
        return mBridge.InvokeRequestHandler(StringCRC("plugin_browser.load"), data);
    }

    // Executing a present command loads/unloads through the loader; an absent command reports failure.
    static bool PaletteLoadExists(const char* lowerTypeId)
    {
        const std::string name = std::string("plugin.load.") + lowerTypeId;
        return Dia::API::ExecuteCommandJson(StringCRC(name.c_str()), Json::Value(Json::objectValue))["success"].asBool();
    }

    // Editor startup: tag game-scoped types from the known games before any game loads (EditorModelModule::DoStart).
    void DiscoverAtStartup()
    {
        GameScopeDiscovery::DiscoverAndTag({ mFiles.dir }, 3, {});
    }

    bool LoadStubGame()  { return mModel.LoadDiagameProject(mFiles.stubGame.c_str()); }
    bool LoadOtherGame() { return mModel.LoadDiagameProject(mFiles.otherGame.c_str()); }

    Json::Value SwitchCall(const char* name, const Json::Value& data = Json::Value(Json::objectValue))
    {
        return mSwitchBridge.InvokeRequestHandler(StringCRC(name), data);
    }

    Json::Value SwitchRespond(const char* choice)
    {
        Json::Value d;
        d["choice"] = choice;
        return SwitchCall("game_switch.respond", d);
    }

    WebUIBridge mBridge;
    WebUIBridge mSwitchBridge;
    EditorModel mModel;
    Cluiche::Editor::PluginLoaderModule mLoader;
    GameSwitchController mSwitch;
    PluginBrowserEditorPlugin mBrowser;
    Dia::AssetCatalogue::Editor::DiaAssetCatalogueEditorPlugin mCatalogue;
    bool mBrowserLoaded = false;
    bool mCatalogueLoaded = false;
    FixtureFiles mFiles;
};

// ---------------------------------------------------------------------------
// AC1: stub game loaded -> listed by the browser and loadable
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC1_StubGameLoaded_StubListedAndLoadable)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());

    EXPECT_TRUE(Listed(kStubGameType));

    Json::Value r = BrowserLoad(kStubGameType);
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().loads, 1);
}

TEST_F(GameScopedPluginsIntegration, AC1_StubGameLoaded_PaletteCommandOpensStub)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());

    EXPECT_TRUE(PaletteLoadExists("stubgameplugin"));
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
}

TEST_F(GameScopedPluginsIntegration, AC1_BrowserLoadedAfterGame_StillListsStub)
{
    ASSERT_TRUE(LoadStubGame());
    LoadBrowser();
    EXPECT_TRUE(Listed(kStubGameType));
    EXPECT_TRUE(PaletteLoadExists("stubgameplugin"));
}

// ---------------------------------------------------------------------------
// AC2: other game / no game -> not listed, not loadable, layout/memory/manifest naming it open without it
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC2_OtherGameAfterStubGame_StubNotListedNotLoadable)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(Listed(kStubGameType));
    ASSERT_TRUE(LoadOtherGame());

    EXPECT_FALSE(Listed(kStubGameType));
    Json::Value r = BrowserLoad(kStubGameType);
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["error"].asString(), "plugin is not available in the current game");
    EXPECT_FALSE(PaletteLoadExists("stubgameplugin"));
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().loads, 0);
}

TEST_F(GameScopedPluginsIntegration, AC2_StubGameClosed_StubNotListedNotLoadable)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    mModel.ClearDiagameProject();

    EXPECT_FALSE(Listed(kStubGameType));
    EXPECT_FALSE(BrowserLoad(kStubGameType)["success"].asBool());
    EXPECT_FALSE(PaletteLoadExists("stubgameplugin"));
    EXPECT_EQ(GameCounters().loads, 0);
}

// Spec AC2 "or no game loaded": startup discovery tags the stub before any game loads, so it is out of scope.
TEST_F(GameScopedPluginsIntegration, AC2_ColdStartNoGame_StubNotListedNotLoadable)
{
    DiscoverAtStartup();
    LoadBrowser();

    EXPECT_FALSE(Listed(kStubGameType));
    EXPECT_FALSE(BrowserLoad(kStubGameType)["success"].asBool());
    EXPECT_FALSE(PaletteLoadExists("stubgameplugin"));
}

// The same, with only a non-editor game loaded this session.
TEST_F(GameScopedPluginsIntegration, AC2_ColdStartOtherGameOnly_StubNotListedNotLoadable)
{
    DiscoverAtStartup();
    LoadBrowser();
    ASSERT_TRUE(LoadOtherGame());

    EXPECT_FALSE(Listed(kStubGameType));
    EXPECT_FALSE(BrowserLoad(kStubGameType)["success"].asBool());
}

TEST_F(GameScopedPluginsIntegration, AC2_MemoryNamingStubOpensWithoutIt)
{
    // Same route PluginLoaderModule::DoStart uses for editor memory: every saved type goes through LoadPlugin.
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(LoadOtherGame());

    EditorMemory memory;
    memory.AddPlugin(kStubGameType, kStubGameType);
    memory.AddPlugin(kStubGenericType, kStubGenericType);
    for (unsigned int i = 0; i < memory.GetPluginCount(); ++i)
        mLoader.LoadPlugin(StringCRC(memory.GetPlugin(i).typeId), StringCRC::kZero);

    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().loads, 0);
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGenericType)));
}

TEST_F(GameScopedPluginsIntegration, AC2_ManifestNamingStubOpensWithoutIt)
{
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(LoadOtherGame());

    mLoader.LoadManifest(mFiles.stubManifest.c_str());
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().loads, 0);
}

// ---------------------------------------------------------------------------
// AC3: switching stub game -> other game closes the open stub plugin
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC3_SwitchToOtherGame_ClosesOpenStub)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    ASSERT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    ASSERT_EQ(GameCounters().loads, 1);
    ASSERT_EQ(GameCounters().unloads, 0);

    ASSERT_TRUE(LoadOtherGame());

    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 1);
    EXPECT_FALSE(Listed(kStubGameType));
    EXPECT_FALSE(PaletteLoadExists("stubgameplugin"));
}

TEST_F(GameScopedPluginsIntegration, AC3_CloseGame_ClosesOpenStub)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    ASSERT_EQ(GameCounters().loads, 1);

    mModel.ClearDiagameProject();

    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 1);
}

TEST_F(GameScopedPluginsIntegration, AC3_SwitchBackToStubGame_StubAvailableAgain)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    ASSERT_TRUE(LoadOtherGame());
    ASSERT_FALSE(Listed(kStubGameType));

    ASSERT_TRUE(LoadStubGame());
    EXPECT_TRUE(Listed(kStubGameType));
    EXPECT_TRUE(BrowserLoad(kStubGameType)["success"].asBool());
    EXPECT_EQ(GameCounters().loads, 2);
    EXPECT_EQ(GameCounters().unloads, 1);
}

// ---------------------------------------------------------------------------
// AC4: a game plugin's asset-type route exists only while it is loaded; otherwise open_asset has no editor
// route and falls back to the OS default app
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC4_RouteExistsOnlyWhileGamePluginLoaded)
{
    LoadCatalogue();
    ASSERT_TRUE(LoadStubGame());
    EXPECT_FALSE(StubRouteExists());   // plugin not loaded yet: fallback

    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    EXPECT_TRUE(StubRouteExists());

    ASSERT_TRUE(LoadOtherGame());      // scope change closes the plugin
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_FALSE(StubRouteExists());   // fallback again
}

TEST_F(GameScopedPluginsIntegration, AC4_RouteRemovedWhenGameClosed)
{
    LoadCatalogue();
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    ASSERT_TRUE(StubRouteExists());

    mModel.ClearDiagameProject();
    EXPECT_FALSE(StubRouteExists());
}

TEST_F(GameScopedPluginsIntegration, AC4_RouteReturnsWhenGameReloaded)
{
    LoadCatalogue();
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    ASSERT_TRUE(LoadOtherGame());
    ASSERT_FALSE(StubRouteExists());

    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    EXPECT_TRUE(StubRouteExists());
}

TEST_F(GameScopedPluginsIntegration, AC4_NoCataloguePlugin_LoadAndUnloadStillWork)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    ASSERT_TRUE(LoadOtherGame());
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 1);
}

// ---------------------------------------------------------------------------
// AC5: live-connection diagame_path loads the scope without a restart
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC5_LiveDiagamePath_NoGameLoaded_ScopeAppliesWithoutRestart)
{
    LoadBrowser();
    GameConnectionController gc;
    gc.SetEditorContext(&mModel);
    gc.SetGameSwitchController(&mSwitch);

    gc.ApplyLiveDiagamePath(mFiles.stubGame.c_str());

    EXPECT_TRUE(mModel.GetDiagameProject().IsValid());
    EXPECT_TRUE(Listed(kStubGameType));
    EXPECT_TRUE(BrowserLoad(kStubGameType)["success"].asBool());
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
}

TEST_F(GameScopedPluginsIntegration, AC5_LiveDiagamePath_DifferentGame_AppliesScopeAfterConfirm)
{
    LoadBrowser();
    ASSERT_TRUE(LoadOtherGame());
    GameConnectionController gc;
    gc.SetEditorContext(&mModel);
    gc.SetGameSwitchController(&mSwitch);

    gc.ApplyLiveDiagamePath(mFiles.stubGame.c_str());

    // Pending confirmation: the scope has not moved yet (AC9 prompt).
    Json::Value s = SwitchCall("game_switch.get_state");
    EXPECT_STREQ(s["prompt"].asCString(), "confirm_switch");
    EXPECT_NE(std::string(mModel.GetDiagameProject().diagamePath), mFiles.stubGame);   // not switched yet
    EXPECT_FALSE(EditorPluginRegistry::Instance().IsGameScoped(StringCRC(kStubGameType)));

    Json::Value r = SwitchRespond("confirm");
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsGameScoped(StringCRC(kStubGameType)));
    EXPECT_STREQ(r["outcome"].asCString(), "switched");
    EXPECT_TRUE(Listed(kStubGameType));
    EXPECT_TRUE(BrowserLoad(kStubGameType)["success"].asBool());
}

TEST_F(GameScopedPluginsIntegration, AC5_LiveDiagamePath_SwitchAwayClosesOpenStub)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    GameConnectionController gc;
    gc.SetEditorContext(&mModel);
    gc.SetGameSwitchController(&mSwitch);

    gc.ApplyLiveDiagamePath(mFiles.otherGame.c_str());
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));   // still waiting on confirm

    SwitchRespond("confirm");
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 1);
}

TEST_F(GameScopedPluginsIntegration, AC5_LiveDiagamePath_DeclineKeepsStubGameAndPlugin)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    GameConnectionController gc;
    gc.SetEditorContext(&mModel);
    gc.SetGameSwitchController(&mSwitch);

    gc.ApplyLiveDiagamePath(mFiles.otherGame.c_str());
    SwitchRespond("cancel");

    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 0);
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsAvailable(StringCRC(kStubGameType)));
}

// ---------------------------------------------------------------------------
// AC6: generic plugins are available in every case
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC6_GenericPlugin_AvailableWithNoGame)
{
    LoadBrowser();
    EXPECT_TRUE(Listed(kStubGenericType));
    EXPECT_TRUE(BrowserLoad(kStubGenericType)["success"].asBool());
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGenericType)));
}

TEST_F(GameScopedPluginsIntegration, AC6_GenericPlugin_AvailableWithStubGame)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    EXPECT_TRUE(Listed(kStubGenericType));
    EXPECT_TRUE(BrowserLoad(kStubGenericType)["success"].asBool());
}

TEST_F(GameScopedPluginsIntegration, AC6_GenericPlugin_AvailableWithOtherGame)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(LoadOtherGame());
    EXPECT_TRUE(Listed(kStubGenericType));
    EXPECT_TRUE(PaletteLoadExists("stubgenericplugin"));
}

TEST_F(GameScopedPluginsIntegration, AC6_GenericPlugin_SurvivesGameSwitches)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGenericType), StringCRC("inst"));
    ASSERT_TRUE(LoadOtherGame());
    mModel.ClearDiagameProject();

    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGenericType)));
    EXPECT_EQ(GenericCounters().unloads, 0);
}

// ---------------------------------------------------------------------------
// AC7: a .diagame without an editor key behaves as today
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC7_DiagameWithoutEditorKey_LoadsAndTagsNothing)
{
    LoadBrowser();
    ASSERT_TRUE(LoadOtherGame());

    EXPECT_TRUE(mModel.GetDiagameProject().IsValid());
    EXPECT_FALSE(EditorPluginRegistry::Instance().IsGameScoped(StringCRC(kStubGameType)));
    EXPECT_FALSE(EditorPluginRegistry::Instance().IsGameScoped(StringCRC(kStubGenericType)));
    EXPECT_TRUE(Listed(kStubGenericType));
    EXPECT_TRUE(BrowserLoad(kStubGenericType)["success"].asBool());
}

TEST_F(GameScopedPluginsIntegration, AC7_DiagameWithoutEditorKey_ClosesNothingGeneric)
{
    mLoader.LoadPlugin(StringCRC(kStubGenericType), StringCRC("inst"));
    ASSERT_TRUE(LoadOtherGame());
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGenericType)));
    EXPECT_EQ(GenericCounters().unloads, 0);
}

// ---------------------------------------------------------------------------
// AC8 / AC9 are covered by TestGameSwitchController.cpp with a fake loader. This single test only checks
// the real PluginLoaderModule feeds the gate: a dirty in-scope game plugin is reported and Cancel keeps it.
// ---------------------------------------------------------------------------

TEST_F(GameScopedPluginsIntegration, AC8_RealLoader_DirtyStubPromptsAndCancelKeepsGame)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    GameCounters().dirty = true;

    EXPECT_EQ(mSwitch.RequestLoad(mFiles.otherGame.c_str(), SwitchOrigin::kUser), SwitchOutcome::kNone);
    Json::Value s = SwitchCall("game_switch.get_state");
    EXPECT_STREQ(s["prompt"].asCString(), "unsaved");
    ASSERT_EQ(s["dirty"].size(), 1u);
    EXPECT_STREQ(s["dirty"][0]["typeId"].asCString(), kStubGameType);

    SwitchRespond("cancel");
    EXPECT_TRUE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 0);
}

TEST_F(GameScopedPluginsIntegration, AC8_RealLoader_SaveThenSwitchClosesStub)
{
    ASSERT_TRUE(LoadStubGame());
    mLoader.LoadPlugin(StringCRC(kStubGameType), StringCRC("inst"));
    GameCounters().dirty = true;

    mSwitch.RequestLoad(mFiles.otherGame.c_str(), SwitchOrigin::kUser);
    Json::Value r = SwitchRespond("save");

    EXPECT_STREQ(r["outcome"].asCString(), "switched");
    EXPECT_EQ(GameCounters().saves, 1);
    EXPECT_FALSE(mLoader.IsPluginTypeLoaded(StringCRC(kStubGameType)));
    EXPECT_EQ(GameCounters().unloads, 1);
}

// ---------------------------------------------------------------------------
// Plan note: palette Register/UnregisterCommandJson must not be rejected when a game loads inside a
// command callback (DiaAPI rejects registry mutation from inside a callback).
// ---------------------------------------------------------------------------

// DiaAPI rejects Register/UnregisterCommandJson from inside a command callback, so the browser defers the
// palette sync to its next OnUpdate when a game loads from a command.
TEST_F(GameScopedPluginsIntegration, PaletteCommandsSyncedWhenGameLoadsInsideCommandCallback)
{
    LoadBrowser();
    // Tag the stub and move out of scope first so the palette entries really have to be (un)registered.
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(LoadOtherGame());
    ASSERT_FALSE(PaletteLoadExists("stubgameplugin"));

    auto* prevAssert = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = CountingAssertHandler;
    gAssertCount.store(0);

    auto makeCmd = [this](const char* name, bool loadStub)
    {
        Dia::API::CommandInfoJson cmd;
        cmd.name = StringCRC(name);
        cmd.description = "test: switch game from inside a command callback";
        cmd.category = StringCRC("gsp_test");
        cmd.owner = "GameScopedPluginsIntegration";
        cmd.callback = [this, loadStub](const Json::Value&) -> Json::Value
        {
            if (loadStub) LoadStubGame(); else LoadOtherGame();
            return Json::Value(Json::objectValue);
        };
        return Dia::API::RegisterCommandJson(cmd);
    };
    ASSERT_TRUE(makeCmd("gsp_test.load_stub_game", true));
    ASSERT_TRUE(makeCmd("gsp_test.load_other_game", false));

    Dia::API::ExecuteCommandJson(StringCRC("gsp_test.load_stub_game"), Json::Value(Json::objectValue));
    const int assertsOnLoad = gAssertCount.load();
    const bool presentAfterLoad = Listed(kStubGameType);   // registry scope itself, independent of palette

    Dia::API::ExecuteCommandJson(StringCRC("gsp_test.load_other_game"), Json::Value(Json::objectValue));
    const int assertsTotal = gAssertCount.load();

    Dia::Core::g_pAssertFunc = prevAssert;
    Dia::API::UnregisterCommandJson(StringCRC("gsp_test.load_stub_game"));
    Dia::API::UnregisterCommandJson(StringCRC("gsp_test.load_other_game"));

    EXPECT_TRUE(presentAfterLoad);
    EXPECT_EQ(assertsOnLoad, 0) << "palette RegisterCommandJson was rejected inside the callback";
    EXPECT_EQ(assertsTotal, 0) << "palette UnregisterCommandJson was rejected inside the callback";
    mBrowser.OnUpdate(0.0f);
    EXPECT_FALSE(PaletteLoadExists("stubgameplugin"));
}

// Same as above (register direction).
TEST_F(GameScopedPluginsIntegration, PaletteCommandsAppearWhenStubGameLoadsInsideCommandCallback)
{
    LoadBrowser();
    ASSERT_TRUE(LoadStubGame());
    ASSERT_TRUE(LoadOtherGame());

    auto* prevAssert = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = CountingAssertHandler;
    gAssertCount.store(0);

    Dia::API::CommandInfoJson cmd;
    cmd.name = StringCRC("gsp_test.load_stub_game2");
    cmd.description = "test: load the stub game from inside a command callback";
    cmd.category = StringCRC("gsp_test");
    cmd.owner = "GameScopedPluginsIntegration";
    cmd.callback = [this](const Json::Value&) -> Json::Value { LoadStubGame(); return Json::Value(Json::objectValue); };
    ASSERT_TRUE(Dia::API::RegisterCommandJson(cmd));

    Dia::API::ExecuteCommandJson(StringCRC("gsp_test.load_stub_game2"), Json::Value(Json::objectValue));
    const int asserts = gAssertCount.load();
    Dia::Core::g_pAssertFunc = prevAssert;
    Dia::API::UnregisterCommandJson(StringCRC("gsp_test.load_stub_game2"));

    EXPECT_EQ(asserts, 0);
    mBrowser.OnUpdate(0.0f);
    EXPECT_TRUE(PaletteLoadExists("stubgameplugin"));
}
