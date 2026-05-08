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
    bool WriteTempFileF7(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF7Alias = "test_arun_f7";

    void SetupF7Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF7Alias);
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

    Dia::Core::FilePath MakeF7FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF7Alias), filename);
    }

    static const char* kThreeAssetJson = R"({
        "assets": [
            {"id": "asset.alpha", "scope": "stage",  "deploy_path": "alpha.png"},
            {"id": "asset.beta",  "scope": "stage",  "deploy_path": "beta.png"},
            {"id": "asset.gamma", "scope": "global", "deploy_path": "gamma.json"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["asset.alpha", "asset.beta"]},
            {"id": "stage.s2", "assets": ["asset.beta", "asset.gamma"]}
        ]
    })";

    bool LoadF7Runtime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF7(filename, kThreeAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF7FilePath(filename));
    }

    bool ContainsCRC7(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& arr,
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

} // anonymous namespace

typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> QueryResults7;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class GetAllAssetsTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF7Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — basic correctness
// ---------------------------------------------------------------------------

TEST_F(GetAllAssetsTest, GetAllAssets_CountEqualsManifestAssets)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_count.json"));

    QueryResults7 results;
    unsigned int count = runtime.GetAllAssets(results);

    EXPECT_EQ(count, 3u);
    EXPECT_EQ(results.Size(), 3u);
}

TEST_F(GetAllAssetsTest, GetAllAssets_ContainsAllIds)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_contains.json"));

    QueryResults7 results;
    runtime.GetAllAssets(results);

    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.alpha")));
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.beta")));
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.gamma")));
}

TEST_F(GetAllAssetsTest, GetAllAssets_IncludesNullStateAssets)
{
    // All assets begin Null after manifest load — GetAllAssets must include them.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_null.json"));

    QueryResults7 results;
    unsigned int count = runtime.GetAllAssets(results);

    EXPECT_EQ(count, 3u);
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        EXPECT_EQ(runtime.GetAssetState(results[i]),
                  Dia::AssetRuntime::AssetState::Null);
    }
}

TEST_F(GetAllAssetsTest, GetAllAssets_IncludesLoadedAssets)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_loaded.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults7 results;
    unsigned int count = runtime.GetAllAssets(results);

    EXPECT_EQ(count, 3u);
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.alpha")));
}

TEST_F(GetAllAssetsTest, GetAllAssets_MixedStates_AllPresent)
{
    // s1 loaded (alpha + beta = Loaded), gamma still Null
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_mixed.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.gamma")),
              Dia::AssetRuntime::AssetState::Null);

    QueryResults7 results;
    unsigned int count = runtime.GetAllAssets(results);

    EXPECT_EQ(count, 3u);
    EXPECT_EQ(results.Size(), 3u);
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.alpha")));
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.beta")));
    EXPECT_TRUE(ContainsCRC7(results, Dia::Core::StringCRC("asset.gamma")));
}

TEST_F(GetAllAssetsTest, GetAllAssets_DoesNotModifyState)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_nomodify.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    QueryResults7 results;
    runtime.GetAllAssets(results);

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.gamma")),
              Dia::AssetRuntime::AssetState::Null);
}

TEST_F(GetAllAssetsTest, GetAllAssets_ReturnValueMatchesResultSize)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF7Runtime(runtime, "f7_returnval.json"));

    QueryResults7 results;
    unsigned int count = runtime.GetAllAssets(results);

    EXPECT_EQ(count, results.Size());
}
