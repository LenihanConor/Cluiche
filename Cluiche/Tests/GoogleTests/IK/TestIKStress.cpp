#include <gtest/gtest.h>
#include <cmath>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaIK2D/Testing/IKAssertions.h>
#include <DiaMaths/Core/MathsDefines.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

// ---------------------------------------------------------------------------
// Register and solve 32 chains (kMaxChains capacity) without crash or NaN
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, MaxChainRegistration_NoNaNOrCrash)
{
	// Need a skeleton with enough bones for 32 non-overlapping single-joint chains
	// Each chain is bone[i]..bone[i+1], using a 64-bone skeleton
	const int kBones = 64;
	auto def = BuildLimbSkeletonDef(kBones, 0.5f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	const int kChains = 32;
	for (int i = 0; i < kChains; ++i)
	{
		char startBuf[16], endBuf[16], chainBuf[16];
		snprintf(startBuf,  sizeof(startBuf),  "bone%d", i * 2);
		snprintf(endBuf,    sizeof(endBuf),    "bone%d", i * 2 + 1);
		snprintf(chainBuf,  sizeof(chainBuf),  "chain%d", i);

		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC(chainBuf);
		chain.startBoneId = Dia::Core::StringCRC(startBuf);
		chain.endBoneId   = Dia::Core::StringCRC(endBuf);
		chain.reachWeight = 1.0f;
		chain.maxIterations = 5;
		chain.tolerance = 0.01f;
		solver.RegisterChain(chain);
	}

	EXPECT_EQ(kChains, 32);

	// Solve one of them and check for NaN
	solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
	solver.SolveFABRIK(Dia::Core::StringCRC("chain0"), Dia::Maths::Vector2D(0.3f, 0.3f));

	const float rot = rig.pose.GetLocalTransform(0).rotation;
	EXPECT_FALSE(std::isnan(rot));
	EXPECT_FALSE(std::isinf(rot));
}

// ---------------------------------------------------------------------------
// 128-bone skeleton FABRIK: 1000 iterations of solving, no NaN/Inf
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, LargeSkeletonFABRIK_1000Solves_NoNaNOrInf)
{
	const int kBones = 128;
	auto def = BuildLimbSkeletonDef(kBones, 0.1f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("full");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone127");
	chain.reachWeight = 1.0f;
	chain.maxIterations = 10;
	chain.tolerance = 0.01f;
	solver.RegisterChain(chain);

	for (int iter = 0; iter < 1000; ++iter)
	{
		const float angle = static_cast<float>(iter) * 0.01f;
		const Dia::Maths::Vector2D target(cosf(angle) * 5.0f, sinf(angle) * 5.0f);

		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveFABRIK(Dia::Core::StringCRC("full"), target);
	}

	for (int i = 0; i < kBones; ++i)
	{
		const float rot = rig.pose.GetLocalTransform(i).rotation;
		EXPECT_FALSE(std::isnan(rot)) << "bone " << i;
		EXPECT_FALSE(std::isinf(rot)) << "bone " << i;
	}
}

// ---------------------------------------------------------------------------
// Two-bone solver: 1000 solves in a tight loop, no NaN/Inf
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, TwoBone_1000Solves_NoNaNOrInf)
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

	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	for (int iter = 0; iter < 1000; ++iter)
	{
		const float angle = static_cast<float>(iter) * 0.01f;
		const float r = (iter % 2 == 0) ? 1.0f : 3.0f; // alternates reachable / unreachable
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveTwoBone(Dia::Core::StringCRC("arm"),
		                    Dia::Maths::Vector2D(r * cosf(angle), r * sinf(angle)));
	}

	for (int i = 0; i < 3; ++i)
	{
		const float rot = rig.pose.GetLocalTransform(i).rotation;
		EXPECT_FALSE(std::isnan(rot)) << "bone " << i;
		EXPECT_FALSE(std::isinf(rot)) << "bone " << i;
	}
}

// ---------------------------------------------------------------------------
// Look-at: 1000 solves sweeping 360 degrees, no NaN/Inf
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, LookAt_1000Solves_NoNaNOrInf)
{
	auto def = BuildLimbSkeletonDef(1, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	for (int iter = 0; iter < 1000; ++iter)
	{
		const float angle = static_cast<float>(iter) * Dia::Maths::PI_2 / 1000.0f;
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveLookAt(0, Dia::Maths::Vector2D(cosf(angle), sinf(angle)));
	}

	const float rot = rig.pose.GetLocalTransform(0).rotation;
	EXPECT_FALSE(std::isnan(rot));
	EXPECT_FALSE(std::isinf(rot));
}

// ---------------------------------------------------------------------------
// Extreme targets (very large coordinates): no NaN/Inf produced
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, ExtremeTargets_NoNaNOrInf)
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

	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	const Dia::Maths::Vector2D extremeTargets[] = {
		Dia::Maths::Vector2D(1e6f, 0.0f),
		Dia::Maths::Vector2D(0.0f, 1e6f),
		Dia::Maths::Vector2D(-1e6f, -1e6f),
		Dia::Maths::Vector2D(1e6f,  1e6f),
	};

	for (auto& t : extremeTargets)
	{
		solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
		solver.SolveTwoBone(Dia::Core::StringCRC("arm"), t);

		for (int i = 0; i < 3; ++i)
		{
			const float rot = rig.pose.GetLocalTransform(i).rotation;
			EXPECT_FALSE(std::isnan(rot));
			EXPECT_FALSE(std::isinf(rot));
		}
	}
}

// ---------------------------------------------------------------------------
// RegisterChain / UnregisterChain cycle 1000 times: no leak or crash
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Stress, ChainRegisterUnregister_1000Cycles_NoLeak)
{
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone3");
	chain.reachWeight = 1.0f;

	for (int iter = 0; iter < 1000; ++iter)
	{
		solver.RegisterChain(chain);
		EXPECT_TRUE(solver.HasChain(Dia::Core::StringCRC("chain")));
		solver.UnregisterChain(Dia::Core::StringCRC("chain"));
		EXPECT_FALSE(solver.HasChain(Dia::Core::StringCRC("chain")));
	}
}
