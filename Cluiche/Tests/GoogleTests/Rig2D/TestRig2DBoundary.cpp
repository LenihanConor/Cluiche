#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/Bone.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cmath>

using namespace Dia::Rig2D;

// ---------------------------------------------------------------------------
// Zero rotation: FK still produces valid output
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, ZeroRotation_FKValid)
{
    SkeletonDef def = Testing::MakeSimpleChain(4);
    Skeleton sk(def);
    Pose pose(sk);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    EXPECT_EQ(static_cast<int>(out.Size()), 4);
    for (int i = 0; i < 4; ++i)
        EXPECT_FALSE(std::isnan(out[i].position.X()));
}

// ---------------------------------------------------------------------------
// Negative scale: FK remains finite
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, NegativeScale_FKFinite)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);
    Pose pose(sk);

    pose.GetLocalTransform(0).scale.Set(-1.0f, -1.0f);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X()));
        EXPECT_FALSE(std::isinf(out[i].position.X()));
    }
}

// ---------------------------------------------------------------------------
// Near-zero scale: FK remains finite (not div-by-zero)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, NearZeroScaleX_FKFinite)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    pose.GetLocalTransform(0).scale.Set(1e-7f, 1.0f);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    EXPECT_FALSE(std::isnan(out[1].position.X()));
    EXPECT_FALSE(std::isinf(out[1].position.X()));
}

// ---------------------------------------------------------------------------
// Max metadata per bone (8 entries): set and find all
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, MaxMetadataPerBone_AllEntriesAccessible)
{
    Bone bone;
    for (int i = 0; i < static_cast<int>(kMaxMetadataPerBone); ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        bone.SetMetadata(Dia::Core::StringCRC(key), MetadataValue::FromInt(i));
    }

    EXPECT_EQ(bone.metadata.Size(), kMaxMetadataPerBone);

    for (int i = 0; i < static_cast<int>(kMaxMetadataPerBone); ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        const MetadataValue* v = bone.FindMetadata(Dia::Core::StringCRC(key));
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(v->intVal, i);
    }
}

// ---------------------------------------------------------------------------
// FindBoneIndex on empty name (CRC of "")
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, FindBoneIndex_EmptyStringCRC_ReturnsMinusOne)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);
    EXPECT_EQ(sk.FindBoneIndex(Dia::Core::StringCRC("")), -1);
}

// ---------------------------------------------------------------------------
// Blend at exact t=0.5: scale is linear midpoint
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, BlendAtHalf_ScaleMidpoint)
{
    SkeletonDef def = Testing::MakeSimpleChain(1);
    Skeleton sk(def);

    Pose poseA(sk), poseB(sk), result(sk);
    poseA.GetLocalTransform(0).scale.Set(1.0f, 1.0f);
    poseB.GetLocalTransform(0).scale.Set(3.0f, 5.0f);

    BlendPoses(poseA, poseB, 0.5f, result);
    EXPECT_NEAR(result.GetLocalTransform(0).scale.X(), 2.0f, 0.001f);
    EXPECT_NEAR(result.GetLocalTransform(0).scale.Y(), 3.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Root rotation PI: child at (0,1) should be at (-0, -1) world
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, RootRotationPI_ChildFlipsDown)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    const float kPi = 3.14159265f;
    pose.GetLocalTransform(0).rotation = kPi;

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    pose.ComputeWorldTransforms(sk, root, out);

    // child local (0,1) rotated PI = (0, -1) approximately
    EXPECT_NEAR(out[1].position.X(),  0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(), -1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Pose::GetLocalTransform out-of-range: death test
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, GetLocalTransform_OutOfRange_Dies)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    Pose pose(sk);

    EXPECT_DEATH(pose.GetLocalTransform(-1), "");
    EXPECT_DEATH(pose.GetLocalTransform(2), "");
}

// ---------------------------------------------------------------------------
// Skeleton::GetBone out-of-range: death test
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, GetBone_OutOfRange_Dies)
{
    SkeletonDef def = Testing::MakeSimpleChain(3);
    Skeleton sk(def);

    EXPECT_DEATH(sk.GetBone(-1), "");
    EXPECT_DEATH(sk.GetBone(3), "");
}

// ---------------------------------------------------------------------------
// Skeleton::GetRequiredBoneIndex with missing bone fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Boundary, GetRequiredBoneIndex_MissingBone_Dies)
{
    SkeletonDef def = Testing::MakeSimpleChain(2);
    Skeleton sk(def);
    EXPECT_DEATH(sk.GetRequiredBoneIndex(Dia::Core::StringCRC("nonexistent")), "");
}
