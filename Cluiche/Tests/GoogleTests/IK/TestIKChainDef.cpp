#include <gtest/gtest.h>

#include <DiaIK2D/IKChainDef.h>
#include <DiaMaths/Core/MathsDefines.h>

using namespace Dia::IK2D;

TEST(IKChainDef, JointLimitDefDefaults)
{
	JointLimitDef limit;
	EXPECT_FLOAT_EQ(limit.minAngle, -Dia::Maths::PI);
	EXPECT_FLOAT_EQ(limit.maxAngle,  Dia::Maths::PI);
	EXPECT_FALSE(limit.enabled);
}

TEST(IKChainDef, IKChainDefDefaults)
{
	IKChainDef def;
	EXPECT_FLOAT_EQ(def.reachWeight, 1.0f);
	EXPECT_EQ(def.maxIterations, 20);
	EXPECT_FLOAT_EQ(def.tolerance, 0.001f);
	EXPECT_TRUE(def.jointLimits.IsEmpty());
}

TEST(IKChainDef, PoleVectorDefaults)
{
	PoleVector pv;
	EXPECT_FLOAT_EQ(pv.direction.x, 1.0f);
	EXPECT_FLOAT_EQ(pv.direction.y, 0.0f);
	EXPECT_FLOAT_EQ(pv.weight, 1.0f);
}

TEST(IKChainDef, JointLimitsPartialArray)
{
	IKChainDef def;

	JointLimitDef lim0;
	lim0.minAngle = -0.5f;
	lim0.maxAngle =  0.5f;
	lim0.enabled  = true;
	def.jointLimits.Add(lim0);

	JointLimitDef lim1;
	lim1.enabled = false;
	def.jointLimits.Add(lim1);

	EXPECT_EQ(def.jointLimits.Size(), 2u);
	EXPECT_TRUE(def.jointLimits[0].enabled);
	EXPECT_FALSE(def.jointLimits[1].enabled);
	EXPECT_FLOAT_EQ(def.jointLimits[0].minAngle, -0.5f);
}
