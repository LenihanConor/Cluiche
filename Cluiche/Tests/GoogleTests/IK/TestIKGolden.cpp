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
	// 3-bone arm along +X, each bone length 1
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

// ---------------------------------------------------------------------------
// Two-bone +X arm, target (1,1): law of cosines gives exact 90-deg elbow bend
// d=sqrt(2), l1=l2=1, cosAngle=0 → midLocalAngle = PI/2
// startLocalAngle = 0 (shoulder stays aligned to baseline)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, TwoBone_SquareTarget_ExactElbowAngle)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(1.0f, 1.0f));

	// elbow (bone 1) local rotation: PI/2
	AssertBoneRotation(rig.pose, 1, Dia::Maths::PI_HALF, 0.005f);
	// end effector must reach (1,1)
	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2,
	                          Dia::Maths::Vector2D(1.0f, 1.0f), 0.005f);
}

// ---------------------------------------------------------------------------
// Two-bone +X arm fully extended: target along +X at max reach
// Both bones aligned → shoulder local = 0, elbow local = 0
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, TwoBone_FullExtension_BothBonesAligned)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(2.0f, 0.0f));

	AssertBoneRotation(rig.pose, 0, 0.0f, 0.005f);
	AssertBoneRotation(rig.pose, 1, 0.0f, 0.005f);
	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2,
	                          Dia::Maths::Vector2D(2.0f, 0.0f), 0.005f);
}

// ---------------------------------------------------------------------------
// Two-bone arm, target on -X axis: unreachable direction
// Chain fully extends toward (-5,0) — shoulder local ≈ PI, elbow local ≈ 0
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, TwoBone_UnreachableNegativeX_ExtendsCorrectly)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain());

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(-5.0f, 0.0f));

	// End effector at (-2,0) — full extension in -X direction
	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2,
	                          Dia::Maths::Vector2D(-2.0f, 0.0f), 0.01f);
}

// ---------------------------------------------------------------------------
// FABRIK 3-joint (+Y), target at (0, 3): straight up, full reach
// All 3 segments along +Y → end effector at (0,3)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, FABRIK_StraightUp_EndEffectorAtTopOfChain)
{
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("spine");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone3");
	chain.reachWeight = 1.0f;
	chain.maxIterations = 50;
	chain.tolerance = 0.001f;
	solver.RegisterChain(chain);

	solver.SolveFABRIK(Dia::Core::StringCRC("spine"), Dia::Maths::Vector2D(0.0f, 3.0f));

	AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 3,
	                          Dia::Maths::Vector2D(0.0f, 3.0f), 0.01f);
}

// ---------------------------------------------------------------------------
// Look-at: bone at origin, target (1,0) — world angle 0 = local angle 0
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, LookAt_TargetRight_LocalAngleZero)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	solver.SolveLookAt(0, Dia::Maths::Vector2D(1.0f, 0.0f));
	AssertBoneRotation(rig.pose, 0, 0.0f, 0.005f);
}

// ---------------------------------------------------------------------------
// Look-at: bone at origin, target (0,1) — world angle PI/2 = local angle PI/2
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, LookAt_TargetUp_LocalAngleHalfPi)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	solver.SolveLookAt(0, Dia::Maths::Vector2D(0.0f, 1.0f));
	AssertBoneRotation(rig.pose, 0, Dia::Maths::PI_HALF, 0.005f);
}

// ---------------------------------------------------------------------------
// Look-at: bone at origin, target (0,-1) — world angle -PI/2 = local angle -PI/2
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, LookAt_TargetDown_LocalAngleMinusHalfPi)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	solver.SolveLookAt(0, Dia::Maths::Vector2D(0.0f, -1.0f));
	AssertBoneRotation(rig.pose, 0, -Dia::Maths::PI_HALF, 0.005f);
}

// ---------------------------------------------------------------------------
// Reach weight 0.5: blended rotation is midpoint between snapshot and IK result
// For look-at target at (0,1) with weight=0.5: result should be ~PI/4
// (snapshot=0, IK=PI/2, blend=PI/4)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, LookAt_ReachWeightHalf_BlendedAngle)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	solver.SolveLookAt(0, Dia::Maths::Vector2D(0.0f, 1.0f), 0.5f);
	// snapshot=0, IK=PI/2, weight=0.5 → PI/4
	AssertBoneRotation(rig.pose, 0, Dia::Maths::PI_HALF * 0.5f, 0.01f);
}

// ---------------------------------------------------------------------------
// Two-bone reach weight 0.5: mid-bone rotation is midpoint
// snapshot=0, IK=PI/2, weight=0.5 → PI/4
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Golden, TwoBone_ReachWeightHalf_ElbowBlended)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain = MakeArmChain();
	chain.reachWeight = 0.5f;
	solver.RegisterChain(chain);

	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(1.0f, 1.0f));

	// elbow snapshot=0, IK≈PI/2, blend=PI/4
	AssertBoneRotation(rig.pose, 1, Dia::Maths::PI_HALF * 0.5f, 0.01f);
}
