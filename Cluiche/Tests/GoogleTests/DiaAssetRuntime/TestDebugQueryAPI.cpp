#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetState.h>
#include <DiaAssetRuntime/IAssetTypeHandler.h>

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

    struct ImmediateHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            callback->OnLoadComplete(assetId);
        }
        void Unload(const Dia::Core::StringCRC&) override {}
    };

    // Handler that does NOT call callback — leaves assets in Loading state
    struct DeferredHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        void Load(const Dia::Core::StringCRC&,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback*) override {}
        void Unload(const Dia::Core::StringCRC&) override {}
    };

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
// Tests — GetStagedAssets (assets that are still in Staged state — only
// possible if no handler is registered to move them forward, which means
// they go to Failed. GetStagedAssets in the new model is less useful as
// Staged is a transient state, but let's verify correctness.)
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

// ---------------------------------------------------------------------------
// Tests — GetLoadedAssets
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, GetLoadedAssets_EmptyBeforeLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_loaded_before.json"));

    QueryResults results;
    unsigned int count = runtime.GetLoadedAssets(results);
    EXPECT_EQ(count, 0u);
}

TEST_F(DebugQueryAPITest, GetLoadedAssets_CorrectAfterStageLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_loaded_after.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults results;
    unsigned int count = runtime.GetLoadedAssets(results);

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.a")));
    EXPECT_TRUE(ContainsCRC(results, Dia::Core::StringCRC("asset.b")));
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
    static const unsigned int kStageCapacity = 128;

    char json[65536];
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

    EXPECT_EQ(total, kStageCapacity);
    EXPECT_EQ(results.Size(), 128u);
}

// ---------------------------------------------------------------------------
// Tests — queries don't modify state
// ---------------------------------------------------------------------------

TEST_F(DebugQueryAPITest, Queries_DoNotModifyState)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF5Runtime(runtime, "f5_no_modify.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults loaded;
    runtime.GetLoadedAssets(loaded);
    QueryResults deps;
    runtime.GetStageDependencies(Dia::Core::StringCRC("stage.s1"), deps);

    // States unchanged after queries
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.a")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.b")),
              Dia::AssetRuntime::AssetState::Loaded);
}
