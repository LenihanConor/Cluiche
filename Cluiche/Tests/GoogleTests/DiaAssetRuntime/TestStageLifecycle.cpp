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

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    bool WriteTempFileF3(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF3Alias = "test_arun_f3";

    void SetupF3Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF3Alias);
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

    Dia::Core::FilePath MakeF3FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF3Alias), filename);
    }

    // Two stages sharing one global asset.
    // stage.a  → [asset.stage_only_a, asset.shared_global]
    // stage.b  → [asset.stage_only_b, asset.shared_global]
    static const char* kSharedJson = R"({
        "assets": [
            {"id": "asset.stage_only_a", "scope": "stage",  "deploy_path": "sa.png"},
            {"id": "asset.stage_only_b", "scope": "stage",  "deploy_path": "sb.png"},
            {"id": "asset.shared_global","scope": "global", "deploy_path": "global.png"}
        ],
        "stages": [
            {"id": "stage.a", "assets": ["asset.stage_only_a", "asset.shared_global"]},
            {"id": "stage.b", "assets": ["asset.stage_only_b", "asset.shared_global"]}
        ]
    })";

    bool LoadSharedRuntime(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF3(filename, kSharedJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF3FilePath(filename));
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class StageLifecycleTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF3Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — single stage load / unload
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, SingleStageLoad_TransitionsAssetsToStaged)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_single_load.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.stage_only_a")),
              Dia::AssetRuntime::AssetState::Staged);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.shared_global")),
              Dia::AssetRuntime::AssetState::Staged);
    // stage.b's asset unaffected
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.stage_only_b")),
              Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(StageLifecycleTest, SingleStageLoad_RefCountsCorrect)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_refcount_load.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_a")), 1u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 1u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_b")), 0u);
}

TEST_F(StageLifecycleTest, SingleStageUnload_TransitionsToUnloading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_single_unload.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.stage_only_a")),
              Dia::AssetRuntime::AssetState::Unloading);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.shared_global")),
              Dia::AssetRuntime::AssetState::Unloading);
}

TEST_F(StageLifecycleTest, SingleStageUnload_RefCountsDropToZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_refcount_unload.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_a")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 0u);
}

// ---------------------------------------------------------------------------
// Tests — shared global asset across two stages
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, SharedGlobalAsset_RefCountTwoWhenBothStagesLoaded)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_shared_both.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.b"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 2u);
}

TEST_F(StageLifecycleTest, SharedGlobalAsset_StaysLoadedWhenOneStageUnloads)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_shared_stay.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.b"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));

    // Still ref'd by stage.b
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 1u);
    EXPECT_NE(runtime.GetAssetState(Dia::Core::StringCRC("asset.shared_global")),
              Dia::AssetRuntime::AssetState::Unloading);

    // stage.a's exclusive asset should be Unloading
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.stage_only_a")),
              Dia::AssetRuntime::AssetState::Unloading);
}

TEST_F(StageLifecycleTest, SharedGlobalAsset_TransitionsToUnloadingWhenBothStagesUnload)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_shared_unload_both.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.b"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.b"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 0u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.shared_global")),
              Dia::AssetRuntime::AssetState::Unloading);
}

// ---------------------------------------------------------------------------
// Tests — double load idempotency
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, DoubleLoad_IncrementsRefCountTwice)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_double_load.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_a")), 2u);
    // State should be Staged (already staged from first load; no re-transition)
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.stage_only_a")),
              Dia::AssetRuntime::AssetState::Staged);
}

// ---------------------------------------------------------------------------
// Tests — double unload clamping
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, DoubleUnload_RefCountClampedAtZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_double_unload.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.a"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));
    // Second unload should warn but not underflow
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.a"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_a")), 0u);
}

// ---------------------------------------------------------------------------
// Tests — unknown stage
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, UnknownStageLoad_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_unknown_load.json"));

    // Should log warning and return without crashing.
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.does_not_exist"));
    SUCCEED();
}

TEST_F(StageLifecycleTest, UnknownStageUnload_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_unknown_unload.json"));

    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.does_not_exist"));
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Tests — ref counts initialised to zero after manifest load
// ---------------------------------------------------------------------------

TEST_F(StageLifecycleTest, RefCountInitialisedToZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_refinit.json"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_a")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.stage_only_b")), 0u);
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.shared_global")), 0u);
}

TEST_F(StageLifecycleTest, GetAssetRefCount_UnknownId_ReturnsZero)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadSharedRuntime(runtime, "f3_refunknown.json"));

    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.ghost")), 0u);
}
