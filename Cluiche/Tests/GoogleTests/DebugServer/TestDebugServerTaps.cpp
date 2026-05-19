#include <gtest/gtest.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/OverflowPolicy.h>
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

        Dia::ApplicationFlow::IStreamStore* FindStream(
            const Dia::Core::StringCRC& id) override
        {
            if (id == Dia::Core::StringCRC("$lifecycle"))
                return &mLifecycleStore;
            return nullptr;
        }
    };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Test 1: Start attaches the lifecycle tap
TEST(DebugServerTaps, LifecycleTap_AttachedOnStart)
{
    TapTestProvider provider;
    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 0u);

    Dia::DebugServer::DebugServer server;
    server.EnableAutoStart(false);
    server.SetStateProvider(&provider);
    server.Start();

    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 1u);

    server.Stop();
}

// Test 2: Stop detaches the lifecycle tap
TEST(DebugServerTaps, LifecycleTap_DetachedOnStop)
{
    TapTestProvider provider;

    Dia::DebugServer::DebugServer server;
    server.EnableAutoStart(false);
    server.SetStateProvider(&provider);
    server.Start();

    ASSERT_EQ(provider.mLifecycleStore.GetTapCount(), 1u);

    server.Stop();
    EXPECT_EQ(provider.mLifecycleStore.GetTapCount(), 0u);
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
