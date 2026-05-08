#include <gtest/gtest.h>
#include <cmath>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaIK2D/Testing/IKAssertions.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

namespace
{
	IKChainDef MakeChain(int startBone, int endBone)
	{
		char startBuf[16], endBuf[16];
		snprintf(startBuf, sizeof(startBuf), "bone%d", startBone);
		snprintf(endBuf,   sizeof(endBuf),   "bone%d", endBone);

		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("chain");
		chain.startBoneId = Dia::Core::StringCRC(startBuf);
		chain.endBoneId   = Dia::Core::StringCRC(endBuf);
		chain.reachWeight = 1.0f;
		chain.maxIterations = 50;
		chain.tolerance     = 0.001f;
		return chain;
	}
}

TEST(FABRIKSolver, ThreeJointChainReachesTarget)
{
	// 4 bones, each length 1 along +Y. Target within reach.
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeChain(0, 3));

	const Dia::Maths::Vector2D target(1.5f, 1.5f);
	const bool result = solver.SolveFABRIK(Dia::Core::StringCRC("chain"), target);
	EXPECT_TRUE(result);

	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 3, target, 0.01f);
}

TEST(FABRIKSolver, UnreachableFullExtension)
{
	// BuildLimbSkeletonDef lays bones along +Y. Pointing toward +X requires a -pi/2 rotation
	// of the first bone (local rest is +Y, need to rotate CCW by -pi/2 to point +X).
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeChain(0, 2));

	// Total chain length = 2; target at (10, 0)
	const bool result = solver.SolveFABRIK(Dia::Core::StringCRC("chain"),
	                                        Dia::Maths::Vector2D(10.0f, 0.0f));
	EXPECT_TRUE(result);

	// Verify end effector is at (2, 0) after full extension toward +X
	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2,
	                          Dia::Maths::Vector2D(2.0f, 0.0f), 0.01f);
}

TEST(FABRIKSolver, ReturnsFalseForUnknownChain)
{
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const bool result = solver.SolveFABRIK(Dia::Core::StringCRC("nonexistent"),
	                                        Dia::Maths::Vector2D(1.0f, 0.0f));
	EXPECT_FALSE(result);
}

TEST(FABRIKSolver, ReachWeightZeroLeavesUnchanged)
{
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);

	const float origRot0 = rig.pose.GetLocalTransform(0).rotation;
	const float origRot1 = rig.pose.GetLocalTransform(1).rotation;

	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain = MakeChain(0, 2);
	chain.reachWeight = 0.0f;
	solver.RegisterChain(chain);

	solver.SolveFABRIK(Dia::Core::StringCRC("chain"), Dia::Maths::Vector2D(1.0f, 0.5f));

	AssertBoneUnchanged(rig.pose, 0, origRot0);
	AssertBoneUnchanged(rig.pose, 1, origRot1);
}

TEST(FABRIKSolver, SingleJointDegenerateCase)
{
	// 2-bone skeleton: degenerate 1-joint chain
	auto def = BuildLimbSkeletonDef(2, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeChain(0, 1));

	const bool result = solver.SolveFABRIK(Dia::Core::StringCRC("chain"),
	                                        Dia::Maths::Vector2D(0.0f, 1.0f));
	EXPECT_TRUE(result);
}
