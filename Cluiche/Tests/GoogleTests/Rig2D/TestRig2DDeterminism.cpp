#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaRig2D/Testing/PoseComparison.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Rig2D;

// ---------------------------------------------------------------------------
// Identical FK inputs produce bit-identical outputs
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Determinism, FK_SameInputTwice_BitIdenticalOutput)
{
    SkeletonDef def = Testing::MakeHumanoid();
    Skeleton sk(def);

    auto setupPose = [&](Pose& pose)
    {
        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            pose.GetLocalTransform(i).rotation = static_cast<float>(i) * 0.37f;
            pose.GetLocalTransform(i).position.Set(static_cast<float>(i) * 0.1f, static_cast<float>(i) * 0.2f);
            pose.GetLocalTransform(i).scale.Set(1.0f + static_cast<float>(i) * 0.05f, 1.0f);
        }
    };

    Pose pose1(sk);
    setupPose(pose1);
    BoneTransform root1;
    root1.position.Set(1.5f, 2.5f);
    root1.rotation = 0.7f;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out1;
    pose1.ComputeWorldTransforms(sk, root1, out1);

    Pose pose2(sk);
    setupPose(pose2);
    BoneTransform root2;
    root2.position.Set(1.5f, 2.5f);
    root2.rotation = 0.7f;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out2;
    pose2.ComputeWorldTransforms(sk, root2, out2);

    ASSERT_EQ(static_cast<int>(out1.Size()), static_cast<int>(out2.Size()));
    for (int i = 0; i < static_cast<int>(out1.Size()); ++i)
    {
        EXPECT_FLOAT_EQ(out1[i].position.X(), out2[i].position.X()) << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].position.Y(), out2[i].position.Y()) << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].rotation,     out2[i].rotation)     << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].scale.X(),    out2[i].scale.X())    << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].scale.Y(),    out2[i].scale.Y())    << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// Identical blend inputs produce bit-identical outputs
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Determinism, BlendPoses_SameInputTwice_BitIdenticalOutput)
{
    SkeletonDef def = Testing::MakeSimpleChain(8);
    Skeleton sk(def);

    auto setupPoses = [&](Pose& a, Pose& b)
    {
        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            a.GetLocalTransform(i).rotation = static_cast<float>(i) * 0.5f;
            a.GetLocalTransform(i).position.Set(static_cast<float>(i), 0.0f);
            b.GetLocalTransform(i).rotation = static_cast<float>(i) * 0.25f;
            b.GetLocalTransform(i).position.Set(0.0f, static_cast<float>(i));
        }
    };

    Pose a1(sk), b1(sk), result1(sk);
    setupPoses(a1, b1);
    BlendPoses(a1, b1, 0.33f, result1);

    Pose a2(sk), b2(sk), result2(sk);
    setupPoses(a2, b2);
    BlendPoses(a2, b2, 0.33f, result2);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_FLOAT_EQ(result1.GetLocalTransform(i).rotation,     result2.GetLocalTransform(i).rotation)     << "bone=" << i;
        EXPECT_FLOAT_EQ(result1.GetLocalTransform(i).position.X(), result2.GetLocalTransform(i).position.X()) << "bone=" << i;
        EXPECT_FLOAT_EQ(result1.GetLocalTransform(i).position.Y(), result2.GetLocalTransform(i).position.Y()) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// 100-iteration FK+blend pipeline is bit-identical across two runs
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Determinism, FKBlendPipeline_100Iters_BitIdenticalAcrossRuns)
{
    SkeletonDef def = Testing::MakeSimpleChain(16);
    Skeleton sk(def);
    BoneTransform root;

    auto runPipeline = [&]() -> Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>
    {
        Pose poseA(sk), poseB(sk), blended(sk);
        Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> lastOut;

        for (int iter = 0; iter < 100; ++iter)
        {
            float angle = static_cast<float>(iter) * 0.063f;
            for (int i = 0; i < sk.GetBoneCount(); ++i)
            {
                poseA.GetLocalTransform(i).rotation = angle * static_cast<float>(i + 1);
                poseB.GetLocalTransform(i).rotation = angle * static_cast<float>(i + 2) * 0.5f;
            }

            float t = static_cast<float>(iter % 101) / 100.0f;
            BlendPoses(poseA, poseB, t, blended);
            blended.ComputeWorldTransforms(sk, root, lastOut);
        }

        return lastOut;
    };

    auto out1 = runPipeline();
    auto out2 = runPipeline();

    ASSERT_EQ(static_cast<int>(out1.Size()), static_cast<int>(out2.Size()));
    for (int i = 0; i < static_cast<int>(out1.Size()); ++i)
    {
        EXPECT_FLOAT_EQ(out1[i].position.X(), out2[i].position.X()) << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].position.Y(), out2[i].position.Y()) << "bone=" << i;
        EXPECT_FLOAT_EQ(out1[i].rotation,     out2[i].rotation)     << "bone=" << i;
    }
}
