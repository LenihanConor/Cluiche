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

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    bool WriteTempFileF8(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF8Alias = "test_arun_f8";

    void SetupF8Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF8Alias);
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

    Dia::Core::FilePath MakeF8FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF8Alias), filename);
    }

    static const char* kTwoAssetJson = R"({
        "assets": [
            {"id": "asset.alpha", "scope": "stage",  "deploy_path": "alpha.png"},
            {"id": "asset.beta",  "scope": "global", "deploy_path": "beta.json"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["asset.alpha", "asset.beta"]}
        ]
    })";

    bool LoadF8Runtime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF8(filename, kTwoAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF8FilePath(filename));
    }

    struct ImmediateHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        unsigned int unloadCount = 0;

        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            callback->OnLoadComplete(assetId);
        }

        void Unload(const Dia::Core::StringCRC&) override
        {
            unloadCount++;
        }
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class ResetTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF8Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — state after Reset
// ---------------------------------------------------------------------------

TEST_F(ResetTest, Reset_AllAssetsReturnToNull)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_state.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);

    runtime.Reset();

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Null);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Null);
}

TEST_F(ResetTest, Reset_RefCountsDropToZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_refcount.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);

    runtime.Reset();

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.beta")),  0u);
}

TEST_F(ResetTest, Reset_CallsHandlerUnload_ForLoadedAssets)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_unload.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    runtime.Reset();

    EXPECT_EQ(handler.unloadCount, 2u);
}

TEST_F(ResetTest, Reset_FromAllNull_NoUnloadCalls)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_noevents.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);

    runtime.Reset();

    EXPECT_EQ(handler.unloadCount, 0u);
}

TEST_F(ResetTest, Reset_FollowedByStageLoad_WorksCorrectly)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_reload.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.Reset();

    // Second load cycle should work normally (Null->Staged->Loading->Loaded)
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);
}

TEST_F(ResetTest, Reset_OnAlreadyReset_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_double.json"));

    runtime.Reset();
    runtime.Reset();
    SUCCEED();
}
