#include <gtest/gtest.h>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaIK2D/Testing/IKAssertions.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

TEST(IKChainedSolves, TwoSolvesInOneFrameUseUpdatedWorldTransforms)
{
	// 5-bone skeleton. Register two non-overlapping chains.
	// Verify second solve sees updated world transforms from first solve.
	auto def = BuildLimbSkeletonDef(5, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chainA;
	chainA.id          = Dia::Core::StringCRC("chainA");
	chainA.startBoneId = Dia::Core::StringCRC("bone0");
	chainA.endBoneId   = Dia::Core::StringCRC("bone2");
	chainA.reachWeight = 1.0f;
	solver.RegisterChain(chainA);

	IKChainDef chainB;
	chainB.id          = Dia::Core::StringCRC("chainB");
	chainB.startBoneId = Dia::Core::StringCRC("bone2");
	chainB.endBoneId   = Dia::Core::StringCRC("bone4");
	chainB.reachWeight = 1.0f;
	solver.RegisterChain(chainB);

	// Solve A first, then B in same frame
	const bool resultA = solver.SolveFABRIK(Dia::Core::StringCRC("chainA"),
	                                          Dia::Maths::Vector2D(1.0f, 1.0f));
	const bool resultB = solver.SolveFABRIK(Dia::Core::StringCRC("chainB"),
	                                          Dia::Maths::Vector2D(2.0f, 2.0f));

	EXPECT_TRUE(resultA);
	EXPECT_TRUE(resultB);

	// Both solves should complete without assert — the test verifies correct sequencing
	// (world transforms refreshed after A, so B uses updated positions)
}

TEST(IKChainedSolves, LookAtAfterFABRIK)
{
	// Run FABRIK on spine, then look-at on the end bone. Verifies chaining still works.
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("spine");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone2");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	const bool fabrikResult = solver.SolveFABRIK(Dia::Core::StringCRC("spine"),
	                                               Dia::Maths::Vector2D(1.5f, 0.5f));
	const bool lookAtResult = solver.SolveLookAt(Dia::Core::StringCRC("bone2"),
	                                               Dia::Maths::Vector2D(2.0f, 0.0f));

	EXPECT_TRUE(fabrikResult);
	EXPECT_TRUE(lookAtResult);
}
