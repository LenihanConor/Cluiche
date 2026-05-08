#include <gtest/gtest.h>
#include <cmath>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaIK2D/Testing/IKAssertions.h>
#include <DiaMaths/Core/MathsDefines.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

namespace
{
	Dia::Rig2D::SkeletonDef MakeArmDef()
	{
		Dia::Rig2D::SkeletonDef def;
		def.id = Dia::Core::StringCRC("Arm");
		auto add = [&](const char* name, int parent, float lx)
		{
			Dia::Rig2D::Bone b;
			b.name          = Dia::Core::StringCRC(name);
			b.parentIndex   = parent;
			b.localPosition = Dia::Maths::Vector2D(lx, 0.0f);
			b.localRotation = 0.0f;
			b.localScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
			b.length        = 1.0f;
			def.bones.Add(b);
		};
		add("shoulder", -1, 0.0f);
		add("elbow",     0, 1.0f);
		add("wrist",     1, 1.0f);
		return def;
	}
}

// ---------------------------------------------------------------------------
// RegisterChain with startBoneId not in skeleton fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, RegisterChain_UnknownStartBone_Dies)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("nonexistent");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;

	EXPECT_DEATH(solver.RegisterChain(chain), "");
}

// ---------------------------------------------------------------------------
// RegisterChain with endBoneId not in skeleton fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, RegisterChain_UnknownEndBone_Dies)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("nonexistent");
	chain.reachWeight = 1.0f;

	EXPECT_DEATH(solver.RegisterChain(chain), "");
}

// ---------------------------------------------------------------------------
// RegisterChain with startIndex == endIndex fires DIA_ASSERT (same bone)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, RegisterChain_SameBone_Dies)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("shoulder");
	chain.reachWeight = 1.0f;

	EXPECT_DEATH(solver.RegisterChain(chain), "");
}

// ---------------------------------------------------------------------------
// SolveTwoBone with chain that has jointCount != 2 fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, SolveTwoBone_WrongJointCount_Dies)
{
	// 5-bone chain has 4 joints — two-bone requires exactly 2 joints
	auto def = BuildLimbSkeletonDef(5, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone4");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	EXPECT_DEATH(solver.SolveTwoBone(Dia::Core::StringCRC("chain"),
	                                  Dia::Maths::Vector2D(1.0f, 0.0f)), "");
}

// ---------------------------------------------------------------------------
// SolveTwoBone before SetRootTransform fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, SolveTwoBone_BeforeSetRootTransform_Dies)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver(rig.skeleton, rig.pose); // no SetRootTransform

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	EXPECT_DEATH(solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
	                                  Dia::Maths::Vector2D(1.0f, 0.0f)), "");
}

// ---------------------------------------------------------------------------
// SolveFABRIK before SetRootTransform fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, SolveFABRIK_BeforeSetRootTransform_Dies)
{
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver(rig.skeleton, rig.pose); // no SetRootTransform

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone2");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	EXPECT_DEATH(solver.SolveFABRIK(Dia::Core::StringCRC("chain"),
	                                  Dia::Maths::Vector2D(1.0f, 0.0f)), "");
}

// ---------------------------------------------------------------------------
// SolveLookAt before SetRootTransform fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, SolveLookAt_BeforeSetRootTransform_Dies)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver(rig.skeleton, rig.pose); // no SetRootTransform

	EXPECT_DEATH(solver.SolveLookAt(0, Dia::Maths::Vector2D(1.0f, 0.0f)), "");
}

// ---------------------------------------------------------------------------
// SolveLookAt with boneIndex out of range fires DIA_ASSERT
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, SolveLookAt_BoneIndexOutOfRange_Dies)
{
	auto def = BuildLimbSkeletonDef(2, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	EXPECT_DEATH(solver.SolveLookAt(-1, Dia::Maths::Vector2D(1.0f, 0.0f)), "");
	EXPECT_DEATH(solver.SolveLookAt(2,  Dia::Maths::Vector2D(1.0f, 0.0f)), "");
}

// ---------------------------------------------------------------------------
// Target exactly at chain reach boundary (d == l1+l2): handled without NaN
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, TwoBone_TargetAtExactReachLimit_NoNaN)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	// Exactly at reach limit: d = 2.0 (= l1+l2)
	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(2.0f, 0.0f));

	for (int i = 0; i < 3; ++i)
	{
		const float rot = rig.pose.GetLocalTransform(i).rotation;
		EXPECT_FALSE(std::isnan(rot));
		EXPECT_FALSE(std::isinf(rot));
	}
}

// ---------------------------------------------------------------------------
// Joint limits min > max (inverted): clamp still does not produce NaN
// (Clamp(v, lo, hi) with lo>hi: returns lo when lo>hi per Clamp impl)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, TwoBone_InvertedJointLimits_NoNaN)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;

	JointLimitDef invertedLimit;
	invertedLimit.minAngle = 0.5f;
	invertedLimit.maxAngle = -0.5f;  // inverted
	invertedLimit.enabled  = true;
	chain.jointLimits.Add(invertedLimit);
	chain.jointLimits.Add(JointLimitDef{});
	solver.RegisterChain(chain);

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(1.0f, 1.0f));

	for (int i = 0; i < 3; ++i)
	{
		const float rot = rig.pose.GetLocalTransform(i).rotation;
		EXPECT_FALSE(std::isnan(rot));
		EXPECT_FALSE(std::isinf(rot));
	}
}

// ---------------------------------------------------------------------------
// UnregisterChain for non-existent chain: no crash, HasChain still false
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Boundary, UnregisterNonExistentChain_NoCrash)
{
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Should not crash
	solver.UnregisterChain(Dia::Core::StringCRC("ghost"));
	EXPECT_FALSE(solver.HasChain(Dia::Core::StringCRC("ghost")));
}
