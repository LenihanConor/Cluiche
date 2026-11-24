// Unit tests for Dia::Editor::GameScopeDiscovery (startup tagging of game-scoped plugin types).

#include <gtest/gtest.h>
#include "GameScopedPluginFixtures.h"

#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Project/GameScopeDiscovery.h>
#include <DiaEditor/Project/GamePath.h>
#include <ApplicationFlow/Modules/EditorModelModule.h>

#include <filesystem>
#include <fstream>

using namespace Dia::Editor;
using Dia::Core::StringCRC;
using namespace GameScopedFixtures;
namespace fs = std::filesystem;

class GameScopeDiscoveryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureStubsRegistered();
        ResetEditorRegistry();
        fs::remove_all(kRoot);
        fs::create_directories(kRoot);
    }
    void TearDown() override
    {
        ResetEditorRegistry();
        std::error_code ec;
        fs::remove_all(kRoot, ec);
    }

    // Writes stub_editor.diaapp + game.diagame (listing StubGamePlugin) into <kRoot>/<sub>.
    std::string WriteStubGameIn(const std::string& sub)
    {
        const fs::path dir = fs::path(kRoot) / sub;
        fs::create_directories(dir);
        {
            std::ofstream o(dir / "stub_editor.diaapp");
            o << "{ \"editor\": { \"enabled\": true, \"plugins\": [ {\"type\": \"" << kStubGameType << "\"} ] } }\n";
        }
        const fs::path game = dir / "game.diagame";
        {
            std::ofstream o(game);
            o << "{ \"name\": \"G\", \"version\": \"1.0\", \"imports\": [], \"config\": {},"
                 " \"editor\": { \"manifests\": [\"stub_editor.diaapp\"] } }\n";
        }
        return game.generic_string();
    }

    static constexpr const char* kRoot = "game_scope_discovery_tmp";
};

TEST_F(GameScopeDiscoveryTest, Scan_FindsNestedDiagames)
{
    WriteStubGameIn("a");
    WriteStubGameIn("b/c");
    std::vector<std::string> found;
    GameScopeDiscovery::ScanForDiagames(kRoot, 3, found);
    EXPECT_EQ(found.size(), 2u);
}

TEST_F(GameScopeDiscoveryTest, Scan_DepthLimitStopsRecursion)
{
    WriteStubGameIn("a");              // file at depth 1
    WriteStubGameIn("x/y/z");          // file at depth 3
    std::vector<std::string> shallow;
    GameScopeDiscovery::ScanForDiagames(kRoot, 1, shallow);
    EXPECT_EQ(shallow.size(), 1u);
    std::vector<std::string> deep;
    GameScopeDiscovery::ScanForDiagames(kRoot, 3, deep);
    EXPECT_EQ(deep.size(), 2u);
}

TEST_F(GameScopeDiscoveryTest, Scan_MissingRootIsEmptyAndHarmless)
{
    std::vector<std::string> found;
    GameScopeDiscovery::ScanForDiagames("game_scope_discovery_does_not_exist", 3, found);
    GameScopeDiscovery::ScanForDiagames(nullptr, 3, found);
    GameScopeDiscovery::ScanForDiagames("", 3, found);
    EXPECT_TRUE(found.empty());
    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({ "game_scope_discovery_does_not_exist" }, 3, { "missing.diagame" }), 0u);
}

TEST_F(GameScopeDiscoveryTest, Discover_TagsTypeWithoutActivatingScope)
{
    WriteStubGameIn("a");
    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({ kRoot }, 3, {}), 1u);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EXPECT_TRUE(r.IsGameScoped(StringCRC(kStubGameType)));
    EXPECT_TRUE(r.IsOutOfGameScope(StringCRC(kStubGameType)));
    EXPECT_FALSE(r.IsAvailable(StringCRC(kStubGameType)));
    EXPECT_TRUE(r.GetActiveGameId() == StringCRC());
    EXPECT_FALSE(r.IsGameScoped(StringCRC(kStubGenericType)));
}

TEST_F(GameScopeDiscoveryTest, Discover_MalformedDiagameSkippedOthersStillTagged)
{
    const std::string good = WriteStubGameIn("good");
    fs::create_directories(fs::path(kRoot) / "bad");
    { std::ofstream o(fs::path(kRoot) / "bad" / "bad.diagame"); o << "{ not json"; }
    { std::ofstream o(fs::path(kRoot) / "empty.diagame"); }

    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({ kRoot }, 3, {}), 1u);
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsOutOfGameScope(StringCRC(kStubGameType)));
}

TEST_F(GameScopeDiscoveryTest, Discover_MissingEditorManifestTagsNothing)
{
    const fs::path game = fs::path(kRoot) / "g.diagame";
    {
        std::ofstream o(game);
        o << "{ \"name\": \"G\", \"version\": \"1.0\", \"imports\": [], \"config\": {},"
             " \"editor\": { \"manifests\": [\"nope.diaapp\"] } }\n";
    }
    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({ kRoot }, 3, {}), 1u);
    EXPECT_FALSE(EditorPluginRegistry::Instance().IsGameScoped(StringCRC(kStubGameType)));
}

TEST_F(GameScopeDiscoveryTest, Discover_DedupesRootScanAndExplicitPaths)
{
    const std::string game = WriteStubGameIn("a");
    const std::string respelled = std::string(kRoot) + "/a/../a/./game.diagame";
    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({ kRoot, kRoot }, 3, { game, respelled }), 1u);
}

TEST_F(GameScopeDiscoveryTest, Discover_ExplicitPathsAreTaggedWithoutRoots)
{
    const std::string game = WriteStubGameIn("a");
    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag({}, 3, { game }), 1u);
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsOutOfGameScope(StringCRC(kStubGameType)));
}

TEST_F(GameScopeDiscoveryTest, DiscoveredGameId_MatchesLoadDiagameProjectId)
{
    const std::string game = WriteStubGameIn("a");
    GameScopeDiscovery::DiscoverAndTag({ kRoot }, 3, {});
    ASSERT_TRUE(EditorPluginRegistry::Instance().IsOutOfGameScope(StringCRC(kStubGameType)));

    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(game.c_str()));

    EXPECT_FALSE(EditorPluginRegistry::Instance().IsOutOfGameScope(StringCRC(kStubGameType)));
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsAvailable(StringCRC(kStubGameType)));
}

TEST_F(GameScopeDiscoveryTest, TagGameFromDiagame_FalseForMissingOrEmptyPath)
{
    EXPECT_FALSE(GameScopeDiscovery::TagGameFromDiagame(nullptr));
    EXPECT_FALSE(GameScopeDiscovery::TagGameFromDiagame(""));
    EXPECT_FALSE(GameScopeDiscovery::TagGameFromDiagame("game_scope_discovery_nope.diagame"));
}

// ---------------------------------------------------------------------------
// Default discovery roots (EditorModelModule::ComputeDefaultGameRoots): deployed assets + source Assets
// ---------------------------------------------------------------------------

namespace
{
    // Mirrors the repo layout: <root>/Cluiche/bin/CluicheEditor/Debug/x64 (exe dir), <root>/Cluiche/Assets (source).
    std::string MakeExeDir(const std::string& root)
    {
        const fs::path exe = fs::path(root) / "Cluiche" / "bin" / "CluicheEditor" / "Debug" / "x64";
        fs::create_directories(exe);
        return exe.generic_string();
    }
}

TEST_F(GameScopeDiscoveryTest, DefaultRoots_IncludeDeployedAssetsAndExistingSourceAssets)
{
    const std::string exe = MakeExeDir(kRoot);
    fs::create_directories(fs::path(kRoot) / "Cluiche" / "Assets");

    std::vector<std::string> roots;
    Cluiche::Editor::EditorModelModule::ComputeDefaultGameRoots(exe.c_str(), roots);

    ASSERT_EQ(roots.size(), 2u);
    EXPECT_EQ(roots[0], exe + "/assets");
    EXPECT_TRUE(IsSameGamePath(roots[1].c_str(), (fs::path(kRoot) / "Cluiche" / "Assets").string().c_str()));
}

TEST_F(GameScopeDiscoveryTest, DefaultRoots_OmitSourceAssetsWhenMissing)
{
    const std::string exe = MakeExeDir(kRoot);

    std::vector<std::string> roots;
    Cluiche::Editor::EditorModelModule::ComputeDefaultGameRoots(exe.c_str(), roots);

    ASSERT_EQ(roots.size(), 1u);
    EXPECT_EQ(roots[0], exe + "/assets");
}

TEST_F(GameScopeDiscoveryTest, DefaultRoots_EmptyExeDirYieldsNothing)
{
    std::vector<std::string> roots;
    Cluiche::Editor::EditorModelModule::ComputeDefaultGameRoots("", roots);
    Cluiche::Editor::EditorModelModule::ComputeDefaultGameRoots(nullptr, roots);
    EXPECT_TRUE(roots.empty());
}

TEST_F(GameScopeDiscoveryTest, DefaultRoots_GameFoundViaDotDotRootHasSameIdAsPlainAbsolutePath)
{
    const std::string exe = MakeExeDir(kRoot);
    // The stub game lives under <root>/Cluiche/Assets/Stub/, reachable only through the ".." default root.
    const fs::path assetsDir = fs::path(kRoot) / "Cluiche" / "Assets";
    fs::create_directories(assetsDir);
    const std::string relGame = WriteStubGameIn("Cluiche/Assets/Stub");

    std::vector<std::string> roots;
    Cluiche::Editor::EditorModelModule::ComputeDefaultGameRoots(exe.c_str(), roots);
    ASSERT_EQ(roots.size(), 2u);
    ASSERT_NE(roots[1].find(".."), std::string::npos);

    EXPECT_EQ(GameScopeDiscovery::DiscoverAndTag(roots, 4, {}), 1u);
    ASSERT_TRUE(EditorPluginRegistry::Instance().IsOutOfGameScope(StringCRC(kStubGameType)));

    // Load by the plain absolute path (what File>Open / --project / diagame_path produce).
    const std::string plainAbsolute = fs::absolute(fs::path(relGame)).lexically_normal().string();
    ASSERT_EQ(plainAbsolute.find(".."), std::string::npos);
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(plainAbsolute.c_str()));
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsAvailable(StringCRC(kStubGameType)));
}
