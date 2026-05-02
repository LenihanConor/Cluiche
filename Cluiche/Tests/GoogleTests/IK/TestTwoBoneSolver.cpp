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
	// 3-bone skeleton: root at (0,0), bones along +X, each length 1.0
	Dia::Rig2D::SkeletonDef MakeArmDef()
	{
		Dia::Rig2D::SkeletonDef def;
		def.id = Dia::Core::StringCRC("Arm");

		auto addBone = [&](const char* name, int parent, float localX)
		{
			Dia::Rig2D::Bone b;
			b.name         = Dia::Core::StringCRC(name);
			b.parentIndex  = parent;
			b.localPosition = Dia::Maths::Vector2D(localX, 0.0f);
			b.localRotation = 0.0f;
			b.localScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
			b.length        = 1.0f;
			def.bones.Add(b);
		};

		addBone("shoulder", -1, 0.0f);
		addBone("elbow",     0, 1.0f);
		addBone("wrist",     1, 1.0f);
		return def;
	}

	IKChainDef MakeArmChain()
	{
		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("arm");
		chain.startBoneId = Dia::Core::StringCRC("shoulder");
		chain.endBoneId   = Dia::Core::StringCRC("wrist");
		chain.reachWeight = 1.0f;
		return chain;
	}
}

TEST(TwoBoneSolver, ReachableTarget)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	// Target at (1, 1): directly reachable by bending elbow 90 degrees
	const Dia::Maths::Vector2D target(1.0f, 1.0f);
	const bool result = solver.SolveTwoBone(Dia::Core::StringCRC("arm"), target);
	EXPECT_TRUE(result);

	// Verify end effector reaches target within tolerance
	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2, target, 1e-3f);
}

TEST(TwoBoneSolver, UnreachableTargetExtendsChain)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	// Target at (5, 0): far beyond total chain length of 2
	const bool result = solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(5.0f, 0.0f));
	EXPECT_TRUE(result);

	// Both bones should be pointing toward +X (rotation ~ 0)
	AssertBoneRotation(rig.pose, 0, 0.0f, 1e-3f);
	AssertBoneRotation(rig.pose, 1, 0.0f, 1e-3f);
}

TEST(TwoBoneSolver, ReturnsFalseForUnknownChain)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const bool result = solver.SolveTwoBone(Dia::Core::StringCRC("nonexistent"),
	                                         Dia::Maths::Vector2D(1.0f, 0.0f));
	EXPECT_FALSE(result);
}

TEST(TwoBoneSolver, ReachWeightZeroLeavesUnchanged)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const float origRot0 = rig.pose.GetLocalTransform(0).rotation;
	const float origRot1 = rig.pose.GetLocalTransform(1).rotation;

	IKChainDef chain = MakeArmChain();
	chain.reachWeight = 0.0f;
	solver.RegisterChain(chain);

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(1.0f, 1.0f));

	AssertBoneUnchanged(rig.pose, 0, origRot0);
	AssertBoneUnchanged(rig.pose, 1, origRot1);
}

TEST(TwoBoneSolver, JointLimitsClamped)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain = MakeArmChain();
	// Limit elbow (mid bone) to max -0.1 (prevent full 90-deg bend)
	JointLimitDef midLimit;
	midLimit.minAngle = -0.1f;
	midLimit.maxAngle =  0.1f;
	midLimit.enabled  = true;
	chain.jointLimits.Add(JointLimitDef{}); // start bone: no limit
	chain.jointLimits.Add(midLimit);
	solver.RegisterChain(chain);

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(1.0f, 1.0f));

	const float midRot = rig.pose.GetLocalTransform(1).rotation;
	EXPECT_GE(midRot, midLimit.minAngle - 1e-3f);
	EXPECT_LE(midRot, midLimit.maxAngle + 1e-3f);
}

TEST(TwoBoneSolver, DegenerateZeroDistance)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	// Target exactly at shoulder position — treated as unreachable/extend
	const bool result = solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
	                                         Dia::Maths::Vector2D(0.0f, 0.0f));
	EXPECT_TRUE(result);
}
