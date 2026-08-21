// Suite: CalloutObserverBusAdapterTests
//
// Covers the observer/notification half (plan task "18a") of the
// callout-observer-bus-adapter feature: ICalloutObserver + CalloutObserverSubject
// wired into CalloutRegistry::Emit/Claim/Release. The DiaMessageBus bus-delivery
// half of this feature is a separate, later task and will add cases to this same
// file — it is NOT covered here. Zero DiaMessageBus dependency in this file.

#include <gtest/gtest.h>

#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/ICalloutObserver.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

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
