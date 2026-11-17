#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/RangeTiming.h>
#include <DiaDeferredStep/CapGate.h>
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
			for (int i = 0; i < 48; ++i)
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
					for (int i = 0; i < 16; ++i) { gate.Admit(*items[c * 16 + i]); }
				});
			}
			for (auto& t : admitters) { t.join(); }
			for (auto& item : items)
			{
				EXPECT_EQ(gate.WaitItem(*item), GateItem::State::kDone);
			}

			EXPECT_EQ(ran.load(), 48);
			EXPECT_LE(probe.Peak(), static_cast<int>(m));
			const CapStats stats = gate.GetStats();
			EXPECT_LE(stats.peakSlotsHeld, m);
			EXPECT_EQ(stats.workerCap, m);
			EXPECT_EQ(stats.admitted, 48u);
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
