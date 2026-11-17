#include <gtest/gtest.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

// TODO(task4): test in-flight unregister/destructor (Wait, Release, no Apply, discard warning)

class DeferredStepSchedulerTest : public ::testing::Test
{
protected:
	DeferredStepSchedulerTest() : mScheduler(mLauncher) {}
	~DeferredStepSchedulerTest() override {}

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	Testing::RecordingStep mStep;
};

TEST_F(DeferredStepSchedulerTest, RegisterFreeChannelSucceedsAndIsIdle)
{
	EXPECT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, RegisterDuplicateFailsAndCountUnchanged)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_FALSE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, NinthChannelFails)
{
	const char* names[9] = { "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8" };
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_TRUE(mScheduler.Register(StringCRC(names[i]), mStep));
	}
	EXPECT_FALSE(mScheduler.Register(StringCRC(names[8]), mStep));
	EXPECT_EQ(mScheduler.GetChannelCount(), 8u);
	for (int i = 0; i < 8; ++i)
	{
		mScheduler.Unregister(StringCRC(names[i]));
	}
	EXPECT_EQ(mScheduler.GetChannelCount(), 0u);
}

TEST_F(DeferredStepSchedulerTest, UnregisterIdleRemovesChannel)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), mStep));
	mScheduler.Unregister(StringCRC("A"));
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	ChannelStats stats;
	EXPECT_FALSE(mScheduler.GetChannelStats(StringCRC("A"), stats));
	EXPECT_TRUE(mScheduler.GetChannelStats(StringCRC("B"), stats));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, UnregisterUnknownIsNoOp)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	mScheduler.Unregister(StringCRC("Nope"));
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, GetChannelStatsRegisteredIsZeroedUnknownFalse)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ChannelStats stats;
	stats.kicks = 99;
	EXPECT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), stats));
	EXPECT_EQ(stats.kicks, 0u);
	EXPECT_EQ(stats.kicksRejected, 0u);
	EXPECT_EQ(stats.applies, 0u);
	EXPECT_EQ(stats.stalls, 0u);
	EXPECT_EQ(stats.lastRunMicros, 0u);
	EXPECT_EQ(stats.lastStallMicros, 0u);
	EXPECT_EQ(stats.totalStallMicros, 0u);
	EXPECT_EQ(stats.lastSlackKeys, 0);
	EXPECT_FALSE(mScheduler.GetChannelStats(StringCRC("Nope"), stats));
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, EmptySchedulerDestructsCleanly)
{
	Testing::ManualStepLauncher launcher;
	{
		DeferredStepScheduler scheduler(launcher);
		EXPECT_EQ(scheduler.GetChannelCount(), 0u);
	}
	EXPECT_EQ(launcher.GetLaunchCount(), 0u);
}

TEST_F(DeferredStepSchedulerTest, UnregisterThenReRegisterSameIdWorks)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	mScheduler.Unregister(StringCRC("A"));
	EXPECT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, StatsStartZeroed)
{
	const SchedulerStats& s = mScheduler.GetStats();
	EXPECT_EQ(s.inFlight, 0u);
	EXPECT_EQ(s.kicks, 0u);
	EXPECT_EQ(s.kicksRejected, 0u);
	EXPECT_EQ(s.applies, 0u);
	EXPECT_EQ(s.stalls, 0u);
	EXPECT_EQ(s.totalStallMicros, 0u);
	EXPECT_EQ(s.drains, 0u);
	EXPECT_EQ(s.drainedSteps, 0u);
}
