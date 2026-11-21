#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/CappedParallelExecutor.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <thread>
#include <vector>

using namespace Dia::DeferredStep;

namespace
{
	using Dia::Threading::JobSystem;

	struct Latch
	{
		std::atomic<bool> open{ false };
		void Open() { open.store(true); }
		void Wait() const { while (!open.load()) { std::this_thread::yield(); } }
	};

	CapConfig CapOf(uint32_t m, uint32_t rangesPerWorker = 4)
	{
		CapConfig c;
		c.workerCap = m;
		c.rangesPerWorker = rangesPerWorker;
		return c;
	}

	uint64_t NowNs()
	{
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	void SpinMicroseconds(int us)
	{
		const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
		while (std::chrono::steady_clock::now() < end) {}
	}

	// A kCoordinator item admitted through the gate (the body holds a gate slot as in production).
	struct CoordRun
	{
		GateItem            item;
		std::promise<void>  promise;
		std::future<void>   done;

		void Start(CapGate& gate, std::function<void()> body)
		{
			done = promise.get_future();
			item.Prepare(GateItem::Kind::kCoordinator, [this, body]
			{
				try { body(); promise.set_value(); }
				catch (...) { promise.set_exception(std::current_exception()); }
			});
			gate.Admit(item);
		}

		void Finish(CapGate& gate)
		{
			if (done.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
			{
				ADD_FAILURE() << "coordinator did not finish within 10 s";
				std::exit(1);
			}
			gate.WaitItem(item);
			done.get();
		}

		// Gate queue wait of the coordinator item itself (0 when admitted immediately).
		uint64_t QueueWaitNs() const
		{
			return item.GetAdmitNs() > item.GetEnqueueNs() ? item.GetAdmitNs() - item.GetEnqueueNs() : 0;
		}
	};

	// Spec table invariants for every range of the last ParallelFor.
	void CheckTimings(const CappedParallelExecutor& exec)
	{
		for (uint32_t i = 0; i < exec.GetRangeTimingCount(); ++i)
		{
			const RangeTiming& t = exec.GetRangeTimingAt(i);
			SCOPED_TRACE(::testing::Message() << "range " << i);
			EXPECT_LE(t.enqueueNs, t.startNs);
			EXPECT_LE(t.startNs, t.endNs);
			const bool stolen = (t.flags & kRangeStolen) != 0;
			const bool failed = (t.flags & kRangeTrySubmitFailed) != 0;
			EXPECT_FALSE(stolen && failed);
			if (i == 0)
			{
				EXPECT_EQ(t.flags, 0u);
				EXPECT_EQ(t.admitNs, 0u);
				EXPECT_EQ(t.capWaitNs, 0u);
			}
			else if (stolen)
			{
				EXPECT_EQ(t.admitNs, 0u);
				EXPECT_EQ(t.capWaitNs, t.startNs - t.enqueueNs);
			}
			else if (failed)
			{
				EXPECT_EQ(t.admitNs, 0u);
				EXPECT_EQ(t.capWaitNs, 0u);
			}
			else
			{
				EXPECT_NE(t.admitNs, 0u);
				EXPECT_LE(t.enqueueNs, t.admitNs);
				EXPECT_LE(t.admitNs, t.startNs);
				EXPECT_EQ(t.capWaitNs, t.admitNs - t.enqueueNs);
			}
		}
	}

	uint64_t SumCapWait(const CappedParallelExecutor& exec)
	{
		uint64_t sum = 0;
		for (uint32_t i = 0; i < exec.GetRangeTimingCount(); ++i) { sum += exec.GetRangeTimingAt(i).capWaitNs; }
		return sum;
	}

	LaunchInfo Info() { return LaunchInfo(); }
}

// AC 15: forced TrySubmit failures run inline, flagged, never conflated with stolen.
// M=2: a blocker item and the coordinator fill both slots so every range queues. The first stolen range
// (run on the coordinator) releases the blocker and waits for the gate to pump one item; the hook fails
// that pump-time submit, so one range is failed-inline, at least one is stolen, and result is unchanged.
TEST(DeferredStepCapTimingTest, ForcedSubmitFailureIsInlineFlaggedAndNotStolen)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(2, 4));
	std::atomic<uint32_t> calls{ 0 };
	gate.SetSubmitFailureHookForTest([&calls]
	{
		const uint32_t n = ++calls;   // 1 blocker, 2 coordinator, 3+ ranges
		return n >= 3 && (n % 2) == 1;
	});

	const uint32_t count = 800;
	std::vector<uint64_t> out(count, 0);
	Latch blockerRelease;

	CoordRun blocker;
	blocker.Start(gate, [&] { blockerRelease.Wait(); });

	CoordRun coord;
	std::unique_ptr<CappedParallelExecutor> exec;
	std::thread::id coordThread;
	coord.Start(gate, [&]
	{
		coordThread = std::this_thread::get_id();
		exec = std::make_unique<CappedParallelExecutor>(gate, Info());
		bool released = false;
		exec->ParallelFor(count, 1, [&](uint32_t b, uint32_t e)
		{
			for (uint32_t i = b; i < e; ++i) { out[i] = static_cast<uint64_t>(i) * i + 1; }
			if (b > 0 && !released && std::this_thread::get_id() == coordThread)
			{
				released = true;   // first stolen range on the coordinator
				blockerRelease.Open();
				// Wait until the blocker's release pumped one queued item (admitted or failed).
				while (gate.GetStats().admitted + gate.GetStats().trySubmitFailed < 3) { std::this_thread::yield(); }
			}
		});
	});
	coord.Finish(gate);
	blocker.Finish(gate);

	for (uint32_t i = 0; i < count; ++i)
	{
		ASSERT_EQ(out[i], static_cast<uint64_t>(i) * i + 1) << i;
	}

	uint32_t failed = 0, stolen = 0;
	for (uint32_t i = 0; i < exec->GetRangeTimingCount(); ++i)
	{
		const RangeTiming& t = exec->GetRangeTimingAt(i);
		if (t.flags & kRangeTrySubmitFailed) { ++failed; EXPECT_EQ(t.flags & kRangeStolen, 0u); }
		if (t.flags & kRangeStolen) { ++stolen; EXPECT_EQ(t.flags & kRangeTrySubmitFailed, 0u); }
	}
	const CapStats stats = gate.GetStats();
	EXPECT_GE(failed, 1u);
	EXPECT_GE(stolen, 1u);
	EXPECT_EQ(stats.trySubmitFailed, failed);
	EXPECT_EQ(stats.stolen, stolen);
	CheckTimings(*exec);
	EXPECT_EQ(stats.slotsHeld, 0u);
	jobs.Shutdown();
}

// AC 28: one Warning per ParallelFor even when every range submit fails. ParallelFor is called from the
// test thread (the one LogCapture registered) so the log entry is capturable.
TEST(DeferredStepCapTimingTest, AllRangeSubmitsFailLogsExactlyOneWarning)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(4, 4));   // up to 16 ranges
	gate.SetSubmitFailureHookForTest([] { return true; });

	std::vector<uint32_t> out(1600, 0);
	Dia::DeferredStep::Testing::LogCapture log;
	CappedParallelExecutor exec(gate, Info());
	exec.ParallelFor(1600, 1, [&](uint32_t b, uint32_t e)
	{
		for (uint32_t i = b; i < e; ++i) { out[i] = i + 7; }
	});

	EXPECT_EQ(exec.GetLastRangeCount(), 16u);
	for (uint32_t i = 0; i < 1600; ++i) { ASSERT_EQ(out[i], i + 7) << i; }
	EXPECT_EQ(log.Warnings("Range submit failed"), 1u);
	EXPECT_EQ(gate.GetStats().trySubmitFailed, 15u);
	for (uint32_t i = 1; i < 16; ++i)
	{
		EXPECT_EQ(exec.GetRangeTimingAt(i).flags, kRangeTrySubmitFailed) << i;
	}
	CheckTimings(exec);
	jobs.Shutdown();
}

// AC 26 (M=1): the second coordinator waits in the gate queue; its ranges report capWaitNs > 0.
TEST(DeferredStepCapTimingTest, SecondChannelAtM1WaitsInQueueAndReportsCapWait)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1, 4));
	Latch holdA;

	CoordRun a;
	a.Start(gate, [&] { holdA.Wait(); });

	std::unique_ptr<CappedParallelExecutor> execB;
	CoordRun b;
	b.Start(gate, [&]
	{
		execB = std::make_unique<CappedParallelExecutor>(gate, Info());
		execB->ParallelFor(64, 1, [](uint32_t, uint32_t) { SpinMicroseconds(50); });
	});

	EXPECT_GE(gate.GetStats().queuedAtLeastOnce, 1u);
	EXPECT_EQ(b.item.GetState(), GateItem::State::kQueued);
	SpinMicroseconds(1000);   // let B accrue measurable queue time before A releases
	holdA.Open();
	a.Finish(gate);
	b.Finish(gate);

	EXPECT_GT(gate.GetStats().totalCapWaitNs, 0u);
	EXPECT_GT(b.QueueWaitNs(), 0u);
	uint32_t withWait = 0;
	for (uint32_t i = 0; i < execB->GetRangeTimingCount(); ++i)
	{
		if (execB->GetRangeTimingAt(i).capWaitNs > 0) { ++withWait; }
	}
	EXPECT_GE(withWait, 1u);
	CheckTimings(*execB);
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	jobs.Shutdown();
}

// AC 26 (M = worker count, idle pool): no range waits for the cap. rangesPerWorker=1 keeps the range
// count at M, so the coordinator plus M-1 helpers fit the slots immediately.
TEST(DeferredStepCapTimingTest, IdlePoolAtMEqualsWorkersHasZeroCapWait)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(4, 1));
	CappedParallelExecutor exec(gate, Info());
	CoordRun c;
	c.Start(gate, [&]
	{
		exec.ParallelFor(400, 1, [](uint32_t, uint32_t) { SpinMicroseconds(100); });
	});
	c.Finish(gate);

	EXPECT_EQ(exec.GetLastRangeCount(), 4u);
	for (uint32_t i = 0; i < exec.GetRangeTimingCount(); ++i)
	{
		EXPECT_EQ(exec.GetRangeTimingAt(i).capWaitNs, 0u) << i;
		EXPECT_EQ(exec.GetRangeTimingAt(i).flags, 0u) << i;
	}
	EXPECT_EQ(gate.GetStats().totalCapWaitNs, 0u);
	EXPECT_EQ(gate.GetStats().stolen, 0u);
	CheckTimings(exec);
	jobs.Shutdown();
}

// AC 27: totalCapWaitNs == sum of range capWaitNs. The gate also adds each coordinator item's own queue
// wait (admit - enqueue), so the comparison adds those explicitly (stats are not reset, to avoid racing
// pump-time accounting).
TEST(DeferredStepCapTimingTest, TotalCapWaitEqualsSumOfRangeAndCoordinatorWaits)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(2, 4));

	const uint32_t kChannels = 3;
	std::vector<std::unique_ptr<CappedParallelExecutor>> execs(kChannels);
	std::vector<std::unique_ptr<CoordRun>> runs;
	for (uint32_t c = 0; c < kChannels; ++c)
	{
		runs.push_back(std::make_unique<CoordRun>());
		runs.back()->Start(gate, [&, c]
		{
			execs[c] = std::make_unique<CappedParallelExecutor>(gate, Info());
			execs[c]->ParallelFor(64, 1, [](uint32_t, uint32_t) { SpinMicroseconds(200); });
		});
	}
	for (auto& r : runs) { r->Finish(gate); }

	uint64_t expected = 0;
	for (uint32_t c = 0; c < kChannels; ++c)
	{
		expected += SumCapWait(*execs[c]) + runs[c]->QueueWaitNs();
		CheckTimings(*execs[c]);
	}
	const CapStats stats = gate.GetStats();
	EXPECT_GE(stats.queuedAtLeastOnce, 1u);
	EXPECT_EQ(stats.totalCapWaitNs, expected);
	EXPECT_EQ(stats.slotsHeld, 0u);
	jobs.Shutdown();
}
