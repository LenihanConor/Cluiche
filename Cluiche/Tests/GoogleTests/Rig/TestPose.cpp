#include <gtest/gtest.h>

#include <DiaRig2D/Pose.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>
#include <DiaRig2D/Testing/PoseComparison.h>
#include <cmath>

using namespace Dia::Rig2D;

TEST(Rig2D_Pose, Constructor_InitializesToBindPose)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);
	Pose pose(skel);

	EXPECT_EQ(pose.GetBoneCount(), 3);
	EXPECT_FLOAT_EQ(pose.GetLocalTransform(0).position.x, 0.0f);
	EXPECT_FLOAT_EQ(pose.GetLocalTransform(1).position.y, 1.0f);
}

TEST(Rig2D_Pose, SetToBindPose_ResetsModifications)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose pose(skel);

	pose.GetLocalTransform(0).rotation = 1.0f;
	pose.SetToBindPose(skel);

	EXPECT_FLOAT_EQ(pose.GetLocalTransform(0).rotation, 0.0f);
}

TEST(Rig2D_Pose, FK_SimpleChain_IdentityRoot)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);
	Pose pose(skel);

	BoneTransform rootTransform;
	Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
	pose.ComputeWorldTransforms(skel, rootTransform, worldTransforms);

	EXPECT_EQ(static_cast<int>(worldTransforms.Size()), 3);

	EXPECT_NEAR(worldTransforms[0].position.x, 0.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[0].position.y, 0.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[1].position.x, 0.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[1].position.y, 1.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[2].position.x, 0.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[2].position.y, 2.0f, 0.001f);
}

TEST(Rig2D_Pose, FK_WithRootOffset)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose pose(skel);

	BoneTransform rootTransform;
	rootTransform.position.Set(10.0f, 20.0f);

	Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
	pose.ComputeWorldTransforms(skel, rootTransform, worldTransforms);

	EXPECT_NEAR(worldTransforms[0].position.x, 10.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[0].position.y, 20.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[1].position.x, 10.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[1].position.y, 21.0f, 0.001f);
}

TEST(Rig2D_Pose, FK_WithRotation_SRTOrder)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose pose(skel);

	float halfPi = 3.14159265f / 2.0f;
	pose.GetLocalTransform(0).rotation = halfPi;

	BoneTransform rootTransform;
	Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
	pose.ComputeWorldTransforms(skel, rootTransform, worldTransforms);

	// Root bone at origin, rotated 90 degrees
	// Child at local (0,1) should end up at world (-1, 0) after 90-degree rotation
	EXPECT_NEAR(worldTransforms[1].position.x, -1.0f, 0.01f);
	EXPECT_NEAR(worldTransforms[1].position.y, 0.0f, 0.01f);
}

TEST(Rig2D_Pose, FK_NegativeScale_Mirroring)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);
	Pose pose(skel);

	pose.GetLocalTransform(0).scale.Set(-1.0f, 1.0f);

	BoneTransform rootTransform;
	Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
	pose.ComputeWorldTransforms(skel, rootTransform, worldTransforms);

	EXPECT_NEAR(worldTransforms[0].scale.x, -1.0f, 0.001f);
}

TEST(Rig2D_Pose, FK_Branching_ChildrenShareParentTransform)
{
	SkeletonDef def = Testing::MakeBranching();
	Skeleton skel(def);
	Pose pose(skel);

	BoneTransform rootTransform;
	rootTransform.position.Set(5.0f, 5.0f);

	Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
	pose.ComputeWorldTransforms(skel, rootTransform, worldTransforms);

	// child_a at local (-1, 1) => world (4, 6)
	EXPECT_NEAR(worldTransforms[1].position.x, 4.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[1].position.y, 6.0f, 0.001f);
	// child_b at local (0, 1) => world (5, 6)
	EXPECT_NEAR(worldTransforms[2].position.x, 5.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[2].position.y, 6.0f, 0.001f);
	// child_c at local (1, 1) => world (6, 6)
	EXPECT_NEAR(worldTransforms[3].position.x, 6.0f, 0.001f);
	EXPECT_NEAR(worldTransforms[3].position.y, 6.0f, 0.001f);
}
