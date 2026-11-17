#include <gtest/gtest.h>
#include <chrono>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

namespace
{
#ifdef DEBUG
	int gTickAssertCount = 0;
	void CountingAssertHandler(const char*, const char*, int, const char*, ...)
	{
		++gTickAssertCount;
	}
#endif

	// Records how many Wait calls the launcher had seen when Apply ran.
	class WaitProbeStep final : public IDeferredStep
	{
	public:
		explicit WaitProbeStep(const Testing::ManualStepLauncher& launcher) : mLauncher(launcher) {}
		void Capture() override {}
		void Run(IParallelExecutor&) override {}
		void Apply() override
		{
			++mApplyCount;
			for (uint32_t i = 0; i < mLauncher.GetCallCount(); ++i)
			{
				if (mLauncher.GetCall(i).kind == Testing::ManualStepLauncher::Call::kWait) { ++mWaitsAtApply; }
				if (mLauncher.GetCall(i).kind == Testing::ManualStepLauncher::Call::kRelease) { ++mReleasesAtApply; }
			}
		}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		uint32_t mApplyCount = 0;
		uint32_t mWaitsAtApply = 0;
		uint32_t mReleasesAtApply = 0;
	private:
		const Testing::ManualStepLauncher& mLauncher;
	};

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

// ---- Tick ----

namespace
{
	uint32_t CountCalls(const Testing::ManualStepLauncher& launcher, Testing::ManualStepLauncher::Call::Kind kind)
	{
		uint32_t n = 0;
		for (uint32_t i = 0; i < launcher.GetCallCount(); ++i)
		{
			if (launcher.GetCall(i).kind == kind) { ++n; }
		}
		return n;
	}
}

TEST_F(DeferredStepSchedulerTest, TickBelowApplyKeyDoesNotApplyEvenWhenComplete)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(4);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickAtApplyKeyAppliesOnceThenReleasesAndIdles)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mLauncher.ClearCalls();
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(CountCalls(mLauncher, Testing::ManualStepLauncher::Call::kWait), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.applies, 1u);
	EXPECT_EQ(mScheduler.GetStats().applies, 1u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickAboveApplyKeyApplies)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(6);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickAppliesInKickOrderNotRegistrationOrder)
{
	std::vector<int> order;
	Testing::RecordingStep a(1, &order), b(2, &order), c(3, &order);
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	ASSERT_TRUE(mScheduler.Register(StringCRC("C"), c));
	ASSERT_EQ(mScheduler.Kick(StringCRC("C"), 1, 5), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(5);
	ASSERT_EQ(order.size(), 3u);
	EXPECT_EQ(order[0], 3);
	EXPECT_EQ(order[1], 1);
	EXPECT_EQ(order[2], 2);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
	mScheduler.Unregister(StringCRC("C"));
}

TEST_F(DeferredStepSchedulerTest, TickKickOrderWinsOverApplyKeyOrder)
{
	std::vector<int> order;
	Testing::RecordingStep a(1, &order), b(2, &order);
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 9), KickResult::kLaunched);   // kicked first, later apply key
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 3), KickResult::kLaunched);   // kicked second, earlier apply key
	mLauncher.CompleteAll();
	mScheduler.Tick(10);
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0], 2);
	EXPECT_EQ(order[1], 1);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, TickJumpPastApplyKeyAppliesOnce)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(15);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickSameKeyTwiceAppliesOnce)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(5);
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickOnEmptySchedulerDoesNothing)
{
	mScheduler.Tick(100);
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().applies, 0u);
}

TEST_F(DeferredStepSchedulerTest, TickWithAllChannelsIdleMakesNoLauncherCalls)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), mStep));
	mScheduler.Tick(100);
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, TickBackwardsKeyAssertsAndAppliesNothing)
{
#ifdef DEBUG
	auto* prev = Dia::Core::g_pAssertFunc;
	Dia::Core::g_pAssertFunc = CountingAssertHandler;
	gTickAssertCount = 0;
#endif
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(10);   // applies
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 11), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(8);    // backwards: nothing applied
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
#ifdef DEBUG
	EXPECT_GT(gTickAssertCount, 0);
	Dia::Core::g_pAssertFunc = prev;
#endif
	mScheduler.Tick(11);   // forward again works
	EXPECT_EQ(mStep.GetApplyCount(), 2u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, KickLowerKickKeyAfterApplyRejectedNotMonotonic)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 11), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(11);
	ASSERT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	const uint32_t launches = mLauncher.GetLaunchCount();
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 9, 12), KickResult::kKeyNotMonotonic);
	EXPECT_EQ(mLauncher.GetLaunchCount(), launches);
	EXPECT_EQ(mStep.GetCaptureCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicksRejected, 1u);
	// Equal to the previous kickKey is accepted.
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 10, 12), KickResult::kLaunched);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, AlreadyCompleteLaunchStillRunningUntilTickAppliesAtApplyKey)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();   // launcher reports complete before any Tick
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kRunning);
	mScheduler.Tick(4);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TickIncompleteAtApplyKeyWaitsBeforeApply)
{
	WaitProbeStep probe(mLauncher);
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), probe));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.ClearCalls();
	mScheduler.Tick(5);
	EXPECT_EQ(probe.mApplyCount, 1u);
	EXPECT_EQ(probe.mWaitsAtApply, 1u);
	EXPECT_EQ(probe.mReleasesAtApply, 0u);   // Release only after Apply
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, StallCountersOnSchedulerAndChannelWithMeasuredDuration)
{
	mStep.SetOnRun([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.stalls, 1u);
	EXPECT_GE(cs.lastStallMicros, 1000u);
	EXPECT_GE(cs.totalStallMicros, cs.lastStallMicros);
	EXPECT_EQ(cs.totalStallMicros, cs.lastStallMicros);
	EXPECT_EQ(mScheduler.GetStats().totalStallMicros, cs.totalStallMicros);
	// TODO(task9): assert exactly one Warning naming channel "A" (no reusable log capture outside DiaCellLayers).
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TwoStallsAccumulate)
{
	mStep.SetOnRun([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	ChannelStats first;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), first));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 6, 9), KickResult::kLaunched);
	mScheduler.Tick(9);
	ChannelStats second;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), second));
	EXPECT_EQ(second.stalls, 2u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 2u);
	EXPECT_GE(second.lastStallMicros, 1000u);
	EXPECT_EQ(second.totalStallMicros, first.lastStallMicros + second.lastStallMicros);
	EXPECT_EQ(mScheduler.GetStats().totalStallMicros, second.totalStallMicros);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, CompleteBeforeTickNoStallAndNoWait)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mLauncher.ClearCalls();
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	EXPECT_EQ(mScheduler.GetStats().totalStallMicros, 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.stalls, 0u);
	EXPECT_EQ(cs.lastStallMicros, 0u);
	EXPECT_EQ(cs.totalStallMicros, 0u);
	for (uint32_t i = 0; i < mLauncher.GetCallCount(); ++i)
	{
		EXPECT_NE(mLauncher.GetCall(i).kind, Testing::ManualStepLauncher::Call::kWait);
	}
	// TODO(task9): assert zero log records for a non-stall.
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, CompletedAtKeyBeforeApplyKeyNoStall)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(4);
	mLauncher.CompleteAll();   // completes just before the apply key
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.stalls, 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, StallOnOneChannelDoesNotAffectAnother)
{
	Testing::RecordingStep other;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), other));
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 5), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	ASSERT_TRUE(mLauncher.CompleteNext());   // oldest launch is B; A stays incomplete
	mScheduler.Tick(5);
	ChannelStats a, b;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("B"), b));
	EXPECT_EQ(a.stalls, 1u);
	EXPECT_EQ(b.stalls, 0u);
	EXPECT_EQ(b.totalStallMicros, 0u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, ApplyKeyUnchangedAfterStallAndNextKickWorks)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(7);   // late tick: stalls, applies once
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 8, 10), KickResult::kLaunched);
	mScheduler.Tick(9);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	mScheduler.Tick(10);
	EXPECT_EQ(mStep.GetApplyCount(), 2u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 2u);
	mScheduler.Unregister(StringCRC("A"));
}

// ---- Poll pass: completion observation, slack and run time (AC 23, 24) ----

namespace
{
	uint32_t CountIsComplete(const Testing::ManualStepLauncher& launcher, uint64_t tokenValue)
	{
		uint32_t n = 0;
		for (uint32_t i = 0; i < launcher.GetCallCount(); ++i)
		{
			const auto& c = launcher.GetCall(i);
			if (c.kind == Testing::ManualStepLauncher::Call::kIsComplete && c.token.value == tokenValue) { ++n; }
		}
		return n;
	}
}

TEST_F(DeferredStepSchedulerTest, PollEarlyCompletionRecordsSlackAndPollsOnce)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mLauncher.ClearCalls();
	mScheduler.Tick(12);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 3);
	mLauncher.ClearCalls();
	mScheduler.Tick(13);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 0u);
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 3);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, CompletionFirstSeenOnApplyKeyHasZeroSlackNoStall)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(15);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 0);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, CompletionSeenOneKeyEarlyHasSlackOne)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mScheduler.Tick(13);
	mLauncher.CompleteAll();
	mScheduler.Tick(14);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 1);
	mScheduler.Tick(15);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 1);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, LateStepHasZeroSlackAndStalls)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mScheduler.Tick(15);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 0);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, IncompleteStepIsPolledEachTickUntilComplete)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 20), KickResult::kLaunched);
	mLauncher.ClearCalls();
	mScheduler.Tick(12);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	mLauncher.ClearCalls();
	mScheduler.Tick(13);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	mLauncher.CompleteAll();
	mLauncher.ClearCalls();
	mScheduler.Tick(14);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 6);
	mLauncher.ClearCalls();
	mScheduler.Tick(15);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, LastRunMicrosSpansKickToFirstObservationAndDoesNotGrow)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 20), KickResult::kLaunched);
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	mLauncher.CompleteAll();
	mScheduler.Tick(12);
	ChannelStats first;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), first));
	EXPECT_GE(first.lastRunMicros, 1000u);
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	mScheduler.Tick(13);
	ChannelStats second;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), second));
	EXPECT_EQ(second.lastRunMicros, first.lastRunMicros);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, StalledStepRecordsLastRunMicrosAfterWait)
{
	mStep.SetOnRun([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mScheduler.Tick(15);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_GE(cs.lastRunMicros, 1000u);
	EXPECT_EQ(cs.lastSlackKeys, 0);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerTest, TwoChannelsPolledIndependently)
{
	Testing::RecordingStep other;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), other));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 20), KickResult::kLaunched);   // token 1
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 10, 20), KickResult::kLaunched);   // token 2
	ASSERT_TRUE(mLauncher.CompleteNext());   // A completes
	mLauncher.ClearCalls();
	mScheduler.Tick(12);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	EXPECT_EQ(CountIsComplete(mLauncher, 2), 1u);
	ChannelStats a, b;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("B"), b));
	EXPECT_EQ(a.lastSlackKeys, 8);
	EXPECT_EQ(b.lastSlackKeys, 0);
	mLauncher.ClearCalls();
	mScheduler.Tick(13);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 0u);
	EXPECT_EQ(CountIsComplete(mLauncher, 2), 1u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerTest, DueChannelIsCheckedOncePerTickNotDoublePolled)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 15), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mLauncher.ClearCalls();
	mScheduler.Tick(15);
	EXPECT_EQ(CountIsComplete(mLauncher, 1), 1u);
	mScheduler.Unregister(StringCRC("A"));
}
