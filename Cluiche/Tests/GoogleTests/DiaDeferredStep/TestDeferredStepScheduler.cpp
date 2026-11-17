#include <gtest/gtest.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

// TODO(task5): kKeyNotMonotonic needs Tick to return the channel to idle (floor persists after apply)

namespace
{
	// Records the launcher's launch count at the moment Capture runs, to prove Capture precedes Launch.
	class OrderProbeStep final : public IDeferredStep
	{
	public:
		explicit OrderProbeStep(const Testing::ManualStepLauncher& launcher) : mLauncher(launcher) {}
		void Capture() override { ++mCaptureCount; mLaunchesAtCapture = mLauncher.GetLaunchCount(); }
		void Run(IParallelExecutor&) override {}
		void Apply() override {}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		uint32_t mCaptureCount = 0;
		uint32_t mLaunchesAtCapture = 99;
	private:
		const Testing::ManualStepLauncher& mLauncher;
	};
}

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

TEST_F(DeferredStepSchedulerTest, KickCapturesBeforeLaunchOnceEach)
{
	OrderProbeStep probe(mLauncher);
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), probe));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 10, 12), KickResult::kLaunched);
	EXPECT_EQ(probe.mCaptureCount, 1u);
	EXPECT_EQ(probe.mLaunchesAtCapture, 0u);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickPassesLaunchInfoAndCapturesOnCallingThread)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 7, 9), KickResult::kLaunched);
	EXPECT_TRUE(mLauncher.GetLastLaunchInfo().channelId == StringCRC("A"));
	EXPECT_EQ(mLauncher.GetLastLaunchInfo().kickKey, 7u);
	EXPECT_EQ(mStep.GetCaptureCount(), 1u);
	EXPECT_EQ(mStep.GetCaptureThread(), std::this_thread::get_id());
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickStatsIncrementOnSchedulerAndChannel)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 1, 2), KickResult::kLaunched);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicks, 1u);
	EXPECT_EQ(cs.kicksRejected, 0u);
	EXPECT_EQ(mScheduler.GetStats().kicks, 1u);
	EXPECT_EQ(mScheduler.GetStats().kicksRejected, 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 1u);
	mScheduler.Unregister(StringCRC("A"));
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
}

TEST_F(DeferredStepSchedulerTest, KickFirstKeyZeroAccepted)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 0, 0), KickResult::kLaunched);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickWhileInFlightRejected)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 2, 6), KickResult::kAlreadyInFlight);
	EXPECT_EQ(mStep.GetCaptureCount(), 1u);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicks, 1u);
	EXPECT_EQ(cs.kicksRejected, 1u);
	EXPECT_EQ(mScheduler.GetStats().kicksRejected, 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickApplyBeforeKickRejected)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 10, 9), KickResult::kApplyBeforeKick);
	EXPECT_EQ(mStep.GetCaptureCount(), 0u);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicks, 0u);
	EXPECT_EQ(cs.kicksRejected, 1u);
	EXPECT_EQ(mScheduler.GetStats().kicksRejected, 1u);
	// A rejected kick must not set the key floor: a lower valid key is still accepted.
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 3, 4), KickResult::kLaunched);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickApplyEqualsKickAccepted)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 10, 10), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickUnknownChannelRejected)
{
	EXPECT_EQ(mScheduler.Kick(StringCRC("Nope"), 1, 2), KickResult::kUnknownChannel);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().kicksRejected, 1u);
	EXPECT_EQ(mScheduler.GetStats().kicks, 0u);
}

TEST_F(DeferredStepSchedulerTest, KickOnOneChannelDoesNotAffectAnother)
{
	Testing::RecordingStep other;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), other));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 5, 6), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.GetState(StringCRC("B")), StepState::kIdle);
	EXPECT_EQ(mScheduler.Kick(StringCRC("B"), 1, 2), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 2u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, UnregisterInFlightWaitsThenReleasesNeverApplies)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.ClearCalls();
	mScheduler.Unregister(StringCRC("A"));

	int waitIndex = -1;
	int releaseIndex = -1;
	for (uint32_t i = 0; i < mLauncher.GetCallCount(); ++i)
	{
		const auto& call = mLauncher.GetCall(i);
		if (call.kind == Testing::ManualStepLauncher::Call::kWait && waitIndex < 0) { waitIndex = static_cast<int>(i); }
		if (call.kind == Testing::ManualStepLauncher::Call::kRelease) { releaseIndex = static_cast<int>(i); }
	}
	EXPECT_GE(waitIndex, 0);
	EXPECT_GT(releaseIndex, waitIndex);
	EXPECT_EQ(mStep.GetRunCount(), 1u);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mScheduler.GetChannelCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
}

TEST_F(DeferredStepSchedulerTest, UnregisterInFlightCompletedDiscardsWithoutApply)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteNext();
	mScheduler.Unregister(StringCRC("A"));
	EXPECT_EQ(mStep.GetRunCount(), 1u);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
}

TEST_F(DeferredStepSchedulerTest, DestroyAfterInFlightUnregisterReleasesExactlyOnce)
{
	Testing::ManualStepLauncher launcher;
	Testing::RecordingStep step;
	{
		DeferredStepScheduler scheduler(launcher);
		ASSERT_TRUE(scheduler.Register(StringCRC("A"), step));
		ASSERT_EQ(scheduler.Kick(StringCRC("A"), 1, 2), KickResult::kLaunched);
		scheduler.Unregister(StringCRC("A"));
	}
	EXPECT_EQ(launcher.GetReleaseCount(), 1u);
	EXPECT_EQ(launcher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(step.GetApplyCount(), 0u);
}
