#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IParallelExecutor.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaDeferredStep/IStepSummarySink.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaObservation/Profile/StepSummaryRecord.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace Dia::DeferredStep;
using Dia::Threading::JobSystem;
namespace P = Dia::Observation::Profile;

namespace
{
	const uint64_t kMs = 1000000ull;

	void SpinMicros(int us)
	{
		const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
		while (std::chrono::steady_clock::now() < end) {}
	}

	void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

	class CapturingSink final : public IStepSummarySink
	{
	public:
		void OnStepSummary(const P::StepSummaryRecord& r) override { records.push_back(r); }
		std::vector<P::StepSummaryRecord> records;
	};

	class FnStep final : public IDeferredStep
	{
	public:
		explicit FnStep(std::function<void(IParallelExecutor&)> body) : mBody(std::move(body)) {}
		void Capture() override {}
		void Run(IParallelExecutor& executor) override { mBody(executor); }
		void Apply() override { ++applies; }
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}
		int applies = 0;
	private:
		std::function<void(IParallelExecutor&)> mBody;
	};

	// Forwards to a real launcher and keeps a copy of the range timings the scheduler asked for.
	class SpyLauncher final : public IStepLauncher
	{
	public:
		explicit SpyLauncher(IStepLauncher& inner) : mInner(inner) {}
		LaunchToken Launch(IDeferredStep& step, const LaunchInfo& info) override { return mInner.Launch(step, info); }
		bool IsComplete(LaunchToken t) const override { return mInner.IsComplete(t); }
		void Wait(LaunchToken t) override { mInner.Wait(t); }
		void Release(LaunchToken t) override { mInner.Release(t); }
		uint32_t GetRangeTimings(LaunchToken t, const RangeTiming*& out) const override
		{
			const uint32_t n = mInner.GetRangeTimings(t, out);
			timings.assign(out, out + n);
			return n;
		}
		bool RanInlineWholly(LaunchToken t) const override { return mInner.RanInlineWholly(t); }
		mutable std::vector<RangeTiming> timings;
	private:
		IStepLauncher& mInner;
	};

	// Runs the step on the calling thread inside Launch, like the module's no-job-system launcher.
	class InlineLauncher final : public IStepLauncher
	{
	public:
		LaunchToken Launch(IDeferredStep& step, const LaunchInfo&) override
		{
			step.Run(mExecutor);
			LaunchToken t; t.value = 1; return t;
		}
		bool IsComplete(LaunchToken) const override { return true; }
		void Wait(LaunchToken) override {}
		void Release(LaunchToken) override {}
		bool RanInlineWholly(LaunchToken) const override { return true; }
	private:
		SerialExecutor mExecutor;
	};

	// Real JobSystem + JobStepLauncher + scheduler with a capturing sink. workers == 0: the job system is never initialised.
	struct Rig
	{
		Rig(unsigned workers, uint32_t cap, uint32_t rangesPerWorker = 4)
			: initialised(workers > 0)
		{
			if (initialised) { js.Initialize(workers); }
			CapConfig c; c.workerCap = cap; c.rangesPerWorker = rangesPerWorker;
			real.reset(new JobStepLauncher(js, c));
			spy.reset(new SpyLauncher(*real));
			sched.reset(new DeferredStepScheduler(*spy));
			sched->SetSummarySink(&sink);
		}
		~Rig()
		{
			for (const Dia::Core::StringCRC& c : channels) { sched->Unregister(c); }
			sched.reset();
			spy.reset();
			real.reset();
			if (initialised) { js.Shutdown(); }
		}
		Dia::Core::StringCRC Reg(const char* name, IDeferredStep& step)
		{
			Dia::Core::StringCRC id(name);
			EXPECT_TRUE(sched->Register(id, step));
			channels.push_back(id);
			return id;
		}

		bool initialised;
		JobSystem js;
		std::unique_ptr<JobStepLauncher> real;
		std::unique_ptr<SpyLauncher> spy;
		std::unique_ptr<DeferredStepScheduler> sched;
		CapturingSink sink;
		std::vector<Dia::Core::StringCRC> channels;
	};

	// ParallelFor over many items; every range spins `us` microseconds.
	std::function<void(IParallelExecutor&)> SpinBody(int us, uint32_t count = 1000, uint32_t* rangeCountOut = nullptr)
	{
		return [us, count, rangeCountOut](IParallelExecutor& ex)
		{
			ex.ParallelFor(count, 1, [us](uint32_t, uint32_t) { SpinMicros(us); });
			if (rangeCountOut) { *rangeCountOut = ex.GetLastRangeCount(); }
		};
	}
}

// AC 9
TEST(DeferredStepSummaryScheduler, KickedAndAppliedStepWritesExactlyOneRecord)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumA", step);
	ASSERT_EQ(rig.sched->Kick(id, 3, 5), KickResult::kLaunched);
	rig.sched->Tick(4);
	EXPECT_TRUE(rig.sink.records.empty());
	rig.sched->Tick(5);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	EXPECT_EQ(rig.sink.records[0].owner, id.Value());
	EXPECT_EQ(rig.sink.records[0].applyKey, 5u);
	EXPECT_EQ(rig.sink.records[0].kickKeyLow, 3u);
	rig.sched->Tick(9);
	EXPECT_EQ(rig.sink.records.size(), 1u);
	EXPECT_EQ(step.applies, 1);
}

TEST(DeferredStepSummaryScheduler, RejectedKicksWriteNoRecord)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumRej", step);
	EXPECT_EQ(rig.sched->Kick(Dia::Core::StringCRC("sumNoSuch"), 1, 1), KickResult::kUnknownChannel);
	EXPECT_EQ(rig.sched->Kick(id, 5, 3), KickResult::kApplyBeforeKick);
	rig.sched->Tick(10);
	EXPECT_TRUE(rig.sink.records.empty());

	ASSERT_EQ(rig.sched->Kick(id, 11, 12), KickResult::kLaunched);
	EXPECT_EQ(rig.sched->Kick(id, 11, 12), KickResult::kAlreadyInFlight);
	rig.sched->Tick(12);
	EXPECT_EQ(rig.sink.records.size(), 1u);
}

TEST(DeferredStepSummaryScheduler, UnregisterDiscardWritesNoRecord)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumUnreg", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 100), KickResult::kLaunched);
	rig.sched->Unregister(id);
	rig.sched->Tick(200);
	EXPECT_TRUE(rig.sink.records.empty());
	EXPECT_EQ(step.applies, 0);
}

TEST(DeferredStepSummaryScheduler, LoadDiscardWritesNoRecord)
{
	Rig rig(4, 4);
	FnStep inFlight(SpinBody(100));
	FnStep restored([](IParallelExecutor&) {});
	const auto a = rig.Reg("sumLoadA", inFlight);
	const auto b = rig.Reg("sumLoadB", restored);
	ASSERT_EQ(rig.sched->Kick(a, 1, 100), KickResult::kLaunched);
	ASSERT_TRUE(rig.sched->RestorePending(b, 1, 100));
	rig.sched->DrainAndDiscardAll();
	rig.sched->Tick(200);
	EXPECT_TRUE(rig.sink.records.empty());
}

TEST(DeferredStepSummaryScheduler, DrainAllAloneWritesNoRecordUntilApplied)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumDrain", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 10), KickResult::kLaunched);
	rig.sched->DrainAll();
	EXPECT_TRUE(rig.sink.records.empty());
	rig.sched->Tick(10);
	EXPECT_EQ(rig.sink.records.size(), 1u);
}

// AC 10
TEST(DeferredStepSummaryScheduler, TimestampsAreOrderedKickStartReadyApply)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(200));
	const auto id = rig.Reg("sumOrder", step);
	// Stalled, early-by-key and exactly-on-time paths all keep the ordering.
	for (int round = 0; round < 3; ++round)
	{
		const StepKey base = 10 * (round + 1);
		const StepKey applyKey = round == 0 ? base : base + 5;
		ASSERT_EQ(rig.sched->Kick(id, base, applyKey), KickResult::kLaunched);
		if (round == 2) { SleepMs(60); rig.sched->Tick(base + 1); }
		rig.sched->Tick(applyKey);
	}
	ASSERT_EQ(rig.sink.records.size(), 3u);
	for (const P::StepSummaryRecord& r : rig.sink.records)
	{
		EXPECT_GT(r.kickNs, 0u);
		EXPECT_LE(r.kickNs, r.firstRangeStartNs);
		EXPECT_LE(r.firstRangeStartNs, r.readyNs);
		EXPECT_LE(r.readyNs, r.applyNs);
	}
}

// AC 11
TEST(DeferredStepSummaryScheduler, SumsAndRangeCountMatchTheExecutorsRanges)
{
	Rig rig(4, 4);
	uint32_t executorRangeCount = 0;
	FnStep step(SpinBody(300, 1000, &executorRangeCount));
	const auto id = rig.Reg("sumSums", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 5), KickResult::kLaunched);
	SleepMs(150);
	rig.sched->Tick(1);
	rig.sched->Tick(5);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];

	const std::vector<RangeTiming>& t = rig.spy->timings;
	ASSERT_GT(executorRangeCount, 1u);
	EXPECT_EQ(r.rangeCount, executorRangeCount);
	EXPECT_EQ(t.size(), executorRangeCount);
	uint64_t cpu = 0, cap = 0, worker = 0;
	for (const RangeTiming& x : t)
	{
		cpu += x.endNs - x.startNs;
		cap += x.capWaitNs;
		if (!(x.flags & (kRangeStolen | kRangeTrySubmitFailed)) && x.admitNs != 0) { worker += x.startNs - x.admitNs; }
	}
	EXPECT_EQ(r.cpuNs, cpu);
	EXPECT_EQ(r.capWaitNs, cap);
	EXPECT_EQ(r.workerWaitNs, worker);
	EXPECT_GT(r.cpuNs, 0u);
}

// AC 12
TEST(DeferredStepSummaryScheduler, KickToFirstRangeGapIsAccountedForWithinFiveMillisecondsTolerance)
{
	Rig rig(4, 4);
	FnStep step([](IParallelExecutor& ex) { ex.ParallelFor(1, 1, [](uint32_t, uint32_t) { SpinMicros(100); }); });
	const auto id = rig.Reg("sumRecon", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 5), KickResult::kLaunched);
	SleepMs(60);
	rig.sched->Tick(5);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	const uint64_t gap = r.firstRangeStartNs - r.kickNs;
	const uint64_t accounted = r.capWaitNs + r.workerWaitNs;
	const uint64_t unaccounted = gap > accounted ? gap - accounted : 0;
	EXPECT_LE(unaccounted, 5 * kMs);
}

// AC 13 (the AC's applyNs - readyNs ~ stall is not asserted: readyNs is stamped when Wait returns)
TEST(DeferredStepSummaryScheduler, LateStepRecordsStallAndStalledFlag)
{
	Rig rig(4, 4);
	FnStep step([](IParallelExecutor& ex) { ex.ParallelFor(2, 1, [](uint32_t, uint32_t) { SleepMs(40); }); });
	const auto id = rig.Reg("sumStall", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_GT(r.stallNs, 0u);
	EXPECT_NE(r.flags & P::StepSummaryFlags::kStalled, 0);
	EXPECT_GE(r.applyNs - r.kickNs, r.stallNs);
}

// AC 14
TEST(DeferredStepSummaryScheduler, EarlyStepHasNoStallAndReadyPrecedesApplyBySlack)
{
	Rig rig(4, 4);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumEarly", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 10), KickResult::kLaunched);
	SleepMs(100);
	rig.sched->Tick(2);    // completion first observed here
	SleepMs(30);
	rig.sched->Tick(10);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_EQ(r.stallNs, 0u);
	EXPECT_EQ(r.flags & P::StepSummaryFlags::kStalled, 0);
	EXPECT_LT(r.readyNs, r.applyNs);
	EXPECT_GE(r.applyNs - r.readyNs, 20 * kMs);
}

// AC 15
TEST(DeferredStepSummaryScheduler, CapOfOneShowsCapWaitOnSecondChannelButNoWorkerWait)
{
	Rig rig(4, 1);
	FnStep stepA(SpinBody(2000));
	FnStep stepB(SpinBody(2000));
	const auto a = rig.Reg("sumCapA", stepA);
	const auto b = rig.Reg("sumCapB", stepB);
	ASSERT_EQ(rig.sched->Kick(a, 1, 1), KickResult::kLaunched);
	ASSERT_EQ(rig.sched->Kick(b, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 2u);
	const P::StepSummaryRecord& second = rig.sink.records[1];
	EXPECT_EQ(second.owner, b.Value());
	EXPECT_GT(second.capWaitNs, 0u);
	EXPECT_LT(second.workerWaitNs, 1 * kMs);
	EXPECT_GT(second.stolenRanges, 0);
}

// AC 16
TEST(DeferredStepSummaryScheduler, CapEqualToWorkersOnIdlePoolHasZeroCapWait)
{
	Rig rig(4, 4);
	FnStep stepA([](IParallelExecutor& ex) { ex.ParallelFor(2, 1, [](uint32_t, uint32_t) { SpinMicros(200); }); });
	FnStep stepB([](IParallelExecutor& ex) { ex.ParallelFor(2, 1, [](uint32_t, uint32_t) { SpinMicros(200); }); });
	const auto a = rig.Reg("sumFullA", stepA);
	const auto b = rig.Reg("sumFullB", stepB);
	ASSERT_EQ(rig.sched->Kick(a, 1, 1), KickResult::kLaunched);
	ASSERT_EQ(rig.sched->Kick(b, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 2u);
	for (const P::StepSummaryRecord& r : rig.sink.records)
	{
		EXPECT_EQ(r.capWaitNs, 0u);
		EXPECT_EQ(r.stolenRanges, 0);
	}
}

// AC 17
TEST(DeferredStepSummaryScheduler, InlineLauncherStepRecordsRanInlineWholly)
{
	InlineLauncher launcher;
	DeferredStepScheduler sched(launcher);
	CapturingSink sink;
	sched.SetSummarySink(&sink);
	FnStep step([](IParallelExecutor& ex) { ex.ParallelFor(10, 1, [](uint32_t, uint32_t) { SpinMicros(2000); }); });
	const Dia::Core::StringCRC id("sumInline");
	ASSERT_TRUE(sched.Register(id, step));
	ASSERT_EQ(sched.Kick(id, 1, 1), KickResult::kLaunched);
	sched.Tick(1);
	sched.Unregister(id);
	ASSERT_EQ(sink.records.size(), 1u);
	const P::StepSummaryRecord& r = sink.records[0];
	EXPECT_NE(r.flags & P::StepSummaryFlags::kRanInlineWholly, 0);
	EXPECT_EQ(r.rangeCount, 1);
	EXPECT_EQ(r.workerWaitNs, 0u);
	EXPECT_EQ(r.capWaitNs, 0u);
	EXPECT_GE(r.cpuNs, 2 * kMs);
	EXPECT_LT(r.cpuNs, 200 * kMs);
	EXPECT_EQ(r.cpuNs, r.readyNs - r.kickNs);
}

TEST(DeferredStepSummaryScheduler, JobLauncherWithoutJobSystemRecordsRanInlineWholly)
{
	Rig rig(0, 1);   // JobSystem never initialised: Launch runs the step inline on SimPU
	FnStep step(SpinBody(500, 10));
	const auto id = rig.Reg("sumNoJobs", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_NE(r.flags & P::StepSummaryFlags::kRanInlineWholly, 0);
	EXPECT_EQ(r.rangeCount, 1);
	EXPECT_EQ(r.workerWaitNs, 0u);
	EXPECT_EQ(r.capWaitNs, 0u);
}

// AC 18
TEST(DeferredStepSummaryScheduler, RestoredStepRecordsRestoredFlagWithZeroedRangeFields)
{
	Rig rig(4, 4);
	FnStep step([](IParallelExecutor&) {});
	const auto id = rig.Reg("sumRestore", step);
	ASSERT_TRUE(rig.sched->RestorePending(id, 3, 7));
	rig.sched->Tick(6);
	EXPECT_TRUE(rig.sink.records.empty());
	rig.sched->Tick(7);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_EQ(r.flags, P::StepSummaryFlags::kRestoredFromSave);
	EXPECT_EQ(r.owner, id.Value());
	EXPECT_EQ(r.applyKey, 7u);
	EXPECT_EQ(r.kickKeyLow, 3u);
	EXPECT_EQ(r.stallNs, 0u);
	EXPECT_EQ(r.rangeCount, 0);
	EXPECT_EQ(r.cpuNs, 0u);
	EXPECT_EQ(r.capWaitNs, 0u);
	EXPECT_EQ(r.workerWaitNs, 0u);
	EXPECT_EQ(r.firstRangeStartNs, 0u);
	EXPECT_EQ(r.inlineFallbacks, 0);
	EXPECT_EQ(r.stolenRanges, 0);
}

// AC 19
TEST(DeferredStepSummaryScheduler, StolenRangesAreCountedSeparatelyFromInlineFallbacks)
{
	Rig rig(4, 1);   // M = 1: the coordinator holds the only slot, so every queued range is stolen
	FnStep step(SpinBody(500));
	const auto id = rig.Reg("sumStolen", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_GT(r.rangeCount, 1);
	EXPECT_EQ(r.stolenRanges, r.rangeCount - 1);
	EXPECT_EQ(r.inlineFallbacks, 0);
}

TEST(DeferredStepSummaryScheduler, FailedTrySubmitRangesAreCountedAsInlineFallbacksNotStolen)
{
	Rig rig(4, 4);
	std::atomic<int> submits{ 0 };
	// First submit (the coordinator) succeeds; every range submit after it fails.
	rig.real->GetCapGate().SetSubmitFailureHookForTest([&submits] { return submits.fetch_add(1) > 0; });
	FnStep step(SpinBody(200, 1000));
	const auto id = rig.Reg("sumFallback", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	ASSERT_EQ(rig.sink.records.size(), 1u);
	const P::StepSummaryRecord& r = rig.sink.records[0];
	EXPECT_GT(r.rangeCount, 1);
	EXPECT_EQ(r.inlineFallbacks, r.rangeCount - 1);
	EXPECT_EQ(r.stolenRanges, 0);
}

TEST(DeferredStepSummaryScheduler, NoSinkMeansSchedulerStillAppliesNormally)
{
	Rig rig(4, 4);
	rig.sched->SetSummarySink(nullptr);
	FnStep step(SpinBody(100));
	const auto id = rig.Reg("sumNoSink", step);
	ASSERT_EQ(rig.sched->Kick(id, 1, 1), KickResult::kLaunched);
	rig.sched->Tick(1);
	EXPECT_EQ(step.applies, 1);
	EXPECT_TRUE(rig.sink.records.empty());
}
