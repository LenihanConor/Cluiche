// Save-and-Load scheduler state (DFS-005): pending enumeration, RestorePending, DrainAndDiscardAll.

#include <gtest/gtest.h>
#include <vector>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

class DeferredStepSavePending : public ::testing::Test
{
protected:
	DeferredStepSavePending()
		: mScheduler(mLauncher)
		, mStepA(1, &mOrder)
		, mStepB(2, &mOrder)
		, mStepC(3, &mOrder)
		, mA("SavePendingA")
		, mB("SavePendingB")
		, mC("SavePendingC")
	{
		mScheduler.Register(mA, mStepA);
		mScheduler.Register(mB, mStepB);
		mScheduler.Register(mC, mStepC);
	}

	~DeferredStepSavePending() override
	{
		mScheduler.Unregister(mA);
		mScheduler.Unregister(mB);
		mScheduler.Unregister(mC);
	}

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	std::vector<int> mOrder;
	Testing::RecordingStep mStepA;
	Testing::RecordingStep mStepB;
	Testing::RecordingStep mStepC;
	StringCRC mA;
	StringCRC mB;
	StringCRC mC;
};

TEST_F(DeferredStepSavePending, PendingEnumeratesInKickOrderAfterDrain)
{
	ASSERT_EQ(mScheduler.Kick(mC, 5, 20), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(mA, 6, 21), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(mB, 7, 22), KickResult::kLaunched);
	mScheduler.DrainAll();

	ASSERT_EQ(mScheduler.GetPendingCount(), 3u);
	PendingStepInfo info;
	ASSERT_TRUE(mScheduler.GetPendingAt(0, info));
	EXPECT_EQ(info.channelId, mC);
	EXPECT_EQ(info.kickKey, 5u);
	EXPECT_EQ(info.applyKey, 20u);
	EXPECT_EQ(info.step, &mStepC);
	ASSERT_TRUE(mScheduler.GetPendingAt(1, info));
	EXPECT_EQ(info.channelId, mA);
	EXPECT_EQ(info.kickKey, 6u);
	EXPECT_EQ(info.applyKey, 21u);
	EXPECT_EQ(info.step, &mStepA);
	ASSERT_TRUE(mScheduler.GetPendingAt(2, info));
	EXPECT_EQ(info.channelId, mB);
	EXPECT_EQ(info.kickKey, 7u);
	EXPECT_EQ(info.applyKey, 22u);
	EXPECT_EQ(info.step, &mStepB);
	EXPECT_FALSE(mScheduler.GetPendingAt(3, info));
}

TEST_F(DeferredStepSavePending, RunningAndIdleChannelsAreNotPending)
{
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
	ASSERT_EQ(mScheduler.Kick(mA, 1, 10), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kRunning);
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
	PendingStepInfo info;
	EXPECT_FALSE(mScheduler.GetPendingAt(0, info));
	mScheduler.DrainAll();
	EXPECT_EQ(mScheduler.GetPendingCount(), 1u);
}

TEST_F(DeferredStepSavePending, RestoreUnknownChannelFails)
{
	EXPECT_FALSE(mScheduler.RestorePending(StringCRC("SavePendingNope"), 1, 2));
}

TEST_F(DeferredStepSavePending, RestoreNonIdleChannelFailsWithoutStateChange)
{
	ASSERT_EQ(mScheduler.Kick(mA, 1, 10), KickResult::kLaunched);
	EXPECT_FALSE(mScheduler.RestorePending(mA, 2, 11));
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kRunning);
	StepKey key = 0;
	ASSERT_TRUE(mScheduler.GetApplyKey(mA, key));
	EXPECT_EQ(key, 10u);

	mScheduler.DrainAll();
	EXPECT_FALSE(mScheduler.RestorePending(mA, 2, 11));
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kComplete);
	ASSERT_TRUE(mScheduler.GetApplyKey(mA, key));
	EXPECT_EQ(key, 10u);
}

TEST_F(DeferredStepSavePending, RestoredChannelAppliesOnceWithoutLauncherCalls)
{
	ASSERT_TRUE(mScheduler.RestorePending(mA, 4, 9));
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kComplete);
	StepKey key = 0;
	ASSERT_TRUE(mScheduler.GetApplyKey(mA, key));
	EXPECT_EQ(key, 9u);
	EXPECT_EQ(mScheduler.GetPendingCount(), 1u);

	mScheduler.Tick(8);
	EXPECT_EQ(mStepA.GetApplyCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kComplete);

	mScheduler.Tick(9);
	EXPECT_EQ(mStepA.GetApplyCount(), 1u);
	EXPECT_EQ(mStepA.GetRunCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(mA), StepState::kIdle);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	EXPECT_EQ(mScheduler.GetStats().applies, 1u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(mA, cs));
	EXPECT_EQ(cs.stalls, 0u);
	EXPECT_EQ(cs.applies, 1u);

	mScheduler.Tick(10);
	EXPECT_EQ(mStepA.GetApplyCount(), 1u);

	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 0u);
}

TEST_F(DeferredStepSavePending, RestoreSetsKeyFloor)
{
	ASSERT_TRUE(mScheduler.RestorePending(mA, 10, 11));
	mScheduler.Tick(11);
	ASSERT_EQ(mScheduler.GetState(mA), StepState::kIdle);
	EXPECT_EQ(mScheduler.Kick(mA, 9, 20), KickResult::kKeyNotMonotonic);
	EXPECT_EQ(mScheduler.Kick(mA, 10, 20), KickResult::kLaunched);
}

TEST_F(DeferredStepSavePending, RestoredChannelsApplyInRestoreOrder)
{
	ASSERT_TRUE(mScheduler.RestorePending(mC, 1, 5));
	ASSERT_TRUE(mScheduler.RestorePending(mA, 2, 5));
	ASSERT_TRUE(mScheduler.RestorePending(mB, 3, 5));
	mScheduler.Tick(5);
	ASSERT_EQ(mOrder.size(), 3u);
	EXPECT_EQ(mOrder[0], 3);
	EXPECT_EQ(mOrder[1], 1);
	EXPECT_EQ(mOrder[2], 2);
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
}

TEST_F(DeferredStepSavePending, DrainAndDiscardAllClearsRunningAndRestored)
{
	ASSERT_EQ(mScheduler.Kick(mA, 1, 10), KickResult::kLaunched);
	ASSERT_TRUE(mScheduler.RestorePending(mB, 2, 11));

	mScheduler.DrainAndDiscardAll();

	EXPECT_EQ(mScheduler.GetState(mA), StepState::kIdle);
	EXPECT_EQ(mScheduler.GetState(mB), StepState::kIdle);
	EXPECT_EQ(mStepA.GetApplyCount(), 0u);
	EXPECT_EQ(mStepB.GetApplyCount(), 0u);
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);

	mScheduler.Tick(100);
	EXPECT_EQ(mStepA.GetApplyCount(), 0u);
	EXPECT_EQ(mStepB.GetApplyCount(), 0u);

	// Key floors survive the discard.
	EXPECT_EQ(mScheduler.Kick(mB, 1, 200), KickResult::kKeyNotMonotonic);
}

TEST_F(DeferredStepSavePending, DrainAndDiscardAllWithNothingIsNoOp)
{
	mScheduler.DrainAndDiscardAll();
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
}

TEST_F(DeferredStepSavePending, UnregisterRestoredChannelResetsWithoutLauncherCalls)
{
	ASSERT_TRUE(mScheduler.RestorePending(mA, 1, 5));
	mScheduler.Unregister(mA);
	EXPECT_EQ(mScheduler.GetChannelCount(), 2u);
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
	mScheduler.Tick(10);
	EXPECT_EQ(mStepA.GetApplyCount(), 0u);
	// Re-register so the fixture teardown stays balanced.
	mScheduler.Register(mA, mStepA);
}
