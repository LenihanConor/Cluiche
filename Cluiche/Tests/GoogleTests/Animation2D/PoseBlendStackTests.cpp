#include <gtest/gtest.h>

#include <DiaAnimation2D/PoseBlendStack.h>
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

// Build a pose with all bone rotations set to a uniform value
static Dia::Rig2D::Pose MakePoseWithRotation(const Dia::Rig2D::Skeleton& sk, float rot)
{
    Dia::Rig2D::Pose p(sk);
    p.SetToBindPose(sk);
    for (int i = 0; i < sk.GetBoneCount(); ++i)
        p.GetLocalTransform(i).rotation = rot;
    return p;
}

// ============================================================
// PoseBlendStack tests
// ============================================================

TEST(PoseBlendStack, SingleLayer_WeightOne_ExactCopy)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose source = MakePoseWithRotation(skeleton, 0.75f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("layer0"), &source, 1.0f, 0);
    stack.Evaluate(skeleton, output);

    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        EXPECT_NEAR(output.GetLocalTransform(i).rotation, 0.75f, 1e-5f) << "bone " << i;
}

TEST(PoseBlendStack, TwoLayers_CascadingLerp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 0.5f, 1);
    stack.Evaluate(skeleton, output);

    // Base rot=0, overlay rot=1 at weight 0.5: expected ≈ 0.5
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        EXPECT_NEAR(output.GetLocalTransform(i).rotation, 0.5f, 1e-4f) << "bone " << i;
}

TEST(PoseBlendStack, ZeroWeightLayer_Skipped)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base   = MakePoseWithRotation(skeleton, 0.3f);
    Dia::Rig2D::Pose zeroL  = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),  &base,  1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("zero"),  &zeroL, 0.0f, 1);
    stack.Evaluate(skeleton, output);

    // Zero-weight layer should have no effect
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        EXPECT_NEAR(output.GetLocalTransform(i).rotation, 0.3f, 1e-4f) << "bone " << i;
}

TEST(PoseBlendStack, PriorityOrder_Respected)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseA = MakePoseWithRotation(skeleton, 0.2f);
    Dia::Rig2D::Pose poseB = MakePoseWithRotation(skeleton, 0.8f);

    // Order A (priority 0) then B (priority 1) at weight 0.3
    // Note: overlay weight must be asymmetric (not 0.5) so cascading lerp
    // produces different results depending on which pose is the base.
    Dia::Rig2D::Pose outputAB(skeleton);
    outputAB.SetToBindPose(skeleton);
    {
        Dia::Animation2D::PoseBlendStack stack;
        stack.AddLayer(Dia::Core::StringCRC("layA"), &poseA, 1.0f, 0);
        stack.AddLayer(Dia::Core::StringCRC("layB"), &poseB, 0.3f, 1);
        stack.Evaluate(skeleton, outputAB);
    }

    // Order B (priority 0) then A (priority 1) at weight 0.3
    Dia::Rig2D::Pose outputBA(skeleton);
    outputBA.SetToBindPose(skeleton);
    {
        Dia::Animation2D::PoseBlendStack stack;
        stack.AddLayer(Dia::Core::StringCRC("layB"), &poseB, 1.0f, 0);
        stack.AddLayer(Dia::Core::StringCRC("layA"), &poseA, 0.3f, 1);
        stack.Evaluate(skeleton, outputBA);
    }

    // Results should differ
    EXPECT_NE(outputAB.GetLocalTransform(0).rotation,
              outputBA.GetLocalTransform(0).rotation);
}

TEST(PoseBlendStack, SamePriority_RegistrationOrder)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseFirst  = MakePoseWithRotation(skeleton, 0.1f);
    Dia::Rig2D::Pose poseSecond = MakePoseWithRotation(skeleton, 0.9f);

    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    // Both priority 0 — first added is base
    stack.AddLayer(Dia::Core::StringCRC("first"),  &poseFirst,  1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("second"), &poseSecond, 0.5f, 0);
    stack.Evaluate(skeleton, output);

    // First (0.1) is base, second (0.9) at 0.5 weight: result ~ 0.5
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 0.5f, 0.05f);
}

TEST(PoseBlendStack, BoneMask_RestrictsBones)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output  = MakePoseWithRotation(skeleton, 0.0f);

    // Mask: only bone0
    Dia::Animation2D::BoneMask mask;
    mask.Add(Dia::Core::StringCRC("bone0"));

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 1.0f, 1, &mask);
    stack.Evaluate(skeleton, output);

    // bone0 should be affected by overlay (rot=1)
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 1.0f, 1e-4f);
    // bone1 should be unchanged from base (rot=0)
    EXPECT_NEAR(output.GetLocalTransform(1).rotation, 0.0f, 1e-4f);
}

TEST(PoseBlendStack, BoneMask_InvalidBoneId_NoCrash)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose source = MakePoseWithRotation(skeleton, 0.5f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::BoneMask mask;
    mask.Add(Dia::Core::StringCRC("invalid_bone_xyz"));

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("layer"), &source, 1.0f, 0, &mask);

    // Should not crash
    stack.Evaluate(skeleton, output);
    SUCCEED();
}

TEST(PoseBlendStack, NullBoneMask_AffectsAllBones)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose source = MakePoseWithRotation(skeleton, 0.6f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("layer"), &source, 1.0f, 0, nullptr);
    stack.Evaluate(skeleton, output);

    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        EXPECT_NEAR(output.GetLocalTransform(i).rotation, 0.6f, 1e-4f) << "bone " << i;
}

TEST(PoseBlendStack, ZeroLayers_NoOp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose output = MakePoseWithRotation(skeleton, 0.42f);

    Dia::Animation2D::PoseBlendStack stack; // empty
    stack.Evaluate(skeleton, output);

    // Output unchanged
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        EXPECT_NEAR(output.GetLocalTransform(i).rotation, 0.42f, 1e-5f) << "bone " << i;
}

#ifdef _DEBUG
TEST(PoseBlendStack, OutputAlias_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose pose(skeleton);
    pose.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("layer"), &pose, 1.0f, 0);

    // outPose == layer source — should assert
    EXPECT_DEATH(stack.Evaluate(skeleton, pose), "");
}
#endif // _DEBUG

TEST(PoseBlendStack, Rotation_ShortestArc)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseA = MakePoseWithRotation(skeleton, -3.0f);
    Dia::Rig2D::Pose poseB = MakePoseWithRotation(skeleton,  3.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("A"), &poseA, 1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("B"), &poseB, 0.5f, 1);
    stack.Evaluate(skeleton, output);

    // Shortest arc: -3 and +3 are close across ±pi; midpoint near ±pi
    const float result = output.GetLocalTransform(0).rotation;
    const float distFromPi   = std::abs(std::abs(result) - 3.14159265f);
    const float distFromZero = std::abs(result);
    EXPECT_LT(distFromPi, distFromZero);
}

TEST(PoseBlendStack, AddLayer_GetLayerCount)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("l0"), &p, 1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("l1"), &p, 1.0f, 1);
    stack.AddLayer(Dia::Core::StringCRC("l2"), &p, 1.0f, 2);

    EXPECT_EQ(stack.GetLayerCount(), 3);
}

TEST(PoseBlendStack, RemoveLayer_ByID)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("layA"), &p, 1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("layB"), &p, 1.0f, 1);

    stack.RemoveLayer(Dia::Core::StringCRC("layA"));

    EXPECT_FALSE(stack.HasLayer(Dia::Core::StringCRC("layA")));
    EXPECT_EQ(stack.GetLayerCount(), 1);
}

TEST(PoseBlendStack, SetLayerWeight_UpdatesBlend)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 0.0f, 1);

    // Before: weight 0 → no effect
    stack.Evaluate(skeleton, output);
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 0.0f, 1e-4f);

    // After: weight 0.5 → partial blend
    stack.SetLayerWeight(Dia::Core::StringCRC("overlay"), 0.5f);
    stack.Evaluate(skeleton, output);
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 0.5f, 1e-4f);
}

TEST(PoseBlendStack, SetLayerPriority_ChangesOrder)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose poseA = MakePoseWithRotation(skeleton, 0.1f);
    Dia::Rig2D::Pose poseB = MakePoseWithRotation(skeleton, 0.9f);

    Dia::Rig2D::Pose outputBefore(skeleton);
    outputBefore.SetToBindPose(skeleton);
    {
        Dia::Animation2D::PoseBlendStack stack;
        stack.AddLayer(Dia::Core::StringCRC("A"), &poseA, 1.0f, 0);
        stack.AddLayer(Dia::Core::StringCRC("B"), &poseB, 0.5f, 1);
        stack.Evaluate(skeleton, outputBefore);
    }

    Dia::Rig2D::Pose outputAfter(skeleton);
    outputAfter.SetToBindPose(skeleton);
    {
        Dia::Animation2D::PoseBlendStack stack;
        stack.AddLayer(Dia::Core::StringCRC("A"), &poseA, 1.0f, 0);
        stack.AddLayer(Dia::Core::StringCRC("B"), &poseB, 0.5f, 1);
        // Swap priorities
        stack.SetLayerPriority(Dia::Core::StringCRC("A"), 1);
        stack.SetLayerPriority(Dia::Core::StringCRC("B"), 0);
        stack.Evaluate(skeleton, outputAfter);
    }

    EXPECT_NE(outputBefore.GetLocalTransform(0).rotation,
              outputAfter.GetLocalTransform(0).rotation);
}

TEST(PoseBlendStack, HasLayer_TrueFalse)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("known"), &p, 1.0f, 0);

    EXPECT_TRUE(stack.HasLayer(Dia::Core::StringCRC("known")));
    EXPECT_FALSE(stack.HasLayer(Dia::Core::StringCRC("unknown")));
}

TEST(PoseBlendStack, Clear_EmptiesStack)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("l0"), &p, 1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("l1"), &p, 1.0f, 1);
    stack.Clear();

    EXPECT_EQ(stack.GetLayerCount(), 0);
}

#ifdef _DEBUG
TEST(PoseBlendStack, DuplicateLayerId_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("dup"), &p, 1.0f, 0);

    EXPECT_DEATH(stack.AddLayer(Dia::Core::StringCRC("dup"), &p, 1.0f, 1), "");
}
#endif // _DEBUG

TEST(PoseBlendStack, SetLayerWeight_OutOfRange_Clamps)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 1.0f, 1);
    stack.SetLayerWeight(Dia::Core::StringCRC("overlay"), 1.5f); // should clamp to 1.0

    stack.Evaluate(skeleton, output);

    // With weight clamped to 1.0, overlay fully wins
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 1.0f, 1e-4f);
}

TEST(PoseBlendStack, ThreeLayers_CascadingLerp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    // Layer0 (priority 0): rot=0, weight=1
    // Layer1 (priority 1): rot=1, weight=0.5  → result so far = 0.5
    // Layer2 (priority 2): rot=0, weight=0.5  → result = lerp(0.5, 0, 0.5) = 0.25
    Dia::Rig2D::Pose p0 = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose p1 = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose p2 = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("L0"), &p0, 1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("L1"), &p1, 0.5f, 1);
    stack.AddLayer(Dia::Core::StringCRC("L2"), &p2, 0.5f, 2);
    stack.Evaluate(skeleton, output);

    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 0.25f, 1e-4f);
}

TEST(PoseBlendStack, BoneMask_CopiedAtAddLayer_ExternalChangeIgnored)
{
    // Verifies the lazy-copy fix: mutating a BoneMask after AddLayer should not
    // affect the blend result (the stack owns a snapshot, not the pointer).
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::BoneMask mask;
    mask.Add(Dia::Core::StringCRC("bone0")); // only bone0

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 1.0f, 1, &mask);

    // Mutate the original mask after AddLayer
    mask.Add(Dia::Core::StringCRC("bone1"));
    mask.Add(Dia::Core::StringCRC("bone2"));

    stack.Evaluate(skeleton, output);

    // The stack should have used the snapshot (only bone0 masked)
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 1.0f, 1e-4f); // masked → overlay
    EXPECT_NEAR(output.GetLocalTransform(1).rotation, 0.0f, 1e-4f); // NOT in snapshot → base
}

#ifdef _DEBUG
TEST(PoseBlendStack, MaxLayers_33rd_Asserts)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    for (int i = 0; i < 32; ++i)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "l%d", i);
        stack.AddLayer(Dia::Core::StringCRC(buf), &p, 1.0f, i);
    }

    EXPECT_DEATH(stack.AddLayer(Dia::Core::StringCRC("overflow"), &p, 1.0f, 32), "");
}
#endif // _DEBUG

TEST(PoseBlendStack, Position_BlendedCorrectly)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base(skeleton);
    base.SetToBindPose(skeleton);
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        base.GetLocalTransform(i).position = Dia::Maths::Vector2D(0.0f, 0.0f);

    Dia::Rig2D::Pose overlay(skeleton);
    overlay.SetToBindPose(skeleton);
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        overlay.GetLocalTransform(i).position = Dia::Maths::Vector2D(4.0f, 6.0f);

    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 0.5f, 1);
    stack.Evaluate(skeleton, output);

    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
    {
        EXPECT_NEAR(output.GetLocalTransform(i).position.X(), 2.0f, 1e-4f) << "bone " << i;
        EXPECT_NEAR(output.GetLocalTransform(i).position.Y(), 3.0f, 1e-4f) << "bone " << i;
    }
}

TEST(PoseBlendStack, Scale_BlendedCorrectly)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base(skeleton);
    base.SetToBindPose(skeleton);
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        base.GetLocalTransform(i).scale = Dia::Maths::Vector2D(1.0f, 1.0f);

    Dia::Rig2D::Pose overlay(skeleton);
    overlay.SetToBindPose(skeleton);
    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
        overlay.GetLocalTransform(i).scale = Dia::Maths::Vector2D(3.0f, 5.0f);

    Dia::Rig2D::Pose output(skeleton);
    output.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 0.5f, 1);
    stack.Evaluate(skeleton, output);

    for (int i = 0; i < skeleton.GetBoneCount(); ++i)
    {
        EXPECT_NEAR(output.GetLocalTransform(i).scale.X(), 2.0f, 1e-4f) << "bone " << i;
        EXPECT_NEAR(output.GetLocalTransform(i).scale.Y(), 3.0f, 1e-4f) << "bone " << i;
    }
}

TEST(PoseBlendStack, RemoveNonexistentLayer_SilentNoOp)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose p(skeleton);
    p.SetToBindPose(skeleton);

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("present"), &p, 1.0f, 0);
    stack.RemoveLayer(Dia::Core::StringCRC("absent")); // should not crash
    EXPECT_EQ(stack.GetLayerCount(), 1);
}

TEST(PoseBlendStack, BoneMask_PartialSkeleton)
{
    Dia::Rig2D::SkeletonDef skelDef = MakeTestSkelDef();
    Dia::Rig2D::Skeleton skeleton(skelDef);

    Dia::Rig2D::Pose base    = MakePoseWithRotation(skeleton, 0.0f);
    Dia::Rig2D::Pose overlay = MakePoseWithRotation(skeleton, 1.0f);
    Dia::Rig2D::Pose output  = MakePoseWithRotation(skeleton, 0.0f);

    // Mask bones 0 and 1 only
    Dia::Animation2D::BoneMask mask;
    mask.Add(Dia::Core::StringCRC("bone0"));
    mask.Add(Dia::Core::StringCRC("bone1"));

    Dia::Animation2D::PoseBlendStack stack;
    stack.AddLayer(Dia::Core::StringCRC("base"),    &base,    1.0f, 0);
    stack.AddLayer(Dia::Core::StringCRC("overlay"), &overlay, 1.0f, 1, &mask);
    stack.Evaluate(skeleton, output);

    // Bones 0,1 should be at 1.0
    EXPECT_NEAR(output.GetLocalTransform(0).rotation, 1.0f, 1e-4f);
    EXPECT_NEAR(output.GetLocalTransform(1).rotation, 1.0f, 1e-4f);

    // Bones 2,3,4 should be unchanged (0.0)
    EXPECT_NEAR(output.GetLocalTransform(2).rotation, 0.0f, 1e-4f);
    EXPECT_NEAR(output.GetLocalTransform(3).rotation, 0.0f, 1e-4f);
    EXPECT_NEAR(output.GetLocalTransform(4).rotation, 0.0f, 1e-4f);
}
