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
	Dia::Rig2D::SkeletonDef MakeSingleBoneDef()
	{
		Dia::Rig2D::SkeletonDef def;
		def.id = Dia::Core::StringCRC("Eye");

		Dia::Rig2D::Bone b;
		b.name         = Dia::Core::StringCRC("eye");
		b.parentIndex  = -1;
		b.localPosition = Dia::Maths::Vector2D(0.0f, 0.0f);
		b.localRotation = 0.0f;
		b.localScale    = Dia::Maths::Vector2D(1.0f, 1.0f);
		b.length        = 1.0f;
		def.bones.Add(b);
		return def;
	}
}

TEST(LookAtConstraint, TargetRight)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Target to the right: expected local angle ~= 0
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(1.0f, 0.0f));
	AssertBoneRotation(rig.pose, 0, 0.0f, 1e-3f);
}

TEST(LookAtConstraint, TargetUp)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Target directly up: expected local angle ~= pi/2
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(0.0f, 1.0f));
	AssertBoneRotation(rig.pose, 0, Dia::Maths::PI_HALF, 1e-3f);
}

TEST(LookAtConstraint, TargetLeft)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Target directly left: expected local angle ~= +/-pi
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(-1.0f, 0.0f));
	const float rot = rig.pose.GetLocalTransform(0).rotation;
	EXPECT_NEAR(fabsf(rot), Dia::Maths::PI, 1e-3f);
}

TEST(LookAtConstraint, ReachWeightZeroLeavesUnchanged)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const float origRot = rig.pose.GetLocalTransform(0).rotation;
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(0.0f, 1.0f), 0.0f);
	AssertBoneUnchanged(rig.pose, 0, origRot);
}

TEST(LookAtConstraint, AxisAngleOffset)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// Bone forward is +Y (offset = -pi/2). Target right => world angle 0 - (-pi/2) = pi/2 local
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(1.0f, 0.0f),
	                   1.0f, -Dia::Maths::PI_HALF);
	AssertBoneRotation(rig.pose, 0, Dia::Maths::PI_HALF, 1e-3f);
}

TEST(LookAtConstraint, CoincidentTargetNoOp)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const float origRot = rig.pose.GetLocalTransform(0).rotation;
	// Target at bone origin (0,0): coincident guard should fire and return true without changing pose
	const bool result = solver.SolveLookAt(Dia::Core::StringCRC("eye"),
	                                        Dia::Maths::Vector2D(0.0f, 0.0f));
	EXPECT_TRUE(result);
	AssertBoneUnchanged(rig.pose, 0, origRot);
}

TEST(LookAtConstraint, IntOverloadMatchesCRCOverload)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	solver.SolveLookAt(0, Dia::Maths::Vector2D(0.0f, 1.0f));
	const float rotInt = rig.pose.GetLocalTransform(0).rotation;

	rig.pose.GetLocalTransform(0).rotation = 0.0f;
	solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
	solver.SolveLookAt(Dia::Core::StringCRC("eye"), Dia::Maths::Vector2D(0.0f, 1.0f));
	const float rotCRC = rig.pose.GetLocalTransform(0).rotation;

	EXPECT_NEAR(rotInt, rotCRC, 1e-5f);
}

TEST(LookAtConstraint, ReturnsFalseForUnknownBone)
{
	auto def = MakeSingleBoneDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const bool result = solver.SolveLookAt(Dia::Core::StringCRC("nonexistent"),
	                                        Dia::Maths::Vector2D(1.0f, 0.0f));
	EXPECT_FALSE(result);
}
