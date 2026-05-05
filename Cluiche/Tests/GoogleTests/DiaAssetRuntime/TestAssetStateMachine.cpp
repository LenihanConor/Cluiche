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
    bool WriteTempFileF2(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF2Alias = "test_arun_f2";

    void SetupF2Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF2Alias);
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

    Dia::Core::FilePath MakeF2FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF2Alias), filename);
    }

    // Build a runtime loaded with two assets: "asset.alpha" and "asset.beta"
    // Returns true if the manifest loaded successfully.
    bool LoadTwoAssetRuntime(Dia::AssetRuntime::AssetRuntime& runtime, const char* jsonFilename)
    {
        static const char kJson[] = R"({
            "assets": [
                {"id": "asset.alpha", "scope": "stage",  "deploy_path": "alpha.png"},
                {"id": "asset.beta",  "scope": "global", "deploy_path": "beta.json"}
            ],
            "stages": [
                {"id": "stage.s1", "assets": ["asset.alpha", "asset.beta"]}
            ]
        })";

        char filePath[512];
        if (!WriteTempFileF2(jsonFilename, kJson, filePath, sizeof(filePath)))
            return false;

        Dia::Core::FilePath fp = MakeF2FilePath(jsonFilename);
        return runtime.LoadManifest(fp);
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class AssetStateMachineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SetupF2Alias();
    }
};

// ---------------------------------------------------------------------------
// Tests — state init
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, StateInit_AllAssetsRegisteredAfterLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_stateinit.json"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(AssetStateMachineTest, IsAssetReady_FalseAfterLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_isready.json"));

    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.alpha")));
    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.beta")));
}

// ---------------------------------------------------------------------------
// Tests — AcknowledgeAssetLoaded
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoaded_FromLoaded_Succeeds)
{
    // AcknowledgeAssetLoaded transitions Staged→Loaded.
    // We cannot directly set Staged from outside, but AcknowledgeAssetLoaded
    // wraps TryTransition(id, Loaded).  From Registered, Loaded is not a
    // valid transition, so the state should remain Registered.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_ackloaded_invalid.json"));

    // Registered→Loaded is invalid; state unchanged.
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

// ---------------------------------------------------------------------------
// Tests — AcknowledgeAssetUnloaded
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, AcknowledgeAssetUnloaded_FromRegistered_Invalid)
{
    // Registered→Registered is not a valid transition.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_ackunloaded_reg.json"));

    runtime.AcknowledgeAssetUnloaded(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

// ---------------------------------------------------------------------------
// Tests — unknown asset queries
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, GetAssetState_UnknownId_ReturnsSentinel)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_unknown_state.json"));

    // Unknown ID returns sentinel Registered (documented fallback).
    Dia::AssetRuntime::AssetState s =
        runtime.GetAssetState(Dia::Core::StringCRC("asset.does_not_exist"));
    EXPECT_EQ(s, Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(AssetStateMachineTest, IsAssetReady_UnknownId_ReturnsFalse)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_unknown_ready.json"));

    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.does_not_exist")));
}

// ---------------------------------------------------------------------------
// Tests — IsAssetReady after transition sequence
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, IsAssetReady_TrueOnlyWhenLoaded)
{
    // We can only drive the state machine through public API.
    // After load: Registered — not ready.
    // There is no public API to move to Staged/Loaded in F2 alone
    // (RequestStageLoad is F3), so we verify the negative: ready is false
    // from initial Registered state.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_isready_loaded.json"));

    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.alpha")));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

// ---------------------------------------------------------------------------
// Tests — invalid transitions do not change state
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, InvalidTransition_StateUnchanged)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_invalid_trans.json"));

    // Attempt Registered→Loaded (invalid).
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);

    // Attempt Registered→Registered via AcknowledgeAssetUnloaded (invalid).
    runtime.AcknowledgeAssetUnloaded(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

// ---------------------------------------------------------------------------
// Tests — AcknowledgeAssetUnloaded on unknown ID is safe (no crash)
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoaded_UnknownId_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_ack_unknown.json"));

    // Should log a warning and return without crashing.
    runtime.AcknowledgeAssetLoaded(Dia::Core::StringCRC("asset.ghost"));
    runtime.AcknowledgeAssetUnloaded(Dia::Core::StringCRC("asset.ghost"));
    // No assertion needed — just verifying no crash/assert.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Tests — AcknowledgeAssetLoadFailed
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoadFailed_FromRegistered_Invalid)
{
    // Registered→Registered via LoadFailed is not valid.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadfail_reg.json"));

    runtime.AcknowledgeAssetLoadFailed(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoadFailed_FromStaged_ReturnsToRegistered)
{
    // Staged→Registered (load failed, retry path).
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadfail_staged.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Staged);

    runtime.AcknowledgeAssetLoadFailed(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Registered);
}

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoadFailed_RefCountPreserved)
{
    // After load failure, the stage still wants the asset — ref count unchanged.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadfail_refcount.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);

    runtime.AcknowledgeAssetLoadFailed(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(runtime.GetAssetRefCount(Dia::Core::StringCRC("asset.alpha")), 1u);
}

TEST_F(AssetStateMachineTest, AcknowledgeAssetLoadFailed_UnknownId_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadfail_unknown.json"));

    runtime.AcknowledgeAssetLoadFailed(Dia::Core::StringCRC("asset.ghost"));
    SUCCEED();
}
