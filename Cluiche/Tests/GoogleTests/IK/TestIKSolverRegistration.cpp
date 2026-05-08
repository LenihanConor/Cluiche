#include <gtest/gtest.h>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;

namespace
{
	Dia::Rig2D::SkeletonDef MakeDef()
	{
		return BuildLimbSkeletonDef(3, 1.0f);
	}
}

TEST(IKSolverRegistration, HasChainReturnsFalseBeforeRegister)
{
	auto def = MakeDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	EXPECT_FALSE(solver.HasChain(Dia::Core::StringCRC("arm")));
}

TEST(IKSolverRegistration, RegisterAndHasChain)
{
	auto def = MakeDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone2");
	solver.RegisterChain(chain);

	EXPECT_TRUE(solver.HasChain(Dia::Core::StringCRC("arm")));
}

TEST(IKSolverRegistration, UnregisterChain)
{
	auto def = MakeDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef chain;
	chain.id          = Dia::Core::StringCRC("arm");
	chain.startBoneId = Dia::Core::StringCRC("bone0");
	chain.endBoneId   = Dia::Core::StringCRC("bone2");
	solver.RegisterChain(chain);

	solver.UnregisterChain(Dia::Core::StringCRC("arm"));
	EXPECT_FALSE(solver.HasChain(Dia::Core::StringCRC("arm")));
}

TEST(IKSolverRegistration, MultipleChains)
{
	auto def = BuildLimbSkeletonDef(5, 1.0f);
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	IKChainDef arm;
	arm.id          = Dia::Core::StringCRC("arm");
	arm.startBoneId = Dia::Core::StringCRC("bone0");
	arm.endBoneId   = Dia::Core::StringCRC("bone2");
	solver.RegisterChain(arm);

	IKChainDef leg;
	leg.id          = Dia::Core::StringCRC("leg");
	leg.startBoneId = Dia::Core::StringCRC("bone2");
	leg.endBoneId   = Dia::Core::StringCRC("bone4");
	solver.RegisterChain(leg);

	EXPECT_TRUE(solver.HasChain(Dia::Core::StringCRC("arm")));
	EXPECT_TRUE(solver.HasChain(Dia::Core::StringCRC("leg")));
}

TEST(IKSolverRegistration, GetSkeletonAndPose)
{
	auto def = MakeDef();
	TestRig rig(def);
	IKSolver solver = BuildTestIKSolver(rig.skeleton, rig.pose);

	EXPECT_EQ(&solver.GetSkeleton(), &rig.skeleton);
	EXPECT_EQ(&solver.GetPose(), &rig.pose);
}
