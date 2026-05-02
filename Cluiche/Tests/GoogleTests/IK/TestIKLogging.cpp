#include <gtest/gtest.h>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>

// These tests exercise code paths that emit DIA_LOG_WARNING in debug builds.
// They verify that the solver completes without crashing and returns the expected
// bool result even when the warning path is hit.

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

TEST(IKLogging, TwoBoneUnreachableTargetCompletesSuccessfully)
{
	// Total arm length = 2.  Target at (100,0) triggers unreachable warning.
	auto def = BuildLimbSkeletonDef(3, 1.0f);

	// Rebuild as +X arm for cleaner geometry
	Dia::Rig2D::SkeletonDef armDef;
	armDef.id = Dia::Core::StringCRC("Arm");
	auto addBone = [&](const char* name, int parent, float lx)
	{
		Dia::Rig2D::Bone b;
		b.name         = Dia::Core::StringCRC(name);
		b.parentIndex  = parent;
		b.localPosition = Dia::Maths::Vector2D(lx, 0.0f);
		b.localRotation = 0.0f;
		b.localScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
		b.length        = 1.0f;
		armDef.bones.Add(b);
	};
	addBone("s", -1, 0.0f);
	addBone("m",  0, 1.0f);
	addBone("e",  1, 1.0f);

	TestRig rig(armDef);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("s");
	chain.endBoneId   = Dia::Core::StringCRC("e");
	solver.RegisterChain(chain);

	const bool result = solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
	                                         Dia::Maths::Vector2D(100.0f, 0.0f));
	EXPECT_TRUE(result);
}

TEST(IKLogging, FABRIKUnreachableTargetCompletesSuccessfully)
{
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone2");
	chain.maxIterations = 5;
	solver.RegisterChain(chain);

	const bool result = solver.SolveFABRIK(Dia::Core::StringCRC("chain"),
	                                        Dia::Maths::Vector2D(100.0f, 0.0f));
	EXPECT_TRUE(result);
}

TEST(IKLogging, LookAtCoincidentTargetCompletesSuccessfully)
{
	auto def = BuildLimbSkeletonDef(2, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Coincident target at bone position (0,0) triggers warning but returns true
	const bool result = solver.SolveLookAt(Dia::Core::StringCRC("bone0"),
	                                        Dia::Maths::Vector2D(0.0f, 0.0f));
	EXPECT_TRUE(result);
}
