#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cmath>

using namespace Dia::Rig2D;

// ---------------------------------------------------------------------------
// 128-bone chain: construct, compute FK, check no NaN/Inf
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, MaxBones128_FKNoNaNOrInf)
{
    SkeletonDef def = Testing::MakeSimpleChain(128);
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    EXPECT_EQ(static_cast<int>(out.Size()), 128);

    for (int i = 0; i < 128; ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X())) << "bone=" << i;
        EXPECT_FALSE(std::isnan(out[i].position.Y())) << "bone=" << i;
        EXPECT_FALSE(std::isinf(out[i].position.X())) << "bone=" << i;
        EXPECT_FALSE(std::isinf(out[i].position.Y())) << "bone=" << i;
        EXPECT_FALSE(std::isnan(out[i].rotation)) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// 128-bone chain: rapid FK+blend cycle (1000 iterations)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, MaxBones128_RapidFKAndBlend_NoNaN)
{
    SkeletonDef def = Testing::MakeSimpleChain(128);
    Skeleton sk(def);
    Pose poseA(sk);
    Pose poseB(sk);
    Pose result(sk);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;

    for (int iter = 0; iter < 1000; ++iter)
    {
        float t = static_cast<float>(iter % 101) / 100.0f;

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            poseA.GetLocalTransform(i).rotation = static_cast<float>(iter) * 0.01f;
            poseB.GetLocalTransform(i).rotation = static_cast<float>(iter) * 0.02f;
        }

        BlendPoses(poseA, poseB, t, result);
        result.ComputeWorldTransforms(sk, root, out);
    }

    for (int i = 0; i < 128; ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X())) << "bone=" << i;
        EXPECT_FALSE(std::isnan(out[i].position.Y())) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// Extreme rotation values: multiple full rotations, no NaN/Inf
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, ExtremeRotations_FKStaysFinite)
{
    SkeletonDef def = Testing::MakeSimpleChain(10);
    Skeleton sk(def);
    Pose pose(sk);

    const float kBigAngle = 1000.0f * 3.14159265f;
    for (int i = 0; i < sk.GetBoneCount(); ++i)
        pose.GetLocalTransform(i).rotation = kBigAngle;

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X()));
        EXPECT_FALSE(std::isnan(out[i].position.Y()));
        EXPECT_FALSE(std::isinf(out[i].position.X()));
        EXPECT_FALSE(std::isinf(out[i].position.Y()));
    }
}

// ---------------------------------------------------------------------------
// Extreme scale values: no NaN/Inf in FK output
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, ExtremeScale_FKStaysFinite)
{
    SkeletonDef def = Testing::MakeSimpleChain(5);
    Skeleton sk(def);
    Pose pose(sk);

    pose.GetLocalTransform(0).scale.Set(1000.0f, 1000.0f);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X()));
        EXPECT_FALSE(std::isnan(out[i].position.Y()));
    }
}

// ---------------------------------------------------------------------------
// Single-bone skeleton: minimum valid input
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, SingleBone_FKAndBlend_NoNaN)
{
    SkeletonDef def = Testing::MakeSimpleChain(1);
    Skeleton sk(def);
    Pose poseA(sk);
    Pose poseB(sk);
    Pose result(sk);

    poseA.GetLocalTransform(0).rotation = 3.14159f;
    poseB.GetLocalTransform(0).rotation = -3.14159f;

    for (int iter = 0; iter < 1000; ++iter)
    {
        float t = static_cast<float>(iter) / 999.0f;
        BlendPoses(poseA, poseB, t, result);

        BoneTransform root;
        Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
        result.ComputeWorldTransforms(sk, root, out);

        EXPECT_FALSE(std::isnan(out[0].rotation));
        EXPECT_FALSE(std::isinf(out[0].rotation));
    }
}

// ---------------------------------------------------------------------------
// Humanoid: 1000 FK evaluations, no NaN
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, Humanoid_1000FKEvals_NoNaN)
{
    SkeletonDef def = Testing::MakeHumanoid();
    Skeleton sk(def);
    Pose pose(sk);
    BoneTransform root;

    for (int iter = 0; iter < 1000; ++iter)
    {
        float angle = static_cast<float>(iter) * 0.001f;
        for (int i = 0; i < sk.GetBoneCount(); ++i)
            pose.GetLocalTransform(i).rotation = angle * static_cast<float>(i + 1);

        Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
        pose.ComputeWorldTransforms(sk, root, out);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            EXPECT_FALSE(std::isnan(out[i].position.X())) << "iter=" << iter;
            EXPECT_FALSE(std::isnan(out[i].position.Y())) << "iter=" << iter;
        }
    }
}

// ---------------------------------------------------------------------------
// Branching skeleton: FK on all branches produces no NaN
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Stress, BranchingSkeleton_FKNoNaN)
{
    SkeletonDef def = Testing::MakeBranching();
    Skeleton sk(def);
    Pose pose(sk);
    BoneTransform root;

    for (int iter = 0; iter < 500; ++iter)
    {
        for (int i = 0; i < sk.GetBoneCount(); ++i)
            pose.GetLocalTransform(i).rotation = static_cast<float>(iter) * 0.01f;

        Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
        pose.ComputeWorldTransforms(sk, root, out);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            EXPECT_FALSE(std::isnan(out[i].position.X()));
            EXPECT_FALSE(std::isnan(out[i].position.Y()));
        }
    }
}
