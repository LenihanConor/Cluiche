#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetState.h>

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    bool WriteTempFileF5(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
    {
        char tmpDir[256];
#if defined(_MSC_VER)
        GetTempPathA(sizeof(tmpDir), tmpDir);
#else
        strncpy(tmpDir, "/tmp/", sizeof(tmpDir) - 1);
        tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
        snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);

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

    static const char* kF5Alias = "test_arun_f5";

    void SetupF5Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF5Alias);
        if (!Dia::Core::PathStore::IsPathAliasRegistered(aliasCRC))
        {
            char tmpDir[256];
#if defined(_MSC_VER)
            GetTempPathA(sizeof(tmpDir), tmpDir);
            for (char* p = tmpDir; *p; ++p)
                if (*p == '\\') *p = '/';
            int len = (int)strlen(tmpDir);
            if (len > 0 && tmpDir[len - 1] == '/')
                tmpDir[len - 1] = '\0';
#else
            strncpy(tmpDir, "/tmp", sizeof(tmpDir) - 1);
            tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
            Dia::Core::Path::String p(tmpDir);
            Dia::Core::PathStore::RegisterToStore(aliasCRC, p);
        }
    }

    Dia::Core::FilePath MakeF5FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF5Alias), filename);
    }

    static const char* kThreeAssetJson = R"({
        "assets": [
            {"id": "asset.a", "scope": "stage",  "deploy_path": "a.png"},
            {"id": "asset.b", "scope": "stage",  "deploy_path": "b.png"},
            {"id": "asset.c", "scope": "global", "deploy_path": "c.json"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["asset.a", "asset.b"]},
            {"id": "stage.s2", "assets": ["asset.b", "asset.c"]}
        ]
    })";

    bool LoadF5Runtime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF5(filename, kThreeAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF5FilePath(filename));
    }

    bool ContainsCRC(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& arr,
                     const Dia::Core::StringCRC& id)
    {
        for (unsigned int i = 0; i < arr.Size(); ++i)
            if (arr[i] == id) return true;
        return false;
    }

} // anonymous namespace

typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> QueryResults;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class DebugQueryAPITest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF5Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — GetStagedAssets
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, GetStagedAssets_EmptyBeforeLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_staged_empty.json"));

    QueryResults results;
    unsigned int count = runtime.GetStagedAssets(results);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results.Size(), 0u);
}

TEST_F(DebugQueryAPITest, GetStagedAssets_CorrectAfterStageLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_staged_after_load.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults results;
    unsigned int count = runtime.GetStagedAssets(results);

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.a")));
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.b")));
    EXPECT_FALSE(ContainsCRC(results, Dia::Core::StringCRC("asset.c")));
}

// ---------------------------------------------------------------------------
// Tests — GetLoadedAssets
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, GetLoadedAssets_EmptyBeforeAcknowledge)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_loaded_before_ack.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults results;
    unsigned int count = runtime.GetLoadedAssets(results);
    EXPECT_EQ(count, 0u); // staged, not loaded
}

TEST_F(DebugQueryAPITest, GetLoadedAssets_CorrectAfterAcknowledge)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_loaded_after_ack.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.a"));

    QueryResults results;
    unsigned int count = runtime.GetLoadedAssets(results);

    EXPECT_EQ(count, 1u);
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.a")));
    EXPECT_FALSE(ContainsCRC(results, Dia::Core::StringCRC("asset.b")));
}

// ---------------------------------------------------------------------------
// Tests — GetStageDependencies
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, GetStageDependencies_ReturnsCorrectAssets)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_stagedeps.json"));

    QueryResults results;
    unsigned int count = runtime.GetStageDependencies(Dia::Core::StringCRC("stage.s1"), results);

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.a")));
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.b")));
}

TEST_F(DebugQueryAPITest, GetStageDependencies_AcrossStages)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_stagedeps_s2.json"));

    QueryResults results;
    unsigned int count = runtime.GetStageDependencies(Dia::Core::StringCRC("stage.s2"), results);

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.b")));
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.c")));
}

TEST_F(DebugQueryAPITest, GetStageDependencies_UnknownStage_ReturnsZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_stagedeps_unknown.json"));

    QueryResults results;
    unsigned int count = runtime.GetStageDependencies(Dia::Core::StringCRC("stage.ghost"), results);

    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Tests — overflow truncation
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, GetStageDependencies_MaxStageCapacity)
{
    // RuntimeStageEntry holds up to 64 asset IDs (DynamicArrayC<StringCRC, 64>).
    // Build a manifest that fills a stage to capacity and verify all 64 are returned.
    static const unsigned int kStageCapacity = 64;

    char json[16384];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"assets\":[");
    for (unsigned int i = 0; i < kStageCapacity; ++i)
    {
        if (i > 0) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"id\":\"asset.x%u\",\"scope\":\"stage\",\"deploy_path\":\"x%u.png\"}", i, i);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "],\"stages\":[{\"id\":\"stage.big\",\"assets\":[");
    for (unsigned int i = 0; i < kStageCapacity; ++i)
    {
        if (i > 0) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos, "\"asset.x%u\"", i);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}]}");

    char filePath[512];
    ASSERT_TRUE(WriteTempFileF5("f5_overflow.json", json, filePath, sizeof(filePath)));

    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(runtime.LoadManifest(MakeF5FilePath("f5_overflow.json")));

    QueryResults results;
    unsigned int total = runtime.GetStageDependencies(Dia::Core::StringCRC("stage.big"), results);

    EXPECT_EQ(total, kStageCapacity);     // all 64 returned
    EXPECT_EQ(results.Size(), kStageCapacity); // fits within 128-entry result array
}

// ---------------------------------------------------------------------------
// Tests — queries don't modify state
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, Queries_DoNotModifyState)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_no_modify.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults staged;
    runtime.GetStagedAssets(staged);
    QueryResults loaded;
    runtime.GetLoadedAssets(loaded);
    QueryResults deps;
    runtime.GetStageDependencies(Dia::Core::StringCRC("stage.s1"), deps);

    // States unchanged after queries
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.a")),
              Dia::AssetRuntime::AssetState::Staged);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.b")),
              Dia::AssetRuntime::AssetState::Staged);
}
