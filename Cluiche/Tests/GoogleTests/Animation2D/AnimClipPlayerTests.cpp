#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/AnimClip.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

// ============================================================
// Helpers
// ============================================================

static Dia::Rig2D::SkeletonDef MakeTestSkelDef()
{
    Dia::Rig2D::SkeletonDef def;
    def.id = Dia::Core::StringCRC("test");
    const char* names[] = { "bone0", "bone1", "bone2", "bone3", "bone4" };
    for (int i = 0; i < 5; ++i)
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

// Build a 1-bone clip with duration=1.0, bone="bone0", rotation 0→1
static Dia::Animation2D::AnimClip MakeSimpleClip(const Dia::Rig2D::Skeleton& skeleton)
{
    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 0.0f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::Keyframe kf1;
    kf1.time = 1.0f; kf1.rotation = 1.0f;
    kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf0);
    track.keyframes.Add(kf1);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("simple_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    return Dia::Animation2D::AnimClip(def, skeleton);
}

// ============================================================
// AnimClipPlayer tests
// ============================================================

TEST(AnimClipPlayer, Play_SetsIsPlaying)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);

    EXPECT_TRUE(player.IsPlaying());
}

TEST(AnimClipPlayer, Stop_ClearsState)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);
    player.Stop();

    EXPECT_FALSE(player.IsPlaying());
    EXPECT_EQ(player.GetCurrentClip(), nullptr);
}

TEST(AnimClipPlayer, Sample_WhenNotPlaying_IsNoOp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    const float bindRot = pose.GetLocalTransform(0).rotation;

    Dia::Animation2D::AnimClipPlayer player; // not playing
    player.Sample(skeleton, pose);

    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, bindRot, 1e-5f);
}

TEST(AnimClipPlayer, PlayWhilePlaying_RestartsFromZero)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);
    player.Update(0.5f);
    player.Play(&clip); // restart

    EXPECT_NEAR(player.GetCurrentTime(), 0.0f, 1e-5f);
}

TEST(AnimClipPlayer, OneShot_StopsAtDuration)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(false);
    player.Play(&clip);
    player.Update(2.0f); // well past duration

    EXPECT_FALSE(player.IsPlaying());
    EXPECT_NEAR(player.GetCurrentTime(), clip.GetDuration(), 1e-5f);
}

TEST(AnimClipPlayer, Looping_WrapsTime)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(true);
    player.Play(&clip);
    player.Update(clip.GetDuration() * 2.0f); // two full loops

    EXPECT_TRUE(player.IsPlaying());
    EXPECT_GE(player.GetCurrentTime(), 0.0f);
    EXPECT_LT(player.GetCurrentTime(), clip.GetDuration());
}

TEST(AnimClipPlayer, NegativeSpeed_OneShot_ClampsToZero)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(false);
    player.SetSpeed(-1.0f);
    player.Play(&clip);
    player.Update(2.0f);

    EXPECT_FALSE(player.IsPlaying());
    EXPECT_NEAR(player.GetCurrentTime(), 0.0f, 1e-5f);
}

TEST(AnimClipPlayer, NegativeSpeed_Looping_WrapsBackward)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(true);
    player.SetSpeed(-1.0f);
    player.Play(&clip);
    player.Update(0.1f); // would step back from 0 — should wrap to a positive time

    EXPECT_TRUE(player.IsPlaying());
    EXPECT_GE(player.GetCurrentTime(), 0.0f);
    EXPECT_LE(player.GetCurrentTime(), clip.GetDuration());
}

TEST(AnimClipPlayer, SpeedZero_Pauses)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);
    player.SetSpeed(0.0f);

    const float timeBefore = player.GetCurrentTime();
    player.Update(1.0f);

    EXPECT_NEAR(player.GetCurrentTime(), timeBefore, 1e-5f);
    EXPECT_TRUE(player.IsPlaying());
}

TEST(AnimClipPlayer, GetNormalizedTime_Range)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);

    // At start: normalized == 0
    EXPECT_NEAR(player.GetNormalizedTime(), 0.0f, 1e-5f);

    // Advance to end
    player.SetLooping(false);
    player.Update(clip.GetDuration());
    EXPECT_NEAR(player.GetNormalizedTime(), 1.0f, 1e-5f);
}

TEST(AnimClipPlayer, GetCurrentClip_ReturnsClip)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(&clip);

    EXPECT_EQ(player.GetCurrentClip(), &clip);
}

TEST(AnimClipPlayer, Speed_DoublesAdvance)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(false);
    player.SetSpeed(2.0f);
    player.Play(&clip);
    player.Update(0.25f); // at speed 2, this advances 0.5 seconds

    EXPECT_NEAR(player.GetCurrentTime(), 0.5f, 1e-4f);
}

TEST(AnimClipPlayer, IsLooping_ReflectsSetLooping)
{
    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(true);
    EXPECT_TRUE(player.IsLooping());
    player.SetLooping(false);
    EXPECT_FALSE(player.IsLooping());
}

TEST(AnimClipPlayer, PlayWithMode_Looping_SetsIsLooping)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.Play(clip, Dia::Animation2D::PlaybackMode::kLooping);
    EXPECT_TRUE(player.IsLooping());
}

TEST(AnimClipPlayer, PlayWithMode_OneShot_ClearsIsLooping)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(true); // prime as looping
    player.Play(clip, Dia::Animation2D::PlaybackMode::kOneShot);
    EXPECT_FALSE(player.IsLooping());
}

TEST(AnimClipPlayer, Play_NullPtr_WhenStopped_IsNoOp)
{
    Dia::Animation2D::AnimClipPlayer player; // default: not playing
    player.Play(nullptr);
    EXPECT_FALSE(player.IsPlaying());
    EXPECT_EQ(player.GetCurrentClip(), nullptr);
}

TEST(AnimClipPlayer, PlayPointerOverload_PreservesLoopingMode)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);
    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimClipPlayer player;
    player.SetLooping(true);
    player.Play(&clip); // pointer overload — should preserve looping
    EXPECT_TRUE(player.IsLooping());
}
