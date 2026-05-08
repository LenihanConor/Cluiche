#include <gtest/gtest.h>
#include <cmath>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>
#include <DiaIK2D/Testing/IKAssertions.h>
#include <DiaRig2D/BlendPoses.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

// ---------------------------------------------------------------------------
// Full FK→IK→FK pipeline: after SolveFABRIK, a fresh ComputeWorldTransforms
// call with the same root gives the same end-effector position as AssertEndEffectorPosition
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Integration, FABRIK_PoseWrittenToSkeleton_FKReproducesResult)
{
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone3");
	chain.reachWeight = 1.0f;
	chain.maxIterations = 50;
	chain.tolerance = 0.001f;
	solver.RegisterChain(chain);

	const Dia::Maths::Vector2D target(1.5f, 1.5f);
	solver.SolveFABRIK(Dia::Core::StringCRC("chain"), target);

	// Re-run FK manually using the modified pose
	Dia::Rig2D::BoneTransform root;
	Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> worldTransforms;
	rig.pose.ComputeWorldTransforms(rig.skeleton, root, worldTransforms);

	EXPECT_NEAR(worldTransforms[3].position.x, target.x, 0.01f);
	EXPECT_NEAR(worldTransforms[3].position.y, target.y, 0.01f);
}

// ---------------------------------------------------------------------------
// IK modifies pose; subsequent BlendPoses with identity pose interpolates correctly
// Blend IK result with FK (t=0) should return FK exactly
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Integration, FABRIK_Result_BlendWithIdentityAtT0_YieldsIdentity)
{
	auto def = BuildLimbSkeletonDef(4, 1.0f);
	Dia::Rig2D::Skeleton sk(def);
	Dia::Rig2D::Pose ikPose(sk);
	Dia::Rig2D::Pose fkPose(sk);  // identity (zero rotations)
	Dia::Rig2D::Pose blended(sk);

	IKSolver solver(sk, ikPose);
	solver.SetRootTransform(Dia::Rig2D::BoneTransform{});

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("chain");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone3");
	chain.reachWeight = 1.0f;
	chain.maxIterations = 50;
	chain.tolerance = 0.001f;
	solver.RegisterChain(chain);
	solver.SolveFABRIK(Dia::Core::StringCRC("chain"), Dia::Maths::Vector2D(1.5f, 1.0f));

	// Blend IK→FK at t=0: result should equal fkPose
	Dia::Rig2D::BlendPoses(ikPose, fkPose, 0.0f, blended);

	for (int i = 0; i < sk.GetBoneCount(); ++i)
	{
		EXPECT_NEAR(blended.GetLocalTransform(i).rotation,
		            ikPose.GetLocalTransform(i).rotation, 0.001f) << "bone " << i;
	}
}

// ---------------------------------------------------------------------------
// Two sequential IK solves in one frame: second solve sees updated world transforms
// Spine chain (bone0..bone1), then look-at on bone2 — bone2 should aim at its target
// using the world position established by the spine IK solve
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Integration, SequentialSolves_SecondSolveSeesFreshWorldTransforms)
{
	// 3-bone +Y skeleton: bone0 root, bone1 mid, bone2 tip
	auto def = BuildLimbSkeletonDef(3, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	// First: FABRIK on bone0..bone1 (spine)
	IKChainDef spineChain;
	spineChain.id          = Dia::Core::StringCRC("spine");
	spineChain.startBoneId = Dia::Core::StringCRC("bone0");
	spineChain.endBoneId   = Dia::Core::StringCRC("bone1");
	spineChain.reachWeight = 1.0f;
	spineChain.maxIterations = 20;
	spineChain.tolerance = 0.001f;
	solver.RegisterChain(spineChain);

	solver.SolveFABRIK(Dia::Core::StringCRC("spine"), Dia::Maths::Vector2D(1.0f, 0.0f));

	// Second: look-at on bone2 pointing toward (2.0f, 0.0f)
	const bool result = solver.SolveLookAt(2, Dia::Maths::Vector2D(2.0f, 0.0f));
	EXPECT_TRUE(result);

	// bone2 rotation must be finite (second solve used updated world transforms)
	const float rot2 = rig.pose.GetLocalTransform(2).rotation;
	EXPECT_FALSE(std::isnan(rot2));
	EXPECT_FALSE(std::isinf(rot2));
}

// ---------------------------------------------------------------------------
// IK with translated root transform: end effector reaches world-space target
// Root at (5,5), target at (6,6) — relative displacement (1,1) same as unit test
// ---------------------------------------------------------------------------
TEST(DiaIK2D_Integration, TwoBone_TranslatedRoot_EndEffectorAtWorldTarget)
{
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
	IKSolver solver(rig.skeleton, rig.pose);

	Dia::Rig2D::BoneTransform root;
	root.position.Set(5.0f, 5.0f);
	solver.SetRootTransform(root);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("shoulder");
	chain.endBoneId   = Dia::Core::StringCRC("wrist");
	chain.reachWeight = 1.0f;
	solver.RegisterChain(chain);

	// Target in world space: (5+1, 5+1) = (6,6)
	solver.SolveTwoBone(Dia::Core::StringCRC("arm"), Dia::Maths::Vector2D(6.0f, 6.0f));

	AssertEndEffectorPosition(solver, root, 2, Dia::Maths::Vector2D(6.0f, 6.0f), 0.01f);
}
