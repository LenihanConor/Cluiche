#include <gtest/gtest.h>
#include <DiaEditor/Plugin/PluginBrowserEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

using namespace Dia::Editor;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace
{
    class BrowserStubFactory : public IEditorPluginFactory
    {
    public:
        IEditorPlugin* Create() override { return nullptr; }
        EditorPluginInfo GetPluginInfo() override { return EditorPluginInfo(); }
    };

    class RecordingLoader : public IPluginLoader
    {
    public:
        void LoadPlugin(const StringCRC& typeId, const StringCRC&) override { mLoaded = typeId; ++mLoadCalls; }
        bool UnloadPlugin(const StringCRC&) override { return false; }
        bool IsPluginTypeLoaded(const StringCRC&) const override { return false; }
        bool IsPluginPinned(const StringCRC&) const override { return false; }

        unsigned int mLoadCalls = 0;
        StringCRC mLoaded;
    };

    BrowserStubFactory sFactory;

    const StringCRC kBuiltIn("PBScopeTest_BuiltIn");
    const StringCRC kProjOnly("PBScopeTest_ProjOnly");
    const StringCRC kGameA("PBScopeTest_GameA");
    const StringCRC kGameB("PBScopeTest_GameB");
    const StringCRC kGameIdA("PBScopeTest_IdA");
    const StringCRC kGameIdB("PBScopeTest_IdB");
    const StringCRC kProjManifest("PBScopeTest_ProjManifest");

    typedef DynamicArrayC<StringCRC, EditorPluginRegistry::kMaxPlugins> ScopeList;

    ScopeList List(const StringCRC& a)
    {
        ScopeList l;
        l.Add(a);
        return l;
    }
}

class PluginBrowserGameScope : public ::testing::Test
{
protected:
    PluginBrowserGameScope() : mBridge(nullptr) {}

    void SetUp() override
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        const StringCRC ids[] = { kBuiltIn, kProjOnly, kGameA, kGameB };
        for (const StringCRC& id : ids)
        {
            if (!r.IsPluginRegistered(id))
                r.RegisterPlugin(id, &sFactory);
        }
        Reset();
        r.TagPluginManifest(kProjOnly, kProjManifest);
        r.TagGameScoped(kGameA, kGameIdA);
        r.TagGameScoped(kGameB, kGameIdB);
    }

    void LoadBrowser()
    {
        EditorPluginContext ctx;
        ctx.mBridge = &mBridge;
        ctx.mPluginLoader = &mLoader;
        mPlugin.OnLoad(ctx);
        mBrowserLoaded = true;
    }

    void TearDown() override
    {
        if (mBrowserLoaded)
            mPlugin.OnUnload();
        Reset();
    }

    void Reset()
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        r.ClearGameScope();
        r.ClearActiveManifests();
        const StringCRC ids[] = { kBuiltIn, kProjOnly, kGameA, kGameB };
        for (const StringCRC& id : ids)
        {
            r.TagGameScoped(id, StringCRC());
            r.TagPluginManifest(id, StringCRC());
        }
    }

    bool Listed(const char* typeId)
    {
        Json::Value result = mBridge.InvokeRequestHandler(StringCRC("plugin_browser.get_available"), Json::Value());
        const Json::Value& plugins = result["plugins"];
        for (Json::ArrayIndex i = 0; i < plugins.size(); ++i)
        {
            if (std::string(plugins[i]["typeId"].asString()) == typeId)
                return true;
        }
        return false;
    }

    Json::Value Load(const char* typeId)
    {
        Json::Value data;
        data["typeId"] = typeId;
        return mBridge.InvokeRequestHandler(StringCRC("plugin_browser.load"), data);
    }

    static bool PaletteLoadExists(const char* lowerTypeId)
    {
        std::string name = std::string("plugin.load.") + lowerTypeId;
        Json::Value r = Dia::API::ExecuteCommandJson(StringCRC(name.c_str()), Json::Value(Json::objectValue));
        return r["success"].asBool();
    }

    static bool PaletteUnloadExists(const char* lowerTypeId)
    {
        std::string name = std::string("plugin.unload.") + lowerTypeId;
        Json::Value r = Dia::API::ExecuteCommandJson(StringCRC(name.c_str()), Json::Value(Json::objectValue));
        return r["success"].asBool();
    }

    WebUIBridge mBridge;
    RecordingLoader mLoader;
    PluginBrowserEditorPlugin mPlugin;
    bool mBrowserLoaded = false;
};

TEST_F(PluginBrowserGameScope, GetAvailableOmitsGameScopedPluginWithNoGame)
{
    LoadBrowser();
    EXPECT_TRUE(Listed("PBScopeTest_BuiltIn"));
    EXPECT_FALSE(Listed("PBScopeTest_GameA"));
    EXPECT_FALSE(Listed("PBScopeTest_GameB"));
}

TEST_F(PluginBrowserGameScope, GetAvailableIncludesGameScopedPluginWhenItsGameIsActive)
{
    LoadBrowser();
    EditorPluginRegistry::Instance().SetGameScope(kGameIdA, List(kGameA));
    EXPECT_TRUE(Listed("PBScopeTest_GameA"));
    EXPECT_FALSE(Listed("PBScopeTest_GameB"));
    EditorPluginRegistry::Instance().SetGameScope(kGameIdB, List(kGameB));
    EXPECT_FALSE(Listed("PBScopeTest_GameA"));
    EXPECT_TRUE(Listed("PBScopeTest_GameB"));
}

TEST_F(PluginBrowserGameScope, GetAvailableStillOmitsCluicheprojFilteredPlugin)
{
    LoadBrowser();
    DynamicArrayC<StringCRC, EditorPluginRegistry::kMaxManifests> active;
    active.Add(StringCRC("PBScopeTest_OtherManifest"));
    EditorPluginRegistry::Instance().SetActiveManifests(active);
    EXPECT_FALSE(Listed("PBScopeTest_ProjOnly"));

    EditorPluginRegistry::Instance().ClearActiveManifests();
    EXPECT_TRUE(Listed("PBScopeTest_ProjOnly"));
}

TEST_F(PluginBrowserGameScope, LoadHandlerRejectsOutOfScopeType)
{
    LoadBrowser();
    Json::Value r = Load("PBScopeTest_GameA");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["error"].asString(), "plugin is not available in the current game");
    EXPECT_EQ(mLoader.mLoadCalls, 0u);
}

TEST_F(PluginBrowserGameScope, LoadHandlerLoadsInScopeType)
{
    LoadBrowser();
    EditorPluginRegistry::Instance().SetGameScope(kGameIdA, List(kGameA));
    Json::Value r = Load("PBScopeTest_GameA");
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(mLoader.mLoadCalls, 1u);
    EXPECT_TRUE(mLoader.mLoaded == kGameA);
}

TEST_F(PluginBrowserGameScope, PaletteCommandsAbsentForOutOfScopePluginAtLoad)
{
    LoadBrowser();
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_builtin"));
    EXPECT_TRUE(PaletteUnloadExists("pbscopetest_builtin"));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gamea"));
    EXPECT_FALSE(PaletteUnloadExists("pbscopetest_gamea"));
}

TEST_F(PluginBrowserGameScope, PaletteCommandsFollowGameScope)
{
    LoadBrowser();
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();

    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_gamea"));
    EXPECT_TRUE(PaletteUnloadExists("pbscopetest_gamea"));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gameb"));

    r.SetGameScope(kGameIdB, List(kGameB));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gamea"));
    EXPECT_FALSE(PaletteUnloadExists("pbscopetest_gamea"));
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_gameb"));

    r.ClearGameScope();
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gameb"));
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_builtin"));
}

TEST_F(PluginBrowserGameScope, PaletteCommandsPresentAtLoadWhenGameAlreadyActive)
{
    EditorPluginRegistry::Instance().SetGameScope(kGameIdA, List(kGameA));
    LoadBrowser();
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_gamea"));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gameb"));
}

TEST_F(PluginBrowserGameScope, UnloadRemovesPaletteCommandsAndListener)
{
    LoadBrowser();
    EditorPluginRegistry::Instance().SetGameScope(kGameIdA, List(kGameA));
    EXPECT_TRUE(PaletteLoadExists("pbscopetest_gamea"));

    mPlugin.OnUnload();
    mBrowserLoaded = false;
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gamea"));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_builtin"));

    // Scope change after unload must not touch the destroyed/unloaded browser (no listener left)
    EditorPluginRegistry::Instance().SetGameScope(kGameIdB, List(kGameB));
    EXPECT_FALSE(PaletteLoadExists("pbscopetest_gameb"));
}
