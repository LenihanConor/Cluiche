#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

namespace
{
	uint64_t NowNs()
	{
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}
	const StringCRC kChannel("StepTimesChannel");
}

class DeferredStepSummaryTimes : public ::testing::Test
{
protected:
	DeferredStepSummaryTimes() : mScheduler(mLauncher) { mScheduler.Register(kChannel, mStep); }
	~DeferredStepSummaryTimes() override { mScheduler.Unregister(kChannel); }

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	Testing::RecordingStep mStep;
};

TEST_F(DeferredStepSummaryTimes, NoTimesBeforeAnyApply)
{
	StepTimes t;
	EXPECT_FALSE(mScheduler.GetLastStepTimes(t));
}

TEST_F(DeferredStepSummaryTimes, NormalStepIsOrdered)
{
	const uint64_t before = NowNs();
	mScheduler.Kick(kChannel, 1, 5);
	mLauncher.CompleteAll();
	mScheduler.Tick(2);
	mScheduler.Tick(5);
	const uint64_t after = NowNs();

	StepTimes t;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(t));
	EXPECT_GE(t.kickNs, before);
	EXPECT_LE(t.kickNs, t.readyNs);
	EXPECT_LE(t.readyNs, t.applyNs);
	EXPECT_LE(t.applyNs, after);
	EXPECT_EQ(t.stallNs, 0u);
}

TEST_F(DeferredStepSummaryTimes, ReadyIsFirstObservationAndDoesNotMove)
{
	const uint64_t before = NowNs();
	mScheduler.Kick(kChannel, 1, 9);
	mLauncher.CompleteAll();
	mScheduler.Tick(2);   // first observation
	const uint64_t afterFirstTick = NowNs();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	mScheduler.Tick(3);
	mScheduler.Tick(4);
	mScheduler.Tick(9);

	StepTimes t;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(t));
	EXPECT_GE(t.readyNs, before);
	EXPECT_LE(t.readyNs, afterFirstTick);
	EXPECT_GE(t.applyNs - t.readyNs, 20ull * 1000 * 1000);
}

TEST_F(DeferredStepSummaryTimes, StalledStepHasStallAndReadyBeforeApply)
{
	mStep.SetOnRun([] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
	mScheduler.Kick(kChannel, 1, 2);
	mScheduler.Tick(2);   // not complete: stalls in Wait (runs inline)

	StepTimes t;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(t));
	EXPECT_GT(t.stallNs, 0u);
	EXPECT_GE(t.stallNs, 9ull * 1000 * 1000);
	EXPECT_GE(t.readyNs, t.kickNs);
	EXPECT_LE(t.readyNs, t.applyNs);
	EXPECT_LE(t.applyNs - t.readyNs, 5ull * 1000 * 1000);   // ready is stamped when Wait returns; apply follows immediately
	EXPECT_GE(t.applyNs - t.kickNs, t.stallNs);
}

TEST_F(DeferredStepSummaryTimes, RestoredStepHasOnlyApplyTime)
{
	ASSERT_TRUE(mScheduler.RestorePending(kChannel, 1, 3));
	mScheduler.Tick(3);

	StepTimes t;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(t));
	EXPECT_EQ(t.kickNs, 0u);
	EXPECT_EQ(t.readyNs, 0u);
	EXPECT_GT(t.applyNs, 0u);
	EXPECT_EQ(t.stallNs, 0u);
}

TEST_F(DeferredStepSummaryTimes, DiscardedStepsDoNotUpdateTimes)
{
	mScheduler.Kick(kChannel, 1, 2);
	mLauncher.CompleteAll();
	mScheduler.Tick(2);
	StepTimes first;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(first));

	mScheduler.Kick(kChannel, 3, 8);
	mScheduler.DrainAndDiscardAll();
	mScheduler.Kick(kChannel, 4, 8);
	mScheduler.Unregister(kChannel);

	StepTimes after;
	ASSERT_TRUE(mScheduler.GetLastStepTimes(after));
	EXPECT_EQ(after.kickNs, first.kickNs);
	EXPECT_EQ(after.readyNs, first.readyNs);
	EXPECT_EQ(after.applyNs, first.applyNs);
	EXPECT_EQ(after.stallNs, first.stallNs);
	mScheduler.Register(kChannel, mStep);   // keep fixture teardown balanced
}
