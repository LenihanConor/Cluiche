#include <gtest/gtest.h>

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

// ============================================================
// AnimClip tests
// ============================================================

TEST(AnimClip, DataStructs_FieldTypes)
{
    Dia::Animation2D::Keyframe kf;
    kf.time     = 0.5f;
    kf.rotation = 1.0f;
    kf.position = Dia::Maths::Vector2D(1.0f, 2.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef clipDef;
    clipDef.id       = Dia::Core::StringCRC("test_clip");
    clipDef.duration = 1.0f;
    clipDef.tracks.Add(track);

    EXPECT_EQ(clipDef.tracks.Size(), 1u);
}

#ifdef _DEBUG
TEST(AnimClip, Construction_ZeroDuration_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("bad_clip");
    def.duration = 0.0f;

    EXPECT_DEATH(Dia::Animation2D::AnimClip clip(def, skeleton), "");
}

TEST(AnimClip, Construction_UnsortedKeyframes_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.5f; kf0.rotation = 0.0f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::Keyframe kf1;
    kf1.time = 0.0f; kf1.rotation = 1.0f;
    kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf0);
    track.keyframes.Add(kf1);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("unsorted_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    EXPECT_DEATH(Dia::Animation2D::AnimClip clip(def, skeleton), "");
}

TEST(AnimClip, Construction_DuplicateBoneTracks_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.0f;
    kf.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("dup_track_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);
    def.tracks.Add(track); // duplicate bone

    EXPECT_DEATH(Dia::Animation2D::AnimClip clip(def, skeleton), "");
}
#endif // _DEBUG

TEST(AnimClip, Construction_EmptyTrack_Skipped)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Empty track (0 keyframes)
    Dia::Animation2D::KeyframeTrack emptyTrack;
    emptyTrack.boneId = Dia::Core::StringCRC("bone0");

    // Valid track
    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.5f;
    kf.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    Dia::Animation2D::KeyframeTrack validTrack;
    validTrack.boneId = Dia::Core::StringCRC("bone1");
    validTrack.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("skip_empty");
    def.duration = 1.0f;
    def.tracks.Add(emptyTrack);
    def.tracks.Add(validTrack);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    EXPECT_EQ(clip.GetTrackCount(), 1);
}

TEST(AnimClip, Construction_MissingBone_NoCrash)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.5f;
    kf.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("missing");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("missing_bone_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    // No crash — missing bone track is silently skipped
    Dia::Animation2D::AnimClip clip(def, skeleton);
    EXPECT_EQ(clip.GetTrackCount(), 0);
}

TEST(AnimClip, Sample_AtExactKeyframeTimes_ExactValues)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 0.5f;
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
    def.id       = Dia::Core::StringCRC("exact_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    clip.Sample(0.0f, skeleton, pose);
    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, 0.5f, 1e-5f);

    pose.SetToBindPose(skeleton);
    clip.Sample(1.0f, skeleton, pose);
    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, 1.0f, 1e-5f);
}

TEST(AnimClip, Sample_BetweenKeyframes_LinearLerp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 0.0f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::Keyframe kf1;
    kf1.time = 1.0f; kf1.rotation = 0.0f;
    kf1.position = Dia::Maths::Vector2D(2.0f, 4.0f);
    kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf0);
    track.keyframes.Add(kf1);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("lerp_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    clip.Sample(0.5f, skeleton, pose);
    EXPECT_NEAR(pose.GetLocalTransform(0).position.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(pose.GetLocalTransform(0).position.Y(), 2.0f, 1e-5f);
}

TEST(AnimClip, Sample_ShortestArcRotation)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 2.9f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::Keyframe kf1;
    kf1.time = 1.0f; kf1.rotation = -2.9f;
    kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf0);
    track.keyframes.Add(kf1);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("shortest_arc_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    clip.Sample(0.5f, skeleton, pose);

    // Shortest arc: crosses ±pi boundary, so midpoint should be near ±pi, not near 0
    const float result = pose.GetLocalTransform(0).rotation;
    const float distFromPi  = std::abs(std::abs(result) - 3.14159265f);
    const float distFromZero = std::abs(result);
    EXPECT_LT(distFromPi, distFromZero);
}

TEST(AnimClip, Sample_PartialKeyframes_UsesBindPose)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Rotation-only track on bone0; position should come from bind pose (0,0)
    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 1.0f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId       = Dia::Core::StringCRC("bone0");
    track.rotationOnly = true;
    track.keyframes.Add(kf0);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("partial_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    clip.Sample(0.0f, skeleton, pose);

    // Position unchanged from bind pose (0,0)
    EXPECT_NEAR(pose.GetLocalTransform(0).position.X(), 0.0f, 1e-5f);
    EXPECT_NEAR(pose.GetLocalTransform(0).position.Y(), 0.0f, 1e-5f);
}

TEST(AnimClip, Sample_SingleKeyframeTrack_ConstantValue)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.7f;
    kf.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("single_kf_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);

    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        Dia::Rig2D::Pose pose(skeleton);
        pose.SetToBindPose(skeleton);
        clip.Sample(t, skeleton, pose);
        EXPECT_NEAR(pose.GetLocalTransform(0).rotation, 0.7f, 1e-5f) << "t=" << t;
    }
}

TEST(AnimClip, Sample_TimeClampedToDuration)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

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
    def.id       = Dia::Core::StringCRC("clamp_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);

    Dia::Rig2D::Pose poseAtDuration(skeleton);
    poseAtDuration.SetToBindPose(skeleton);
    clip.Sample(1.0f, skeleton, poseAtDuration);

    Dia::Rig2D::Pose posePastDuration(skeleton);
    posePastDuration.SetToBindPose(skeleton);
    clip.Sample(2.0f, skeleton, posePastDuration);

    EXPECT_NEAR(poseAtDuration.GetLocalTransform(0).rotation,
                posePastDuration.GetLocalTransform(0).rotation, 1e-5f);
}

TEST(AnimClip, Sample_TimeClampedToZero)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf0;
    kf0.time = 0.0f; kf0.rotation = 0.25f;
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
    def.id       = Dia::Core::StringCRC("clamp_zero_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);

    Dia::Rig2D::Pose poseAtZero(skeleton);
    poseAtZero.SetToBindPose(skeleton);
    clip.Sample(0.0f, skeleton, poseAtZero);

    Dia::Rig2D::Pose poseNegative(skeleton);
    poseNegative.SetToBindPose(skeleton);
    clip.Sample(-0.5f, skeleton, poseNegative);

    EXPECT_NEAR(poseAtZero.GetLocalTransform(0).rotation,
                poseNegative.GetLocalTransform(0).rotation, 1e-5f);
}

TEST(AnimClip, GetId_GetDuration_GetTrackCount_Correct)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.0f;
    kf.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("accessor_clip");
    def.duration = 2.5f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);

    EXPECT_EQ(clip.GetId(), def.id);
    EXPECT_NEAR(clip.GetDuration(), 2.5f, 1e-5f);
    EXPECT_EQ(clip.GetTrackCount(), 1);
}

TEST(AnimClip, Sample_NonUniformTrackLengths)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Track A: two keyframes at t=0 and t=1
    Dia::Animation2D::Keyframe kfA0, kfA1;
    kfA0.time = 0.0f; kfA0.rotation = 0.0f;
    kfA0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kfA0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    kfA1.time = 1.0f; kfA1.rotation = 2.0f;
    kfA1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kfA1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack trackA;
    trackA.boneId = Dia::Core::StringCRC("bone0");
    trackA.keyframes.Add(kfA0);
    trackA.keyframes.Add(kfA1);

    // Track B: three keyframes at t=0, t=0.5, t=1
    Dia::Animation2D::Keyframe kfB0, kfB1, kfB2;
    kfB0.time = 0.0f; kfB0.rotation = 0.0f;
    kfB0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kfB0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    kfB1.time = 0.5f; kfB1.rotation = 1.0f;
    kfB1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kfB1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    kfB2.time = 1.0f; kfB2.rotation = 0.0f;
    kfB2.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kfB2.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack trackB;
    trackB.boneId = Dia::Core::StringCRC("bone1");
    trackB.keyframes.Add(kfB0);
    trackB.keyframes.Add(kfB1);
    trackB.keyframes.Add(kfB2);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("nonuniform_clip");
    def.duration = 1.0f;
    def.tracks.Add(trackA);
    def.tracks.Add(trackB);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    clip.Sample(0.5f, skeleton, pose);

    // Track A at t=0.5: lerp(0, 2, 0.5) = 1.0
    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, 1.0f, 1e-5f);
    // Track B at t=0.5: exactly at keyframe -> 1.0
    EXPECT_NEAR(pose.GetLocalTransform(1).rotation, 1.0f, 1e-5f);
}

TEST(AnimClip, Sample_IsPureWrite_NotReadModify)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Track covers bone0 with known rotation
    Dia::Animation2D::Keyframe kf;
    kf.time = 0.0f; kf.rotation = 0.5f;
    kf.position = Dia::Maths::Vector2D(1.0f, 2.0f); // explicit position in keyframe
    kf.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("purewrite_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    Dia::Animation2D::AnimClip clip(def, skeleton);
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    // Pre-set bone0 position to something unusual
    pose.GetLocalTransform(0).position = Dia::Maths::Vector2D(99.0f, 99.0f);

    clip.Sample(0.0f, skeleton, pose);

    // After sample, bone0 should have the clip's value, not the pre-set 99,99
    EXPECT_NEAR(pose.GetLocalTransform(0).position.X(), 1.0f, 1e-5f);
    EXPECT_NEAR(pose.GetLocalTransform(0).position.Y(), 2.0f, 1e-5f);
}
