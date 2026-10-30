#include <gtest/gtest.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>

using namespace Dia::Editor;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace
{
    class ScopeStubFactory : public IEditorPluginFactory
    {
    public:
        IEditorPlugin* Create() override { return nullptr; }
        EditorPluginInfo GetPluginInfo() override { return EditorPluginInfo(); }
    };

    ScopeStubFactory sStubFactory;

    const StringCRC kBuiltIn("GameScopeTest_BuiltIn");
    const StringCRC kProjOnly("GameScopeTest_ProjOnly");
    const StringCRC kGameA("GameScopeTest_GameA");
    const StringCRC kGameB("GameScopeTest_GameB");
    const StringCRC kGameIdA("GameScopeTest_IdA");
    const StringCRC kGameIdB("GameScopeTest_IdB");
    const StringCRC kProjManifest("GameScopeTest_ProjManifest");

    typedef DynamicArrayC<StringCRC, EditorPluginRegistry::kMaxPlugins> ScopeList;

    struct ChangeRecorder
    {
        unsigned int count = 0;
        StringCRC oldGame;
        StringCRC newGame;
    };

    void OnChanged(void* userData, const StringCRC& oldGame, const StringCRC& newGame)
    {
        ChangeRecorder* rec = static_cast<ChangeRecorder*>(userData);
        ++rec->count;
        rec->oldGame = oldGame;
        rec->newGame = newGame;
    }
}

class EditorPluginRegistryGameScope : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        const StringCRC ids[] = { kBuiltIn, kProjOnly, kGameA, kGameB };
        for (const StringCRC& id : ids)
        {
            if (!r.IsPluginRegistered(id))
                r.RegisterPlugin(id, &sStubFactory);
        }
        Reset();
        r.TagPluginManifest(kProjOnly, kProjManifest);
    }

    void TearDown() override { Reset(); }

    void Reset()
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        r.ClearGameScope();
        r.ClearActiveManifests();
        // An empty id clears a tag
        const StringCRC ids[] = { kBuiltIn, kProjOnly, kGameA, kGameB };
        for (const StringCRC& id : ids)
        {
            r.TagGameScoped(id, StringCRC());
            r.TagPluginManifest(id, StringCRC());
        }
    }

    static ScopeList List(const StringCRC& a)
    {
        ScopeList l;
        l.Add(a);
        return l;
    }
};

TEST_F(EditorPluginRegistryGameScope, BuiltInAlwaysAvailable)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EXPECT_TRUE(r.IsAvailable(kBuiltIn));
    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_TRUE(r.IsAvailable(kBuiltIn));
    EXPECT_FALSE(r.IsGameScoped(kBuiltIn));
}

TEST_F(EditorPluginRegistryGameScope, CluicheprojEntryUnchangedByGameScope)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    DynamicArrayC<StringCRC, EditorPluginRegistry::kMaxManifests> active;
    active.Add(StringCRC("GameScopeTest_OtherManifest"));
    r.SetActiveManifests(active);

    EXPECT_FALSE(r.IsInScopeFilter(kProjOnly));
    EXPECT_EQ(r.IsAvailable(kProjOnly), r.IsInScopeFilter(kProjOnly));

    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_EQ(r.IsAvailable(kProjOnly), r.IsInScopeFilter(kProjOnly));

    r.ClearActiveManifests();
    EXPECT_TRUE(r.IsAvailable(kProjOnly));
}

TEST_F(EditorPluginRegistryGameScope, GameScopedUnavailableWithNoGame)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.TagGameScoped(kGameA, kGameIdA);
    EXPECT_TRUE(r.IsGameScoped(kGameA));
    EXPECT_FALSE(r.IsAvailable(kGameA));
}

TEST_F(EditorPluginRegistryGameScope, AvailableWhenItsGameIsActive)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_TRUE(r.GetActiveGameId() == kGameIdA);
    EXPECT_TRUE(r.IsGameScoped(kGameA));
    EXPECT_TRUE(r.IsAvailable(kGameA));
    EXPECT_FALSE(r.IsGameScoped(kGameB));
}

TEST_F(EditorPluginRegistryGameScope, UnavailableAfterSwitchingGame)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, List(kGameA));
    r.SetGameScope(kGameIdB, List(kGameB));
    EXPECT_FALSE(r.IsAvailable(kGameA));
    EXPECT_TRUE(r.IsGameScoped(kGameA));
    EXPECT_TRUE(r.IsAvailable(kGameB));
}

TEST_F(EditorPluginRegistryGameScope, UnavailableAfterClear)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, List(kGameA));
    r.ClearGameScope();
    EXPECT_FALSE(r.IsAvailable(kGameA));
    EXPECT_TRUE(r.GetActiveGameId() == StringCRC());
}

TEST_F(EditorPluginRegistryGameScope, DiagameWinsOverCluicheproj)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, List(kProjOnly));
    EXPECT_TRUE(r.IsGameScoped(kProjOnly));
    EXPECT_TRUE(r.IsAvailable(kProjOnly));

    r.ClearGameScope();
    EXPECT_FALSE(r.IsAvailable(kProjOnly));
}

TEST_F(EditorPluginRegistryGameScope, EmptyScopeListScopesNothing)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, ScopeList());
    EXPECT_TRUE(r.GetActiveGameId() == kGameIdA);
    EXPECT_FALSE(r.IsGameScoped(kGameA));
    EXPECT_FALSE(r.IsGameScoped(kGameB));
    EXPECT_TRUE(r.IsAvailable(kBuiltIn));
}

TEST_F(EditorPluginRegistryGameScope, UnregisteredTypeNotAvailable)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    const StringCRC unknown("GameScopeTest_Unregistered");
    EXPECT_FALSE(r.IsAvailable(unknown));
    EXPECT_FALSE(r.IsGameScoped(unknown));
}

TEST_F(EditorPluginRegistryGameScope, ChangeNotificationFiresOnChangeOnly)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    ChangeRecorder rec;
    r.AddGameScopeListener(&OnChanged, &rec);

    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_EQ(rec.count, 1u);
    EXPECT_TRUE(rec.oldGame == StringCRC());
    EXPECT_TRUE(rec.newGame == kGameIdA);

    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_EQ(rec.count, 1u);

    r.SetGameScope(kGameIdB, List(kGameB));
    EXPECT_EQ(rec.count, 2u);
    EXPECT_TRUE(rec.oldGame == kGameIdA);
    EXPECT_TRUE(rec.newGame == kGameIdB);

    r.ClearGameScope();
    EXPECT_EQ(rec.count, 3u);
    EXPECT_TRUE(rec.newGame == StringCRC());

    r.ClearGameScope();
    EXPECT_EQ(rec.count, 3u);

    r.RemoveGameScopeListener(&OnChanged, &rec);
    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_EQ(rec.count, 3u);
}

TEST_F(EditorPluginRegistryGameScope, OutOfGameScopeFalseForBuiltInAndUnregistered)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EXPECT_FALSE(r.IsOutOfGameScope(kBuiltIn));
    EXPECT_FALSE(r.IsOutOfGameScope(StringCRC("GameScopeTest_Unregistered")));
}

TEST_F(EditorPluginRegistryGameScope, OutOfGameScopeFalseForFilteredCluicheprojPlugin)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    DynamicArrayC<StringCRC, EditorPluginRegistry::kMaxManifests> active;
    active.Add(StringCRC("GameScopeTest_OtherManifest"));
    r.SetActiveManifests(active);
    EXPECT_FALSE(r.IsAvailable(kProjOnly));
    EXPECT_FALSE(r.IsOutOfGameScope(kProjOnly));
}

TEST_F(EditorPluginRegistryGameScope, OutOfGameScopeTrueWhenGameInactive)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.TagGameScoped(kGameA, kGameIdA);
    EXPECT_TRUE(r.IsOutOfGameScope(kGameA));
    r.SetGameScope(kGameIdB, List(kGameB));
    EXPECT_TRUE(r.IsOutOfGameScope(kGameA));
    EXPECT_FALSE(r.IsOutOfGameScope(kGameB));
}

TEST_F(EditorPluginRegistryGameScope, OutOfGameScopeFalseWhenGameActive)
{
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    r.SetGameScope(kGameIdA, List(kGameA));
    EXPECT_FALSE(r.IsOutOfGameScope(kGameA));
    r.ClearGameScope();
    EXPECT_TRUE(r.IsOutOfGameScope(kGameA));
}
