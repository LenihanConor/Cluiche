#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/RangeTiming.h>

using namespace Dia::DeferredStep;

TEST(DeferredStepCapResolveTest, DerivesQuarterOfWorkersMinOne)
{
	CapConfig config;
	bool clamped = true;
	EXPECT_EQ(ResolveWorkerCap(config, 1, &clamped), 1u);
	EXPECT_FALSE(clamped);
	EXPECT_EQ(ResolveWorkerCap(config, 3, &clamped), 1u);
	EXPECT_EQ(ResolveWorkerCap(config, 4, &clamped), 1u);
	EXPECT_EQ(ResolveWorkerCap(config, 8, &clamped), 2u);
	EXPECT_EQ(ResolveWorkerCap(config, 64, &clamped), 16u);
	EXPECT_FALSE(clamped);
}

TEST(DeferredStepCapResolveTest, ExplicitWithinRangeIsKept)
{
	CapConfig config;
	config.workerCap = 3;
	bool clamped = true;
	EXPECT_EQ(ResolveWorkerCap(config, 8, &clamped), 3u);
	EXPECT_FALSE(clamped);
	config.workerCap = 8;
	EXPECT_EQ(ResolveWorkerCap(config, 8, &clamped), 8u);
	EXPECT_FALSE(clamped);
}

TEST(DeferredStepCapResolveTest, ExplicitOverWorkerCountIsClampedAndFlagged)
{
	CapConfig config;
	config.workerCap = 20;
	bool clamped = false;
	EXPECT_EQ(ResolveWorkerCap(config, 8, &clamped), 8u);
	EXPECT_TRUE(clamped);
}

TEST(DeferredStepCapResolveTest, ZeroWorkersTreatedAsOne)
{
	CapConfig config;
	bool clamped = true;
	EXPECT_EQ(ResolveWorkerCap(config, 0, &clamped), 1u);
	EXPECT_FALSE(clamped);
	config.workerCap = 5;
	EXPECT_EQ(ResolveWorkerCap(config, 0, &clamped), 1u);
	EXPECT_TRUE(clamped);
}

TEST(DeferredStepCapResolveTest, NullOutParamAccepted)
{
	CapConfig config;
	config.workerCap = 99;
	EXPECT_EQ(ResolveWorkerCap(config, 4, nullptr), 4u);
}

TEST(DeferredStepCapResolveTest, DefaultsAndRangeTimingFlags)
{
	CapConfig config;
	EXPECT_EQ(config.workerCap, 0u);
	EXPECT_EQ(config.rangesPerWorker, 4u);
	RangeTiming t;
	EXPECT_EQ(t.flags, 0u);
	EXPECT_EQ(kRangeStolen, 1u);
	EXPECT_EQ(kRangeTrySubmitFailed, 2u);
}
