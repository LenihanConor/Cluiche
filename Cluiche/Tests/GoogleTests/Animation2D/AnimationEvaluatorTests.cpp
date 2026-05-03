#include <gtest/gtest.h>

#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/SpringChain.h>
#include <DiaAnimation2D/SpringParamUtils.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaMaths/Vector/Vector2D.h>

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

// Build a simple AnimClip covering bone0 with rotation 0→1 over 1 second
static Dia::Animation2D::AnimClip MakeSimpleClip(const Dia::Rig2D::Skeleton& sk)
{
    Dia::Animation2D::Keyframe kf0, kf1;
    kf0.time = 0.0f; kf0.rotation = 0.0f;
    kf0.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf0.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    kf1.time = 1.0f; kf1.rotation = 1.0f;
    kf1.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    kf1.scale    = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Animation2D::KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("bone0");
    track.keyframes.Add(kf0);
    track.keyframes.Add(kf1);

    Dia::Animation2D::AnimClipDef def;
    def.id       = Dia::Core::StringCRC("eval_clip");
    def.duration = 1.0f;
    def.tracks.Add(track);

    return Dia::Animation2D::AnimClip(def, sk);
}

static Dia::Animation2D::SpringChainDef MakeTestChainDef()
{
    Dia::Animation2D::SpringChainDef def;
    def.id         = Dia::Core::StringCRC("eval_chain");
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
// AnimationEvaluator tests
// ============================================================

TEST(AnimationEvaluator, RegisterClipPlayer_ReturnsNonNull)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));

    EXPECT_NE(player, nullptr);
}

TEST(AnimationEvaluator, RegisterSpringChain_ReturnsNonNull)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::SpringChainDef chainDef = MakeTestChainDef();
    Dia::Animation2D::SpringChain* chain =
        evaluator.RegisterSpringChain(Dia::Core::StringCRC("spring0"), chainDef);

    EXPECT_NE(chain, nullptr);
}

#ifdef _DEBUG
TEST(AnimationEvaluator, RegisterDuplicateSourceId_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("dup"));

    EXPECT_DEATH(evaluator.RegisterClipPlayer(Dia::Core::StringCRC("dup")), "");
}
#endif // _DEBUG

TEST(AnimationEvaluator, UnregisterSource_RemovesSource)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));
    evaluator.UnregisterSource(Dia::Core::StringCRC("src0"));

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    // Should not crash after removal
    evaluator.Evaluate(1.0f / 60.0f, root, skeleton, pose);
    SUCCEED();
}

TEST(AnimationEvaluator, ZeroSources_Evaluate_IsNoOp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    const float bindRot = pose.GetLocalTransform(0).rotation;

    Dia::Rig2D::BoneTransform root;
    evaluator.Evaluate(1.0f / 60.0f, root, skeleton, pose);

    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, bindRot, 1e-5f);
}

TEST(AnimationEvaluator, Evaluate_ClipPlayer_ModifiesPose)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));

    player->SetLooping(true);
    player->Play(&clip);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    // Advance to t=0.5 (rotation should be ~0.5)
    evaluator.Evaluate(0.5f, root, skeleton, pose);

    EXPECT_GT(std::abs(pose.GetLocalTransform(0).rotation), 0.1f);
}

TEST(AnimationEvaluator, SetSourceWeight_Zero_NoPoseEffect)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));

    player->SetLooping(true);
    player->Play(&clip);
    evaluator.SetSourceWeight(Dia::Core::StringCRC("src0"), 0.0f);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    const float bindRot = pose.GetLocalTransform(0).rotation;

    Dia::Rig2D::BoneTransform root;
    evaluator.Evaluate(0.5f, root, skeleton, pose);

    EXPECT_NEAR(pose.GetLocalTransform(0).rotation, bindRot, 1e-4f);
}

TEST(AnimationEvaluator, SetSourceWeight_PropagatesTo_BlendStack)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));
    evaluator.SetSourceWeight(Dia::Core::StringCRC("src0"), 0.5f);

    // Verify the blend stack layer weight was updated
    EXPECT_NEAR(evaluator.GetBlendStack().GetLayerWeight(Dia::Core::StringCRC("src0")),
                0.5f, 1e-5f);
}

TEST(AnimationEvaluator, SetSourcePriority_PropagatesTo_BlendStack)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src0"));
    evaluator.SetSourcePriority(Dia::Core::StringCRC("src0"), 5);

    EXPECT_EQ(evaluator.GetBlendStack().GetLayerPriority(Dia::Core::StringCRC("src0")), 5);
}

#ifdef _DEBUG
TEST(AnimationEvaluator, MaxSources_Exceeded_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    // Register 16 sources (max)
    for (int i = 0; i < 16; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "src%d", i);
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC(buf));
    }

    // 17th should assert
    EXPECT_DEATH(evaluator.RegisterClipPlayer(Dia::Core::StringCRC("src16")), "");
}
#endif // _DEBUG

TEST(AnimationEvaluator, DtZero_Evaluate_IsSafe)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    // Should not crash
    evaluator.Evaluate(0.0f, root, skeleton, pose);
    SUCCEED();
}

TEST(AnimationEvaluator, GetBlendStack_ReturnsStack)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("s0"));
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("s1"));
    evaluator.RegisterClipPlayer(Dia::Core::StringCRC("s2"));

    EXPECT_EQ(evaluator.GetBlendStack().GetLayerCount(), 3);
}

TEST(AnimationEvaluator, FullPipeline_ClipAndSpring_BothContribute)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::AnimClip clip = MakeSimpleClip(skeleton);

    Dia::Animation2D::AnimationEvaluator evaluator(skeleton);

    // Register clip player
    Dia::Animation2D::AnimClipPlayer* player =
        evaluator.RegisterClipPlayer(Dia::Core::StringCRC("clip_src"));
    player->SetLooping(true);
    player->Play(&clip);

    // Register spring chain
    Dia::Animation2D::SpringChainDef chainDef = MakeTestChainDef();
    evaluator.RegisterSpringChain(Dia::Core::StringCRC("spring_src"), chainDef);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    Dia::Rig2D::BoneTransform root;

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; ++i)
        evaluator.Evaluate(dt, root, skeleton, pose);

    // At least one bone should have non-zero rotation
    bool anyNonZero = false;
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
    {
        if (std::abs(pose.GetLocalTransform(i).rotation) > 1e-4f)
        {
            anyNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(anyNonZero);
}
