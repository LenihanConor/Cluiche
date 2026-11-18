// DeferredStepModule step-summary ring (Feature 5 AC 21, 22, 24).
// The no-allocation claim (AC 22) is structural: the ring is a fixed array member (no container, no new on the
// sink path); the sink only copies a trivially-copyable record. There is no allocation-counting hook in the repo.
#include "DeferredStepModuleHarness.h"
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
