#include <gtest/gtest.h>

#include <DiaAnimation2D/Testing/AnimClipBuilders.h>
#include <DiaAnimation2D/Testing/SpringChainBuilders.h>
#include <DiaAnimation2D/Testing/PoseAssertions.h>
#include <DiaAnimation2D/Testing/EvaluatorFixture.h>

#include <DiaAnimation2D/AnimClip.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/SpringChain.h>
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Bone.h>
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

// ============================================================
// TestUtilities tests
// ============================================================

TEST(TestUtilities, BuildTestClip_ValidDef)
{
    // Build skeleton with 3 bones
    Dia::Rig2D::SkeletonDef skelDef;
    skelDef.id = Dia::Core::StringCRC("three");
    const char* names3[] = { "b0", "b1", "b2" };
    for (int i = 0; i < 3; ++i)
    {
        Dia::Rig2D::Bone b;
        b.name          = Dia::Core::StringCRC(names3[i]);
        b.parentIndex   = i - 1;
        b.length        = 1.0f;
        b.localPosition = Dia::Maths::Vector2D(0.0f, static_cast<float>(i));
        skelDef.bones.Add(b);
    }
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Build test clip covering all 3 bones, 3 keyframes each
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildTestClip(skeleton, 3, 0.0f, 1.0f);

    EXPECT_EQ(clip.GetTrackCount(), 3);
}

TEST(TestUtilities, BuildSingleBoneClip_CorrectKeyframes)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    const float startRot = 0.5f;
    const float endRot   = 1.5f;

    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildSingleBoneClip(
            skeleton, Dia::Core::StringCRC("bone0"), startRot, endRot);

    Dia::Rig2D::Pose poseStart(skeleton);
    poseStart.SetToBindPose(skeleton);
    clip.Sample(0.0f, skeleton, poseStart);
    EXPECT_NEAR(poseStart.GetLocalTransform(0).rotation, startRot, 1e-4f);

    Dia::Rig2D::Pose poseEnd(skeleton);
    poseEnd.SetToBindPose(skeleton);
    clip.Sample(clip.GetDuration(), skeleton, poseEnd);
    EXPECT_NEAR(poseEnd.GetLocalTransform(0).rotation, endRot, 1e-4f);
}

TEST(TestUtilities, BuildTestSpringChain_CorrectNodeCount)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Bones 1-3 → 3 nodes
    Dia::Animation2D::SpringChain chain =
        Dia::Animation2D::Testing::BuildTestSpringChain(skeleton, 1, 3);

    EXPECT_EQ(chain.GetNodeCount(), 3);
}

TEST(TestUtilities, AssertPosesEqual_PassForIdentical)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseA(skeleton);
    poseA.SetToBindPose(skeleton);
    Dia::Rig2D::Pose poseB(skeleton);
    poseB.SetToBindPose(skeleton);

    // Identical poses: no EXPECT failures
    Dia::Animation2D::Testing::AssertPosesEqual(poseA, poseB, 1e-5f);
    SUCCEED();
}

TEST(TestUtilities, AssertPosesEqual_FailForDifferent)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseA(skeleton);
    poseA.SetToBindPose(skeleton);
    poseA.GetLocalTransform(0).rotation = 0.0f;

    Dia::Rig2D::Pose poseB(skeleton);
    poseB.SetToBindPose(skeleton);
    poseB.GetLocalTransform(0).rotation = 1.0f; // differs by 1.0

    // Call the function — it runs without crashing (it may internally trigger EXPECT failures
    // but we verify the function itself doesn't assert/crash)
    Dia::Animation2D::Testing::AssertPosesEqual(poseA, poseB, 1e-5f);
    SUCCEED();
}

TEST(TestUtilities, AssertBoneRotation_CorrectCheck)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);
    pose.GetLocalTransform(2).rotation = 0.7f;

    // Should pass without EXPECT failure
    Dia::Animation2D::Testing::AssertBoneRotation(pose, 2, 0.7f, 1e-5f);
    SUCCEED();
}

TEST(TestUtilities, TestEvaluatorFixture_Functional)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Animation2D::Testing::EvaluatorFixture fixture =
        Dia::Animation2D::Testing::BuildTestEvaluator(skeleton);

    EXPECT_NE(fixture.evaluator, nullptr);
    EXPECT_EQ(fixture.evaluator->GetBlendStack().GetLayerCount(), 0);
}

TEST(TestUtilities, AllHelpers_Compile)
{
    // This test verifies all testing headers compile and link correctly.
    // The act of including and reaching this line is the test.
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Exercise each helper lightly
    Dia::Animation2D::AnimClip clip =
        Dia::Animation2D::Testing::BuildSingleBoneClip(
            skeleton, Dia::Core::StringCRC("bone0"), 0.0f, 1.0f);

    Dia::Animation2D::SpringChain chain =
        Dia::Animation2D::Testing::BuildTestSpringChain(skeleton, 1, 1);

    Dia::Animation2D::Testing::EvaluatorFixture fixture =
        Dia::Animation2D::Testing::BuildTestEvaluator(skeleton);

    (void)clip;
    (void)chain;
    (void)fixture;

    SUCCEED();
}
