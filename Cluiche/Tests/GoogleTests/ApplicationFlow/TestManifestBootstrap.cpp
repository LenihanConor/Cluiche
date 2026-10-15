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

TEST(ManifestBootstrap, LoadManifest_MissingFile_ReturnsFalse)
{
    ApplicationManifestV3 manifest;
    EXPECT_FALSE(LoadManifest("does/not/exist.diagame", manifest, TypeRegistry::Global()));
}
