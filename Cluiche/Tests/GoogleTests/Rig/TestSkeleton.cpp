#include <gtest/gtest.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>

using namespace Dia::Rig2D;

TEST(Rig2D_Skeleton, SimpleChain_ValidConstruction)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);

	EXPECT_EQ(skel.GetBoneCount(), 3);
	EXPECT_TRUE(skel.IsValid());
}

TEST(Rig2D_Skeleton, SimpleChain_RootBoneHasZeroLength)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);

	EXPECT_FLOAT_EQ(skel.GetBone(0).length, 0.0f);
}

TEST(Rig2D_Skeleton, SimpleChain_ChildBoneLengthComputed)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);

	EXPECT_FLOAT_EQ(skel.GetBone(1).length, 1.0f);
	EXPECT_FLOAT_EQ(skel.GetBone(2).length, 1.0f);
}

TEST(Rig2D_Skeleton, SimpleChain_ExplicitLengthPreserved)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	def.bones[1].length = 5.0f;
	Skeleton skel(def);

	EXPECT_FLOAT_EQ(skel.GetBone(1).length, 5.0f);
}

TEST(Rig2D_Skeleton, FindBoneIndex_FoundAndNotFound)
{
	SkeletonDef def = Testing::MakeSimpleChain(3);
	Skeleton skel(def);

	EXPECT_EQ(skel.FindBoneIndex(Dia::Core::StringCRC("bone_0")), 0);
	EXPECT_EQ(skel.FindBoneIndex(Dia::Core::StringCRC("bone_2")), 2);
	EXPECT_EQ(skel.FindBoneIndex(Dia::Core::StringCRC("nonexistent")), -1);
}

TEST(Rig2D_Skeleton, Humanoid_ValidConstruction)
{
	SkeletonDef def = Testing::MakeHumanoid();
	Skeleton skel(def);

	EXPECT_EQ(skel.GetBoneCount(), 13);
	EXPECT_TRUE(skel.IsValid());
}

TEST(Rig2D_Skeleton, Branching_ValidConstruction)
{
	SkeletonDef def = Testing::MakeBranching();
	Skeleton skel(def);

	EXPECT_EQ(skel.GetBoneCount(), 6);
	EXPECT_TRUE(skel.IsValid());
}

TEST(Rig2D_Skeleton, GetId_MatchesDef)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	Skeleton skel(def);

	EXPECT_EQ(skel.GetId(), Dia::Core::StringCRC("simple_chain"));
}

TEST(Rig2D_Skeleton, NegativeScale_AcceptedByValidation)
{
	SkeletonDef def = Testing::MakeSimpleChain(2);
	def.bones[0].localScale.Set(-1.0f, 1.0f);
	Skeleton skel(def);

	EXPECT_TRUE(skel.IsValid());
}
