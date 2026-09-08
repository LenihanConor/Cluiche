#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/AssetRuntimeBusAdapter.h>
#include <DiaAssetRuntime/AssetState.h>
#include <DiaAssetRuntime/IAssetStateListener.h>
#include <DiaAssetRuntime/IAssetTypeHandler.h>
#include <DiaAssetRuntime/Messages/assetruntime_messages.h>

#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/CRC/StringCRC.h>

#include <DiaMessageBus/Bus.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers — mirrors Cluiche/Tests/GoogleTests/DiaAssetRuntime/TestListenerNotification.cpp,
// with its own alias/filenames so the two test binaries' temp files never collide.
// ---------------------------------------------------------------------------
namespace
{
    bool WriteTempFileBA(const char* filename, const char* content, char* pathOut, unsigned int pathOutSize)
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

    static const char* kBAAlias = "test_arun_busadapter";

    void SetupBAAlias()
    {
        Dia::Core::StringCRC aliasCRC(kBAAlias);
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

    Dia::Core::FilePath MakeBAFilePath(const char* filename)
    {
        return Dia::Core::FilePath(Dia::Core::StringCRC(kBAAlias), filename);
    }

    static const char* kTwoAssetJsonBA = R"({
        "assets": [
            {"id": "asset.alpha", "scope": "stage",  "deploy_path": "alpha.png"},
            {"id": "asset.beta",  "scope": "global", "deploy_path": "beta.json"}
        ],
        "stages": [
            {"id": "stage.s1", "assets": ["asset.alpha", "asset.beta"]}
        ]
    })";

    bool LoadTwoAssetRuntimeBA(Dia::AssetRuntime::AssetRuntime& runtime, const char* filename)
    {
        char filePath[512];
        if (!WriteTempFileBA(filename, kTwoAssetJsonBA, filePath, sizeof(filePath)))
            return false;
        return runtime.LoadManifest(MakeBAFilePath(filename));
    }

    // A handler that lets the test force a load failure for a given asset by
    // invoking IAssetLoadCallback::OnLoadFailed instead of OnLoadComplete.
    // Exercises the pre-existing, private loader mechanism — proving it is
    // unaffected by AssetRuntimeBusAdapter's presence.
    struct FailingHandlerBA : public Dia::AssetRuntime::IAssetTypeHandler
    {
        void Load(const Dia::Core::StringCRC& assetId,
                  const Dia::Core::Containers::String512&,
                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
        {
            callback->OnLoadFailed(assetId, "forced failure for test");
        }

        void Unload(const Dia::Core::StringCRC&) override {}
    };

    // A direct (non-bus) listener used to prove the adapter registers
    // alongside — not instead of — direct IAssetStateListener use.
    struct DirectRecordingListener : public Dia::AssetRuntime::IAssetStateListener
    {
        unsigned int readyCount     = 0;
        unsigned int unloadingCount = 0;
        unsigned int failedCount    = 0;

        void OnAssetReady(const Dia::Core::StringCRC&,
                          const Dia::Core::Containers::String512&) override
        {
            readyCount++;
        }

        void OnAssetUnloading(const Dia::Core::StringCRC&) override
        {
            unloadingCount++;
        }

        void OnAssetLoadFailed(const Dia::Core::StringCRC&) override
        {
            failedCount++;
        }
    };

    // Sets up a Bus with all three assetruntime message types registered
    // (via the generated RegisterMessages — no consumers, so an empty
    // Handlers{} is valid) and Initialize()d, ready for Subscribe/Broadcast.
    void SetupAssetRuntimeBus(Dia::MessageBus::Bus& bus)
    {
        bus.Initialize();
        Dia::AssetRuntime::RegisterMessages(bus, Dia::AssetRuntime::Handlers{});
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class AssetRuntimeBusAdapterTest : public ::testing::Test
{
protected:
    void SetUp() override { SetupBAAlias(); }
};

// ---------------------------------------------------------------------------
// Direct forwarding — adapter called directly (no AssetRuntime involved),
// proving each callback forwards to Bus::Broadcast with the correct payload.
// ---------------------------------------------------------------------------

TEST_F(AssetRuntimeBusAdapterTest, OnAssetReady_ForwardsToBus_WithCorrectPayload)
{
    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    Dia::AssetRuntime::AssetReadyEvent received;
    unsigned int receiveCount = 0;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetReadyEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetReadyEvent& evt) {
            received = evt;
            receiveCount++;
        });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    adapter.OnAssetReady(Dia::Core::StringCRC("asset.alpha"),
                          Dia::Core::Containers::String512("resolved/alpha.png"));

    bus.Update();

    ASSERT_EQ(receiveCount, 1u);
    EXPECT_EQ(received.assetId, Dia::Core::StringCRC("asset.alpha"));
    EXPECT_STREQ(received.resolvedPath.AsCStr(), "resolved/alpha.png");
}

TEST_F(AssetRuntimeBusAdapterTest, OnAssetUnloading_ForwardsToBus_WithCorrectPayload)
{
    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    Dia::AssetRuntime::AssetUnloadingEvent received;
    unsigned int receiveCount = 0;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetUnloadingEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetUnloadingEvent& evt) {
            received = evt;
            receiveCount++;
        });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    adapter.OnAssetUnloading(Dia::Core::StringCRC("asset.beta"));

    bus.Update();

    ASSERT_EQ(receiveCount, 1u);
    EXPECT_EQ(received.assetId, Dia::Core::StringCRC("asset.beta"));
}

TEST_F(AssetRuntimeBusAdapterTest, OnAssetLoadFailed_ForwardsToBus_WithCorrectPayload)
{
    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    Dia::AssetRuntime::AssetLoadFailedEvent received;
    unsigned int receiveCount = 0;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetLoadFailedEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetLoadFailedEvent& evt) {
            received = evt;
            receiveCount++;
        });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    adapter.OnAssetLoadFailed(Dia::Core::StringCRC("asset.alpha"));

    bus.Update();

    ASSERT_EQ(receiveCount, 1u);
    EXPECT_EQ(received.assetId, Dia::Core::StringCRC("asset.alpha"));
}

// ---------------------------------------------------------------------------
// Integration — adapter registered alongside a direct listener; both fire
// from one real AssetRuntime transition.
// ---------------------------------------------------------------------------

TEST_F(AssetRuntimeBusAdapterTest, RegisteredAlongsideDirectListener_BothFireFromOneTransition)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeBA(runtime, "ba_both_fire.json"));

    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    unsigned int busReceiveCount = 0;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetReadyEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetReadyEvent&) { busReceiveCount++; });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    DirectRecordingListener directListener;

    runtime.RegisterListener(&adapter);
    runtime.RegisterListener(&directListener);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    bus.Update();

    // Both assets in stage.s1 transition to Staged — both listeners see both.
    EXPECT_EQ(directListener.readyCount, 2u);
    EXPECT_EQ(busReceiveCount, 2u);
}

// ---------------------------------------------------------------------------
// End-to-end — a real asset transitions to Staged; a Bus::Subscribe<AssetReadyEvent>
// handler receives the event with the correct resolved path.
// ---------------------------------------------------------------------------

TEST_F(AssetRuntimeBusAdapterTest, EndToEnd_RealStagedTransition_BusSubscriberReceivesResolvedPath)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeBA(runtime, "ba_e2e_ready.json"));

    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    bool foundAlpha = false;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetReadyEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetReadyEvent& evt) {
            if (evt.assetId == Dia::Core::StringCRC("asset.alpha"))
            {
                EXPECT_NE(strstr(evt.resolvedPath.AsCStr(), "alpha.png"), nullptr);
                foundAlpha = true;
            }
        });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    runtime.RegisterListener(&adapter);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    bus.Update();

    // Note: no assertion on the post-call GetAssetState here — RequestStageLoad
    // synchronously drives the asset past Staged into DispatchLoad/AutoValidate
    // within the same call (no handler is registered for type "asset" in this
    // test), and since the deploy files are not real files on disk, the asset
    // ends up Failed by the time RequestStageLoad returns. That is orthogonal
    // to this test's concern: the Staged transition still fired synchronously
    // first, and this asserts the bus received that AssetReadyEvent with the
    // correct resolved path — matching TestListenerNotification.cpp's own
    // tests, none of which assert final state after an unhandled load either.
    EXPECT_TRUE(foundAlpha);
}

// ---------------------------------------------------------------------------
// Loader path unaffected — the pre-existing private IAssetLoadCallback
// mechanism (loader -> AssetRuntime::OnLoadComplete/OnLoadFailed) still
// drives OnAssetLoadFailed correctly with the bus adapter registered,
// and the resulting event reaches the bus end-to-end.
// ---------------------------------------------------------------------------

TEST_F(AssetRuntimeBusAdapterTest, LoaderPathUnaffected_ForcedFailureStillReachesBusViaAdapter)
{
    Dia::AssetRuntime::AssetRuntime runtime;
    ASSERT_TRUE(LoadTwoAssetRuntimeBA(runtime, "ba_loader_unaffected.json"));

    FailingHandlerBA failingHandler;
    runtime.RegisterTypeHandler("asset", &failingHandler);

    Dia::MessageBus::Bus bus;
    SetupAssetRuntimeBus(bus);

    unsigned int failedReceiveCount = 0;
    Dia::MessageBus::BusSubscriptionHandle handle = bus.Subscribe<Dia::AssetRuntime::AssetLoadFailedEvent>(
        Dia::Core::StringCRC("TestSubscriber"),
        [&](const Dia::AssetRuntime::AssetLoadFailedEvent&) { failedReceiveCount++; });
    ASSERT_TRUE(handle.IsValid());

    Dia::AssetRuntime::AssetRuntimeBusAdapter adapter(bus);
    runtime.RegisterListener(&adapter);

    runtime.RequestStageLoad(Dia::Core::StringCRC("stage.s1"));
    bus.Update();

    EXPECT_EQ(failedReceiveCount, 2u);
    EXPECT_EQ(runtime.GetAssetState(Dia::Core::StringCRC("asset.alpha")),
              Dia::AssetRuntime::AssetState::Failed);
}
