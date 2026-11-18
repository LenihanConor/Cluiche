#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/CappedParallelExecutor.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/RangeSplit.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaDeferredStep/Testing/ConcurrencyProbe.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace Dia::DeferredStep;

namespace
{
	using Dia::Threading::JobSystem;
	using Dia::DeferredStep::Testing::ConcurrencyProbe;

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

	void SpinMicroseconds(int us)
	{
		const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
		while (std::chrono::steady_clock::now() < end) {}
	}

	// Runs `body` as a kCoordinator item admitted through the gate, so the ParallelFor caller holds a
	// gate slot exactly as in production. Fails (instead of hanging) after 10 s.
	// Returns true if the body finished in time. The body's exception, if any, is rethrown from here.
	bool RunAsCoordinator(CapGate& gate, std::function<void()> body)
	{
		auto promise = std::make_shared<std::promise<void>>();
		std::future<void> done = promise->get_future();
		GateItem item;
		item.Prepare(GateItem::Kind::kCoordinator, [promise, body]
		{
			try { body(); promise->set_value(); }
			catch (...) { promise->set_exception(std::current_exception()); }
		});
		gate.Admit(item);
		if (done.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
		{
			ADD_FAILURE() << "coordinator did not finish within 10 s (stranded range / deadlock)";
			std::exit(1);   // the item is still referenced by a live job; do not unwind past it
		}
		gate.WaitItem(item);
		done.get();
		return true;
	}

	LaunchInfo Info() { return LaunchInfo(); }
}

TEST(DeferredStepCapExecutorTest, VisitsEveryIndexExactlyOnce)
{
	const uint32_t counts[] = { 1, 7, 64, 1000 };
	const uint32_t minRanges[] = { 1, 3, 10, 100 };
	const uint32_t caps[] = { 1, 2, 4 };
	for (uint32_t m : caps)
	{
		JobSystem jobs;
		jobs.Initialize(8);
		CapGate gate(jobs, CapOf(m));
		for (uint32_t count : counts)
		{
			for (uint32_t minRange : minRanges)
			{
				std::vector<std::atomic<uint8_t>> hits(count);
				for (auto& h : hits) { h.store(0); }
				RunAsCoordinator(gate, [&]
				{
					CappedParallelExecutor exec(gate, Info());
					exec.ParallelFor(count, minRange, [&](uint32_t b, uint32_t e)
					{
						for (uint32_t i = b; i < e; ++i) { hits[i].fetch_add(1); }
					});
				});
				for (uint32_t i = 0; i < count; ++i)
				{
					ASSERT_EQ(hits[i].load(), 1) << "m=" << m << " count=" << count << " minRange=" << minRange << " i=" << i;
				}
			}
		}
		jobs.Shutdown();
	}
}

TEST(DeferredStepCapExecutorTest, RangeCountFollowsFormula)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(2, 4));   // max 8 ranges
	struct Case { uint32_t count, minRange; };
	const Case cases[] = { {100, 10}, {100, 1}, {5, 10}, {10, 10}, {11, 10}, {1, 1}, {1000, 3}, {64, 0} };
	for (const Case& c : cases)
	{
		uint32_t reported = 0;
		RunAsCoordinator(gate, [&]
		{
			CappedParallelExecutor exec(gate, Info());
			exec.ParallelFor(c.count, c.minRange, [](uint32_t, uint32_t) {});
			reported = exec.GetLastRangeCount();
			EXPECT_EQ(exec.GetRangeTimingCount(), reported);
		});
		EXPECT_EQ(reported, ComputeRangeCount(c.count, c.minRange, 2, 4)) << c.count << "/" << c.minRange;
	}
	// count <= minRange -> one range.
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(5, 10, [](uint32_t, uint32_t) {});
		EXPECT_EQ(exec.GetLastRangeCount(), 1u);
	});
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, ZeroCountNeverCallsFn)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(2));
	std::atomic<int> calls{ 0 };
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(0, 8, [&](uint32_t, uint32_t) { calls.fetch_add(1); });
		EXPECT_EQ(exec.GetLastRangeCount(), 1u);
	});
	EXPECT_EQ(calls.load(), 0);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, NothingStillRunningOnReturn)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(4, 4));
	ConcurrencyProbe active;
	std::atomic<int> finishedFlags{ 0 };
	Latch go;
	int activeAtReturn = -1;
	int flagsAtReturn = -1;
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		std::thread opener([&] { go.Open(); });
		exec.ParallelFor(16, 1, [&](uint32_t, uint32_t)
		{
			ConcurrencyProbe::Scope scope(active);
			go.Wait();
			SpinMicroseconds(200);
			finishedFlags.fetch_add(1);
		});
		activeAtReturn = active.Current();
		flagsAtReturn = finishedFlags.load();
		opener.join();
	});
	EXPECT_EQ(activeAtReturn, 0);
	EXPECT_EQ(flagsAtReturn, 16);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, ThrowingRangePropagatesAfterOthersFinish)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(2, 8));
	std::atomic<int> completed{ 0 };
	bool caught = false;
	std::string message;
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		try
		{
			exec.ParallelFor(16, 1, [&](uint32_t b, uint32_t)
			{
				if (b == 5) { throw std::runtime_error("range5"); }
				SpinMicroseconds(50);
				completed.fetch_add(1);
			});
		}
		catch (const std::exception& e)
		{
			caught = true;
			message = e.what();
		}
		// The executor stays usable.
		std::atomic<int> after{ 0 };
		exec.ParallelFor(16, 1, [&](uint32_t b, uint32_t e) { after.fetch_add(static_cast<int>(e - b)); });
		EXPECT_EQ(after.load(), 16);
	});
	EXPECT_TRUE(caught);
	EXPECT_EQ(message, "range5");
	EXPECT_EQ(completed.load(), 15);
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, ThrowingRangeZeroStillRunsOthers)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(2, 4));
	std::atomic<int> completed{ 0 };
	bool caught = false;
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		try
		{
			exec.ParallelFor(8, 1, [&](uint32_t b, uint32_t)
			{
				if (b == 0) { throw std::runtime_error("first"); }
				completed.fetch_add(1);
			});
		}
		catch (const std::runtime_error&) { caught = true; }
	});
	EXPECT_TRUE(caught);
	EXPECT_EQ(completed.load(), 7);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, ConcurrencyNeverExceedsCapIncludingCoordinator)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(2, 8));
	ConcurrencyProbe probe;
	std::atomic<int> ran{ 0 };
	RunAsCoordinator(gate, [&]
	{
		ConcurrencyProbe::Scope coordinatorScope(probe);
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(16, 1, [&](uint32_t, uint32_t)
		{
			ConcurrencyProbe::Scope scope(probe);
			for (int s = 0; s < 20; ++s) { std::this_thread::yield(); }
			ran.fetch_add(1);
		});
	});
	EXPECT_EQ(ran.load(), 16);
	// Coordinator body and range bodies overlap, since the coordinator runs ranges itself; the probe
	// counts the coordinator scope once plus one per concurrently running range on *other* threads
	// plus nested inline ranges. Slot accounting is the strict check:
	EXPECT_LE(gate.GetStats().peakSlotsHeld, 2u);
	EXPECT_LE(probe.Peak(), 3);   // coordinator + its own inline range nest = 2 on one thread, +1 worker range
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, CoordinatorStealsEveryQueuedRangeSoNoRangeIsStranded)
{
	// M = 1 on a ONE-worker pool: the coordinator holds the only slot, so no range can ever be
	// admitted. Every range except range 0 (run directly) must be stolen.
	JobSystem jobs;
	jobs.Initialize(1);
	CapGate gate(jobs, CapOf(1, 16));
	std::atomic<int> ran{ 0 };
	uint32_t rangeCount = 0;
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(16, 1, [&](uint32_t, uint32_t) { ran.fetch_add(1); });
		rangeCount = exec.GetLastRangeCount();
		for (uint32_t i = 1; i < exec.GetRangeTimingCount(); ++i)
		{
			EXPECT_NE(exec.GetRangeTimingAt(i).flags & kRangeStolen, 0u) << i;
		}
		EXPECT_EQ(exec.GetRangeTimingAt(0).flags, 0u);
	});
	EXPECT_EQ(rangeCount, 16u);
	EXPECT_EQ(ran.load(), 16);
	// Spec says stolen == 16 for 16 ranges, but range 0 runs directly on the coordinator: 15 stolen.
	EXPECT_EQ(gate.GetStats().stolen, 15u);
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, SaturatedGateStillRunsEveryRange)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1, 16));
	Latch started, release;
	GateItem blocker;
	blocker.Prepare(GateItem::Kind::kCoordinator, [&] { started.Open(); release.Wait(); });
	ASSERT_EQ(gate.Admit(blocker), AdmitResult::kSubmitted);
	started.Wait();

	// Caller is the test thread (holds no slot); the blocker holds the only one.
	std::atomic<int> ran{ 0 };
	CappedParallelExecutor exec(gate, Info());
	exec.ParallelFor(16, 1, [&](uint32_t, uint32_t) { ran.fetch_add(1); });
	EXPECT_EQ(ran.load(), 16);                 // all done while the blocker still holds the slot
	EXPECT_EQ(gate.GetStats().stolen, 15u);
	EXPECT_EQ(gate.GetStats().slotsHeld, 1u);

	release.Open();
	EXPECT_EQ(gate.WaitItem(blocker), GateItem::State::kDone);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, RangeTimingOrderingAndCapWaitDefinition)
{
	JobSystem jobs;
	jobs.Initialize(1);
	CapGate gate(jobs, CapOf(1, 8));
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(8, 1, [](uint32_t, uint32_t) { SpinMicroseconds(50); });
		ASSERT_EQ(exec.GetRangeTimingCount(), 8u);
		for (uint32_t i = 0; i < 8; ++i)
		{
			const RangeTiming& t = exec.GetRangeTimingAt(i);
			EXPECT_LE(t.enqueueNs, t.startNs) << i;
			EXPECT_LT(t.startNs, t.endNs) << i;
			EXPECT_GE(t.endNs - t.startNs, 40000u) << i;   // spun ~50us
			if (i == 0)
			{
				EXPECT_EQ(t.capWaitNs, 0u);
				EXPECT_EQ(t.enqueueNs, t.startNs);
				EXPECT_EQ(t.flags, 0u);
			}
			else
			{
				EXPECT_NE(t.flags & kRangeStolen, 0u);
				EXPECT_EQ(t.admitNs, 0u);
				EXPECT_EQ(t.capWaitNs, t.startNs - t.enqueueNs);
			}
		}
	});
	EXPECT_GT(gate.GetStats().totalCapWaitNs, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, AdmittedRangeCapWaitIsAdmitMinusEnqueue)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(8, 2));
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(8, 1, [](uint32_t, uint32_t) { SpinMicroseconds(100); });
		for (uint32_t i = 0; i < exec.GetRangeTimingCount(); ++i)
		{
			const RangeTiming& t = exec.GetRangeTimingAt(i);
			EXPECT_LE(t.enqueueNs, t.startNs) << i;
			EXPECT_LE(t.startNs, t.endNs) << i;
			if (i > 0 && (t.flags & kRangeStolen) == 0)
			{
				EXPECT_GE(t.admitNs, t.enqueueNs);
				EXPECT_EQ(t.capWaitNs, t.admitNs - t.enqueueNs);
				EXPECT_LE(t.admitNs, t.startNs);
			}
		}
	});
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, TwoSequentialCallsInOneCoordinator)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(1, 4));
	std::atomic<int> first{ 0 }, second{ 0 };
	RunAsCoordinator(gate, [&]
	{
		CappedParallelExecutor exec(gate, Info());
		exec.ParallelFor(40, 10, [&](uint32_t b, uint32_t e) { first.fetch_add(static_cast<int>(e - b)); });
		EXPECT_EQ(exec.GetLastRangeCount(), 4u);
		exec.ParallelFor(6, 3, [&](uint32_t b, uint32_t e) { second.fetch_add(static_cast<int>(e - b)); });
		EXPECT_EQ(exec.GetLastRangeCount(), 2u);
		EXPECT_EQ(exec.GetRangeTimingCount(), 2u);   // timings are for the last call
		EXPECT_EQ(exec.GetTotalRangesRun(), 6u);
	});
	EXPECT_EQ(first.load(), 40);
	EXPECT_EQ(second.load(), 6);
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, SingleRangeDoesNotTouchTheGate)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(8));
	int calls = 0;
	CappedParallelExecutor exec(gate, Info());
	exec.ParallelFor(1, 1, [&](uint32_t b, uint32_t e) { ++calls; EXPECT_EQ(b, 0u); EXPECT_EQ(e, 1u); });
	EXPECT_EQ(calls, 1);
	EXPECT_EQ(exec.GetLastRangeCount(), 1u);
	EXPECT_EQ(gate.GetStats().admitted, 0u);
	EXPECT_EQ(gate.GetStats().queuedAtLeastOnce, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, SubmitFailureRunsRangesInlineAndFlagsThem)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(4, 4));
	gate.SetSubmitFailureHookForTest([] { return true; });
	std::vector<std::atomic<uint8_t>> hits(100);
	for (auto& h : hits) { h.store(0); }
	CappedParallelExecutor exec(gate, Info());
	exec.ParallelFor(100, 1, [&](uint32_t b, uint32_t e)
	{
		for (uint32_t i = b; i < e; ++i) { hits[i].fetch_add(1); }
	});
	for (auto& h : hits) { ASSERT_EQ(h.load(), 1); }
	ASSERT_EQ(exec.GetLastRangeCount(), 16u);
	EXPECT_EQ(exec.GetRangeTimingAt(0).flags, 0u);
	for (uint32_t i = 1; i < 16; ++i)
	{
		EXPECT_EQ(exec.GetRangeTimingAt(i).flags, kRangeTrySubmitFailed) << i;
	}
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	jobs.Shutdown();
}

TEST(DeferredStepCapExecutorTest, ForcedRangeCountOverridesAndIsCappedToCount)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(2, 4));
	CappedParallelExecutor exec(gate, Info());
	std::atomic<int> sum{ 0 };
	auto add = [&](uint32_t b, uint32_t e) { sum.fetch_add(static_cast<int>(e - b)); };

	exec.SetForcedRangeCount(17);
	exec.ParallelFor(100, 50, add);
	EXPECT_EQ(exec.GetLastRangeCount(), 17u);
	EXPECT_EQ(sum.load(), 100);

	sum.store(0);
	exec.ParallelFor(5, 1, add);   // capped to count: no empty ranges
	EXPECT_EQ(exec.GetLastRangeCount(), 5u);
	EXPECT_EQ(sum.load(), 5);

	exec.ParallelFor(0, 1, add);
	EXPECT_EQ(exec.GetLastRangeCount(), 1u);

	exec.SetForcedRangeCount(0);
	exec.ParallelFor(100, 50, add);
	EXPECT_EQ(exec.GetLastRangeCount(), 2u);
	jobs.Shutdown();
}
