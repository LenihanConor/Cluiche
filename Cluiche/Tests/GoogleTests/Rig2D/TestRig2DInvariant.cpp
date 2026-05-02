#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaRig2D/Testing/PoseComparison.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cmath>
#include <random>

using namespace Dia::Rig2D;

// ---------------------------------------------------------------------------
// BlendPoses(a, b, 0) == a  over 500 random inputs
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, BlendAtT0_EqualsA)
{
    SkeletonDef def = Testing::MakeSimpleChain(4);
    Skeleton sk(def);

    std::mt19937 rng(0xABCD1234);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    for (int iter = 0; iter < 500; ++iter)
    {
        Pose poseA(sk);
        Pose poseB(sk);
        Pose result(sk);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            poseA.GetLocalTransform(i).rotation = dist(rng);
            poseA.GetLocalTransform(i).position.Set(dist(rng), dist(rng));
            poseB.GetLocalTransform(i).rotation = dist(rng);
            poseB.GetLocalTransform(i).position.Set(dist(rng), dist(rng));
        }

        BlendPoses(poseA, poseB, 0.0f, result);
        EXPECT_TRUE(Testing::PosesAreEqual(result, poseA, 0.001f)) << "iter=" << iter;
    }
}

// ---------------------------------------------------------------------------
// BlendPoses(a, b, 1) == b  over 500 random inputs
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, BlendAtT1_EqualsB)
{
    SkeletonDef def = Testing::MakeSimpleChain(4);
    Skeleton sk(def);

    std::mt19937 rng(0xDEADBEEF);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    for (int iter = 0; iter < 500; ++iter)
    {
        Pose poseA(sk);
        Pose poseB(sk);
        Pose result(sk);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            poseA.GetLocalTransform(i).rotation = dist(rng);
            poseB.GetLocalTransform(i).rotation = dist(rng);
            poseA.GetLocalTransform(i).position.Set(dist(rng), dist(rng));
            poseB.GetLocalTransform(i).position.Set(dist(rng), dist(rng));
        }

        BlendPoses(poseA, poseB, 1.0f, result);
        EXPECT_TRUE(Testing::PosesAreEqual(result, poseB, 0.001f)) << "iter=" << iter;
    }
}

// ---------------------------------------------------------------------------
// BlendPoses(a, a, t) == a  for any t in [0,1]
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, BlendSamePoses_AlwaysReturnsA)
{
    SkeletonDef def = Testing::MakeSimpleChain(4);
    Skeleton sk(def);

    std::mt19937 rng(0x12345678);
    std::uniform_real_distribution<float> distV(-5.0f, 5.0f);
    std::uniform_real_distribution<float> distT(0.0f, 1.0f);

    for (int iter = 0; iter < 500; ++iter)
    {
        Pose poseA(sk);
        Pose result(sk);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            poseA.GetLocalTransform(i).rotation = distV(rng);
            poseA.GetLocalTransform(i).position.Set(distV(rng), distV(rng));
        }

        float t = distT(rng);
        BlendPoses(poseA, poseA, t, result);
        EXPECT_TRUE(Testing::PosesAreEqual(result, poseA, 0.001f)) << "iter=" << iter << " t=" << t;
    }
}

// ---------------------------------------------------------------------------
// FK: identity root, all bones identity -> world positions equal cumulative local
// For simple chain each bone at (0,1) local: bone[i] world = (0, i)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, FK_IdentityChain_WorldPositionsAccumulate)
{
    const int kBones = 6;
    SkeletonDef def = Testing::MakeSimpleChain(kBones);
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    for (int i = 0; i < kBones; ++i)
    {
        EXPECT_NEAR(out[i].position.X(), 0.0f, 0.001f) << "bone=" << i;
        EXPECT_NEAR(out[i].position.Y(), static_cast<float>(i), 0.001f) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// FK: full 360 rotation returns to same position
// Rotating root bone by 2*PI should leave child world positions unchanged
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, FK_FullRotation_SameAsNoRotation)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);

    Pose poseBase(sk);
    Pose poseFull(sk);

    const float k2Pi = 6.2831853f;
    for (int i = 0; i < sk.GetBoneCount(); ++i)
        poseFull.GetLocalTransform(i).rotation = k2Pi;

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> outBase, outFull;
    poseBase.ComputeWorldTransforms(sk, root, outBase);
    poseFull.ComputeWorldTransforms(sk, root, outFull);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_NEAR(outBase[i].position.X(), outFull[i].position.X(), 0.001f);
        EXPECT_NEAR(outBase[i].position.Y(), outFull[i].position.Y(), 0.001f);
    }
}

// ---------------------------------------------------------------------------
// BlendPoses result is within [min, max] range for each channel
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Invariant, BlendResult_WithinBoundsOfInputs)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);

    std::mt19937 rng(0xFEEDFACE);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> distT(0.0f, 1.0f);

    for (int iter = 0; iter < 500; ++iter)
    {
        Pose poseA(sk);
        Pose poseB(sk);
        Pose result(sk);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            float px = dist(rng), py = dist(rng);
            float qx = dist(rng), qy = dist(rng);
            poseA.GetLocalTransform(i).position.Set(px, py);
            poseA.GetLocalTransform(i).scale.Set(1.0f, 1.0f);
            poseB.GetLocalTransform(i).position.Set(qx, qy);
            poseB.GetLocalTransform(i).scale.Set(1.0f, 1.0f);
        }

        float t = distT(rng);
        BlendPoses(poseA, poseB, t, result);

        for (int i = 0; i < sk.GetBoneCount(); ++i)
        {
            float ax = poseA.GetLocalTransform(i).position.X();
            float bx = poseB.GetLocalTransform(i).position.X();
            float rx = result.GetLocalTransform(i).position.X();
            float lo = std::min(ax, bx) - 0.001f;
            float hi = std::max(ax, bx) + 0.001f;
            EXPECT_GE(rx, lo) << "iter=" << iter;
            EXPECT_LE(rx, hi) << "iter=" << iter;
        }
    }
}
