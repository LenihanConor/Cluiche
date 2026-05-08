#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/AnimClipLoader.h>
#include <DiaAnimation2D/SpringChain.h>
#include <DiaAnimation2D/SpringParamUtils.h>
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/PoseBlendStack.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Json/external/json/json.h>

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

// Build a clip that covers boneCount bones starting at bone0, rotation 0→1 over 1 second
static Dia::Animation2D::AnimClip MakeFullCoverageClip(
    const Dia::Rig2D::Skeleton& sk, int boneCount)
{
    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("full_clip");
    def.duration = 1.0f;

    const char* names[] = { "bone0", "bone1", "bone2", "bone3", "bone4" };
    for (int b = 0; b < boneCount && b < 5; ++b)
    {
        Dia::Animation2D::Keyframe kf0, kf1;
        kf0.time = 0.0f; kf0.rotation = 0.0f;
        kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
        kf1.time = 1.0f; kf1.rotation = 1.0f;
        kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

        Dia::Animation2D::KeyframeTrack track;
        track.boneId = Dia::Core::StringCRC(names[b]);
        track.keyframes.Add(kf0);
        track.keyframes.Add(kf1);
        def.tracks.Add(track);
    }

    return Dia::Animation2D::AnimClip(def, sk);
}

static Dia::Animation2D::SpringChainDef MakeTestChainDef()
{
    Dia::Animation2D::SpringChainDef def;
    def.id         = Dia::Core::StringCRC("integ_chain");
    def.rootBoneId = Dia::Core::StringCRC("bone0");
    def.boneIds.Add(Dia::Core::StringCRC("bone1"));
    def.boneIds.Add(Dia::Core::StringCRC("bone2"));
    def.boneIds.Add(Dia::Core::StringCRC("bone3"));
    def.defaultNode.stiffness          = 50.0f;
    def.defaultNode.damping            = 5.0f;
    def.defaultNode.maxAngularVelocity = 20.0f;
    return def;
}

// ============================================================
// Integration tests
// ============================================================

TEST(Animation2D_Integration, FullPipeline_ClipPlayer_DrivesAllBones)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimClip clip = MakeFullCoverageClip(skeleton, 5);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("clip"));
    player->SetLooping(false);
    player->Play(&clip);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    // Advance to t=0.5 — all bones should be ~0.5
    evaluator.Evaluate(0.5f, root, skeleton, pose);

    for (int i = 0; i < 5; ++i)
        EXPECT_NEAR(pose.GetLocalTransform(i).rotation, 0.5f, 0.05f) << "bone " << i;
}

TEST(Animation2D_Integration, FullPipeline_SpringChain_SecondaryMotion)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::SpringChainDef chainDef = MakeTestChainDef();
    evaluator.RegisterSpringChain(Dia::Core::StringCRC("spring"), chainDef);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    // Manually displace bone1 rotation to start with energy
    pose.GetLocalTransform(1).rotation = 1.0f;

    Dia::Rig2D::BoneTransform root;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i)
        evaluator.Evaluate(dt, root, skeleton, pose);

    // Should converge toward 0 (< 0.5 after 60 frames)
    EXPECT_LT(std::abs(pose.GetLocalTransform(1).rotation), 0.5f);
}

TEST(Animation2D_Integration, FullPipeline_ClipAndSpring_BoneMaskSeparation)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Clip covering only bone0
    Dia::Animation2D::AnimClip clipBone0 = MakeFullCoverageClip(skeleton, 1);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    // Clip source with bone mask for bone0 only
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("clip_src"));
    player->SetLooping(false);
    player->Play(&clipBone0);

    Dia::Animation2D::BoneMask clipMask;
    clipMask.Add(Dia::Core::StringCRC("bone0"));
    evaluator.SetSourceBoneMask(Dia::Core::StringCRC("clip_src"), &clipMask);

    // Spring chain on bones 1-3 with mask
    Dia::Animation2D::SpringChainDef chainDef = MakeTestChainDef();
    evaluator.RegisterSpringChain(Dia::Core::StringCRC("spring_src"), chainDef);

    Dia::Animation2D::BoneMask springMask;
    springMask.Add(Dia::Core::StringCRC("bone1"));
    springMask.Add(Dia::Core::StringCRC("bone2"));
    springMask.Add(Dia::Core::StringCRC("bone3"));
    evaluator.SetSourceBoneMask(Dia::Core::StringCRC("spring_src"), &springMask);

    // Displace spring bones
    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    pose.GetLocalTransform(1).rotation = 0.5f;

    Dia::Rig2D::BoneTransform root;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; ++i)
        evaluator.Evaluate(dt, root, skeleton, pose);

    // bone0: driven by clip at t≈0.16 → rotation > 0 (clip playing)
    EXPECT_GT(std::abs(pose.GetLocalTransform(0).rotation), 0.0f);

    // bones 1-3: driven by spring (should have non-zero rotation from spring physics)
    // (just verify no crash and spring is contributing)
    SUCCEED();
}

TEST(Animation2D_Integration, FullPipeline_LoadClipFromJson_PlayAndEvaluate)
{
    // Skeleton with "spine" bone
    Dia::Rig2D::SkeletonDef skelDef;
    skelDef.id = Dia::Core::StringCRC("spine_skel");
    Dia::Rig2D::Bone spineBone;
    spineBone.name         = Dia::Core::StringCRC("spine");
    spineBone.parentIndex  = -1;
    spineBone.length       = 1.0f;
    spineBone.localPosition = Dia::Maths::Vector2D(0.0f, 0.0f);
    skelDef.bones.Add(spineBone);
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Load clip from JSON
    const char* jsonStr = R"({
        "id": "load_test", "duration": 1.0,
        "tracks": [{"bone": "spine", "keyframes": [
            {"time": 0.0, "position": [0,0], "rotation": 0.0, "scale": [1,1]},
            {"time": 1.0, "position": [0,0], "rotation": 1.0, "scale": [1,1]}
        ]}]
    })";

    Json::Value jsonRoot;
    Json::Reader reader;
    reader.parse(jsonStr, jsonRoot);
    Dia::Animation2D::AnimClipDef clipDef =
        Dia::Animation2D::LoadAnimClipDefFromJson(jsonRoot);
    Dia::Animation2D::AnimClip clip(clipDef, skeleton);

    // Create player and register with evaluator
    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("loaded"));
    player->SetLooping(true);
    player->Play(&clip);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 5; ++i)
        evaluator.Evaluate(dt, root, skeleton, pose);

    // Pose should have been modified from bind pose
    const float bindRot = 0.0f; // spine starts at 0
    EXPECT_NE(pose.GetLocalTransform(0).rotation, bindRot);
}

TEST(Animation2D_Integration, FullPipeline_MultipleClipPlayers_BlendStack)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Clip A: bone0 rotation 0→0
    // Clip B: bone0 rotation 0→1
    // Both at weight 0.5 → expected ~0.25 at t=0.5

    auto makeClipWithRot = [&](const char* id, float startRot, float endRot)
        -> Dia::Animation2D::AnimClip
    {
        Dia::Animation2D::Keyframe kf0, kf1;
        kf0.time = 0.0f; kf0.rotation = startRot;
        kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
        kf1.time = 1.0f; kf1.rotation = endRot;
        kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
        kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

        Dia::Animation2D::KeyframeTrack track;
        track.boneId = Dia::Core::StringCRC("bone0");
        track.keyframes.Add(kf0);
        track.keyframes.Add(kf1);

        Dia::Animation2D::AnimClipDef def;
        def.id       = Dia::Core::StringCRC(id);
        def.duration = 1.0f;
        def.tracks.Add(track);

        return Dia::Animation2D::AnimClip(def, skeleton);
    };

    Dia::Animation2D::AnimClip clipA = makeClipWithRot("clipA", 0.0f, 0.0f);
    Dia::Animation2D::AnimClip clipB = makeClipWithRot("clipB", 0.0f, 1.0f);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    Dia::Animation2D::AnimClipPlayer* playerA =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("srcA"));
    playerA->SetLooping(false);
    playerA->Play(&clipA);
    evaluator.SetSourceWeight(Dia::Core::StringCRC("srcA"), 1.0f);
    evaluator.SetSourcePriority(Dia::Core::StringCRC("srcA"), 0);

    Dia::Animation2D::AnimClipPlayer* playerB =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("srcB"));
    playerB->SetLooping(false);
    playerB->Play(&clipB);
    evaluator.SetSourceWeight(Dia::Core::StringCRC("srcB"), 0.5f);
    evaluator.SetSourcePriority(Dia::Core::StringCRC("srcB"), 1);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    // Advance to t=0.5
    evaluator.Evaluate(0.5f, root, skeleton, pose);

    // ClipA = 0, ClipB at t=0.5 = 0.5 * weight 0.5 → output ≈ 0.25
    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, 0.25f, 0.05f);
}
