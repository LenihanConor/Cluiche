#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetState.h>
#include <DiaAssetRuntime/IAssetStateListener.h>
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
    bool WriteTempFileLN(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kLNAlias = "test_arun_ln";

    void SetupLNAlias()
    {
        Dia::Core::StringCRC aliasCRC(kLNAlias);
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

    Dia::Core::FilePath MakeLNFilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kLNAlias), filename);
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

    bool LoadTwoAssetRuntimeLN(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileLN(filename, kTwoAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeLNFilePath(filename));
    }

    // A handler that lets the test force a load failure for a given asset by
    // invoking IAssetLoadCallback::OnLoadFailed instead of OnLoadComplete.
    struct FailingHandler : public Dia::AssetRuntime::IAssetTypeHandler
    {
        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            callback->OnLoadFailed(assetId, "forced failure for test");
        }

        void Unload(const Dia::Core::StringCRC&) override {}
    };

    // ---------------------------------------------------------------------------
    // Recording listener
    // ---------------------------------------------------------------------------
    struct RecordingListener : public Dia::AssetRuntime::IAssetStateListener
    {
        struct ReadyEvent
        {
            Dia::Core::StringCRC assetId;
            char                 path[512];
        };

        static const unsigned int kMaxEvents = 32;

        ReadyEvent   readyEvents[kMaxEvents];
        unsigned int readyCount = 0;

        Dia::Core::StringCRC unloadingIds[kMaxEvents];
        unsigned int         unloadingCount = 0;

        Dia::Core::StringCRC failedIds[kMaxEvents];
        unsigned int         failedCount = 0;

        void OnAssetReady(const Dia::Core::StringCRC& assetId,
                          const Dia::Core::Containers::String512& resolvedPath) override
        {
            if (readyCount < kMaxEvents)
            {
                readyEvents[readyCount].assetId = assetId;
                strncpy(readyEvents[readyCount].path, resolvedPath.AsCStr(), 511);
                readyEvents[readyCount].path[511] = '\0';
                readyCount++;
            }
        }

        void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override
        {
            if (unloadingCount < kMaxEvents)
                unloadingIds[unloadingCount++] = assetId;
        }

        void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override
        {
            if (failedCount < kMaxEvents)
                failedIds[failedCount++] = assetId;
        }
    };

    // Listener that unregisters itself during OnAssetReady dispatch.
    struct SelfUnregisteringListener : public Dia::AssetRuntime::IAssetStateListener
    {
        Dia::AssetRuntime::AssetRuntime* runtime = nullptr;
        unsigned int readyCount = 0;
        unsigned int unloadingCount = 0;

        void OnAssetReady(const Dia::Core::StringCRC&,
                          const Dia::Core::Containers::String512&) override
        {
            readyCount++;
            if (runtime)
                runtime->UnregisterListener(this);
        }

        void OnAssetUnloading(const Dia::Core::StringCRC&) override
        {
            unloadingCount++;
        }
    };

    // Inert listener used purely to fill listener-list capacity in the
    // overflow test.
    struct InertListener : public Dia::AssetRuntime::IAssetStateListener
    {
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class ListenerNotificationTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupLNAlias(); }
};

// ---------------------------------------------------------------------------
// Tests — OnAssetReady
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, SingleListener_ReceivesOnAssetReady)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_ready_single.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.readyCount, 2u); // both assets staged
}

TEST_F(ListenerNotificationTest, OnAssetReady_ResolvedPathIsCorrect)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_ready_path.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    bool foundAlpha = false;
    for (unsigned int i = 0; i < listener.readyCount; ++i)
    {
        if (listener.readyEvents[i].assetId == Dia::Core::StringCRC("asset.alpha"))
        {
            EXPECT_NE(strstr(listener.readyEvents[i].path, "alpha.png"), nullptr);
            foundAlpha = true;
        }
    }
    EXPECT_TRUE(foundAlpha);
}

TEST_F(ListenerNotificationTest, MultipleListeners_NotifiedInRegistrationOrder)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_multi_listener.json"));

    // Shared sequence counter — each listener records the global call
    // sequence number the first time it is notified, proving registration
    // order (first registered, first notified) for every event.
    struct OrderedListener : public Dia::AssetRuntime::IAssetStateListener
    {
        unsigned int* sequenceCounter = nullptr;
        unsigned int  firstCallSequence = 0;
        unsigned int  callCount = 0;

        void OnAssetReady(const Dia::Core::StringCRC&, const Dia::Core::Containers::String512&) override
        {
            if (callCount == 0)
                firstCallSequence = (*sequenceCounter)++;
            callCount++;
        }
    };

    unsigned int sequenceCounter = 0;
    OrderedListener first, second;
    first.sequenceCounter = &sequenceCounter;
    second.sequenceCounter = &sequenceCounter;

    runtime.RegisterListener(&first);
    runtime.RegisterListener(&second);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(first.callCount, 2u);
    EXPECT_EQ(second.callCount, 2u);
    EXPECT_LT(first.firstCallSequence, second.firstCallSequence);
}

// ---------------------------------------------------------------------------
// Tests — OnAssetUnloading
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, SingleListener_ReceivesOnAssetUnloading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_unloading_single.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.unloadingCount, 2u);
}

// ---------------------------------------------------------------------------
// Tests — no re-fire on restage
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, NoRefire_OnRestageOfAlreadyActiveAsset)
{
    // Second RequestStageLoad on a stage whose assets already have a
    // non-zero ref count only bumps the ref count — no state transition,
    // so no new OnAssetReady events.
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_no_refire.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    unsigned int afterFirst = listener.readyCount;

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(listener.readyCount, afterFirst); // no new events
}

// ---------------------------------------------------------------------------
// Tests — unregister during dispatch
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, UnregisterDuringDispatch_SafeAndDeferred)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_unregister_dispatch.json"));

    SelfUnregisteringListener selfRemover;
    selfRemover.runtime = &runtime;

    RecordingListener after;

    runtime.RegisterListener(&selfRemover);
    runtime.RegisterListener(&after);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    // selfRemover received at least the first event before unregistering.
    EXPECT_GE(selfRemover.readyCount, 1u);
    // "after" is registered later in the list but must still receive every
    // event for this stage load — removal must not corrupt iteration.
    EXPECT_EQ(after.readyCount, 2u);

    // The deferred removal must have been applied once dispatch completed:
    // a subsequent unload should NOT reach selfRemover, but must still
    // reach the listener registered after it.
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1"));
    EXPECT_EQ(selfRemover.unloadingCount, 0u);
    EXPECT_EQ(after.unloadingCount, 2u);
}

// ---------------------------------------------------------------------------
// Tests — duplicate registration
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, DuplicateRegistration_IsNoOp)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_dup_register.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RegisterListener(&listener); // duplicate pointer — should warn, not double-add

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.readyCount, 2u); // exactly 2, not 4
}

// ---------------------------------------------------------------------------
// Tests — capacity overflow (17th listener)
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, SeventeenthListener_RejectedCleanly)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_capacity.json"));

    InertListener fillers[16];
    for (unsigned int i = 0; i < 16; ++i)
        runtime.RegisterListener(&fillers[i]);

    RecordingListener seventeenth;
    runtime.RegisterListener(&seventeenth); // capacity is 16 — must be rejected, no crash

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    // The 17th listener was never actually registered, so it receives nothing.
    EXPECT_EQ(seventeenth.readyCount, 0u);
}

// ---------------------------------------------------------------------------
// Tests — OnAssetLoadFailed
// ---------------------------------------------------------------------------

TEST_F(ListenerNotificationTest, OnAssetLoadFailed_FiresOnFailedLoad)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeLN(runtime, "ln_load_failed.json"));

    FailingHandler failingHandler;
    runtime.RegisterTypeHandler("asset", &failingHandler);

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.failedCount, 2u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
}
