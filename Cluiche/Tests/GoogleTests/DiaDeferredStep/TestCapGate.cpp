#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/ConcurrencyProbe.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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

	CapConfig CapOf(uint32_t m)
	{
		CapConfig c;
		c.workerCap = m;
		return c;
	}

	std::unique_ptr<GateItem> MakeItem(std::function<void()> fn, GateItem::Kind kind = GateItem::Kind::kRange)
	{
		std::unique_ptr<GateItem> item(new GateItem());
		item->Prepare(kind, std::move(fn));
		return item;
	}
}

TEST(DeferredStepCapGateTest, PeakNeverExceedsCapUnderContention)
{
	const uint32_t caps[] = { 1, 2, 4 };
	for (uint32_t m : caps)
	{
		for (int iteration = 0; iteration < 50; ++iteration)
		{
			JobSystem jobs;
			jobs.Initialize(8);
			CapGate gate(jobs, CapOf(m));
			ConcurrencyProbe probe;
			std::atomic<int> ran{ 0 };

			std::vector<std::unique_ptr<GateItem>> items;
			// Stay within the gate queue bound (40 at M=1) so a burst never takes the queue-full path.
			for (int i = 0; i < 24; ++i)
			{
				items.push_back(MakeItem([&]
				{
					ConcurrencyProbe::Scope scope(probe);
					for (int spin = 0; spin < 50; ++spin) { std::this_thread::yield(); }
					ran.fetch_add(1);
				}));
			}
			// Three "channels" admitting from three threads.
			std::vector<std::thread> admitters;
			for (int c = 0; c < 3; ++c)
			{
				admitters.emplace_back([&, c]
				{
					for (int i = 0; i < 8; ++i) { gate.Admit(*items[c * 8 + i]); }
				});
			}
			for (auto& t : admitters) { t.join(); }
			for (auto& item : items)
			{
				EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kDone);
			}

			EXPECT_EQ(ran.load(), 24);
			EXPECT_LE(probe.Peak(), static_cast<int>(m));
			const CapStats stats = gate.GetStats();
			EXPECT_LE(stats.peakSlotsHeld, m);
			EXPECT_EQ(stats.workerCap, m);
			EXPECT_EQ(stats.admitted, 24u);
			jobs.Shutdown();
			EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
		}
	}
}

TEST(DeferredStepCapGateTest, QueuesWhenFullAndAdmitsOnRelease)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::atomic<int> secondRan{ 0 };

	auto first = MakeItem([&] { started.Open(); release.Wait(); });
	auto second = MakeItem([&] { secondRan.fetch_add(1); });

	EXPECT_EQ(gate.Admit(*first), AdmitResult::kSubmitted);
	started.Wait();
	EXPECT_EQ(gate.Admit(*second), AdmitResult::kQueued);
	EXPECT_EQ(second->GetState(), GateItem::State::kQueued);
	EXPECT_EQ(secondRan.load(), 0);

	CapStats mid = gate.GetStats();
	EXPECT_GE(mid.queuedAtLeastOnce, 1u);
	EXPECT_GE(mid.peakQueueDepth, 1u);
	EXPECT_EQ(mid.slotsHeld, 1u);

	release.Open();
	EXPECT_EQ(gate.WaitItem(*first), GateItem::State::kDone);
	EXPECT_EQ(gate.WaitItem(*second), GateItem::State::kDone);
	EXPECT_EQ(secondRan.load(), 1);
	EXPECT_GE(second->GetAdmitNs(), second->GetEnqueueNs());
	jobs.Shutdown();
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
}

TEST(DeferredStepCapGateTest, TrySteal)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::atomic<int> stolenRan{ 0 };

	auto running = MakeItem([&] { started.Open(); release.Wait(); });
	auto queued = MakeItem([&] { stolenRan.fetch_add(1); });
	gate.Admit(*running);
	started.Wait();
	gate.Admit(*queued);

	EXPECT_FALSE(gate.TrySteal(*running));          // already running
	EXPECT_TRUE(gate.TrySteal(*queued));
	EXPECT_EQ(queued->GetState(), GateItem::State::kStolen);
	EXPECT_FALSE(gate.TrySteal(*queued));           // exactly once
	EXPECT_EQ(gate.GetStats().stolen, 1u);

	release.Open();
	gate.WaitItem(*running);
	jobs.Shutdown();
	EXPECT_EQ(stolenRan.load(), 0);                 // never admitted by the gate
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
	EXPECT_FALSE(gate.TrySteal(*running));          // done
}

TEST(DeferredStepCapGateTest, QueuedItemsAdmittedInFifoOrder)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::mutex orderMutex;
	std::vector<int> order;

	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	gate.Admit(*blocker);
	started.Wait();

	std::vector<std::unique_ptr<GateItem>> items;
	for (int i = 0; i < 6; ++i)
	{
		items.push_back(MakeItem([&, i]
		{
			std::lock_guard<std::mutex> lock(orderMutex);
			order.push_back(i);
		}));
		EXPECT_EQ(gate.Admit(*items.back()), AdmitResult::kQueued);
	}
	release.Open();
	for (auto& item : items) { gate.WaitItem(*item); }
	ASSERT_EQ(order.size(), 6u);
	for (int i = 0; i < 6; ++i) { EXPECT_EQ(order[i], i); }
}

TEST(DeferredStepCapGateTest, StealPreservesOrderOfRemaining)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::mutex orderMutex;
	std::vector<int> order;

	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	gate.Admit(*blocker);
	started.Wait();
	std::vector<std::unique_ptr<GateItem>> items;
	for (int i = 0; i < 5; ++i)
	{
		items.push_back(MakeItem([&, i]
		{
			std::lock_guard<std::mutex> lock(orderMutex);
			order.push_back(i);
		}));
		gate.Admit(*items.back());
	}
	EXPECT_TRUE(gate.TrySteal(*items[2]));
	release.Open();
	for (int i = 0; i < 5; ++i) { if (i != 2) { gate.WaitItem(*items[i]); } }
	const std::vector<int> expected = { 0, 1, 3, 4 };
	EXPECT_EQ(order, expected);
}

TEST(DeferredStepCapGateTest, OneWorkerPoolCapOneDoesNotDeadlock)
{
	JobSystem jobs;
	jobs.Initialize(1);
	CapGate gate(jobs, CapOf(1));
	EXPECT_EQ(gate.GetWorkerCap(), 1u);
	std::atomic<int> ran{ 0 };
	std::vector<std::unique_ptr<GateItem>> items;
	for (int i = 0; i < 20; ++i)
	{
		items.push_back(MakeItem([&] { ran.fetch_add(1); }));
		gate.Admit(*items.back());
	}
	for (auto& item : items) { EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kDone); }
	EXPECT_EQ(ran.load(), 20);
}

TEST(DeferredStepCapGateTest, UninitialisedJobSystemReportsSubmitFailed)
{
	JobSystem jobs;   // never initialised: 0 workers
	CapGate gate(jobs, CapOf(0));
	EXPECT_EQ(gate.GetWorkerCap(), 1u);
	std::atomic<int> ran{ 0 };
	auto item = MakeItem([&] { ran.fetch_add(1); }, GateItem::Kind::kCoordinator);
	EXPECT_EQ(gate.Admit(*item), AdmitResult::kSubmitFailed);
	EXPECT_EQ(item->GetState(), GateItem::State::kSubmitFailed);
	EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kSubmitFailed);
	EXPECT_EQ(ran.load(), 0);
	const CapStats stats = gate.GetStats();
	EXPECT_EQ(stats.trySubmitFailed, 1u);
	EXPECT_EQ(stats.slotsHeld, 0u);
}

TEST(DeferredStepCapGateTest, PumpTimeSubmitFailureMarksItemAndReleasesSlot)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::atomic<int> calls{ 0 };
	gate.SetSubmitFailureHookForTest([&] { return calls.fetch_add(1) == 1; });   // fail the 2nd submit

	auto first = MakeItem([&] { started.Open(); release.Wait(); });
	auto second = MakeItem([] {});
	auto third = MakeItem([] {});
	EXPECT_EQ(gate.Admit(*first), AdmitResult::kSubmitted);
	started.Wait();
	gate.Admit(*second);
	gate.Admit(*third);
	release.Open();
	EXPECT_EQ(gate.WaitItem(*second), GateItem::State::kSubmitFailed);
	EXPECT_EQ(gate.WaitItem(*third), GateItem::State::kDone);
	EXPECT_EQ(gate.WaitItem(*first), GateItem::State::kDone);
	jobs.Shutdown();
	const CapStats stats = gate.GetStats();
	EXPECT_EQ(stats.trySubmitFailed, 1u);
	EXPECT_EQ(stats.slotsHeld, 0u);
}

TEST(DeferredStepCapGateTest, ResetStatsClearsCountersButKeepsCap)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(2));
	auto item = MakeItem([] {});
	gate.Admit(*item);
	gate.WaitItem(*item);
	gate.AddCapWait(500);
	jobs.Shutdown();
	CapStats before = gate.GetStats();
	EXPECT_EQ(before.admitted, 1u);
	EXPECT_EQ(before.totalCapWaitNs, 500u);
	EXPECT_EQ(before.peakSlotsHeld, 1u);
	gate.ResetStats();
	CapStats after = gate.GetStats();
	EXPECT_EQ(after.admitted, 0u);
	EXPECT_EQ(after.totalCapWaitNs, 0u);
	EXPECT_EQ(after.peakSlotsHeld, 0u);
	EXPECT_EQ(after.workerCap, 2u);
}

TEST(DeferredStepCapGateTest, CapLargerThanWorkersIsClamped)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(16));
	EXPECT_EQ(gate.GetWorkerCap(), 2u);
	EXPECT_EQ(gate.GetStats().workerCap, 2u);
}

//------------------------------------------------------------------------------------------
// Gap-audit additions (Capped Executor task 8)
//------------------------------------------------------------------------------------------

namespace
{
#ifdef DEBUG
	int gGateAssertCount = 0;
	void GateCountingAssertHandler(const char*, const char*, int, const char*, ...) { ++gGateAssertCount; }

	struct GateAssertScope
	{
		GateAssertScope() { mPrev = Dia::Core::g_pAssertFunc; Dia::Core::g_pAssertFunc = GateCountingAssertHandler; gGateAssertCount = 0; }
		~GateAssertScope() { Dia::Core::g_pAssertFunc = mPrev; }
		Dia::Core::DIA_ASSERT_FUNC mPrev;
	};
#endif
}

// AC 9: an over-cap config logs exactly one Warning from CapGate construction; a valid one logs none.
TEST(DeferredStepCapGateTest, OverCapClampLogsExactlyOneWarning)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		Dia::DeferredStep::Testing::LogCapture log;
		CapGate gate(jobs, CapOf(16));
		EXPECT_EQ(gate.GetWorkerCap(), 4u);
		EXPECT_EQ(log.Warnings(), 1u);
		EXPECT_EQ(log.Total(), 1u);
	}
	{
		Dia::DeferredStep::Testing::LogCapture log;
		CapGate exact(jobs, CapOf(4));
		CapGate derived(jobs, CapOf(0));
		EXPECT_EQ(exact.GetWorkerCap(), 4u);
		EXPECT_EQ(derived.GetWorkerCap(), 1u);   // max(1, 4 / 4)
		EXPECT_EQ(log.Total(), 0u);
	}
	jobs.Shutdown();
}

// AC 9: workerCap == 0 resolves to max(1, workers / 4) on a real gate.
TEST(DeferredStepCapGateTest, DerivedCapOnEightWorkersIsTwo)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(0));
	EXPECT_EQ(gate.GetWorkerCap(), 2u);
	EXPECT_EQ(gate.GetStats().workerCap, 2u);
	jobs.Shutdown();
}

// Every CapStats counter against a scripted scenario at M = 1.
TEST(DeferredStepCapGateTest, CountersAreExactForScriptedScenario)
{
	JobSystem jobs;
	jobs.Initialize(4);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	auto q1 = MakeItem([] {});
	auto q2 = MakeItem([] {});
	auto q3 = MakeItem([] {});

	EXPECT_EQ(gate.Admit(*blocker), AdmitResult::kSubmitted);
	started.Wait();
	EXPECT_EQ(gate.Admit(*q1), AdmitResult::kQueued);
	EXPECT_EQ(gate.Admit(*q2), AdmitResult::kQueued);
	EXPECT_EQ(gate.Admit(*q3), AdmitResult::kQueued);

	CapStats mid = gate.GetStats();
	EXPECT_EQ(mid.slotsHeld, 1u);
	EXPECT_EQ(mid.peakSlotsHeld, 1u);
	EXPECT_EQ(mid.admitted, 1u);
	EXPECT_EQ(mid.queuedAtLeastOnce, 3u);
	EXPECT_EQ(mid.peakQueueDepth, 3u);
	EXPECT_EQ(mid.stolen, 0u);

	EXPECT_TRUE(gate.TrySteal(*q2));
	release.Open();
	EXPECT_EQ(gate.WaitItem(*q1), GateItem::State::kDone);
	EXPECT_EQ(gate.WaitItem(*q3), GateItem::State::kDone);
	EXPECT_EQ(gate.WaitItem(*blocker), GateItem::State::kDone);
	jobs.Shutdown();

	CapStats end = gate.GetStats();
	EXPECT_EQ(end.slotsHeld, 0u);
	EXPECT_EQ(end.peakSlotsHeld, 1u);
	EXPECT_EQ(end.admitted, 3u);          // blocker, q1, q3
	EXPECT_EQ(end.queuedAtLeastOnce, 3u);
	EXPECT_EQ(end.stolen, 1u);
	EXPECT_EQ(end.trySubmitFailed, 0u);
	EXPECT_EQ(end.peakQueueDepth, 3u);
	EXPECT_EQ(end.workerCap, 1u);
}

// peakSlotsHeld reaches the cap exactly and never goes above it.
TEST(DeferredStepCapGateTest, PeakSlotsHeldReachesCapExactly)
{
	JobSystem jobs;
	jobs.Initialize(8);
	CapGate gate(jobs, CapOf(3));
	std::atomic<int> running{ 0 };
	Latch release;
	std::vector<std::unique_ptr<GateItem>> items;
	for (int i = 0; i < 5; ++i)
	{
		items.push_back(MakeItem([&] { running.fetch_add(1); release.Wait(); }));
		gate.Admit(*items.back());
	}
	while (running.load() < 3) { std::this_thread::yield(); }
	EXPECT_EQ(gate.GetStats().slotsHeld, 3u);
	EXPECT_EQ(gate.GetStats().peakSlotsHeld, 3u);
	release.Open();
	for (auto& item : items) { EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kDone); }
	EXPECT_EQ(gate.GetStats().peakSlotsHeld, 3u);
	EXPECT_EQ(gate.GetStats().admitted, 5u);
	jobs.Shutdown();
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
}

// WaitItem treats kStolen as terminal: immediate when already stolen.
TEST(DeferredStepCapGateTest, WaitItemReturnsStolenForAStolenItem)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	auto queued = MakeItem([] {});
	gate.Admit(*blocker);
	started.Wait();
	gate.Admit(*queued);
	ASSERT_TRUE(gate.TrySteal(*queued));
	EXPECT_EQ(gate.WaitItem(*queued), GateItem::State::kStolen);
	release.Open();
	gate.WaitItem(*blocker);
	jobs.Shutdown();
}

// A waiter blocked on a queued item is woken by another thread stealing it.
TEST(DeferredStepCapGateTest, WaiterIsWokenWhenAnotherThreadSteals)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	auto queued = MakeItem([] {});
	gate.Admit(*blocker);
	started.Wait();
	gate.Admit(*queued);

	std::atomic<GateItem::State> seen{ GateItem::State::kIdle };
	std::atomic<bool> waiting{ false };
	std::thread waiter([&]
	{
		waiting.store(true);
		seen.store(gate.WaitItem(*queued));
	});
	while (!waiting.load()) { std::this_thread::yield(); }
	ASSERT_TRUE(gate.TrySteal(*queued));
	waiter.join();   // hangs without the notify in TrySteal unless the waiter reads the state after the steal
	EXPECT_EQ(seen.load(), GateItem::State::kStolen);
	release.Open();
	gate.WaitItem(*blocker);
	jobs.Shutdown();
}

// Queue-full path in Admit: the ring holds kMaxChannelsHint * (M * rangesPerWorker + 1) = 8 * 5 = 40 items
// at M = 1 with 4 ranges per worker. The 41st is never dropped: it asserts (Debug) and runs inline.
TEST(DeferredStepCapGateTest, QueueFullRunsItemInlineAndNeverDropsIt)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	std::atomic<int> ran{ 0 };
	std::thread::id inlineThread;
	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	ASSERT_EQ(gate.Admit(*blocker), AdmitResult::kSubmitted);
	started.Wait();

	std::vector<std::unique_ptr<GateItem>> items;
	for (int i = 0; i < 40; ++i)
	{
		items.push_back(MakeItem([&] { ran.fetch_add(1); }));
		ASSERT_EQ(gate.Admit(*items.back()), AdmitResult::kQueued) << i;
	}
	EXPECT_EQ(gate.GetStats().peakQueueDepth, 40u);

	auto overflow = MakeItem([&] { ran.fetch_add(1); inlineThread = std::this_thread::get_id(); });
#ifdef DEBUG
	GateAssertScope scope;
#endif
	EXPECT_EQ(gate.Admit(*overflow), AdmitResult::kSubmitted);
#ifdef DEBUG
	EXPECT_GE(gGateAssertCount, 1);
#endif
	EXPECT_EQ(overflow->GetState(), GateItem::State::kDone);
	EXPECT_EQ(inlineThread, std::this_thread::get_id());
	EXPECT_EQ(ran.load(), 1);                       // only the overflow item has run so far
	EXPECT_EQ(gate.GetStats().queuedAtLeastOnce, 40u);
	EXPECT_EQ(gate.GetStats().slotsHeld, 1u);       // the inline item took no slot

	release.Open();
	for (auto& item : items) { EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kDone); }
	gate.WaitItem(*blocker);
	EXPECT_EQ(ran.load(), 41);
	jobs.Shutdown();
	EXPECT_EQ(gate.GetStats().slotsHeld, 0u);
}

// ResetStats restarts the peaks at the current level, not zero, while work is in flight.
TEST(DeferredStepCapGateTest, ResetStatsWhileHoldingSlotRestartsPeakAtCurrent)
{
	JobSystem jobs;
	jobs.Initialize(2);
	CapGate gate(jobs, CapOf(1));
	Latch started, release;
	auto blocker = MakeItem([&] { started.Open(); release.Wait(); });
	auto queued = MakeItem([] {});
	gate.Admit(*blocker);
	started.Wait();
	gate.Admit(*queued);
	gate.ResetStats();
	const CapStats s = gate.GetStats();
	EXPECT_EQ(s.peakSlotsHeld, 1u);
	EXPECT_EQ(s.peakQueueDepth, 1u);
	EXPECT_EQ(s.admitted, 0u);
	EXPECT_EQ(s.queuedAtLeastOnce, 0u);
	release.Open();
	gate.WaitItem(*queued);
	gate.WaitItem(*blocker);
	jobs.Shutdown();
}
