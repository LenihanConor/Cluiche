#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/RuntimeManifestLoader.h>
#include <DiaAssetRuntime/AssetRuntime.h>

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    // Write a string to a temp file; return the path written.
    bool WriteTempFile(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
    {
        char tmpDir[256];
#if defined(_MSC_VER)
        GetTempPathA(sizeof(tmpDir), tmpDir);
#else
        strncpy(tmpDir, "/tmp/", sizeof(tmpDir) - 1);
        tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif

#if defined(_MSC_VER)
        snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);
#else
        snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);
#endif

        FILE* f = nullptr;
#if defined(_MSC_VER)
        fopen_s(&f, pathOut, "wb");
#else
        f = fopen(pathOut, "wb");
#endif
        if (!f) return false;
        fwrite(content, 1, strlen(content), f);
        fclose(f);
        return true;
    }

    // Register a PathStore alias for the test directory so FilePath can resolve.
    void EnsureTestAlias(const char* alias, const char* path)
    {
        Dia::Core::StringCRC aliasCRC(alias);
        if (!Dia::Core::PathStore::IsPathAliasRegistered(aliasCRC))
        {
            Dia::Core::Path::String p(path);
            Dia::Core::PathStore::RegisterToStore(aliasCRC, p);
        }
    }

    static const char* kTestAlias = "test_arun";

    void SetupTestAlias()
    {
        char tmpDir[256];
#if defined(_MSC_VER)
        GetTempPathA(sizeof(tmpDir), tmpDir);
        // Normalize slashes
        for (char* p = tmpDir; *p; ++p)
            if (*p == '\\') *p = '/';
        // Strip trailing slash for PathStore
        int len = (int)strlen(tmpDir);
        if (len > 0 && tmpDir[len-1] == '/')
            tmpDir[len-1] = '\0';
#else
        strncpy(tmpDir, "/tmp", sizeof(tmpDir) - 1);
        tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
        EnsureTestAlias(kTestAlias, tmpDir);
    }

    Dia::Core::FilePath MakeFilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kTestAlias), filename);
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class ManifestLoadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SetupTestAlias();
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(ManifestLoadTest, SuccessfulLoad_PopulatesAssetAndStageTables)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.player", "scope": "stage", "deploy_path": "stages/gameplay/player.png"},
            {"id": "config.settings", "scope": "global", "deploy_path": "global/settings.json"}
        ],
        "stages": [
            {"id": "stage.gameplay", "assets": ["texture.player", "config.settings"]}
        ]
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_success.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_success.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    bool result = loader.Load(fp, assetTable, stageTable);
    EXPECT_TRUE(result);
    EXPECT_EQ(assetTable.Size(), 2u);
    EXPECT_EQ(stageTable.Size(), 1u);

    EXPECT_TRUE(assetTable.ContainsKey(Dia::Core::StringCRC("texture.player")));
    EXPECT_TRUE(assetTable.ContainsKey(Dia::Core::StringCRC("config.settings")));
    EXPECT_TRUE(stageTable.ContainsKey(Dia::Core::StringCRC("stage.gameplay")));
}

TEST_F(ManifestLoadTest, PathResolution_ReturnsAbsolutePath)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.ship", "scope": "stage", "deploy_path": "stages/gameplay/ship.png"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_pathres.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_pathres.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    ASSERT_TRUE(loader.Load(fp, assetTable, stageTable));

    const Dia::AssetRuntime::RuntimeAssetEntry* entry =
        assetTable.TryGetItemConst(Dia::Core::StringCRC("texture.ship"));
    ASSERT_NE(entry, nullptr);

    // Path must contain the relative portion
    const char* deployPath = entry->mDeployPath.AsCStr();
    EXPECT_NE(strstr(deployPath, "ship.png"), nullptr);
    // Path must be absolute (longer than just the relative portion)
    EXPECT_GT(strlen(deployPath), strlen("stages/gameplay/ship.png"));
}

TEST_F(ManifestLoadTest, DuplicateAssetId_FailsLoad)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.ship", "scope": "stage", "deploy_path": "a.png"},
            {"id": "texture.ship", "scope": "stage", "deploy_path": "b.png"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_dup.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_dup.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}

TEST_F(ManifestLoadTest, DuplicateStageId_FailsLoad)
{
    static const char kJson[] = R"({
        "assets": [],
        "stages": [
            {"id": "stage.a", "assets": []},
            {"id": "stage.a", "assets": []}
        ]
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_dupstage.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_dupstage.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}

TEST_F(ManifestLoadTest, MissingFile_ReturnsFalse)
{
    Dia::Core::FilePath fp = MakeFilePath("does_not_exist_arun.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}

TEST_F(ManifestLoadTest, MalformedJson_ReturnsFalse)
{
    static const char kJson[] = R"({invalid json!!!})";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_malformed.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_malformed.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}

TEST_F(ManifestLoadTest, FolderAsset_TrailingSlashPreserved)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "folder.ui", "scope": "global", "deploy_path": "global/ui/"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_folder.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_folder.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    ASSERT_TRUE(loader.Load(fp, assetTable, stageTable));

    const Dia::AssetRuntime::RuntimeAssetEntry* entry =
        assetTable.TryGetItemConst(Dia::Core::StringCRC("folder.ui"));
    ASSERT_NE(entry, nullptr);

    const char* path = entry->mDeployPath.AsCStr();
    int len = (int)strlen(path);
    EXPECT_GT(len, 0);
    EXPECT_EQ(path[len - 1], '/');
}

TEST_F(ManifestLoadTest, ScopeParsing_GlobalAndStage)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.a", "scope": "stage",  "deploy_path": "a.png"},
            {"id": "config.b",  "scope": "global", "deploy_path": "b.json"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_scope.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_scope.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    ASSERT_TRUE(loader.Load(fp, assetTable, stageTable));

    const Dia::AssetRuntime::RuntimeAssetEntry* a =
        assetTable.TryGetItemConst(Dia::Core::StringCRC("texture.a"));
    const Dia::AssetRuntime::RuntimeAssetEntry* b =
        assetTable.TryGetItemConst(Dia::Core::StringCRC("config.b"));

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->mScope, Dia::AssetRuntime::AssetScope::kStage);
    EXPECT_EQ(b->mScope, Dia::AssetRuntime::AssetScope::kGlobal);
}

TEST_F(ManifestLoadTest, UnknownScope_FailsLoad)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.x", "scope": "unknown_scope", "deploy_path": "x.png"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_badscope.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_badscope.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}

TEST_F(ManifestLoadTest, StageTablePopulation_CorrectMemberIds)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.hero", "scope": "stage", "deploy_path": "hero.png"},
            {"id": "audio.bgm",   "scope": "stage", "deploy_path": "bgm.ogg"}
        ],
        "stages": [
            {"id": "stage.level1", "assets": ["texture.hero", "audio.bgm"]}
        ]
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_stage.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_stage.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    ASSERT_TRUE(loader.Load(fp, assetTable, stageTable));

    const Dia::AssetRuntime::RuntimeStageEntry* stage =
        stageTable.TryGetItemConst(Dia::Core::StringCRC("stage.level1"));
    ASSERT_NE(stage, nullptr);
    EXPECT_EQ(stage->mAssetIds.Size(), 2u);
}

TEST_F(ManifestLoadTest, AssetRuntime_ResolveAssetPath_KnownId)
{
    static const char kJson[] = R"({
        "assets": [
            {"id": "texture.hero", "scope": "stage", "deploy_path": "hero.png"}
        ],
        "stages": []
    })";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_resolve.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_resolve.json");

    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(runtime.LoadManifest(fp));

    const Dia::Core::Containers::String512* path =
        runtime.ResolveAssetPath(Dia::Core::StringCRC("texture.hero"));
    EXPECT_NE(path, nullptr);
    EXPECT_NE(strstr(path->AsCStr(), "hero.png"), nullptr);
}

TEST_F(ManifestLoadTest, AssetRuntime_ResolveAssetPath_UnknownId_ReturnsNull)
{
    static const char kJson[] = R"({"assets": [], "stages": []})";

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_test_null.json", kJson, filePath, sizeof(filePath)));

    Dia::Core::FilePath fp = MakeFilePath("arun_test_null.json");

    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(runtime.LoadManifest(fp));

    const Dia::Core::Containers::String512* path =
        runtime.ResolveAssetPath(Dia::Core::StringCRC("texture.nonexistent"));
    EXPECT_EQ(path, nullptr);
}

// ---------------------------------------------------------------------------
// Capacity boundary tests
// ---------------------------------------------------------------------------
namespace
{
    void BuildCapacityJson(char* buffer, unsigned int bufferSize, unsigned int assetCount)
    {
        int offset = snprintf(buffer, bufferSize, "{\"assets\":[");
        for (unsigned int i = 0; i < assetCount && offset < (int)bufferSize - 128; ++i)
        {
            if (i > 0)
                offset += snprintf(buffer + offset, bufferSize - offset, ",");
            offset += snprintf(buffer + offset, bufferSize - offset,
                "{\"id\":\"asset.cap%u\",\"scope\":\"stage\",\"deploy_path\":\"cap%u.bin\"}", i, i);
        }
        snprintf(buffer + offset, bufferSize - offset, "],\"stages\":[]}");
    }
}

TEST_F(ManifestLoadTest, AssetTableCapacity_ExactlyMaxAssets_Succeeds)
{
    static const unsigned int kMaxAssets = Dia::AssetRuntime::RuntimeManifestLoader::kMaxAssets;
    static const unsigned int kBufSize = kMaxAssets * 80 + 256;
    char* json = new char[kBufSize];
    BuildCapacityJson(json, kBufSize, kMaxAssets);

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_cap_max.json", json, filePath, sizeof(filePath)));
    delete[] json;

    Dia::Core::FilePath fp = MakeFilePath("arun_cap_max.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_TRUE(loader.Load(fp, assetTable, stageTable));
    EXPECT_EQ(assetTable.Size(), kMaxAssets);
}

TEST_F(ManifestLoadTest, AssetTableCapacity_ExceedsMaxAssets_Fails)
{
    static const unsigned int kOverflow = Dia::AssetRuntime::RuntimeManifestLoader::kMaxAssets + 1;
    static const unsigned int kBufSize = kOverflow * 80 + 256;
    char* json = new char[kBufSize];
    BuildCapacityJson(json, kBufSize, kOverflow);

    char filePath[512];
    ASSERT_TRUE(WriteTempFile("arun_cap_overflow.json", json, filePath, sizeof(filePath)));
    delete[] json;

    Dia::Core::FilePath fp = MakeFilePath("arun_cap_overflow.json");

    Dia::AssetRuntime::RuntimeManifestLoader loader;
    Dia::AssetRuntime::RuntimeManifestLoader::AssetTable assetTable;
    Dia::AssetRuntime::RuntimeManifestLoader::StageTable stageTable;

    EXPECT_FALSE(loader.Load(fp, assetTable, stageTable));
}
