#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/SkeletonJson.h>
#include <DiaRig2D/SkeletonComponent.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaRig2D/Testing/PoseComparison.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cmath>

using namespace Dia::Rig2D;

// ---------------------------------------------------------------------------
// JSON round-trip: FK on loaded skeleton bit-matches FK on original
// (existing Rig2D_SkeletonJson tests verify parsing; this verifies FK parity)
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Integration, JsonRoundTrip_FKMatchesOriginal)
{
    SkeletonDef original = Testing::MakeSimpleChain(6);

    JsonSkeletonLoader loader;
    char buffer[4096];
    ASSERT_TRUE(loader.Save(original, buffer, sizeof(buffer)));

    SkeletonDef loaded;
    ASSERT_TRUE(loader.Load(buffer, loaded));

    Skeleton skOrig(original);
    Skeleton skLoaded(loaded);

    Pose poseOrig(skOrig);
    Pose poseLoaded(skLoaded);

    for (int i = 0; i < skOrig.GetBoneCount(); ++i)
    {
        poseOrig.GetLocalTransform(i).rotation = static_cast<float>(i) * 0.3f;
        poseLoaded.GetLocalTransform(i).rotation = static_cast<float>(i) * 0.3f;
    }

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> outOrig, outLoaded;
    poseOrig.ComputeWorldTransforms(skOrig, root, outOrig);
    poseLoaded.ComputeWorldTransforms(skLoaded, root, outLoaded);

    for (int i = 0; i < skOrig.GetBoneCount(); ++i)
    {
        EXPECT_NEAR(outOrig[i].position.X(), outLoaded[i].position.X(), 0.001f) << "bone=" << i;
        EXPECT_NEAR(outOrig[i].position.Y(), outLoaded[i].position.Y(), 0.001f) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// Full pipeline: JSON -> Skeleton -> Pose -> FK -> Blend -> no NaN
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Integration, FullPipeline_JsonToBlend_NoNaN)
{
    const char* json = R"({
        "id": "test_rig",
        "bones": [
            {"name": "root",  "parent": null, "position": [0, 0]},
            {"name": "child1","parent": "root","position": [0, 1]},
            {"name": "child2","parent": "child1","position": [0, 1]}
        ]
    })";

    JsonSkeletonLoader loader;
    SkeletonDef def;
    ASSERT_TRUE(loader.Load(json, def));

    Skeleton sk(def);
    Pose poseA(sk), poseB(sk), blended(sk);

    poseA.GetLocalTransform(0).rotation = 0.0f;
    poseA.GetLocalTransform(1).rotation = 0.5f;
    poseB.GetLocalTransform(0).rotation = 1.0f;
    poseB.GetLocalTransform(1).rotation = -0.5f;

    BlendPoses(poseA, poseB, 0.5f, blended);

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    blended.ComputeWorldTransforms(sk, root, out);

    for (int i = 0; i < sk.GetBoneCount(); ++i)
    {
        EXPECT_FALSE(std::isnan(out[i].position.X())) << "bone=" << i;
        EXPECT_FALSE(std::isnan(out[i].position.Y())) << "bone=" << i;
    }
}

// ---------------------------------------------------------------------------
// SkeletonComponent + FK via component accessor
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Integration, SkeletonComponent_FKViaAccessor)
{
    const char* json = R"({
        "id": "comp_rig",
        "bones": [
            {"name": "root",  "parent": null, "position": [0, 0]},
            {"name": "arm",   "parent": "root","position": [1, 0]}
        ]
    })";

    JsonSkeletonLoader loader;
    SkeletonDef def;
    ASSERT_TRUE(loader.Load(json, def));

    SkeletonComponent comp(def);
    comp.GetCurrentPose().GetLocalTransform(0).rotation = 1.5707963f;

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> out;
    comp.GetCurrentPose().ComputeWorldTransforms(comp.GetSkeleton(), root, out);

    EXPECT_NEAR(out[1].position.X(), 0.0f, 0.001f);
    EXPECT_NEAR(out[1].position.Y(), 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// JSON -> Skeleton -> Pose -> blend -> ResetToBindPose -> FK matches bind
// Tests that SkeletonComponent.ResetToBindPose() and FK produce consistent results
// across a full modify-reset-evaluate cycle.
// ---------------------------------------------------------------------------
TEST(DiaRig2D_Integration, ModifyBlendResetCycle_FKMatchesBindAfterReset)
{
    SkeletonDef def = Testing::MakeHumanoid();
    SkeletonComponent comp(def);

    Pose poseB(comp.GetSkeleton());
    for (int i = 0; i < comp.GetSkeleton().GetBoneCount(); ++i)
        poseB.GetLocalTransform(i).rotation = static_cast<float>(i) * 1.0f;

    BlendPoses(comp.GetCurrentPose(), poseB, 0.5f, comp.GetCurrentPose());
    comp.ResetToBindPose();

    BoneTransform root;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> outAfterReset;
    comp.GetCurrentPose().ComputeWorldTransforms(comp.GetSkeleton(), root, outAfterReset);

    Skeleton skRef(def);
    Pose bindRef(skRef);
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> outBind;
    bindRef.ComputeWorldTransforms(skRef, root, outBind);

    for (int i = 0; i < comp.GetSkeleton().GetBoneCount(); ++i)
    {
        EXPECT_NEAR(outAfterReset[i].position.X(), outBind[i].position.X(), 0.001f) << "bone=" << i;
        EXPECT_NEAR(outAfterReset[i].position.Y(), outBind[i].position.Y(), 0.001f) << "bone=" << i;
    }
}
