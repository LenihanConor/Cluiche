#include <gtest/gtest.h>

#include <DiaRig2D/SkeletonComponent.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>

using namespace Dia::Rig2D;

TEST(Rig2D_SkeletonComponent, Construction_ValidSkeleton)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	SkeletonComponent comp(def);

	EXPECT_EQ(comp.GetSkeleton().GetBoneCount(), 3);
	EXPECT_EQ(comp.GetCurrentPose().GetBoneCount(), 3);
}

TEST(Rig2D_SkeletonComponent, GetType_ReturnsCorrectId)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	SkeletonComponent comp(def);

	EXPECT_EQ(comp.GetType(), SkeletonComponent::ID);
}

TEST(Rig2D_SkeletonComponent, IsType_MatchesOwnId)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	SkeletonComponent comp(def);

	EXPECT_TRUE(comp.IsType(SkeletonComponent::ID));
	EXPECT_FALSE(comp.IsType(0xDEADBEEF));
}

TEST(Rig2D_SkeletonComponent, MutablePoseAccess)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	SkeletonComponent comp(def);

	comp.GetCurrentPose().GetLocalTransform(0).rotation = 1.5f;
	EXPECT_FLOAT_EQ(comp.GetCurrentPose().GetLocalTransform(0).rotation, 1.5f);
}

TEST(Rig2D_SkeletonComponent, ResetToBindPose_RestoresDefaults)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	SkeletonComponent comp(def);

	comp.GetCurrentPose().GetLocalTransform(0).rotation = 1.5f;
	comp.GetCurrentPose().GetLocalTransform(1).position.Set(99.0f, 99.0f);

	comp.ResetToBindPose();

	EXPECT_FLOAT_EQ(comp.GetCurrentPose().GetLocalTransform(0).rotation, 0.0f);
	EXPECT_FLOAT_EQ(comp.GetCurrentPose().GetLocalTransform(1).position.y, 1.0f);
}

TEST(Rig2D_SkeletonComponent, InheritsIComponent)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	SkeletonComponent comp(def);

	Dia::Core::IComponent* base = &comp;
	EXPECT_TRUE(base->IsType(SkeletonComponent::ID));
}

TEST(Rig2D_SkeletonComponent, Humanoid_FullConstruction)
{
	SkeletonDef def = Testing::MakeHumanoid();
	SkeletonComponent comp(def);

	EXPECT_EQ(comp.GetSkeleton().GetBoneCount(), 13);
	EXPECT_EQ(comp.GetCurrentPose().GetBoneCount(), 13);
}
