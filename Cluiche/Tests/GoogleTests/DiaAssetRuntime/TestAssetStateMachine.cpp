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

    // Mock handler that immediately calls OnLoadComplete
    struct ImmediateHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        unsigned int loadCount = 0;
        unsigned int unloadCount = 0;

        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            loadCount++;
            callback->OnLoadComplete(assetId);
        }

        void Unload(const Dia::Core::StringCRC&) override
        {
            unloadCount++;
        }
    };

    // Mock handler that immediately calls OnLoadFailed
    struct FailingHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        unsigned int loadCount = 0;

        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            loadCount++;
            callback->OnLoadFailed(assetId, "test failure");
        }

        void Unload(const Dia::Core::StringCRC&) override {}
    };

    // Mock handler that does NOT call the callback (simulates async/deferred load)
    struct DeferredHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        Dia::AssetRuntime::IAssetLoadCallback* lastCallback = nullptr;
        Dia::Core::StringCRC lastAssetId;
        unsigned int loadCount = 0;

        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            loadCount++;
            lastCallback = callback;
            lastAssetId = assetId;
        }

        void Unload(const Dia::Core::StringCRC&) override {}
    };

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

TEST_F(AssetStateMachineTest, StateInit_AllAssetsNullAfterLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_stateinit.json"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Null);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Null);
}

TEST_F(AssetStateMachineTest, IsAssetReady_FalseAfterLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_isready.json"));

    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.alpha")));
    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.beta")));
}

// ---------------------------------------------------------------------------
// Tests — handler dispatch on stage load
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, HandlerCalled_OnStageLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_handler_call.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(handler.loadCount, 2u);
}

TEST_F(AssetStateMachineTest, Handler_ImmediateSuccess_AssetsLoaded)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_immediate_loaded.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_TRUE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.alpha")));
}

TEST_F(AssetStateMachineTest, Handler_Failure_AssetsFailed)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_handler_fail.json"));

    FailingHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Failed);
    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.alpha")));
}

TEST_F(AssetStateMachineTest, Handler_Deferred_AssetsInLoading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_handler_deferred.json"));

    DeferredHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(handler.loadCount, 2u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loading);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Loading);
    EXPECT_FALSE(runtime.IsLoadComplete(Dia::Core::StringCRC("stage.s1")));
}

TEST_F(AssetStateMachineTest, NoHandler_AssetsTransitionToFailed)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_no_handler.json"));

    // No handler registered for "asset" prefix
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.beta")),
              Dia::AssetRuntime::AssetState::Failed);
}

// ---------------------------------------------------------------------------
// Tests — IsLoadComplete
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, IsLoadComplete_TrueWhenAllLoaded)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadcomplete.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_TRUE(runtime.IsLoadComplete(Dia::Core::StringCRC("stage.s1")));
}

TEST_F(AssetStateMachineTest, IsLoadComplete_FalseWhenFailed)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_loadcomplete_fail.json"));

    FailingHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_FALSE(runtime.IsLoadComplete(Dia::Core::StringCRC("stage.s1")));
}

// ---------------------------------------------------------------------------
// Tests — GetLoadProgress
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, GetLoadProgress_AllLoaded)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_progress_loaded.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    auto progress = runtime.GetLoadProgress(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(progress.total, 2u);
    EXPECT_EQ(progress.loaded, 2u);
    EXPECT_EQ(progress.failed, 0u);
}

TEST_F(AssetStateMachineTest, GetLoadProgress_AllFailed)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_progress_failed.json"));

    FailingHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    auto progress = runtime.GetLoadProgress(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(progress.total, 2u);
    EXPECT_EQ(progress.loaded, 0u);
    EXPECT_EQ(progress.failed, 2u);
}

// ---------------------------------------------------------------------------
// Tests — RetryAssetLoad
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, RetryAssetLoad_FromFailed_ReDispatchesHandler)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_retry.json"));

    // First attempt fails
    FailingHandler failHandler;
    runtime.RegisterTypeHandler("asset", &failHandler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
    EXPECT_EQ(failHandler.loadCount, 2u);

    // Replace with a succeeding handler and retry
    runtime.UnregisterTypeHandler("asset");
    ImmediateHandler successHandler;
    runtime.RegisterTypeHandler("asset", &successHandler);

    runtime.RetryAssetLoad(Dia::Core::StringCRC("asset.alpha"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);
    EXPECT_EQ(successHandler.loadCount, 1u);
}

TEST_F(AssetStateMachineTest, RetryAssetLoad_NotFailed_NoOp)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_retry_noop.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Loaded);

    unsigned int before = handler.loadCount;
    runtime.RetryAssetLoad(Dia::Core::StringCRC("asset.alpha"));
    EXPECT_EQ(handler.loadCount, before); // no re-dispatch
}

// ---------------------------------------------------------------------------
// Tests — unload
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, Unload_CallsHandlerUnload)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_unload_handler.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(handler.unloadCount, 2u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Unloaded);
}

// ---------------------------------------------------------------------------
// Tests — unknown asset queries
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, GetAssetState_UnknownId_ReturnsNull)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_unknown_state.json"));

    Dia::AssetRuntime::AssetState s =
        runtime.GetAssetState(Dia::Core::StringCRC("asset.does_not_exist"));
    EXPECT_EQ(s, Dia::AssetRuntime::AssetState::Null);
}

TEST_F(AssetStateMachineTest, IsAssetReady_UnknownId_ReturnsFalse)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_unknown_ready.json"));

    EXPECT_FALSE(runtime.IsAssetReady(Dia::Core::StringCRC("asset.does_not_exist")));
}

// ---------------------------------------------------------------------------
// Tests — handler registration
// ---------------------------------------------------------------------------

TEST_F(AssetStateMachineTest, RegisterTypeHandler_DuplicatePrefix_NoDoubleRegister)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_handler_dup.json"));

    ImmediateHandler h1, h2;
    runtime.RegisterTypeHandler("asset", &h1);
    runtime.RegisterTypeHandler("asset", &h2); // should warn, not register

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(h1.loadCount, 2u);
    EXPECT_EQ(h2.loadCount, 0u); // h2 was not registered
}

TEST_F(AssetStateMachineTest, UnregisterTypeHandler_RemovedSuccessfully)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntime(runtime, "f2_handler_unreg.json"));

    ImmediateHandler handler;
    runtime.RegisterTypeHandler("asset", &handler);
    runtime.UnregisterTypeHandler("asset");

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(handler.loadCount, 0u);
    // Assets should be Failed since no handler found
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
}
