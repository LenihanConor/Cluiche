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
    bool WriteTempFileF4(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kF4Alias = "test_arun_f4";

    void SetupF4Alias()
    {
        Dia::Core::StringCRC aliasCRC(kF4Alias);
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

    Dia::Core::FilePath MakeF4FilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kF4Alias), filename);
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

    bool LoadTwoAssetRuntimeF4(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileF4(filename, kTwoAssetJson, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeF4FilePath(filename));
    }

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
    };

    // Listener that unregisters itself during OnAssetReady dispatch.
    struct SelfUnregisteringListener : public Dia::AssetRuntime::IAssetStateListener
    {
        Dia::AssetRuntime::AssetRuntime* runtime = nullptr;
        unsigned int readyCount   = 0;
        unsigned int unloadCount  = 0;

        void OnAssetReady(const Dia::Core::StringCRC&,
                          const Dia::Core::Containers::String512&) override
        {
            readyCount++;
            if (runtime)
                runtime->UnregisterListener(this);
        }

        void OnAssetUnloading(const Dia::Core::StringCRC&) override
        {
            unloadCount++;
        }
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class EventNotificationTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupF4Alias(); }
};

// ---------------------------------------------------------------------------
// Tests — OnAssetReady
// ---------------------------------------------------------------------------

TEST_F(EventNotificationTest, SingleListener_ReceivesOnAssetReady)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_ready_single.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.readyCount, 2u); // both assets staged
}

TEST_F(EventNotificationTest, OnAssetReady_PathContainsFilename)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_ready_path.json"));

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

TEST_F(EventNotificationTest, MultipleListeners_AllNotifiedInOrder)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_multi_listener.json"));

    RecordingListener l1, l2;
    runtime.RegisterListener(&l1);
    runtime.RegisterListener(&l2);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(l1.readyCount, 2u);
    EXPECT_EQ(l2.readyCount, 2u);
}

// ---------------------------------------------------------------------------
// Tests — OnAssetUnloading
// ---------------------------------------------------------------------------

TEST_F(EventNotificationTest, SingleListener_ReceivesOnAssetUnloading)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_unloading_single.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    runtime.RequestStageUnload(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.unloadingCount, 2u);
}

TEST_F(EventNotificationTest, NoEvents_ForAlreadyActiveAssets_OnReload)
{
    // Double load: assets already in Staged state; second load increments ref
    // count but does NOT fire OnAssetReady again (no Registered->Staged transition).
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_no_refire.json"));

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

TEST_F(EventNotificationTest, UnregisterDuringDispatch_SafeNoCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_unregister_dispatch.json"));

    SelfUnregisteringListener selfRemover;
    selfRemover.runtime = &runtime;

    RecordingListener after;

    runtime.RegisterListener(&selfRemover);
    runtime.RegisterListener(&after);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    // selfRemover received at least the first event then unregistered itself.
    EXPECT_GE(selfRemover.readyCount, 1u);
    // after should still receive all events for this stage load.
    EXPECT_EQ(after.readyCount, 2u);
}

// ---------------------------------------------------------------------------
// Tests — duplicate registration
// ---------------------------------------------------------------------------

TEST_F(EventNotificationTest, DuplicateRegistration_NoDoubleNotify)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_dup_register.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.RegisterListener(&listener); // duplicate — should warn, not double-add

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.readyCount, 2u); // exactly 2, not 4
}

// ---------------------------------------------------------------------------
// Tests — no listeners registered
// ---------------------------------------------------------------------------

TEST_F(EventNotificationTest, NoListeners_StageLoadDoesNotCrash)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_no_listeners.json"));

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Tests — unregister before events
// ---------------------------------------------------------------------------

TEST_F(EventNotificationTest, UnregisterBeforeLoad_ReceivesNoEvents)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeF4(runtime, "f4_unreg_before.json"));

    RecordingListener listener;
    runtime.RegisterListener(&listener);
    runtime.UnregisterListener(&listener);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));

    EXPECT_EQ(listener.readyCount, 0u);
}
