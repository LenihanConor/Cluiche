#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetState.h>
#include <DiaAssetRuntime/IAssetStateListener.h>

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

    struct CountingListener : public Dia::AssetRuntime::IAssetStateListener
    {
        unsigned int readyCount     = 0;
        unsigned int unloadingCount = 0;

        void OnAssetReady(const Dia::Core::StringCRC&,
                          const Dia::Core::Containers::String512&) override { readyCount++; }
        void OnAssetUnloading(const Dia::Core::StringCRC&) override          { unloadingCount++; }
        void OnAssetLoadFailed(const Dia::Core::StringCRC&) override          {}
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

TEST_F(ResetTest, Reset_AllAssetsReturnToRegistered)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_state.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Staged);

    runtime.Reset();

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(ResetTest, Reset_RefCountsDropToZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_refcount.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);

    runtime.Reset();

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.beta")),  0u);
}

TEST_F(ResetTest, Reset_FromAllRegistered_NoUnloadingEvents)
{
    // All assets already Registered — Reset should not fire OnAssetUnloading.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_noevents.json"));

    CountingListener listener;
    runtime.RegisterListener(&listener);

    runtime.Reset();

    EXPECT_EQ(listener.unloadingCount, 0u);
}

TEST_F(ResetTest, Reset_StagedAssets_FireOnAssetUnloading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_staged_unloading.json"));

    CountingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    unsigned int readyBefore = listener.readyCount; // 2
    runtime.Reset();

    EXPECT_EQ(listener.unloadingCount, readyBefore); // one unloading per staged asset
}

TEST_F(ResetTest, Reset_LoadedAssets_FireOnAssetUnloading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_loaded_unloading.json"));

    CountingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.alpha"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);

    runtime.Reset();

    // asset.alpha (Loaded) and asset.beta (Staged) both fire unloading
    EXPECT_EQ(listener.unloadingCount, 2u);
}

TEST_F(ResetTest, Reset_AllRegistered_AfterLoadedAndStaged)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_allreg.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.alpha"));

    runtime.Reset();

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Registered);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.beta")),  0u);
}

TEST_F(ResetTest, Reset_FollowedByStageLoad_WorksCorrectly)
{
    // After Reset, the state machine should accept new RequestStageLoad calls.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_reload.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.Reset();

    // Second load cycle should work normally
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Staged);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);
}

TEST_F(ResetTest, Reset_OnAlreadyReset_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadF8Runtime(runtime, "f8_reset_double.json"));

    runtime.Reset();
    runtime.Reset(); // double Reset on already-Registered state — must not crash
    SUCCEED();
}
