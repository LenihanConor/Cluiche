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
// Helper: compute FK and return world transforms
// ---------------------------------------------------------------------------
static Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>
ComputeFK(const Skeleton& sk, const Pose& pose, const BoneTransform& root)
{
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);
    return out;
}

// ---------------------------------------------------------------------------
// Single bone at origin with identity root — world == local
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, SingleBone_IdentityRoot_WorldEqualsLocal)
{
    SkeletonDef def = Testing::MakeSimpleChain(1);
    Skeleton sk(def);
    Pose pose(sk);

    pose.GetLocalTransform(0).position.Set(3.0f, 7.0f);
    pose.GetLocalTransform(0).rotation = 0.0f;
    pose.GetLocalTransform(0).scale.Set(1.0f, 1.0f);

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    EXPECT_NEAR(out[0].position.X(), 3.0f, 0.001f);
    EXPECT_NEAR(out[0].position.Y(), 7.0f, 0.001f);
    EXPECT_NEAR(out[0].rotation, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 2-bone chain: root at origin pointing up, child at (0,1) local
// With identity root: child world pos = (0,1), rotation = 0
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, TwoBoneChain_NoRotation_ChildWorldAtOffset)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    EXPECT_NEAR(out[0].position.X(), 0.0f, 0.001f);
    EXPECT_NEAR(out[0].position.Y(), 0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.X(), 0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(), 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 2-bone chain: root bone rotated 90 degrees (PI/2)
// Child local pos = (0, 1). After 90-degree rotation of parent:
//   world child pos = (-1, 0)  [rotate (0,1) by 90 deg = (-1, 0)]
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, TwoBoneChain_RootRotated90_ChildWorldPositionCorrect)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    const float kPiOver2 = 1.5707963f;
    pose.GetLocalTransform(0).rotation = kPiOver2;

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    // child local (0,1), parent rotated 90 deg -> world (-1, 0)
    EXPECT_NEAR(out[1].position.X(), -1.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(),  0.0f, 0.001f);
    EXPECT_NEAR(out[1].rotation, kPiOver2, 0.001f);
}

// ---------------------------------------------------------------------------
// Root transform translation propagates to all children
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, RootTranslation_OffsetsPropagateToAllBones)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    root.position.Set(10.0f, 5.0f);

    auto out = ComputeFK(sk, pose, root);

    EXPECT_NEAR(out[0].position.X(), 10.0f, 0.001f);
    EXPECT_NEAR(out[0].position.Y(),  5.0f, 0.001f);
    EXPECT_NEAR(out[1].position.X(), 10.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(),  6.0f, 0.001f);
    EXPECT_NEAR(out[2].position.X(), 10.0f, 0.001f);
    EXPECT_NEAR(out[2].position.Y(),  7.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 3-bone chain: root rotated 90, each child at (0,1) local
// bone0 world: (0,0), rot=90
// bone1 world: (-1,0), rot=90
// bone2 world: (-2,0), rot=90
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, ThreeBoneChain_RootRotated90_AllBonePositionsCorrect)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);
    Pose pose(sk);

    const float kPiOver2 = 1.5707963f;
    pose.GetLocalTransform(0).rotation = kPiOver2;

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    EXPECT_NEAR(out[0].position.X(),  0.0f, 0.001f);
    EXPECT_NEAR(out[0].position.Y(),  0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.X(), -1.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(),  0.0f, 0.001f);
    EXPECT_NEAR(out[2].position.X(), -2.0f, 0.001f);
    EXPECT_NEAR(out[2].position.Y(),  0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Scale: parent scale (2,2), child local (0,1) -> world (0,2)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, ParentScale2_ChildLocalPos1_WorldPosIs2)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    pose.GetLocalTransform(0).scale.Set(2.0f, 2.0f);

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    EXPECT_NEAR(out[1].position.X(), 0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(), 2.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// BlendPoses golden: t=0.5 angle wrap across PI boundary
// Blending -PI+0.1 and PI-0.1 should yield ~PI or ~-PI (short path ~-0.1..0.1)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, BlendPoses_AngleWrap_ShortestPath)
{
    SkeletonDef def = Testing::MakeSimpleChain(1);
    Skeleton sk(def);

    Pose poseA(sk);
    Pose poseB(sk);
    Pose result(sk);

    const float kPi = 3.14159265f;
    poseA.GetLocalTransform(0).rotation = -kPi + 0.1f;
    poseB.GetLocalTransform(0).rotation =  kPi - 0.1f;

    BlendPoses(poseA, poseB, 0.5f, result);

    // diff = (PI-0.1) - (-PI+0.1) = 2*PI - 0.2, normalized = -0.2
    // result = (-PI+0.1) + (-0.2 * 0.5) = -PI + 0.1 - 0.1 = -PI
    float r = result.GetLocalTransform(0).rotation;
    EXPECT_NEAR(std::abs(r), kPi, 0.01f);
}

// ---------------------------------------------------------------------------
// Humanoid skeleton: head bone world position (bind pose, identity root)
// root(0,0)->hips(0,1)->spine(0,2)->chest(0,3)->head(0,3.5)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Golden, Humanoid_BindPose_HeadWorldPosition)
{
    SkeletonDef def = Testing::MakeHumanoid();
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    auto out = ComputeFK(sk, pose, root);

    int headIdx = sk.FindBoneIndex(Dia::Core::StringCRC("head"));
    ASSERT_GE(headIdx, 0);

    EXPECT_NEAR(out[headIdx].position.X(), 0.0f, 0.001f);
    EXPECT_NEAR(out[headIdx].position.Y(), 3.5f, 0.001f);
}
