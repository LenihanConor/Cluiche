#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/IAnimClipObserver.h>
#include <DiaAnimation2D/AnimClipBusAdapter.h>
#include <DiaAnimation2D/Messages/animation2d_messages.h>
#include <DiaAnimation2D/Testing/AnimClipBuilders.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Bone.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMessageBus/Bus.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/EntityAddress.h>

// NOTE: This test file hosts both the detection/observer cases (above) and the
// bus-delivery cases for AnimClipBusAdapter (below, this task).

// ============================================================
// Helpers
// ============================================================

namespace {

Dia::Rig2D::SkeletonDef MakeTestSkelDef()
{
    Dia::Rig2D::SkeletonDef def;
    def.id = Dia::Core::StringCRC("test");
    const char* names[] = { "bone0", "bone1" };
    for (int i = 0; i < 2; ++i)
    {
        Dia::Rig2D::Bone b;
        b.name          = Dia::Core::StringCRC(names[i]);
        b.parentIndex   = i - 1;
        b.length        = 1.0f;
        b.localPosition = Dia::Maths::Vector2D(0.0f, static_cast<float>(i));
        def.bones.Add(b);
    }
    return def;
}

// Records every notification it receives, along with which clip pointer fired.
class RecordingObserver : public Dia::Animation2D::IAnimClipObserver
{
public:
    int finishedCount = 0;
    int loopedCount   = 0;
    const Dia::Animation2D::AnimClip* lastFinishedClip = nullptr;
    const Dia::Animation2D::AnimClip* lastLoopedClip   = nullptr;

    void OnClipFinished(const Dia::Animation2D::AnimClip& clip) override
    {
        ++finishedCount;
        lastFinishedClip = &clip;
    }

    void OnClipLooped(const Dia::Animation2D::AnimClip& clip) override
    {
        ++loopedCount;
        lastLoopedClip = &clip;
    }
};

} // namespace

// ============================================================
// ClipCompletionBusAdapter (detection + observer) tests
// ============================================================

TEST(ClipCompletionBusAdapterTests, OneShotClip_ReachesNaturalEnd_FiresOnClipFinished)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f); // reaches the end exactly

    EXPECT_FALSE(player.IsPlaying());
    EXPECT_EQ(observer.finishedCount, 1);
    EXPECT_EQ(observer.lastFinishedClip, &clip);
    EXPECT_EQ(observer.loopedCount, 0);
}

TEST(ClipCompletionBusAdapterTests, OneShotClip_ExplicitStopBeforeEnd_DoesNotFireOnClipFinished)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(0.5f); // halfway — not finished yet
    player.Stop();       // explicit external stop, not natural completion

    EXPECT_FALSE(player.IsPlaying());
    EXPECT_EQ(observer.finishedCount, 0);
}

TEST(ClipCompletionBusAdapterTests, OneShotClip_StopCalledOnCompletionFrame_FiresAtMostOnce)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(2.0f); // well past the end — natural completion fires once
    player.Stop();       // caller reacts to completion by stopping the player
    player.Update(0.1f); // further updates must not add extra fires
    player.Update(0.1f);

    EXPECT_EQ(observer.finishedCount, 1);
}

TEST(ClipCompletionBusAdapterTests, LoopingClip_WrapsOnce_FiresOnClipLooped)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(true);
    player.Play(&clip);
    player.Update(1.5f); // crosses the wrap boundary once

    EXPECT_TRUE(player.IsPlaying());
    EXPECT_EQ(observer.loopedCount, 1);
    EXPECT_EQ(observer.lastLoopedClip, &clip);
    EXPECT_EQ(observer.finishedCount, 0);
}

TEST(ClipCompletionBusAdapterTests, LoopingClip_MultipleWrapsAcrossFrames_FiresOncePerWrap)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(true);
    player.Play(&clip);
    player.Update(0.6f); // no wrap yet
    EXPECT_EQ(observer.loopedCount, 0);

    player.Update(0.6f); // crosses boundary — wrap #1
    EXPECT_EQ(observer.loopedCount, 1);

    player.Update(1.0f); // crosses boundary again — wrap #2
    EXPECT_EQ(observer.loopedCount, 2);
}

TEST(ClipCompletionBusAdapterTests, LoopingClip_NeverFiresOnClipFinished)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(true);
    player.Play(&clip);
    for (int i = 0; i < 20; ++i)
    {
        player.Update(0.3f); // many wraps across many cycles
    }

    EXPECT_GT(observer.loopedCount, 0);
    EXPECT_EQ(observer.finishedCount, 0);
}

TEST(ClipCompletionBusAdapterTests, Subscribe_MultipleObservers_AllReceiveNotification)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observerA;
    RecordingObserver observerB;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observerA);
    player.Subscribe(&observerB);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f);

    EXPECT_EQ(observerA.finishedCount, 1);
    EXPECT_EQ(observerB.finishedCount, 1);
}

TEST(ClipCompletionBusAdapterTests, Unsubscribe_StopsReceivingFutureNotifications)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);
    player.Unsubscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f);

    EXPECT_EQ(observer.finishedCount, 0);
}

TEST(ClipCompletionBusAdapterTests, PlayWhilePlaying_RestartsFromZero_ResetsCompletionState_NoStaleFinishedFire)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f); // natural completion — fires once
    EXPECT_EQ(observer.finishedCount, 1);

    player.Play(&clip); // restart mid/after playback — must reset completion state
    EXPECT_NEAR(player.GetCurrentTime(), 0.0f, 1e-5f);
    EXPECT_TRUE(player.IsPlaying());

    // A stray Update immediately after restart must not spuriously refire.
    player.Update(0.0f);
    EXPECT_EQ(observer.finishedCount, 1);

    player.Update(1.0f); // second natural completion — fires again (count 2)
    EXPECT_EQ(observer.finishedCount, 2);
}

TEST(ClipCompletionBusAdapterTests, OneShotClip_UpdateAfterAlreadyFinished_DoesNotRefireOnClipFinished)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    RecordingObserver observer;
    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&observer);

    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f); // finishes naturally
    EXPECT_EQ(observer.finishedCount, 1);

    player.Update(0.5f);
    player.Update(0.5f);
    player.Update(0.5f);

    EXPECT_EQ(observer.finishedCount, 1);
}

// ============================================================
// AnimClipBusAdapter (bus-forwarding) tests
// ============================================================

namespace {

// Builds a StringCRC whose Value() equals the exact numeric encoding
// MakeEntitySubscriberId() produces, so a Bus::Subscribe<T> subscription
// lands on the same Dia::Mailbox::SubscriberId EntityRouter::ResolveEntity
// computes. Mirrors EntityRouterBusWiringTests.cpp's ToBusSubscriberId().
Dia::Core::StringCRC ToBusSubscriberId(Dia::Mailbox::SubscriberId subId)
{
    Dia::Core::StringCRC result;
    static_cast<Dia::Core::CRC&>(result) = static_cast<unsigned int>(subId.value);
    return result;
}

} // namespace

TEST(AnimClipBusAdapter, NoOwnerSupplied_BroadcastsClipFinishedEvent)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    Dia::Animation2D::AnimClipBusAdapter adapter(bus); // no owner supplied -> Broadcast

    bool received = false;
    Dia::Animation2D::Messages::ClipFinishedEvent receivedEvent{};
    auto handle = bus.Subscribe<Dia::Animation2D::Messages::ClipFinishedEvent>(
        Dia::Core::StringCRC("test"),
        [&](const Dia::Animation2D::Messages::ClipFinishedEvent& e) { receivedEvent = e; received = true; });
    ASSERT_TRUE(handle.IsValid());

    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&adapter);
    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f); // natural completion

    ASSERT_FALSE(received) << "Queued in the mailbox until Update() flushes it";
    bus.Update();

    ASSERT_TRUE(received);
    EXPECT_EQ(receivedEvent.clipId, clip.GetId());

    // Regression guard: the delivery must have gone through the BROADCAST
    // router, not the entity router — proves Broadcast() was called, not
    // Post() to some address.
    const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
    bool foundEntry = false;
    for (uint32_t i = 0; i < ledger.entries.Size(); ++i) {
        if (ledger.entries[i].typeId == Dia::Animation2D::Messages::ClipFinishedEvent::kTypeId) {
            EXPECT_EQ(ledger.entries[i].routerId, Dia::MessageBus::Bus::kBroadcastRouterId);
            foundEntry = true;
        }
    }
    EXPECT_TRUE(foundEntry);
}

TEST(AnimClipBusAdapter, ExplicitDefaultConstructedEntity_GatesToBroadcastBranch)
{
    Dia::MessageBus::Bus bus;
    bus.Initialize();
    // Explicit default-constructed (invalid) Entity — IsValid() must gate
    // this to the same Broadcast branch as the no-argument constructor.
    Dia::Animation2D::AnimClipBusAdapter adapter(bus, Dia::Entity::Entity());

    bool received = false;
    auto handle = bus.Subscribe<Dia::Animation2D::Messages::ClipLoopedEvent>(
        Dia::Core::StringCRC("test"),
        [&](const Dia::Animation2D::Messages::ClipLoopedEvent&) { received = true; });
    ASSERT_TRUE(handle.IsValid());

    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&adapter);
    player.SetLooping(true);
    player.Play(&clip);
    player.Update(1.5f); // crosses the loop-wrap boundary once

    bus.Update();
    EXPECT_TRUE(received);
}

TEST(AnimClipBusAdapter, ValidOwnerSupplied_PostsToEntityAddress_NotBroadcastToOtherSubscribers)
{
    Dia::Entity::Domain domain;
    Dia::Entity::Entity owner = domain.CreateEntity();
    Dia::Entity::Entity other = domain.CreateEntity();
    ASSERT_TRUE(owner.IsValid());
    ASSERT_TRUE(other.IsValid());

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());

    Dia::Animation2D::AnimClipBusAdapter adapter(bus, owner);

    int ownerCallCount = 0;
    int otherCallCount = 0;
    auto ownerHandle = bus.Subscribe<Dia::Animation2D::Messages::ClipFinishedEvent>(
        ToBusSubscriberId(Dia::Entity::MakeEntitySubscriberId(owner)),
        [&ownerCallCount](const Dia::Animation2D::Messages::ClipFinishedEvent&) { ++ownerCallCount; });
    auto otherHandle = bus.Subscribe<Dia::Animation2D::Messages::ClipFinishedEvent>(
        ToBusSubscriberId(Dia::Entity::MakeEntitySubscriberId(other)),
        [&otherCallCount](const Dia::Animation2D::Messages::ClipFinishedEvent&) { ++otherCallCount; });
    ASSERT_TRUE(ownerHandle.IsValid());
    ASSERT_TRUE(otherHandle.IsValid());

    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&adapter);
    player.SetLooping(false);
    player.Play(&clip);
    player.Update(1.0f); // natural completion

    bus.Update();

    // If the adapter had called Broadcast() instead of Post() to the owner's
    // address, BroadcastRouter would fan out to every live subscriber
    // (including "other") — this is the regression BroadcastRouter.h
    // documents (Resolve fans out regardless of addr.payload).
    EXPECT_EQ(ownerCallCount, 1);
    EXPECT_EQ(otherCallCount, 0);

    const Dia::MessageBus::LedgerSnapshot& ledger = bus.GetLastTickLedger();
    bool foundEntry = false;
    for (uint32_t i = 0; i < ledger.entries.Size(); ++i) {
        if (ledger.entries[i].typeId == Dia::Animation2D::Messages::ClipFinishedEvent::kTypeId) {
            EXPECT_EQ(ledger.entries[i].routerId, Dia::Entity::kEntityRouterId);
            foundEntry = true;
        }
    }
    EXPECT_TRUE(foundEntry);
}

TEST(AnimClipBusAdapter, EndToEnd_BusSubscriberReceivesClipFinishedEvent_WithoutPollingIsPlaying)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    Dia::Animation2D::AnimClipBusAdapter adapter(bus);

    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&adapter);
    player.SetLooping(false);
    player.Play(&clip);

    bool finished = false;
    auto handle = bus.Subscribe<Dia::Animation2D::Messages::ClipFinishedEvent>(
        Dia::Core::StringCRC("gameplay_logic"),
        [&](const Dia::Animation2D::Messages::ClipFinishedEvent& e) {
            finished = true;
            EXPECT_EQ(e.clipId, clip.GetId());
        });
    ASSERT_TRUE(handle.IsValid());

    // Drive completion purely through Update() + bus.Update() — the test
    // never calls player.IsPlaying()/GetNormalizedTime() to detect the end.
    player.Update(1.0f);
    bus.Update();

    EXPECT_TRUE(finished);
}

TEST(AnimClipBusAdapter, ExplicitStop_NeverForwardsClipFinishedEventToBus)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 1, 0.0f, 1.0f, 1.0f);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    Dia::Animation2D::AnimClipBusAdapter adapter(bus);

    Dia::Animation2D::AnimClipPlayer player;
    player.Subscribe(&adapter);
    player.SetLooping(false);
    player.Play(&clip);

    bool finished = false;
    auto handle = bus.Subscribe<Dia::Animation2D::Messages::ClipFinishedEvent>(
        Dia::Core::StringCRC("test"),
        [&](const Dia::Animation2D::Messages::ClipFinishedEvent&) { finished = true; });
    ASSERT_TRUE(handle.IsValid());

    player.Update(0.5f); // halfway
    player.Stop();       // explicit external stop, not natural completion
    bus.Update();

    EXPECT_FALSE(finished);
}
