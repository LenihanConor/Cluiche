#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/IAnimClipObserver.h>
#include <DiaAnimation2D/Testing/AnimClipBuilders.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Bone.h>
#include <DiaMaths/Vector/Vector2D.h>

// NOTE: This test file also hosts bus-delivery cases in a later, separate task
// (AnimClipBusAdapter). This task covers detection + observer notification only —
// zero DiaMessageBus dependency here.

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
