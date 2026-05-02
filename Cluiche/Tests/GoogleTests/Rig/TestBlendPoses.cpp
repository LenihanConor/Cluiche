#include <gtest/gtest.h>

#include <DiaRig2D/BlendPoses.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaRig2D/Testing/PoseComparison.h>
#include <cmath>

using namespace Dia::Rig2D;

TEST(Rig2D_BlendPoses, Blend_t0_ReturnsPoseA)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(1).position.Set(0.0f, 1.0f);
	poseB.GetLocalTransform(1).position.Set(0.0f, 3.0f);

	BlendPoses(poseA, poseB, 0.0f, result);

	EXPECT_TRUE(Testing::PosesAreEqual(result, poseA));
}

TEST(Rig2D_BlendPoses, Blend_t1_ReturnsPoseB)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(1).position.Set(0.0f, 1.0f);
	poseB.GetLocalTransform(1).position.Set(0.0f, 3.0f);

	BlendPoses(poseA, poseB, 1.0f, result);

	EXPECT_TRUE(Testing::PosesAreEqual(result, poseB));
}

TEST(Rig2D_BlendPoses, Blend_HalfWay_InterpolatesPosition)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(1).position.Set(0.0f, 0.0f);
	poseB.GetLocalTransform(1).position.Set(0.0f, 4.0f);

	BlendPoses(poseA, poseB, 0.5f, result);

	EXPECT_NEAR(result.GetLocalTransform(1).position.y, 2.0f, 0.001f);
}

TEST(Rig2D_BlendPoses, Blend_ShortestArcRotation)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	float almostPi = 3.0f;
	float almostNegPi = -3.0f;

	poseA.GetLocalTransform(0).rotation = almostPi;
	poseB.GetLocalTransform(0).rotation = almostNegPi;

	BlendPoses(poseA, poseB, 0.5f, result);

	// Shortest arc from 3.0 to -3.0 crosses pi, should blend near pi (or -pi)
	float absResult = fabsf(result.GetLocalTransform(0).rotation);
	EXPECT_NEAR(absResult, 3.14159265f, 0.15f);
}

TEST(Rig2D_BlendPoses, Blend_Scale_Interpolated)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(0).scale.Set(1.0f, 1.0f);
	poseB.GetLocalTransform(0).scale.Set(2.0f, 3.0f);

	BlendPoses(poseA, poseB, 0.5f, result);

	EXPECT_NEAR(result.GetLocalTransform(0).scale.x, 1.5f, 0.001f);
	EXPECT_NEAR(result.GetLocalTransform(0).scale.y, 2.0f, 0.001f);
}

TEST(Rig2D_BlendPoses, Blend_ClampsBelowZero)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(1).position.Set(0.0f, 0.0f);
	poseB.GetLocalTransform(1).position.Set(0.0f, 10.0f);

	BlendPoses(poseA, poseB, -5.0f, result);

	EXPECT_TRUE(Testing::PosesAreEqual(result, poseA));
}

TEST(Rig2D_BlendPoses, Blend_ClampsAboveOne)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose poseA(skel);
	Pose poseB(skel);
	Pose result(skel);

	poseA.GetLocalTransform(1).position.Set(0.0f, 0.0f);
	poseB.GetLocalTransform(1).position.Set(0.0f, 10.0f);

	BlendPoses(poseA, poseB, 5.0f, result);

	EXPECT_TRUE(Testing::PosesAreEqual(result, poseB));
}
