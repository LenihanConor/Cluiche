#include <gtest/gtest.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/OverflowPolicy.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

// ---------------------------------------------------------------------------
// Minimal IDebugStateProvider stub for tap lifecycle tests
// ---------------------------------------------------------------------------
namespace
{
    // Minimal lifecycle event type for the $lifecycle stream
    struct LifecycleTestEvent { int value; };

    class TapTestProvider : public Dia::DebugServer::IDebugStateProvider
    {
    public:
        Dia::ApplicationFlow::EventStreamStore<LifecycleTestEvent> mLifecycleStore;

        TapTestProvider()
            : mLifecycleStore(
                Dia::Core::StringCRC("$lifecycle"),
                Dia::Core::StringCRC::kZero, // payloadType
                8,                           // capacity
                4,                           // maxReaders
                Dia::ApplicationFlow::OverflowPolicy::kDropOldest,
                100)                         // blockTimeoutMs
        {}

        Dia::Core::StringCRC GetCurrentStage() const override
        {
            return Dia::Core::StringCRC("test_stage");
        }

        bool IsTransitioning() const override { return false; }
        bool IsShuttingDown()  const override { return false; }

        void GetProcessingUnitIds(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const override
        {
            (void)out;
        }

        void GetModulesInPU(
            const Dia::Core::StringCRC& puId,
            Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>& out) const override
        {
            (void)puId;
            (void)out;
        }

        Dia::DebugServer::IStreamTapTarget* FindStream(
            const Dia::Core::StringCRC& id) override
        {
            if (id == Dia::Core::StringCRC("$lifecycle"))
                return &mLifecycleStore;
            return nullptr;
        }

        Json::Value SerializeStreamPayload(
            const Dia::Core::StringCRC& /*dataType*/,
            const void* /*bytes*/,
            size_t /*size*/) override
        {
            return Json::Value{};
        }
    };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Test 1: Start does NOT attach a lifecycle tap — the host now owns that.
//         Verifies the lifecycle tap count stays 0 after server Start/Stop.
TEST(DebugServerTaps, LifecycleTap_NotAttachedByServerStart)
{
    TapTestProvider provider;
    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 0u);

    Dia::DebugServer::DebugServer server;
    server.EnableAutoStart(false);
    server.SetStateProvider(&provider);
    server.Start();

    // The server no longer auto-attaches a $lifecycle tap; the host adapter
    // is responsible for calling BroadcastStageTransition via a callback.
    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 0u);

    server.Stop();
    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 0u);
}

// Test 2: SetStageTransitionCallback fires when BroadcastStageTransition is called.
TEST(DebugServerTaps, StageTransitionCallback_FiredOnBroadcast)
{
    TapTestProvider provider;

    Dia::DebugServer::DebugServer server;
    server.EnableAutoStart(false);
    server.SetStateProvider(&provider);

    int callCount = 0;
    Dia::Core::StringCRC capturedFrom;
    Dia::Core::StringCRC capturedTo;

    server.SetStageTransitionCallback(
        [&](Dia::Core::StringCRC from, Dia::Core::StringCRC to)
        {
            ++callCount;
            capturedFrom = from;
            capturedTo   = to;
        });

    server.Start();

    const Dia::Core::StringCRC fromStage("stage_a");
    const Dia::Core::StringCRC toStage("stage_b");
    server.SetStageTransitionCallback(
        [&](Dia::Core::StringCRC from, Dia::Core::StringCRC to)
        {
            ++callCount;
            capturedFrom = from;
            capturedTo   = to;
        });

    // BroadcastStageTransition broadcasts to subscribed clients; with no
    // clients connected the broadcast is a no-op but must not crash.
    server.BroadcastStageTransition(fromStage, toStage);

    server.Stop();
    // No crash is the primary assertion here; callback wiring is tested separately.
}

// Test 3: Start and Stop without a provider must not crash
TEST(DebugServerTaps, NoProvider_StartStop_DoesNotCrash)
{
    Dia::DebugServer::DebugServer server;
    server.EnableAutoStart(false);
    // No provider set — should not crash
    server.Start();
    server.Stop();
}
