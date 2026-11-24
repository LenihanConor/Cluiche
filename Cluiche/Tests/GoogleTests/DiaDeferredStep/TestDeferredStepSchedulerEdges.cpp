// Exhaustive-pass edge tests for DeferredStepScheduler: key extremes, re-entrancy from inside Apply, stats invariants,
// token-leak checks and launchers that return already-complete tokens. Complements TestDeferredStepScheduler.cpp.

#include <gtest/gtest.h>
#include <functional>
#include <vector>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>

using namespace Dia::DeferredStep;
using Dia::Core::StringCRC;

namespace
{
	const StepKey kMaxKey = UINT64_MAX;

#ifdef DEBUG
	int gEdgeAssertCount = 0;
	void EdgeCountingAssertHandler(const char*, const char*, int, const char*, ...)
	{
		++gEdgeAssertCount;
	}
#endif

	// Step whose Apply runs a caller-supplied hook, so tests can re-enter the scheduler from inside Apply.
	class HookStep final : public IDeferredStep
	{
	public:
		void Capture() override { ++mCaptureCount; }
		void Run(Dia::ParallelExecutor::IParallelExecutor&) override { ++mRunCount; }
		void Apply() override
		{
			++mApplyCount;
			if (mOnApply)
			{
				mOnApply();
			}
		}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		std::function<void()> mOnApply;
		uint32_t mCaptureCount = 0;
		uint32_t mRunCount = 0;
		uint32_t mApplyCount = 0;
	};

	// Launcher that runs the step inline inside Launch and hands back an already-complete token.
	class InlineCompleteLauncher final : public IStepLauncher
	{
	public:
		LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) override
		{
			const LaunchToken token = mInner.Launch(step, info);
			mInner.Wait(token);   // runs the step now; marks complete
			return token;
		}
		bool IsComplete(LaunchToken token) const override { return mInner.IsComplete(token); }
		void Wait(LaunchToken token) override { ++mWaitCalls; mInner.Wait(token); }
		void Release(LaunchToken token) override { mInner.Release(token); }

		Testing::ManualStepLauncher mInner;
		uint32_t mWaitCalls = 0;   // scheduler-issued Waits only
	};

	// SchedulerStats.inFlight must equal the number of listed channels that are not idle (i.e. hold a token).
	::testing::AssertionResult InFlightMatchesTokenHolders(const DeferredStepScheduler& scheduler, const char* const* names, unsigned count)
	{
		unsigned holders = 0;
		for (unsigned i = 0; i < count; ++i)
		{
			if (scheduler.GetState(StringCRC(names[i])) != StepState::kIdle)
			{
				++holders;
			}
		}
		if (scheduler.GetStats().inFlight != holders)
		{
			return ::testing::AssertionFailure() << "inFlight " << scheduler.GetStats().inFlight << " but " << holders << " channels hold a token";
		}
		return ::testing::AssertionSuccess();
	}
}

class DeferredStepSchedulerEdgeTest : public ::testing::Test
{
protected:
	DeferredStepSchedulerEdgeTest() : mScheduler(mLauncher) {}

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	Testing::RecordingStep mStep;
};

// ---- StepKey extremes ----

TEST_F(DeferredStepSchedulerEdgeTest, MaxKickKeyAndMaxApplyKeyAppliesAtTickMax)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), kMaxKey, kMaxKey), KickResult::kLaunched);
	StepKey key = 0;
	ASSERT_TRUE(mScheduler.GetApplyKey(StringCRC("A"), key));
	EXPECT_EQ(key, kMaxKey);
	mLauncher.CompleteAll();
	mScheduler.Tick(kMaxKey);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, MaxKickKeyThenSameKeyKickOnSameChannelAccepted)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), kMaxKey, kMaxKey), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(kMaxKey);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), kMaxKey, kMaxKey), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(kMaxKey);   // same key as the previous Tick is not backwards
	EXPECT_EQ(mStep.GetApplyCount(), 2u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 2u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, MaxMinusOneKickWithMaxApplyKeyNotDueUntilTickMax)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), kMaxKey - 1, kMaxKey), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(kMaxKey - 1);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.lastSlackKeys, 1);
	mScheduler.Tick(kMaxKey);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, MaxApplyKeyFromZeroNotDueAtZeroAndPinnedSlackOverflowsToMinusOne)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 0, kMaxKey), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(0);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	// Pinned: slack is applyKey - key cast to int64, so a distance above INT64_MAX wraps negative.
	EXPECT_EQ(cs.lastSlackKeys, -1);
	mScheduler.Tick(kMaxKey);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, ZeroKickZeroApplyTickZeroAppliesWithoutStallWhenComplete)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 0, 0), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(0);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, ApplyKeyEqualsKickKeyIncompleteStallsOnceAtNextTickThenNothing)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 5, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	mScheduler.Tick(5);
	mScheduler.Tick(6);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 1u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, ApplyKeyEqualsKickKeyCompleteAppliesAtNextTickWithoutStall)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 5, 5), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().stalls, 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, PinnedApplyKeyBehindLastTickAppliesAtNextTickEvenAtSameKey)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	mScheduler.Tick(10);
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 3, 3), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(10);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
}

// ---- Register / DrainAll interplay ----

TEST_F(DeferredStepSchedulerEdgeTest, RegisterAfterDrainAllWorksAndNewChannelIsIdle)
{
	Testing::RecordingStep other;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.DrainAll();
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), other));
	EXPECT_EQ(mScheduler.GetState(StringCRC("B")), StepState::kIdle);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kComplete);
	StepKey key = 0;
	EXPECT_FALSE(mScheduler.GetApplyKey(StringCRC("B"), key));
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 2, 6), KickResult::kLaunched);
	mScheduler.DrainAll();   // only B is still kRunning
	EXPECT_EQ(mScheduler.GetStats().drains, 2u);
	EXPECT_EQ(mScheduler.GetStats().drainedSteps, 2u);
	mScheduler.Tick(6);
	EXPECT_EQ(mStep.GetApplyCount(), 1u);
	EXPECT_EQ(other.GetApplyCount(), 1u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("B"));
}

// ---- Re-entrancy from inside Apply ----

TEST_F(DeferredStepSchedulerEdgeTest, UnregisterOtherChannelFromInsideApplyDiscardsItAndLeavesCallerIdle)
{
	HookStep a;
	Testing::RecordingStep b;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	a.mOnApply = [this] { mScheduler.Unregister(StringCRC("B")); };
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(a.mApplyCount, 1u);
	EXPECT_EQ(b.GetRunCount(), 1u);     // Unregister waited for it
	EXPECT_EQ(b.GetApplyCount(), 0u);   // discarded, never applied
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 2u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	EXPECT_EQ(mScheduler.GetStats().applies, 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, UnregisterEarlierKickedChannelFromLaterApplyDoesNotDoubleApply)
{
	HookStep a;
	Testing::RecordingStep b;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	a.mOnApply = [this] { mScheduler.Unregister(StringCRC("B")); };
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 5), KickResult::kLaunched);   // B kicked first: applied before A runs
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(b.GetApplyCount(), 1u);
	EXPECT_EQ(a.mApplyCount, 1u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 2u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, UnregisterOwnChannelFromInsideApplyReleasesOnceAndKeepsInFlightCount)
{
	Testing::LogCapture log;
	HookStep a;
	Testing::RecordingStep b;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	a.mOnApply = [this] { mScheduler.Unregister(StringCRC("A")); };
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 100), KickResult::kLaunched);
	mLauncher.CompleteNext();   // A completes
	mScheduler.Tick(5);
	EXPECT_EQ(a.mApplyCount, 1u);
	EXPECT_EQ(mScheduler.GetChannelCount(), 1u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);        // A's token released exactly once
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 1u);     // B still in flight; not under-counted
	EXPECT_EQ(mScheduler.GetStats().applies, 1u);
	// Pinned: the nested Unregister sees a completed, not-yet-settled result and logs the discard warning.
	EXPECT_EQ(log.Warnings("discarding"), 1u);
	mScheduler.Unregister(StringCRC("B"));
}

TEST_F(DeferredStepSchedulerEdgeTest, KickAnotherChannelFromInsideApplyIsNotAppliedInSameTick)
{
	HookStep a;
	Testing::RecordingStep c;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	ASSERT_TRUE(mScheduler.Register(StringCRC("C"), c));
	KickResult nested = KickResult::kUnknownChannel;
	a.mOnApply = [this, &nested] { nested = mScheduler.Kick(StringCRC("C"), 5, 5); };
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(nested, KickResult::kLaunched);
	// Pinned: the due set is fixed at the start of Tick, so a step kicked from Apply waits for the next Tick even if due.
	EXPECT_EQ(c.GetApplyCount(), 0u);
	EXPECT_EQ(mScheduler.GetState(StringCRC("C")), StepState::kRunning);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 1u);
	mScheduler.Tick(5);
	EXPECT_EQ(c.GetApplyCount(), 1u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("C"));
}

TEST_F(DeferredStepSchedulerEdgeTest, KickSameChannelFromInsideItsOwnApplyIsRejectedAlreadyInFlight)
{
	HookStep a;
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), a));
	KickResult nested = KickResult::kLaunched;
	a.mOnApply = [this, &nested] { nested = mScheduler.Kick(StringCRC("A"), 6, 9); };
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Tick(5);
	EXPECT_EQ(nested, KickResult::kAlreadyInFlight);   // pinned: the channel is still in flight during its own Apply
	EXPECT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 6, 9), KickResult::kLaunched);
	mScheduler.Unregister(StringCRC("A"));
}

// ---- Ordering and unregister interplay ----

TEST_F(DeferredStepSchedulerEdgeTest, EightChannelsDueOnOneKeyApplyInKickOrderExactlyOnce)
{
	const char* names[8] = { "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7" };
	const int kickOrder[8] = { 5, 2, 7, 0, 3, 6, 1, 4 };
	std::vector<int> order;
	std::vector<Testing::RecordingStep> steps;
	steps.reserve(8);
	for (int i = 0; i < 8; ++i)
	{
		steps.emplace_back(i, &order);
	}
	for (int i = 0; i < 8; ++i)
	{
		ASSERT_TRUE(mScheduler.Register(StringCRC(names[i]), steps[i]));
	}
	for (int i = 0; i < 8; ++i)
	{
		ASSERT_EQ(mScheduler.Kick(StringCRC(names[kickOrder[i]]), 1, 10), KickResult::kLaunched);
	}
	mLauncher.CompleteAll();
	mScheduler.Tick(10);
	ASSERT_EQ(order.size(), 8u);
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(order[i], kickOrder[i]);
		EXPECT_EQ(steps[i].GetApplyCount(), 1u);
	}
	mScheduler.Tick(10);
	EXPECT_EQ(order.size(), 8u);
	EXPECT_EQ(mScheduler.GetStats().applies, 8u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 8u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	for (int i = 0; i < 8; ++i)
	{
		mScheduler.Unregister(StringCRC(names[i]));
	}
}

TEST_F(DeferredStepSchedulerEdgeTest, EightChannelsAllStallingStillApplyInKickOrder)
{
	const char* names[8] = { "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7" };
	const int kickOrder[8] = { 3, 7, 1, 5, 0, 6, 2, 4 };
	std::vector<int> order;
	std::vector<Testing::RecordingStep> steps;
	steps.reserve(8);
	for (int i = 0; i < 8; ++i)
	{
		steps.emplace_back(i, &order);
		ASSERT_TRUE(mScheduler.Register(StringCRC(names[i]), steps[i]));
	}
	for (int i = 0; i < 8; ++i)
	{
		ASSERT_EQ(mScheduler.Kick(StringCRC(names[kickOrder[i]]), 1, 10), KickResult::kLaunched);
	}
	mScheduler.Tick(10);   // nothing completed: every channel stalls
	ASSERT_EQ(order.size(), 8u);
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(order[i], kickOrder[i]);
	}
	EXPECT_EQ(mScheduler.GetStats().stalls, 8u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	for (int i = 0; i < 8; ++i)
	{
		mScheduler.Unregister(StringCRC(names[i]));
	}
}

TEST_F(DeferredStepSchedulerEdgeTest, TickAfterUnregisterOfDueChannelDoesNothingForIt)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	mScheduler.Unregister(StringCRC("A"));
	mLauncher.ClearCalls();
	mScheduler.Tick(5);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(mLauncher.GetCallCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().applies, 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 1u);   // only the Unregister release
}

// ---- Stats ----

TEST_F(DeferredStepSchedulerEdgeTest, GetChannelStatsReturnsSnapshotCopy)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	ChannelStats copy;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), copy));
	copy.kicks = 999;
	copy.applies = 999;
	copy.lastSlackKeys = -5;
	ChannelStats fresh;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), fresh));
	EXPECT_EQ(fresh.kicks, 1u);
	EXPECT_EQ(fresh.applies, 0u);
	EXPECT_EQ(fresh.lastSlackKeys, 0);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, KickAfterStallWorksAndAccumulatesBothStalls)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 2), KickResult::kLaunched);
	mScheduler.Tick(2);   // stall
	ASSERT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle);
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 3, 4), KickResult::kLaunched);
	mScheduler.Tick(4);   // stall again
	EXPECT_EQ(mStep.GetApplyCount(), 2u);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.stalls, 2u);
	EXPECT_EQ(cs.kicks, 2u);
	EXPECT_EQ(cs.applies, 2u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 2u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, KicksRejectedAccumulatesAndUnknownChannelCountsOnlyOnScheduler)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 20), KickResult::kLaunched);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 11, 21), KickResult::kAlreadyInFlight);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 12, 22), KickResult::kAlreadyInFlight);
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 13, 12), KickResult::kAlreadyInFlight);   // in-flight check precedes key checks
	EXPECT_EQ(mScheduler.Kick(StringCRC("Nope"), 1, 2), KickResult::kUnknownChannel);
	EXPECT_EQ(mScheduler.Kick(StringCRC("Nope2"), 1, 2), KickResult::kUnknownChannel);
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicksRejected, 3u);
	EXPECT_EQ(cs.kicks, 1u);
	EXPECT_EQ(mScheduler.GetStats().kicksRejected, 5u);
	EXPECT_EQ(mScheduler.GetStats().kicks, 1u);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, CheckOrderApplyBeforeKickReportedBeforeNotMonotonic)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 10, 11), KickResult::kLaunched);
	mLauncher.CompleteAll();
	mScheduler.Tick(11);
	// Both rules violated (kickKey < floor and applyKey < kickKey): applyKey rule is reported.
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 5, 4), KickResult::kApplyBeforeKick);
	mScheduler.Unregister(StringCRC("A"));
}

TEST_F(DeferredStepSchedulerEdgeTest, InFlightEqualsTokenHoldersAcrossScriptedSequence)
{
	const char* names[3] = { "A", "B", "C" };
	Testing::RecordingStep b, c;
	ASSERT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	ASSERT_TRUE(mScheduler.Register(StringCRC("B"), b));
	ASSERT_TRUE(mScheduler.Register(StringCRC("C"), c));
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));

	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 1, 4), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("B"), 1, 6), KickResult::kLaunched);
	ASSERT_EQ(mScheduler.Kick(StringCRC("C"), 1, 8), KickResult::kLaunched);
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));
	EXPECT_EQ(mScheduler.Kick(StringCRC("A"), 2, 9), KickResult::kAlreadyInFlight);   // rejected: no change
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));

	mScheduler.DrainAll();
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));
	EXPECT_EQ(mScheduler.GetStats().inFlight, 3u);

	mScheduler.Tick(4);   // A applied
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, names, 3));
	EXPECT_EQ(mScheduler.GetStats().inFlight, 2u);

	mScheduler.Unregister(StringCRC("B"));
	EXPECT_EQ(mScheduler.GetStats().inFlight, 1u);
	const char* remaining[2] = { "A", "C" };
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, remaining, 2));

	ASSERT_EQ(mScheduler.Kick(StringCRC("A"), 5, 9), KickResult::kLaunched);
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, remaining, 2));
	mScheduler.Tick(9);   // C (kComplete) and A (stall) both applied
	EXPECT_TRUE(InFlightMatchesTokenHolders(mScheduler, remaining, 2));
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), mLauncher.GetLaunchCount());   // every launched token released exactly once
	mScheduler.Unregister(StringCRC("A"));
	mScheduler.Unregister(StringCRC("C"));
}

TEST_F(DeferredStepSchedulerEdgeTest, TwentyKickApplyCyclesOnOneChannelLeakNoTokens)
{
	ASSERT_TRUE(mScheduler.Register(StringCRC("A"), mStep));
	for (StepKey i = 0; i < 20; ++i)
	{
		const StepKey kick = i * 3;
		const StepKey apply = kick + 2;
		ASSERT_EQ(mScheduler.Kick(StringCRC("A"), kick, apply), KickResult::kLaunched) << "cycle " << i;
		if ((i % 2) == 0)
		{
			mLauncher.CompleteAll();
		}
		mScheduler.Tick(apply);
		ASSERT_EQ(mScheduler.GetState(StringCRC("A")), StepState::kIdle) << "cycle " << i;
	}
	ChannelStats cs;
	ASSERT_TRUE(mScheduler.GetChannelStats(StringCRC("A"), cs));
	EXPECT_EQ(cs.kicks, 20u);
	EXPECT_EQ(cs.applies, 20u);
	EXPECT_EQ(cs.stalls, 10u);
	EXPECT_EQ(mScheduler.GetStats().kicks, 20u);
	EXPECT_EQ(mScheduler.GetStats().applies, 20u);
	EXPECT_EQ(mScheduler.GetStats().inFlight, 0u);
	EXPECT_EQ(mLauncher.GetLaunchCount(), 20u);
	EXPECT_EQ(mLauncher.GetReleaseCount(), 20u);
	EXPECT_EQ(mLauncher.GetBadReleaseCount(), 0u);
	EXPECT_EQ(mStep.GetCaptureCount(), 20u);
	EXPECT_EQ(mStep.GetRunCount(), 20u);
	EXPECT_EQ(mStep.GetApplyCount(), 20u);
	mScheduler.Unregister(StringCRC("A"));
	EXPECT_EQ(mLauncher.GetReleaseCount(), 20u);
}

// ---- Launchers that return an already-complete token ----

TEST(DeferredStepSchedulerEdgeInline, InlineCompleteLauncherFullLifecycleWithDrainAll)
{
	InlineCompleteLauncher launcher;
	Testing::RecordingStep step;
	{
		DeferredStepScheduler scheduler(launcher);
		ASSERT_TRUE(scheduler.Register(StringCRC("A"), step));
		ASSERT_EQ(scheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
		EXPECT_EQ(step.GetRunCount(), 1u);                              // ran inside Launch
		EXPECT_EQ(scheduler.GetState(StringCRC("A")), StepState::kRunning);
		scheduler.Tick(3);                                              // poll observes completion early
		ChannelStats cs;
		ASSERT_TRUE(scheduler.GetChannelStats(StringCRC("A"), cs));
		EXPECT_EQ(cs.lastSlackKeys, 2);
		scheduler.DrainAll();
		EXPECT_EQ(launcher.mWaitCalls, 1u);                             // Wait still called on a complete token
		EXPECT_EQ(scheduler.GetState(StringCRC("A")), StepState::kComplete);
		EXPECT_EQ(scheduler.GetStats().drainedSteps, 1u);
		scheduler.Tick(5);
		EXPECT_EQ(step.GetApplyCount(), 1u);
		EXPECT_EQ(step.GetRunCount(), 1u);
		EXPECT_EQ(scheduler.GetStats().stalls, 0u);
		EXPECT_EQ(launcher.mWaitCalls, 1u);                             // no stall wait
		EXPECT_EQ(launcher.mInner.GetReleaseCount(), 1u);
		EXPECT_EQ(launcher.mInner.GetBadReleaseCount(), 0u);
		EXPECT_EQ(scheduler.GetState(StringCRC("A")), StepState::kIdle);
		scheduler.Unregister(StringCRC("A"));
	}
	EXPECT_EQ(launcher.mInner.GetReleaseCount(), 1u);
}

TEST(DeferredStepSchedulerEdgeInline, InlineCompleteLauncherUnregisterInFlightWaitsReleasesAndNeverApplies)
{
	InlineCompleteLauncher launcher;
	Testing::RecordingStep step;
	DeferredStepScheduler scheduler(launcher);
	Testing::LogCapture log;
	ASSERT_TRUE(scheduler.Register(StringCRC("A"), step));
	ASSERT_EQ(scheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
	scheduler.Unregister(StringCRC("A"));
	EXPECT_EQ(launcher.mWaitCalls, 1u);
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(launcher.mInner.GetReleaseCount(), 1u);
	EXPECT_EQ(launcher.mInner.GetBadReleaseCount(), 0u);
	EXPECT_EQ(log.Warnings("discarding"), 1u);
}

TEST(DeferredStepSchedulerEdgeInline, InlineCompleteLauncherRepeatedCyclesNeverStall)
{
	InlineCompleteLauncher launcher;
	Testing::RecordingStep step;
	DeferredStepScheduler scheduler(launcher);
	ASSERT_TRUE(scheduler.Register(StringCRC("A"), step));
	for (StepKey i = 0; i < 10; ++i)
	{
		ASSERT_EQ(scheduler.Kick(StringCRC("A"), i * 2, i * 2 + 1), KickResult::kLaunched);
		scheduler.Tick(i * 2 + 1);
	}
	EXPECT_EQ(step.GetApplyCount(), 10u);
	EXPECT_EQ(scheduler.GetStats().stalls, 0u);
	EXPECT_EQ(launcher.mWaitCalls, 0u);
	EXPECT_EQ(launcher.mInner.GetReleaseCount(), 10u);
	EXPECT_EQ(launcher.mInner.GetBadReleaseCount(), 0u);
	scheduler.Unregister(StringCRC("A"));
}

// ---- Destruction ----

TEST(DeferredStepSchedulerEdgeDestructor, DestructorDrainsInFlightRegisteredChannelsAndAssertsInDebug)
{
	Testing::ManualStepLauncher launcher;
	Testing::RecordingStep a, b;
#ifdef DEBUG
	auto* prev = Dia::Core::g_pAssertFunc;
	Dia::Core::g_pAssertFunc = EdgeCountingAssertHandler;
	gEdgeAssertCount = 0;
#endif
	{
		DeferredStepScheduler scheduler(launcher);
		ASSERT_TRUE(scheduler.Register(StringCRC("A"), a));
		ASSERT_TRUE(scheduler.Register(StringCRC("B"), b));
		ASSERT_EQ(scheduler.Kick(StringCRC("A"), 1, 5), KickResult::kLaunched);
		ASSERT_EQ(scheduler.Kick(StringCRC("B"), 1, 5), KickResult::kLaunched);
	}
#ifdef DEBUG
	EXPECT_GT(gEdgeAssertCount, 0);
	Dia::Core::g_pAssertFunc = prev;
#endif
	EXPECT_EQ(a.GetRunCount(), 1u);
	EXPECT_EQ(b.GetRunCount(), 1u);
	EXPECT_EQ(a.GetApplyCount(), 0u);
	EXPECT_EQ(b.GetApplyCount(), 0u);
	EXPECT_EQ(launcher.GetReleaseCount(), 2u);
	EXPECT_EQ(launcher.GetBadReleaseCount(), 0u);
}
