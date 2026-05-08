#include <gtest/gtest.h>
#include <cmath>
#include <random>

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

	IKChainDef MakeArmChain(float reachWeight = 1.0f)
	{
		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("arm");
		chain.startBoneId = Dia::Core::StringCRC("shoulder");
		chain.endBoneId   = Dia::Core::StringCRC("wrist");
		chain.reachWeight = reachWeight;
		return chain;
	}
}

// ---------------------------------------------------------------------------
// Reach weight 0: SolveTwoBone leaves all bone rotations unchanged (500 random targets)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, TwoBone_ReachWeightZero_NeverChangesPose)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain(0.0f));

	std::mt19937 rng(0xABCD1234);
	std::uniform_real_distribution<float> dist(-3.0f, 3.0f);

	const float snap0 = rig.pose.GetLocalTransform(0).rotation;
	const float snap1 = rig.pose.GetLocalTransform(1).rotation;

	for (int iter = 0; iter < 500; ++iter)
	{
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
		                    Dia::Maths::Vector2D(dist(rng), dist(rng)));
		AssertBoneUnchanged(rig.pose, 0, snap0);
		AssertBoneUnchanged(rig.pose, 1, snap1);
	}
}

// ---------------------------------------------------------------------------
// Reach weight 1: SolveTwoBone end effector always reaches reachable targets
// over 500 random targets within chain length
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, TwoBone_ReachWeightOne_EndEffectorReachesTarget)
{
	auto def = MakeArmDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);
	solver.RegisterChain(MakeArmChain(1.0f));

	std::mt19937 rng(0xDEADBEEF);
	std::uniform_real_distribution<float> angle(0.0f, Dia::Maths::PI_2);
	std::uniform_real_distribution<float> dist(0.1f, 1.9f);

	for (int iter = 0; iter < 500; ++iter)
	{
		const float a = angle(rng);
		const float r = dist(rng);
		const Dia::Maths::Vector2D target(r * cosf(a), r * sinf(a));

		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveTwoBone(Dia::Core::StringCRC("arm"), target);

		AssertEndEffectorPosition(solver, Dia::Rig2D::BoneTransform{}, 2, target, 0.01f);
	}
}

// ---------------------------------------------------------------------------
// FABRIK reach weight 0: never changes pose over 500 random targets
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, FABRIK_ReachWeightZero_NeverChangesPose)
{
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone3");
	chain.reachWeight = 0.0f;
	chain.maxIterations = 20;
	chain.tolerance = 0.001f;
	solver.RegisterChain(chain);

	std::mt19937 rng(0x12345678);
	std::uniform_real_distribution<float> dist(-2.0f, 2.0f);

	float snaps[4];
	for (int i = 0; i < 4; ++i)
		snaps[i] = rig.pose.GetLocalTransform(i).rotation;

	for (int iter = 0; iter < 500; ++iter)
	{
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveFABRIK(Dia::Core::StringCRC("chain"),
		                   Dia::Maths::Vector2D(dist(rng), dist(rng)));
		for (int i = 0; i < 3; ++i)  // startIdx..endIdx-1 are modified; only those matter
			AssertBoneUnchanged(rig.pose, i, snaps[i]);
	}
}

// ---------------------------------------------------------------------------
// FABRIK idempotent: solving twice from same state gives same result
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, FABRIK_SolveTwice_SameResult)
{
	const Dia::Maths::Vector2D target(1.5f, 1.0f);

	// First solve
	auto def1 = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig1(def1);
	IKSolver solver1 = BuildTestIKSolver(rig1.skeleton, rig1.pose);
	{
		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("chain");
		chain.startBoneId = Dia::Core::StringCRC("bone0");
		chain.endBoneId   = Dia::Core::StringCRC("bone3");
		chain.reachWeight = 1.0f;
		chain.maxIterations = 50;
		chain.tolerance = 0.001f;
		solver1.RegisterChain(chain);
	}
	solver1.SolveFABRIK(Dia::Core::StringCRC("chain"), target);

	// Second solve with identical setup
	auto def2 = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig2(def2);
	IKSolver solver2 = BuildTestIKSolver(rig2.skeleton, rig2.pose);
	{
		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("chain");
		chain.startBoneId = Dia::Core::StringCRC("bone0");
		chain.endBoneId   = Dia::Core::StringCRC("bone3");
		chain.reachWeight = 1.0f;
		chain.maxIterations = 50;
		chain.tolerance = 0.001f;
		solver2.RegisterChain(chain);
	}
	solver2.SolveFABRIK(Dia::Core::StringCRC("chain"), target);

	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(rig1.pose.GetLocalTransform(i).rotation,
		            rig2.pose.GetLocalTransform(i).rotation, 1e-5f) << "bone " << i;
	}
}

// ---------------------------------------------------------------------------
// LookAt weight 0: never changes pose over 500 random targets
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, LookAt_ReachWeightZero_NeverChangesPose)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	std::mt19937 rng(0xFEEDFACE);
	std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

	const float snap = rig.pose.GetLocalTransform(0).rotation;

	for (int iter = 0; iter < 500; ++iter)
	{
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveLookAt(0, Dia::Maths::Vector2D(dist(rng), dist(rng)), 0.0f);
		AssertBoneUnchanged(rig.pose, 0, snap);
	}
}

// ---------------------------------------------------------------------------
// LookAt weight 1: world-space direction of bone matches target direction
// over 500 random (non-coincident) targets
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, LookAt_ReachWeightOne_BoneAlwaysAimsAtTarget)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	std::mt19937 rng(0xCAFEBABE);
	std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

	for (int iter = 0; iter < 500; ++iter)
	{
		float tx = dist(rng);
		float ty = dist(rng);
		if (fabsf(tx) < 0.1f && fabsf(ty) < 0.1f)
			tx = 1.0f; // avoid coincident guard

		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveLookAt(0, Dia::Maths::Vector2D(tx, ty), 1.0f);

		const float localRot = rig.pose.GetLocalTransform(0).rotation;
		const float expectedAngle = atan2f(ty, tx);
		// wrap difference to [-PI, PI]
		float diff = localRot - expectedAngle;
		diff = atan2f(sinf(diff), cosf(diff));
		EXPECT_NEAR(diff, 0.0f, 0.005f) << "iter=" << iter;
	}
}

// ---------------------------------------------------------------------------
// ShortestArcLerp property: blending at t=0 always returns snapshot
// over 500 random (snapshot, target) pairs for two-bone solver
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Invariant, TwoBone_BlendAtT0_AlwaysSnapshot)
{
	auto def = MakeArmDef();
	TestRig rig(def);

	std::mt19937 rng(0xBEEFCAFE);
	std::uniform_real_distribution<float> rotDist(-Dia::Maths::PI, Dia::Maths::PI);
	std::uniform_real_distribution<float> angle(0.0f, Dia::Maths::PI_2);
	std::uniform_real_distribution<float> radDist(0.1f, 1.9f);

	for (int iter = 0; iter < 500; ++iter)
	{
		// Set arbitrary initial pose
		const float snap0 = rotDist(rng);
		const float snap1 = rotDist(rng);
		rig.pose.GetLocalTransform(0).rotation = snap0;
		rig.pose.GetLocalTransform(1).rotation = snap1;

		IKSolver solver(rig.skeleton, rig.pose);
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});

		IKChainDef chain = MakeArmChain(0.0f);
		solver.RegisterChain(chain);

		const float a = angle(rng);
		const float r = radDist(rng);
		solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
		                    Dia::Maths::Vector2D(r * cosf(a), r * sinf(a)));

		AssertBoneUnchanged(rig.pose, 0, snap0);
		AssertBoneUnchanged(rig.pose, 1, snap1);
	}
}
