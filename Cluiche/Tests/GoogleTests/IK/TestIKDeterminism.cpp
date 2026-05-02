#include <gtest/gtest.h>
#include <cmath>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaMaths/Core/MathsDefines.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

namespace
{
	struct PoseSnapshot
	{
		float rotations[128];
		int   count;

		PoseSnapshot(const Dia::Rig2D::Pose& pose, int boneCount) : count(boneCount)
		{
			for (int i = 0; i < boneCount; ++i)
				rotations[i] = pose.GetLocalTransform(i).rotation;
		}
	};

	bool SnapshotsEqual(const PoseSnapshot& a, const PoseSnapshot& b)
	{
		if (a.count != b.count) return false;
		for (int i = 0; i < a.count; ++i)
		{
			if (a.rotations[i] != b.rotations[i]) return false;
		}
		return true;
	}
}

// ---------------------------------------------------------------------------
// FABRIK: two identical setups produce bit-identical poses after same sequence
// of solves (no random state, same input order)
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Determinism, FABRIK_IdenticalSetup_BitIdenticalResult)
{
	const Dia::Maths::Vector2D targets[] = {
		Dia::Maths::Vector2D(1.5f,  1.0f),
		Dia::Maths::Vector2D(0.0f,  2.5f),
		Dia::Maths::Vector2D(-1.0f, 1.5f),
		Dia::Maths::Vector2D(2.0f,  0.5f),
		Dia::Maths::Vector2D(1.0f,  2.0f),
	};

	PoseSnapshot* snap1 = nullptr;
	PoseSnapshot* snap2 = nullptr;

	auto runSolve = [&](PoseSnapshot*& outSnap) {
		auto def = BuildLimbSkeletonDef(4, 1.0f);
		TestRig rig(def);
		IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

		IKChainDef chain;
		chain.id          = Dia::Core::StringCRC("chain");
		chain.startBoneId = Dia::Core::StringCRC("bone0");
		chain.endBoneId   = Dia::Core::StringCRC("bone3");
		chain.reachWeight = 1.0f;
		chain.maxIterations = 30;
		chain.tolerance = 0.001f;
		solver.RegisterChain(chain);

		for (auto& t : targets)
		{
			solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
			solver.SolveFABRIK(Dia::Core::StringCRC("chain"), t);
		}

		outSnap = new PoseSnapshot(rig.pose, rig.skeleton.GetBoneCount());
	};

	runSolve(snap1);
	runSolve(snap2);

	ASSERT_NE(snap1, nullptr);
	ASSERT_NE(snap2, nullptr);
	EXPECT_TRUE(SnapshotsEqual(*snap1, *snap2));

	delete snap1;
	delete snap2;
}

// ---------------------------------------------------------------------------
// Two-bone: two identical setups produce bit-identical poses
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Determinism, TwoBone_IdenticalSetup_BitIdenticalResult)
{
	const Dia::Maths::Vector2D targets[] = {
		Dia::Maths::Vector2D(1.0f, 1.0f),
		Dia::Maths::Vector2D(0.5f, 1.5f),
		Dia::Maths::Vector2D(1.5f, 0.5f),
		Dia::Maths::Vector2D(0.0f, 1.9f),
	};

	auto buildAndSolve = [&]() -> PoseSnapshot {
		Dia::Rig2D::SkeletonDef def;
		def.id = Dia::Core::StringCRC("Arm");
		auto add = [&](const char* name, int parent, float lx) {
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

		for (auto& t : targets)
		{
			solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
			solver.SolveTwoBone(Dia::Core::StringCRC("arm"), t);
		}

		return PoseSnapshot(rig.pose, rig.skeleton.GetBoneCount());
	};

	PoseSnapshot s1 = buildAndSolve();
	PoseSnapshot s2 = buildAndSolve();

	EXPECT_TRUE(SnapshotsEqual(s1, s2));
}

// ---------------------------------------------------------------------------
// Look-at: two identical setups produce bit-identical results
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Determinism, LookAt_IdenticalSetup_BitIdenticalResult)
{
	const Dia::Maths::Vector2D targets[] = {
		Dia::Maths::Vector2D(1.0f, 0.0f),
		Dia::Maths::Vector2D(0.0f, 1.0f),
		Dia::Maths::Vector2D(-1.0f, 0.5f),
		Dia::Maths::Vector2D(0.7f, -0.7f),
	};

	auto buildAndSolve = [&]() -> float {
		auto def = BuildLimbSkeletonDef(1, 1.0f);
		TestRig rig(def);
		IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

		for (auto& t : targets)
		{
			solver.SetRootTransform(Dia::Rig2D::BoneTransform{});
			solver.SolveLookAt(0, t, 1.0f);
		}

		return rig.pose.GetLocalTransform(0).rotation;
	};

	const float r1 = buildAndSolve();
	const float r2 = buildAndSolve();

	EXPECT_EQ(r1, r2);
}
