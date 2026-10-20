////////////////////////////////////////////////////////////////////////////////
// Filename: TestManifestBootstrap.cpp
// GoogleTest suite — DiaApplicationFlow ManifestBootstrap
//
// RegisterPathAliases (diagameConfig -> PathStore) and LoadManifest failure paths.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <string>
#include <DiaApplicationFlow/Manifest/ManifestBootstrap.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace {

ApplicationManifestV3 MakeManifestWithAliases(const char* aliasName, const char* relPath)
{
    ApplicationManifestV3 manifest;
    manifest.diagameConfig = new Json::Value(Json::objectValue);
    (*manifest.diagameConfig)["path_aliases"] = Json::Value(Json::objectValue);
    (*manifest.diagameConfig)["path_aliases"][aliasName] = relPath;
    return manifest;
}

} // namespace

TEST(ManifestBootstrap, RegisterPathAliases_ResolvesRelativeToDiagameDirectory)
{
    ApplicationManifestV3 manifest = MakeManifestWithAliases("bootstrap_test_assets", "data/assets");

    RegisterPathAliases(manifest, "games/cow/assets/cow.diagame");

    const Path::Alias alias("bootstrap_test_assets");
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(alias));
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(alias)), "games/cow/assets/data/assets");
}

TEST(ManifestBootstrap, RegisterPathAliases_DiagameWithoutDirectory_ResolvesRelativeToCwd)
{
    ApplicationManifestV3 manifest = MakeManifestWithAliases("bootstrap_test_nodir", "data");

    RegisterPathAliases(manifest, "cow.diagame");

    const Path::Alias alias("bootstrap_test_nodir");
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(alias));
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(alias)), "data");
}

TEST(ManifestBootstrap, RegisterPathAliases_NoDiagameConfig_IsNoOp)
{
    ApplicationManifestV3 manifest;
    RegisterPathAliases(manifest, "assets/cow.diagame"); // must not crash
    SUCCEED();
}

TEST(ManifestBootstrap, RegisterPathAliases_NoPathAliasesKey_IsNoOp)
{
    ApplicationManifestV3 manifest;
    manifest.diagameConfig = new Json::Value(Json::objectValue);
    RegisterPathAliases(manifest, "assets/cow.diagame"); // must not crash
    SUCCEED();
}

TEST(ManifestBootstrap, RegisterDevAliases_SingleArg_Registers)
{
    const char* argv[] = { "app.exe", "--dev-alias", "devalias_one=C:/abs/one" };
    RegisterDevAliases(3, argv);

    const Path::Alias alias("devalias_one");
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(alias));
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(alias)), "C:/abs/one");
    PathStore::UnregisterFromStore(alias);
}

TEST(ManifestBootstrap, RegisterDevAliases_MultipleArgs_AllRegister)
{
    const char* argv[] = { "app.exe", "--dev-alias", "devalias_a=C:/abs/a", "--other", "--dev-alias", "devalias_b=C:/abs/b" };
    RegisterDevAliases(6, argv);

    const Path::Alias a("devalias_a");
    const Path::Alias b("devalias_b");
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(a));
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(b));
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(a)), "C:/abs/a");
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(b)), "C:/abs/b");
    PathStore::UnregisterFromStore(a);
    PathStore::UnregisterFromStore(b);
}

TEST(ManifestBootstrap, RegisterDevAliases_Malformed_Ignored)
{
    const char* argv[] = { "app.exe", "--dev-alias", "devalias_noequals", "--dev-alias", "=C:/abs/x", "--dev-alias" };
    RegisterDevAliases(6, argv); // must not crash

    EXPECT_FALSE(PathStore::IsPathAliasRegistered(Path::Alias("devalias_noequals")));
}

TEST(ManifestBootstrap, RegisterDevAliases_NoArgs_SourceUnresolved)
{
    const char* argv[] = { "app.exe" };
    RegisterDevAliases(1, argv);
    EXPECT_FALSE(PathStore::IsPathAliasRegistered(Path::Alias("source")));
}

TEST(ManifestBootstrap, RegisterPathAliases_ManifestSource_DoesNotOverrideDevAlias)
{
    const char* argv[] = { "app.exe", "--dev-alias", "source=C:/abs/src" };
    RegisterDevAliases(3, argv);

    ApplicationManifestV3 manifest = MakeManifestWithAliases("source", "data/other");
    RegisterPathAliases(manifest, "games/cow/assets/cow.diagame"); // logs Error, must not assert

    const Path::Alias alias("source");
    ASSERT_TRUE(PathStore::IsPathAliasRegistered(alias));
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(alias)), "C:/abs/src");
    PathStore::UnregisterFromStore(alias);
}

TEST(ManifestBootstrap, RegisterPathAliases_ManifestSource_NoDevAlias_StaysUnregistered)
{
    ApplicationManifestV3 manifest = MakeManifestWithAliases("source", "data/other");
    RegisterPathAliases(manifest, "cow.diagame");
    EXPECT_FALSE(PathStore::IsPathAliasRegistered(Path::Alias("source")));
}

TEST(ManifestBootstrap, RegisterPathAliases_ManifestWinsOverDevAliasForNonSource)
{
    const char* argv[] = { "app.exe", "--dev-alias", "devalias_wins=C:/abs/dev" };
    RegisterDevAliases(3, argv);

    ApplicationManifestV3 manifest = MakeManifestWithAliases("devalias_wins", "data");
    RegisterPathAliases(manifest, "assets/cow.diagame");

    const Path::Alias alias("devalias_wins");
    EXPECT_EQ(std::string(PathStore::ResolvePathToCString(alias)), "assets/data");
    PathStore::UnregisterFromStore(alias);
}

TEST(ManifestBootstrap, LoadManifest_MissingFile_ReturnsFalse)
{
    ApplicationManifestV3 manifest;
    EXPECT_FALSE(LoadManifest("does/not/exist.diagame", manifest, TypeRegistry::Global()));
}
