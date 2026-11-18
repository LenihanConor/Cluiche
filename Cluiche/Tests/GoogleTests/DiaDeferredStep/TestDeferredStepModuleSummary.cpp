// DeferredStepModule step-summary ring (Feature 5 AC 21, 22, 24).
// The no-allocation claim (AC 22) is structural: the ring is a fixed array member (no container, no new on the
// sink path); the sink only copies a trivially-copyable record. There is no allocation-counting hook in the repo.
#include "DeferredStepModuleHarness.h"
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <type_traits>

using Dia::Observation::Profile::StepSummaryRecord;

namespace
{
	const StringCRC kSummaryChannel("DSMS_Channel");

	// Kick at key k, apply at k+1, for k in [first, first+count).
	void RunSteps(Dia::DeferredStep::DeferredStepScheduler& sched, Dia::DeferredStep::Testing::RecordingStep& step, uint64_t first, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const uint64_t k = first + i;
			ASSERT_EQ(sched.Kick(kSummaryChannel, k, k + 1), Dia::DeferredStep::KickResult::kLaunched);
			sched.Tick(k + 1);
		}
		(void)step;
	}
}

TEST(DeferredStepModuleSummary, RingIsFixedCapacity16)
{
	EXPECT_EQ(DeferredStepModule::kSummaryRingCapacity, 16u);
	EXPECT_TRUE(std::is_trivially_copyable<StepSummaryRecord>::value);
}

TEST(DeferredStepModuleSummary, EmptyAfterStart)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_probe->GetSummaryCount(), 0u);
	StepSummaryRecord r;
	EXPECT_FALSE(g_probe->GetSummary(0, r));
}

TEST(DeferredStepModuleSummary, OneRecordPerAppliedStepOldestFirst)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 10, 3);
	sched.Unregister(kSummaryChannel);

	EXPECT_EQ(step.GetApplyCount(), 3u);
	ASSERT_EQ(g_probe->GetSummaryCount(), 3u);   // exactly one per step, none per range
	for (uint32_t i = 0; i < 3; ++i)
	{
		StepSummaryRecord r;
		ASSERT_TRUE(g_probe->GetSummary(i, r));
		EXPECT_EQ(r.owner, kSummaryChannel.Value());
		EXPECT_EQ(r.kickKeyLow, 10u + i);
		EXPECT_EQ(r.applyKey, 11u + i);
	}
	StepSummaryRecord out;
	EXPECT_FALSE(g_probe->GetSummary(3, out));
}

TEST(DeferredStepModuleSummary, RingWrapsAtSixteenKeepingNewest)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 1, 20);
	sched.Unregister(kSummaryChannel);

	EXPECT_EQ(step.GetApplyCount(), 20u);
	ASSERT_EQ(g_probe->GetSummaryCount(), 16u);
	StepSummaryRecord first, last;
	ASSERT_TRUE(g_probe->GetSummary(0, first));
	ASSERT_TRUE(g_probe->GetSummary(15, last));
	EXPECT_EQ(first.kickKeyLow, 5u);    // the 5th step is the oldest retained
	EXPECT_EQ(last.kickKeyLow, 20u);
	for (uint32_t i = 0; i < 16; ++i)
	{
		StepSummaryRecord r;
		ASSERT_TRUE(g_probe->GetSummary(i, r));
		EXPECT_EQ(r.kickKeyLow, 5u + i);
	}
}

TEST(DeferredStepModuleSummary, NoTableStepAppliesAndRingFills)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 1, 2);
	sched.Unregister(kSummaryChannel);
	EXPECT_EQ(step.GetApplyCount(), 2u);
	EXPECT_EQ(g_probe->GetSummaryCount(), 2u);
}

TEST(DeferredStepModuleSummary, NonNullTableSeamDoesNotCrashAndRingStillFills)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::Observation::Profile::JobTimingTable table;
	g_probe->SetJobTimingTable(&table);
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 1, 3);
	sched.Unregister(kSummaryChannel);
	g_probe->SetJobTimingTable(nullptr);
	EXPECT_EQ(step.GetApplyCount(), 3u);
	EXPECT_EQ(g_probe->GetSummaryCount(), 3u);
}

TEST(DeferredStepModuleSummary, RestartClearsRingAndRewires)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	{
		auto& sched = g_probe->GetScheduler();
		Dia::DeferredStep::Testing::RecordingStep step;
		ASSERT_TRUE(sched.Register(kSummaryChannel, step));
		RunSteps(sched, step, 1, 4);
		sched.Unregister(kSummaryChannel);
		ASSERT_EQ(g_probe->GetSummaryCount(), 4u);
	}
	EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
	EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
	EXPECT_EQ(g_probe->GetSummaryCount(), 0u);

	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 100, 2);
	sched.Unregister(kSummaryChannel);
	ASSERT_EQ(g_probe->GetSummaryCount(), 2u);
	StepSummaryRecord r;
	ASSERT_TRUE(g_probe->GetSummary(0, r));
	EXPECT_EQ(r.kickKeyLow, 100u);
}

TEST(DeferredStepModuleSummary, ForwardsEachAppliedStepToTableWithConvertedFields)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::Observation::Profile::JobTimingTable table;
	table.Init(64);
	g_probe->SetJobTimingTable(&table);
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 10, 3);
	sched.Unregister(kSummaryChannel);
	g_probe->SetJobTimingTable(nullptr);

	ASSERT_EQ(table.GetStepSummaryTotal(), 3u);
	ASSERT_EQ(g_probe->GetSummaryCount(), 3u);
	for (uint32_t i = 0; i < 3; ++i)
	{
		StepSummaryRecord r;
		Dia::Observation::Profile::StepSummary t;
		ASSERT_TRUE(g_probe->GetSummary(i, r));
		ASSERT_TRUE(table.GetStepSummary(i, t));
		EXPECT_EQ(t.channel, r.owner);
		EXPECT_EQ(t.kickKey, static_cast<uint32_t>(r.kickKeyLow));
		EXPECT_EQ(t.applyKey, static_cast<uint32_t>(r.applyKey & 0xFFFFFFFFull));
		EXPECT_EQ(t.ranges, static_cast<uint16_t>(r.rangeCount));
		EXPECT_EQ(t.kickNs, r.kickNs);
		EXPECT_EQ(t.firstStartNs, r.firstRangeStartNs);
		EXPECT_EQ(t.readyNs, r.readyNs);
		EXPECT_EQ(t.applyNs, r.applyNs);
		EXPECT_NEAR(t.stallMs, static_cast<double>(r.stallNs) / 1.0e6, 1e-3);
		EXPECT_NEAR(t.cpuMs, static_cast<double>(r.cpuNs) / 1.0e6, 1e-3);
		EXPECT_NEAR(t.capWaitMs, static_cast<double>(r.capWaitNs) / 1.0e6, 1e-3);
		EXPECT_NEAR(t.workerWaitMs, static_cast<double>(r.workerWaitNs) / 1.0e6, 1e-3);
	}
}

TEST(DeferredStepModuleSummary, ResolvesOwnProcessingUnitTableAutomatically)
{
	Harness h;
	ASSERT_TRUE(h.Start());   // DoStart already ran with no explicit table
	ProcessingUnit* pu = g_probe->GetProcessingUnit();
	ASSERT_NE(pu, nullptr);
	const uint64_t before = pu->GetJobTimingTable().GetStepSummaryTotal();
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	RunSteps(sched, step, 1, 2);
	sched.Unregister(kSummaryChannel);
	EXPECT_EQ(pu->GetJobTimingTable().GetStepSummaryTotal(), before + 2);
}

TEST(DeferredStepModuleSummary, TableWrittenOnCallingThreadInsideTickNeverFromRun)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::Observation::Profile::JobTimingTable table;
	table.Init(64);
	g_probe->SetJobTimingTable(&table);
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	std::atomic<uint64_t> totalSeenInRun{ 0 };
	std::atomic<bool> ranOnWorker{ false };
	const std::thread::id self = std::this_thread::get_id();
	step.SetOnRun([&]() { totalSeenInRun = table.GetStepSummaryTotal(); if (std::this_thread::get_id() != self) ranOnWorker = true; });
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));
	ASSERT_EQ(sched.Kick(kSummaryChannel, 5, 6), Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(table.GetStepSummaryTotal(), 0u);   // kick (and any worker Run) wrote nothing
	sched.Tick(6);
	EXPECT_EQ(table.GetStepSummaryTotal(), 1u);   // written synchronously by Tick on this thread
	EXPECT_EQ(totalSeenInRun.load(), 0u);
	EXPECT_EQ(step.GetApplyThread(), self);
	sched.Unregister(kSummaryChannel);
	g_probe->SetJobTimingTable(nullptr);
}

TEST(DeferredStepModuleSummary, OutOfRangeIndexReturnsFalseAndLeavesOutUntouchedOnFullAndPartialRing)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kSummaryChannel, step));

	StepSummaryRecord sentinel;
	sentinel.owner = 0xABCD;
	StepSummaryRecord out = sentinel;
	RunSteps(sched, step, 1, 3);
	EXPECT_FALSE(g_probe->GetSummary(3, out));
	EXPECT_FALSE(g_probe->GetSummary(16, out));
	EXPECT_FALSE(g_probe->GetSummary(0xFFFFFFFFu, out));
	EXPECT_EQ(out.owner, 0xABCDu);

	RunSteps(sched, step, 100, 20);   // wraps the ring
	ASSERT_EQ(g_probe->GetSummaryCount(), 16u);
	EXPECT_TRUE(g_probe->GetSummary(15, out));
	EXPECT_FALSE(g_probe->GetSummary(16, out));
	EXPECT_FALSE(g_probe->GetSummary(0xFFFFFFFFu, out));
	sched.Unregister(kSummaryChannel);
}
