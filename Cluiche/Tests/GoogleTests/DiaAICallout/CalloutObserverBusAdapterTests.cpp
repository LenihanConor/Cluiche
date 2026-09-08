// Suite: CalloutObserverBusAdapterTests
//
// Covers the observer/notification half (plan task "18a") of the
// callout-observer-bus-adapter feature: ICalloutObserver + CalloutObserverSubject
// wired into CalloutRegistry::Emit/Claim/Release (zero DiaMessageBus dependency
// in that half).
//
// The remaining cases below cover the bus-forwarding half: CalloutBusAdapter
// forwarding each ICalloutObserver notification onto a Dia::MessageBus::Bus
// via Bus::Broadcast<T>() with the generated CalloutEmittedEvent /
// CalloutClaimedEvent / CalloutReleasedEvent structs.

#include <gtest/gtest.h>

#include <DiaAICallout/CalloutBusAdapter.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/ICalloutObserver.h>
#include <DiaAICallout/Messages/callout_messages.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMessageBus/Bus.h>

#include <string>
#include <vector>

using namespace Dia::AICallout;
using namespace Dia::AICallout::Testing;
using Dia::Core::StringCRC;

namespace {

    // Records every notification it receives, in order, for assertion purposes.
    class CalloutObserverSpy : public ICalloutObserver
    {
    public:
        unsigned int emittedCount  = 0u;
        unsigned int claimedCount  = 0u;
        unsigned int releasedCount = 0u;

        Callout       lastEmittedCallout{};
        CalloutHandle lastEmittedHandle;

        CalloutHandle lastClaimedHandle;
        StringCRC     lastClaimedClaimerEntityId;

        CalloutHandle lastReleasedHandle;
        StringCRC     lastReleasedClaimerEntityId;

        std::vector<std::string> order;

        unsigned int TotalNotifications() const
        {
            return emittedCount + claimedCount + releasedCount;
        }

        void OnCalloutEmitted(const Callout& callout, CalloutHandle handle) override
        {
            ++emittedCount;
            lastEmittedCallout = callout;
            lastEmittedHandle  = handle;
            order.push_back("Emitted:" + std::to_string(handle.GetIndex()) + ":" + std::to_string(handle.GetGeneration()));
        }

        void OnCalloutClaimed(CalloutHandle handle, StringCRC claimerEntityId) override
        {
            ++claimedCount;
            lastClaimedHandle          = handle;
            lastClaimedClaimerEntityId = claimerEntityId;
            order.push_back("Claimed:" + std::to_string(handle.GetIndex()) + ":" + std::to_string(handle.GetGeneration()));
        }

        void OnCalloutReleased(CalloutHandle handle, StringCRC claimerEntityId) override
        {
            ++releasedCount;
            lastReleasedHandle          = handle;
            lastReleasedClaimerEntityId = claimerEntityId;
            order.push_back("Released:" + std::to_string(handle.GetIndex()) + ":" + std::to_string(handle.GetGeneration()));
        }
    };

} // anonymous namespace

TEST(CalloutObserverBusAdapterTests, Emit_AlwaysNotifiesObservers_WithFullCalloutAndHandle)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const StringCRC kind("HelpNeeded");
    const Dia::Maths::Vector2D pos(1.0f, 2.0f);
    const CalloutHandle handle = EmitTestCallout(registry, kind, pos, 50.0f, 5.0f);

    ASSERT_EQ(spy.emittedCount, 1u);
    EXPECT_EQ(spy.lastEmittedHandle.GetIndex(), handle.GetIndex());
    EXPECT_EQ(spy.lastEmittedHandle.GetGeneration(), handle.GetGeneration());
    EXPECT_EQ(spy.lastEmittedCallout.kind, kind);
    EXPECT_FLOAT_EQ(spy.lastEmittedCallout.position.x, pos.x);
    EXPECT_FLOAT_EQ(spy.lastEmittedCallout.position.y, pos.y);
    EXPECT_FLOAT_EQ(spy.lastEmittedCallout.radius, 50.0f);
    EXPECT_FLOAT_EQ(spy.lastEmittedCallout.ttl, 5.0f);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Claim_Success_NotifiesObservers)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const StringCRC entity("EntityA");
    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));

    ASSERT_TRUE(registry.Claim(handle, entity));

    EXPECT_EQ(spy.claimedCount, 1u);
    EXPECT_EQ(spy.lastClaimedHandle.GetIndex(), handle.GetIndex());
    EXPECT_EQ(spy.lastClaimedHandle.GetGeneration(), handle.GetGeneration());
    EXPECT_EQ(spy.lastClaimedClaimerEntityId, entity);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Claim_Failure_AlreadyClaimed_DoesNotNotify)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    ASSERT_TRUE(registry.Claim(handle, StringCRC("EntityA")));
    ASSERT_EQ(spy.claimedCount, 1u);

    // Second claim by a different entity fails — must not add another notification.
    EXPECT_FALSE(registry.Claim(handle, StringCRC("EntityB")));
    EXPECT_EQ(spy.claimedCount, 1u);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Release_RealRelease_NotifiesObservers)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const StringCRC entity("EntityA");
    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    registry.Claim(handle, entity);

    registry.Release(handle, entity);

    EXPECT_EQ(spy.releasedCount, 1u);
    EXPECT_EQ(spy.lastReleasedHandle.GetIndex(), handle.GetIndex());
    EXPECT_EQ(spy.lastReleasedHandle.GetGeneration(), handle.GetGeneration());
    EXPECT_EQ(spy.lastReleasedClaimerEntityId, entity);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Release_NoOp_ExpiredHandle_DoesNotNotify)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    registry.Update(2.0f); // expire
    ASSERT_FALSE(handle.IsValid());

    registry.Release(handle, StringCRC("EntityA"));

    EXPECT_EQ(spy.releasedCount, 0u);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Release_NoOp_AlreadyUnclaimed_DoesNotNotify)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));

    // Never claimed — Release should be a silent no-op.
    registry.Release(handle, StringCRC("EntityA"));

    EXPECT_EQ(spy.releasedCount, 0u);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Release_NoOp_WrongClaimer_DoesNotNotify)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    registry.Claim(handle, StringCRC("EntityA"));

    registry.Release(handle, StringCRC("EntityB")); // wrong claimer

    EXPECT_EQ(spy.releasedCount, 0u);
    EXPECT_TRUE(handle.IsClaimed()); // still claimed by EntityA

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, Update_TTLExpiry_NeverFiresAnyObserverNotification)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    ASSERT_EQ(spy.TotalNotifications(), 1u); // the Emit notification only

    registry.Claim(handle, StringCRC("EntityA"));
    ASSERT_EQ(spy.TotalNotifications(), 2u); // + the Claim notification

    const unsigned int notificationsBeforeExpiry = spy.TotalNotifications();

    registry.Update(2.0f); // expires the (still-claimed) slot

    ASSERT_FALSE(handle.IsValid());
    EXPECT_EQ(spy.TotalNotifications(), notificationsBeforeExpiry) << "TTL expiry must not fire any observer notification";
    EXPECT_EQ(spy.releasedCount, 0u);

    registry.Unsubscribe(&spy);
}

TEST(CalloutObserverBusAdapterTests, MultipleObservers_Subscribed_AllReceiveSameNotification)
{
    CalloutRegistry registry;
    CalloutObserverSpy spyA;
    CalloutObserverSpy spyB;
    registry.Subscribe(&spyA);
    registry.Subscribe(&spyB);

    const CalloutHandle handle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));

    EXPECT_EQ(spyA.emittedCount, 1u);
    EXPECT_EQ(spyB.emittedCount, 1u);
    EXPECT_EQ(spyA.lastEmittedHandle.GetIndex(), handle.GetIndex());
    EXPECT_EQ(spyB.lastEmittedHandle.GetIndex(), handle.GetIndex());

    registry.Claim(handle, StringCRC("EntityA"));
    EXPECT_EQ(spyA.claimedCount, 1u);
    EXPECT_EQ(spyB.claimedCount, 1u);

    registry.Unsubscribe(&spyA);
    registry.Unsubscribe(&spyB);
}

TEST(CalloutObserverBusAdapterTests, Unsubscribe_StopsReceivingFutureNotifications)
{
    CalloutRegistry registry;
    CalloutObserverSpy spy;
    registry.Subscribe(&spy);

    EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    ASSERT_EQ(spy.emittedCount, 1u);

    registry.Unsubscribe(&spy);

    EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(1.0f, 0.0f));
    EXPECT_EQ(spy.emittedCount, 1u); // no additional notification after unsubscribe
}

TEST(CalloutObserverBusAdapterTests, CalloutRegistry_NoObserversSubscribed_BaseAPIStillWorksNormally)
{
    CalloutRegistry registry;

    const StringCRC kind("HelpNeeded");
    const CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f));
    ASSERT_TRUE(handle.IsValid());

    const StringCRC entity("EntityA");
    EXPECT_TRUE(registry.Claim(handle, entity));
    EXPECT_TRUE(handle.IsClaimed());

    registry.Release(handle, entity);
    EXPECT_FALSE(handle.IsClaimed());

    EXPECT_EQ(registry.GetLiveCount(), 1);

    registry.Update(1000.0f); // expire
    EXPECT_FALSE(handle.IsValid());
}

TEST(CalloutObserverBusAdapterTests, Determinism_EmitClaimReleaseSequence_IdenticalNotificationOrderAcrossRuns)
{
    auto RunSequence = [](std::vector<std::string>& outOrder)
    {
        CalloutRegistry registry;
        CalloutObserverSpy spy;
        registry.Subscribe(&spy);

        const StringCRC kind("HelpNeeded");
        const StringCRC entityA("EntityA");
        const StringCRC entityB("EntityB");

        const CalloutHandle h1 = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f));
        const CalloutHandle h2 = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(10.0f, 0.0f));

        registry.Claim(h1, entityA);
        registry.Claim(h2, entityB);

        registry.Release(h1, entityA);
        registry.Release(h2, entityB);

        registry.Unsubscribe(&spy);
        outOrder = spy.order;
    };

    std::vector<std::string> orderRun1;
    std::vector<std::string> orderRun2;
    RunSequence(orderRun1);
    RunSequence(orderRun2);

    ASSERT_EQ(orderRun1.size(), 6u); // 2 emits + 2 claims + 2 releases
    EXPECT_EQ(orderRun1, orderRun2);
}

// =============================================================================
// Bus-forwarding half: CalloutBusAdapter -> Dia::MessageBus::Bus::Broadcast<T>()
// =============================================================================

TEST(CalloutObserverBusAdapterTests, OnCalloutEmitted_BroadcastsFullCalloutAndHandlePayload)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    ASSERT_TRUE((bus.RegisterType<CalloutEmittedEvent, 8>()));

    CalloutEmittedEvent received{};
    unsigned int callCount = 0u;

    auto handle = bus.Subscribe<CalloutEmittedEvent>(
        StringCRC("TestSubscriber"),
        [&](const CalloutEmittedEvent& evt) {
            ++callCount;
            received = evt;
        },
        Dia::MessageBus::Pass::Primary);
    ASSERT_TRUE(handle.IsValid());

    CalloutRegistry registry;
    const StringCRC kind("HelpNeeded");
    const Dia::Maths::Vector2D pos(3.0f, 4.0f);
    const Callout callout{ kind, pos, 25.0f, StringCRC::kZero, 8.0f, Json::Value() };
    const CalloutHandle regHandle = registry.Emit(callout);

    // Drive the adapter directly — its whole job is to forward the observer
    // notification onto the bus, independent of who owns the registry.
    CalloutBusAdapter adapter(bus);
    adapter.OnCalloutEmitted(callout, regHandle);

    bus.Update();

    ASSERT_EQ(callCount, 1u);
    EXPECT_EQ(received.callout.kind, kind);
    EXPECT_FLOAT_EQ(received.callout.position.x, pos.x);
    EXPECT_FLOAT_EQ(received.callout.position.y, pos.y);
    EXPECT_FLOAT_EQ(received.callout.radius, 25.0f);
    EXPECT_FLOAT_EQ(received.callout.ttl, 8.0f);
    EXPECT_EQ(received.handle.GetIndex(), regHandle.GetIndex());
    EXPECT_EQ(received.handle.GetGeneration(), regHandle.GetGeneration());
}

TEST(CalloutObserverBusAdapterTests, OnCalloutClaimed_BroadcastsHandlePlusClaimerId_LighterShape)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    ASSERT_TRUE((bus.RegisterType<CalloutClaimedEvent, 8>()));

    CalloutClaimedEvent received{};
    unsigned int callCount = 0u;

    auto handle = bus.Subscribe<CalloutClaimedEvent>(
        StringCRC("TestSubscriber"),
        [&](const CalloutClaimedEvent& evt) {
            ++callCount;
            received = evt;
        },
        Dia::MessageBus::Pass::Primary);
    ASSERT_TRUE(handle.IsValid());

    CalloutRegistry registry;
    const CalloutHandle regHandle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    const StringCRC claimer("EntityA");

    CalloutBusAdapter adapter(bus);
    adapter.OnCalloutClaimed(regHandle, claimer);

    bus.Update();

    ASSERT_EQ(callCount, 1u);
    EXPECT_EQ(received.handle.GetIndex(), regHandle.GetIndex());
    EXPECT_EQ(received.handle.GetGeneration(), regHandle.GetGeneration());
    EXPECT_EQ(received.claimerEntityId, claimer);
}

TEST(CalloutObserverBusAdapterTests, OnCalloutReleased_BroadcastsHandlePlusClaimerId_LighterShape)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    ASSERT_TRUE((bus.RegisterType<CalloutReleasedEvent, 8>()));

    CalloutReleasedEvent received{};
    unsigned int callCount = 0u;

    auto handle = bus.Subscribe<CalloutReleasedEvent>(
        StringCRC("TestSubscriber"),
        [&](const CalloutReleasedEvent& evt) {
            ++callCount;
            received = evt;
        },
        Dia::MessageBus::Pass::Primary);
    ASSERT_TRUE(handle.IsValid());

    CalloutRegistry registry;
    const CalloutHandle regHandle = EmitTestCallout(registry, StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    const StringCRC claimer("EntityA");

    CalloutBusAdapter adapter(bus);
    adapter.OnCalloutReleased(regHandle, claimer);

    bus.Update();

    ASSERT_EQ(callCount, 1u);
    EXPECT_EQ(received.handle.GetIndex(), regHandle.GetIndex());
    EXPECT_EQ(received.handle.GetGeneration(), regHandle.GetGeneration());
    EXPECT_EQ(received.claimerEntityId, claimer);
}

TEST(CalloutObserverBusAdapterTests, GeneratedEventTypes_RegisterAndBroadcastAgainstBus_Compiles)
{
    // Proves the three .diagamemessages-declared types (CalloutEmittedEvent /
    // CalloutClaimedEvent / CalloutReleasedEvent) are usable end to end through
    // the generated RegisterMessages() wiring, not just hand-rolled RegisterType calls.
    Dia::MessageBus::Bus bus;
    bus.Initialize();

    RegisterMessages(bus, Handlers{}); // no declared consumers yet — empty Handlers is valid

    const CalloutHandle dummyHandle;
    EXPECT_TRUE(bus.Broadcast(CalloutEmittedEvent{ Callout{}, dummyHandle }));
    EXPECT_TRUE(bus.Broadcast(CalloutClaimedEvent{ dummyHandle, StringCRC("EntityA") }));
    EXPECT_TRUE(bus.Broadcast(CalloutReleasedEvent{ dummyHandle, StringCRC("EntityA") }));

    bus.Update(); // no subscribers — just proves the drain path doesn't crash
}

// AICalloutTestStageModule.h (Cluiche/CluicheTest/Modules/TestStages/AICalloutTestStageModule.h)
// is the existing (Done) consumer of CalloutRegistry referenced in this feature's acceptance
// criteria. It is intentionally NOT included or modified here — it lives in the CluicheTest
// application layer (a different vcxproj/include-path universe than this Dia-level GoogleTests
// target) and per the feature's Non-Goals, updating it to demonstrate CalloutBusAdapter wiring
// is an optional follow-up, not required for this feature. Its continued zero-DiaMessageBus-
// dependency compilation is verified out of band via:
//   grep -c DiaMessageBus Cluiche/CluicheTest/Modules/TestStages/AICalloutTestStageModule.h .cpp
// (expected: 0 matches in both files) plus the existing `dia run cluichetest` / stage test-suite
// build, neither of which this feature touches.

TEST(CalloutObserverBusAdapterTests, EndToEnd_RealEmit_ReactsViaBusWithoutCallingQuery)
{
    // Proves the core design goal: a Bus::Subscribe<CalloutEmittedEvent> handler
    // reacts to a real CalloutRegistry::Emit() call using only the event's
    // self-describing payload (kind/position/radius/faction) — zero Query() calls.
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    ASSERT_TRUE((bus.RegisterType<CalloutEmittedEvent, 8>()));

    bool  reactedWithoutQuery = false;
    StringCRC observedKind;
    Dia::Maths::Vector2D observedPosition;
    float observedRadius = 0.0f;

    auto subHandle = bus.Subscribe<CalloutEmittedEvent>(
        StringCRC("ReactingListener"),
        [&](const CalloutEmittedEvent& evt) {
            // Everything a listener needs to decide whether it cares comes
            // straight off evt.callout — no registry.Query() round-trip.
            observedKind     = evt.callout.kind;
            observedPosition = evt.callout.position;
            observedRadius   = evt.callout.radius;
            reactedWithoutQuery = true;
        },
        Dia::MessageBus::Pass::Primary);
    ASSERT_TRUE(subHandle.IsValid());

    CalloutRegistry   registry;
    CalloutBusAdapter adapter(bus);
    registry.Subscribe(&adapter);

    const StringCRC kind("HelpNeeded");
    const Dia::Maths::Vector2D pos(12.0f, -6.0f);
    const CalloutHandle emittedHandle = EmitTestCallout(registry, kind, pos, 40.0f, 6.0f);
    ASSERT_TRUE(emittedHandle.IsValid());

    bus.Update();

    EXPECT_TRUE(reactedWithoutQuery);
    EXPECT_EQ(observedKind, kind);
    EXPECT_FLOAT_EQ(observedPosition.x, pos.x);
    EXPECT_FLOAT_EQ(observedPosition.y, pos.y);
    EXPECT_FLOAT_EQ(observedRadius, 40.0f);

    registry.Unsubscribe(&adapter);
}
