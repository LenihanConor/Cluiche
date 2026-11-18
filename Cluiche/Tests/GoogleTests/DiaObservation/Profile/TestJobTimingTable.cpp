////////////////////////////////////////////////////////////////////////////////
// Filename: TestJobTimingTable.cpp
// GoogleTest suite - Perf Inspector task 13 part A: JobTimingTable, JobChunkBuilder,
// JobChunkAssembler and the ProcessingUnit job inbox integration.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Job/JobInbox.h>
#include <DiaObservation/Job/JobObserver.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkAssembler.h>
#include <DiaObservation/Profile/JobChunkBuilder.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/CRC/StringCRC.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <type_traits>

using namespace Dia::Observation::Profile;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Observation::Job::JobInbox;
using Dia::Observation::Job::JobObserver;
using Dia::Threading::JobTimingRecord;
using Dia::Threading::JobSystem;
using Dia::Threading::JobTag;

namespace
{
	// ms values -> a record: wait = start - submit, dur = end - start.
	JobTimingRecord Rec(uint32_t owner, double waitMs, double durMs, uint64_t startNs = 1000000000ull, uint16_t worker = 0, uint16_t flags = 0)
	{
		JobTimingRecord r{};
		r.startNs = startNs;
		r.submitNs = startNs - static_cast<uint64_t>(waitMs * 1e6);
		r.endNs = startNs + static_cast<uint64_t>(durMs * 1e6);
		r.owner = owner;
		r.tag = 7;
		r.range = 1;
		r.rangeCount = 4;
		r.worker = worker;
		r.flags = flags;
		return r;
	}
}

// ---------------------------------------------------------------------------
// JobTimingTable
// ---------------------------------------------------------------------------
TEST(JobTimingTable, PerOwnerAggregation)
{
	JobTimingTable t;
	t.Init(64);
	t.Record(Rec(0xA, 1.0, 2.0));
	t.Record(Rec(0xA, 3.0, 4.0));
	t.Record(Rec(0xB, 0.5, 1.0));
	t.EndFrame(5);

	ASSERT_EQ(2u, t.GetOwnerCount());
	const JobOwnerTier1 a = t.GetOwner(0);
	EXPECT_EQ(0xAu, a.owner);
	EXPECT_EQ(2u, a.count);
	EXPECT_EQ(6000000ull, a.busyNs);
	EXPECT_NEAR(3.0f, a.worstWaitMs, 1e-3f);
	EXPECT_EQ(1u, t.GetOwner(1).count);
	EXPECT_EQ(3u, t.GetRecordsSeen());

	// One aggregate per owner for the closed frame.
	ASSERT_EQ(2u, t.GetAggregateTotal());
	JobFrameAggregate agg{};
	ASSERT_TRUE(t.GetAggregate(0, agg));
	EXPECT_EQ(5u, agg.frame);
	EXPECT_EQ(0xAu, agg.owner);
	EXPECT_EQ(2u, agg.count);
	EXPECT_NEAR(6.0f, agg.busyMs, 1e-3f);
	EXPECT_NEAR(3.0f, agg.worstWaitMs, 1e-3f);

	// Frame accumulators reset: an empty frame adds nothing.
	t.EndFrame(6);
	EXPECT_EQ(2u, t.GetAggregateTotal());
}

TEST(JobTimingTable, QueueWaitPercentiles)
{
	JobTimingTable t;
	t.Init(8);
	for (int i = 1; i <= 100; ++i)
		t.Record(Rec(0x1, static_cast<double>(i), 1.0, 500000000));
	const JobOwnerTier1 o = t.GetOwner(0);
	EXPECT_NEAR(50.0f, o.waitP50Ms, 0.01f);
	EXPECT_NEAR(95.0f, o.waitP95Ms, 0.01f);
	EXPECT_NEAR(100.0f, o.worstWaitMs, 0.01f);
}

TEST(JobTimingTable, WaitWindowKeepsOnlyNewest)
{
	JobTimingTable t;
	t.Init(8);
	for (uint32_t i = 0; i < JobTimingTable::kWaitWindow; ++i)
		t.Record(Rec(0x1, 100.0, 1.0, 500000000));   // old, large waits
	for (uint32_t i = 0; i < JobTimingTable::kWaitWindow; ++i)
		t.Record(Rec(0x1, 1.0, 1.0, 500000000));     // newest, small waits push the window clear
	EXPECT_NEAR(1.0f, t.GetOwner(0).waitP95Ms, 0.01f);
	EXPECT_NEAR(100.0f, t.GetOwner(0).worstWaitMs, 0.01f);   // worst stays cumulative
}

TEST(JobTimingTable, EmptyOwnerAndOutOfRange)
{
	JobTimingTable t;
	t.Init(8);
	EXPECT_EQ(0u, t.GetOwnerCount());
	EXPECT_EQ(0u, t.GetOwner(0).count);
}

TEST(JobTimingTable, OwnerOverflowBucket)
{
	JobTimingTable t;
	t.Init(8);
	for (uint32_t i = 1; i <= JobTimingTable::kMaxOwners + 5; ++i)
		t.Record(Rec(i, 0.0, 1.0));
	EXPECT_EQ(JobTimingTable::kMaxOwners, t.GetOwnerCount());
	uint32_t total = 0;
	for (uint32_t i = 0; i < t.GetOwnerCount(); ++i) total += t.GetOwner(i).count;
	EXPECT_EQ(JobTimingTable::kMaxOwners + 5, total);
	EXPECT_EQ(0u, t.GetOwner(JobTimingTable::kMaxOwners - 1).owner);
	EXPECT_EQ(6u, t.GetOwner(JobTimingTable::kMaxOwners - 1).count);
}

TEST(JobTimingTable, RawRingWrapDropsOldest)
{
	JobTimingTable t;
	t.Init(4);
	for (uint32_t i = 0; i < 10; ++i)
		t.Record(Rec(0x1, 0.0, 1.0, 1000000 + i * 1000000, static_cast<uint16_t>(i)));
	EXPECT_EQ(10u, t.GetRawTotal());
	EXPECT_EQ(4u, t.GetRawCount());
	EXPECT_EQ(6u, t.GetRawOldest());
	EXPECT_EQ(6u, t.GetRawOverwritten());

	JobRecordPacked p{};
	EXPECT_FALSE(t.GetRaw(5, p));   // dropped
	ASSERT_TRUE(t.GetRaw(6, p));
	EXPECT_EQ(6u, p.worker);
	ASSERT_TRUE(t.GetRaw(9, p));
	EXPECT_EQ(9u, p.worker);
	EXPECT_EQ(7u, p.tag);
	EXPECT_EQ(4u, p.rangeCount);
	EXPECT_EQ(1000000u, p.durNs);
	EXPECT_FALSE(t.GetRaw(10, p));
}

TEST(JobTimingTable, CopyRawFiltersByTimeAndReportsTruncation)
{
	JobTimingTable t;
	t.Init(64);
	for (uint32_t i = 0; i < 10; ++i)
		t.Record(Rec(0x1, 0.0, 0.5, 10000000ull * (i + 1)));   // starts at 10, 20, ... 100 ms, 0.5 ms long
	JobRecordPacked out[16];
	bool truncated = true;
	EXPECT_EQ(3u, t.CopyRaw(20000000, 40000000, out, 16, &truncated));
	EXPECT_FALSE(truncated);
	EXPECT_EQ(20000000ull, out[0].startNs);
	EXPECT_EQ(2u, t.CopyRaw(0, 1000000000, out, 2, &truncated));
	EXPECT_TRUE(truncated);
}

TEST(JobTimingTable, DrainsRealInbox)
{
	JobInbox inbox(64);
	JobTimingTable t;
	t.Init(64);
	for (uint32_t i = 0; i < 10; ++i)
		ASSERT_TRUE(inbox.TryPush(Rec(0x5, 1.0, 1.0)));
	EXPECT_EQ(10u, t.Drain(inbox, 1));
	EXPECT_EQ(10u, t.GetOwner(0).count);
	EXPECT_EQ(10u, t.GetRawTotal());
	EXPECT_EQ(1u, t.GetAggregateTotal());
	EXPECT_EQ(0u, t.Drain(inbox, 2));   // empty inbox: nothing recorded, no aggregate
	EXPECT_EQ(1u, t.GetAggregateTotal());
}

TEST(JobTimingTable, DrainBeyondOneBatch)
{
	JobInbox inbox(512);
	JobTimingTable t;
	t.Init(1024);
	for (uint32_t i = 0; i < 300; ++i)
		ASSERT_TRUE(inbox.TryPush(Rec(0x5, 0.0, 1.0)));
	EXPECT_EQ(300u, t.Drain(inbox, 1));
	EXPECT_EQ(300u, t.GetOwner(0).count);
}

TEST(JobTimingTable, InboxDropsAndInlineFallbacksCounted)
{
	JobInbox inbox(2);   // capacity 2
	JobTimingTable t;
	t.Init(16);
	for (int i = 0; i < 5; ++i)
		inbox.TryPush(Rec(0x5, 0.0, 1.0, 1000000, 0, JobTimingTable::kFlagInline));
	t.Drain(inbox, 1);
	EXPECT_EQ(3u, t.GetPool().inboxDrops);
	EXPECT_EQ(2u, t.GetPool().inlineFallbacks);
	// Fields the job observation does not expose stay 0.
	EXPECT_EQ(0u, t.GetPool().capUse);
	EXPECT_EQ(0u, t.GetPool().peakQueueDepth);
	EXPECT_FLOAT_EQ(0.0f, t.GetPool().timeAtCapMs);
}

TEST(JobTimingTable, PoolSampleHook)
{
	JobTimingTable t;
	t.Init(4);
	t.NotePoolSample(3, 2, 4, 10.0f);
	t.NotePoolSample(9, 4, 4, 10.0f);
	t.NotePoolSample(1, 4, 4, 5.0f);
	EXPECT_EQ(9u, t.GetPool().peakQueueDepth);
	EXPECT_EQ(4u, t.GetPool().capUse);
	EXPECT_FLOAT_EQ(15.0f, t.GetPool().timeAtCapMs);
}

TEST(JobTimingTable, KillSwitchOffKeepsCountersOnly)
{
	JobInbox inbox(64);
	JobTimingTable t;
	t.Init(64);
	t.SetPerJobRecording(false);
	EXPECT_FALSE(t.GetPerJobRecording());
	for (uint32_t i = 0; i < 20; ++i)
		inbox.TryPush(Rec(0x9, 2.0, 3.0, 1000000000ull, 0, JobTimingTable::kFlagInline));
	t.Drain(inbox, 1);

	EXPECT_EQ(0u, t.GetRawTotal());          // per-job record path not taken
	EXPECT_EQ(0u, t.GetAggregateTotal());
	ASSERT_EQ(1u, t.GetOwnerCount());        // tier 1 still produced
	EXPECT_EQ(20u, t.GetOwner(0).count);
	EXPECT_EQ(60000000ull, t.GetOwner(0).busyNs);
	EXPECT_NEAR(2.0f, t.GetOwner(0).waitP95Ms, 1e-3f);
	EXPECT_EQ(20u, t.GetPool().inlineFallbacks);

	t.SetPerJobRecording(true);
	inbox.TryPush(Rec(0x9, 2.0, 3.0));
	t.Drain(inbox, 2);
	EXPECT_EQ(1u, t.GetRawTotal());
}

TEST(JobTimingTable, StepSummaryHookStartsEmptyAndWraps)
{
	JobTimingTable t;
	t.Init(4);
	EXPECT_EQ(0u, t.GetStepSummaryCount());
	StepSummary s{};
	EXPECT_FALSE(t.GetStepSummary(0, s));
	for (uint32_t i = 0; i < JobTimingTable::kStepCapacity + 3; ++i)
	{
		s.channel = 0xC; s.kickKey = i; s.cpuMs = 1.5f; s.capWaitMs = 0.25f; s.workerWaitMs = 0.5f; s.ranges = 8;
		t.AddStepSummary(s);
	}
	EXPECT_EQ(JobTimingTable::kStepCapacity, t.GetStepSummaryCount());
	ASSERT_TRUE(t.GetStepSummary(0, s));
	EXPECT_EQ(3u, s.kickKey);                               // oldest kept
	ASSERT_TRUE(t.GetStepSummary(JobTimingTable::kStepCapacity - 1, s));
	EXPECT_EQ(JobTimingTable::kStepCapacity + 2, s.kickKey);
	EXPECT_FLOAT_EQ(0.25f, s.capWaitMs);
	EXPECT_FALSE(t.GetStepSummary(JobTimingTable::kStepCapacity, s));
}

TEST(JobTimingTable, HotPathCostSmoke)
{
	// Not a gate: a loose guard that Record stays O(1) (10,000 records well under 50 ms even in Debug).
	// SD-PERF-7 per-frame cost is NOT measured here.
	JobTimingTable t;
	t.Init(JobTimingTable::kDefaultRawCapacity);
	const auto t0 = std::chrono::steady_clock::now();
	for (uint32_t i = 0; i < 10000; ++i)
		t.Record(Rec(0x1 + (i % 4), 0.1, 0.2));
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	EXPECT_LT(ms, 50.0);
}

// ---------------------------------------------------------------------------
// JobChunkBuilder
// ---------------------------------------------------------------------------
TEST(JobChunk, StructIsTriviallyCopyableAndUnderBudget)
{
	EXPECT_TRUE(std::is_trivially_copyable<JobChunk>::value);
	EXPECT_LE(sizeof(JobChunk), 9000u);
}

TEST(JobChunkBuilder, PacksTier1AndRecords)
{
	JobTimingTable t;
	t.Init(64);
	t.Record(Rec(0xA, 1.0, 2.0, 1000000, 3));
	t.Record(Rec(0xB, 2.0, 1.0, 2000000, 4));
	t.EndFrame(9);

	JobChunkBuilder b;
	b.Begin(0x77);
	const JobChunk& c = b.Take(t, 100.0);
	EXPECT_EQ(0x77u, c.unitCrc);
	EXPECT_EQ(0u, c.seq);
	EXPECT_NE(0u, c.flags & JobChunk::kFlagPerJob);
	EXPECT_EQ(2u, c.ownerCount);
	EXPECT_EQ(0xAu, c.owners[0].owner);
	EXPECT_EQ(2000000ull, c.owners[0].busyNs);
	EXPECT_EQ(2u, c.recordsSeen);
	EXPECT_EQ(2u, c.aggCount);
	EXPECT_EQ(9u, c.aggs[0].frame);
	EXPECT_EQ(2u, c.rawCount);
	EXPECT_EQ(0ull, c.rawFirst);
	EXPECT_EQ(3u, c.raw[0].worker);
	EXPECT_EQ(4u, c.raw[1].worker);
}

TEST(JobChunkBuilder, OnlyUnsentRecordsAndSeqIncrements)
{
	JobTimingTable t;
	t.Init(64);
	JobChunkBuilder b;
	b.Begin(1);
	t.Record(Rec(0xA, 0, 1)); t.EndFrame(1);
	EXPECT_EQ(1u, b.Take(t, 100.0).rawCount);
	t.Record(Rec(0xA, 0, 1)); t.Record(Rec(0xA, 0, 1)); t.EndFrame(2);
	const JobChunk& c = b.Take(t, 200.0);
	EXPECT_EQ(1u, c.seq);
	EXPECT_EQ(2u, c.rawCount);
	EXPECT_EQ(1ull, c.rawFirst);
	EXPECT_EQ(1ull, c.aggFirst);
	EXPECT_EQ(1u, c.aggCount);
}

TEST(JobChunkBuilder, ThrottleWithFakeTimeAndIdleSkip)
{
	JobTimingTable t;
	t.Init(64);
	JobChunkBuilder b;
	b.Begin(1);
	EXPECT_FALSE(b.ShouldFlush(t, 500.0));          // nothing happened: nothing to send
	t.Record(Rec(0xA, 0, 1));
	EXPECT_TRUE(b.ShouldFlush(t, 500.0));
	b.Take(t, 500.0);
	t.Record(Rec(0xA, 0, 1));
	EXPECT_FALSE(b.ShouldFlush(t, 550.0));          // 50 ms: throttled
	EXPECT_FALSE(b.ShouldFlush(t, 599.0));
	EXPECT_TRUE(b.ShouldFlush(t, 600.0));           // 100 ms
	b.Take(t, 600.0);
	EXPECT_FALSE(b.ShouldFlush(t, 800.0));          // unchanged since the last send
}

TEST(JobChunkBuilder, BurstLargerThanChunkKeepsNewest)
{
	JobTimingTable t;
	t.Init(1024);
	for (uint32_t i = 0; i < 500; ++i)
		t.Record(Rec(0xA, 0, 1, 1000000 + i, static_cast<uint16_t>(i)));
	JobChunkBuilder b;
	b.Begin(1);
	const JobChunk& c = b.Take(t, 100.0);
	EXPECT_EQ(JobChunk::kMaxRaw, c.rawCount);
	EXPECT_EQ(500ull - JobChunk::kMaxRaw, c.rawFirst);
	EXPECT_EQ(499u, c.raw[c.rawCount - 1].worker);
}

TEST(JobChunkBuilder, KillSwitchOffSendsTier1Only)
{
	JobTimingTable t;
	t.Init(64);
	t.SetPerJobRecording(false);
	for (int i = 0; i < 10; ++i) t.Record(Rec(0xA, 0, 1));
	t.EndFrame(1);
	JobChunkBuilder b;
	b.Begin(1);
	const JobChunk& c = b.Take(t, 100.0);
	EXPECT_EQ(0u, c.flags & JobChunk::kFlagPerJob);
	EXPECT_EQ(1u, c.ownerCount);
	EXPECT_EQ(10u, c.owners[0].count);
	EXPECT_EQ(0u, c.rawCount);
	EXPECT_EQ(0u, c.aggCount);
}

// ---------------------------------------------------------------------------
// Deferred-step summaries through the chunk
// ---------------------------------------------------------------------------
namespace
{
	StepSummary Step(uint32_t channel, uint32_t kickKey)
	{
		StepSummary s{};
		s.channel = channel;
		s.kickKey = kickKey;
		s.applyKey = kickKey + 2;
		s.ranges = 4;
		s.kickNs = 1000ull * kickKey;
		s.applyNs = 2000ull * kickKey;
		s.stallMs = 1.5f;
		return s;
	}
}

TEST(JobChunkBuilder, NoStepsGivesEmptySection)
{
	JobTimingTable t; t.Init(64);
	t.Record(Rec(0xA, 0, 1));
	JobChunkBuilder b; b.Begin(1);
	const JobChunk& c = b.Take(t, 100.0);
	EXPECT_EQ(0u, c.stepCount);
	EXPECT_EQ(0ull, c.stepFirst);
	EXPECT_EQ(0ull, c.stepsDropped);
}

TEST(JobChunkBuilder, StepSummariesShipExactlyOnce)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(1);
	t.AddStepSummary(Step(0xC1, 10));
	t.AddStepSummary(Step(0xC2, 11));
	const JobChunk c0 = b.Take(t, 100.0);
	ASSERT_EQ(2u, c0.stepCount);
	EXPECT_EQ(0ull, c0.stepFirst);
	EXPECT_EQ(0xC1u, c0.steps[0].channel);
	EXPECT_EQ(10u, c0.steps[0].kickKey);
	EXPECT_EQ(12u, c0.steps[0].applyKey);
	EXPECT_EQ(4u, c0.steps[0].ranges);
	EXPECT_FLOAT_EQ(1.5f, c0.steps[0].stallMs);
	EXPECT_EQ(11u, c0.steps[1].kickKey);

	const JobChunk c1 = b.Take(t, 200.0);   // nothing new
	EXPECT_EQ(0u, c1.stepCount);
	EXPECT_EQ(2ull, c1.stepFirst);

	t.AddStepSummary(Step(0xC1, 12));
	const JobChunk c2 = b.Take(t, 300.0);
	ASSERT_EQ(1u, c2.stepCount);
	EXPECT_EQ(2ull, c2.stepFirst);
	EXPECT_EQ(12u, c2.steps[0].kickKey);
	EXPECT_EQ(0ull, c2.stepsDropped);
}

TEST(JobChunkBuilder, PendingStepSummariesTriggerFlush)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(1);
	EXPECT_FALSE(b.ShouldFlush(t, 500.0));
	t.AddStepSummary(Step(1, 1));
	EXPECT_TRUE(b.ShouldFlush(t, 500.0));          // no job records at all, a step alone is enough
	b.Take(t, 500.0);
	EXPECT_FALSE(b.ShouldFlush(t, 700.0));
}

TEST(JobChunkBuilder, StepBurstOverChunkLimitIsCarriedNotLost)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(1);
	const uint32_t n = JobChunk::kMaxSteps * 2 + 1;
	for (uint32_t i = 0; i < n; ++i) t.AddStepSummary(Step(1, 100 + i));

	uint32_t next = 100;
	double now = 100.0;
	uint32_t chunks = 0;
	while (b.ShouldFlush(t, now))
	{
		const JobChunk& c = b.Take(t, now);
		EXPECT_LE(c.stepCount, JobChunk::kMaxSteps);
		EXPECT_EQ(next - 100, static_cast<uint32_t>(c.stepFirst));
		for (uint32_t i = 0; i < c.stepCount; ++i)
			EXPECT_EQ(next++, c.steps[i].kickKey);
		EXPECT_EQ(0ull, c.stepsDropped);
		now += 100.0;
		ASSERT_LT(++chunks, 10u);
	}
	EXPECT_EQ(100 + n, next);
	EXPECT_EQ(3u, chunks);
}

TEST(JobChunkBuilder, StepRingWrapPastCursorCountsDropped)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(1);
	t.AddStepSummary(Step(1, 0));
	b.Take(t, 100.0);                                   // cursor at 1
	const uint32_t extra = JobTimingTable::kStepCapacity + 5;
	for (uint32_t i = 0; i < extra; ++i) t.AddStepSummary(Step(1, 1 + i));
	// ring now holds absolute indexes [6, 70); the cursor (1) wrapped out by 5
	const JobChunk c = b.Take(t, 200.0);
	EXPECT_EQ(5ull, c.stepsDropped);
	EXPECT_EQ(6ull, c.stepFirst);
	ASSERT_EQ(JobChunk::kMaxSteps, c.stepCount);
	EXPECT_EQ(6u, c.steps[0].kickKey);   // absolute index i carries kickKey i
}

TEST(JobChunkBuilder, BeginRestartsStepCursorAndDropCount)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(1, 1u);
	for (uint32_t i = 0; i < JobTimingTable::kStepCapacity + 2; ++i) t.AddStepSummary(Step(1, i));
	b.Take(t, 100.0);
	b.Begin(1, 2u);
	const JobChunk& c = b.Take(t, 200.0);
	EXPECT_EQ(2ull, c.stepFirst);                      // same rule as raw records: restart re-ships what the ring holds
	EXPECT_EQ(2ull, c.stepsDropped);
	EXPECT_EQ(3u, c.stepCount);
}

TEST(JobChunkAssembler, StepsMergeInOrderAndStreamGapCountsLost)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(0x42);
	JobChunkAssembler a;
	t.AddStepSummary(Step(1, 0)); t.AddStepSummary(Step(1, 1));
	a.Apply(b.Take(t, 100.0));
	t.AddStepSummary(Step(1, 2));
	a.Apply(b.Take(t, 200.0));
	EXPECT_EQ(3u, a.GetStepCount(0x42));
	EXPECT_EQ(0ull, a.GetStepsLost(0x42));

	t.AddStepSummary(Step(1, 3)); t.AddStepSummary(Step(1, 4));
	(void)b.Take(t, 300.0);                            // lost on the stream
	t.AddStepSummary(Step(1, 5));
	a.Apply(b.Take(t, 400.0));
	EXPECT_EQ(2ull, a.GetStepsLost(0x42));
	StepSummary out[8];
	ASSERT_EQ(4u, a.CopySteps(0x42, out, 8));
	EXPECT_EQ(0u, out[0].kickKey);
	EXPECT_EQ(5u, out[3].kickKey);
	ASSERT_EQ(1u, a.CopySteps(0x42, out, 1));          // newest only
	EXPECT_EQ(5u, out[0].kickKey);
	EXPECT_EQ(0u, a.GetStepCount(0xBAD));
}

TEST(JobChunkAssembler, StepHistoryKeepsNewestWhenFull)
{
	JobTimingTable t; t.Init(64);
	JobChunkBuilder b; b.Begin(0x42);
	JobChunkAssembler a;
	const uint32_t total = JobChunkAssembler::kStepHistoryCapacity + 10;
	for (uint32_t i = 0; i < total; ++i)
	{
		t.AddStepSummary(Step(1, i));
		a.Apply(b.Take(t, 100.0 * (i + 1)));
	}
	EXPECT_EQ(JobChunkAssembler::kStepHistoryCapacity, a.GetStepCount(0x42));
	StepSummary out[JobChunkAssembler::kStepHistoryCapacity];
	ASSERT_EQ(JobChunkAssembler::kStepHistoryCapacity, a.CopySteps(0x42, out, JobChunkAssembler::kStepHistoryCapacity));
	EXPECT_EQ(10u, out[0].kickKey);
	EXPECT_EQ(total - 1, out[JobChunkAssembler::kStepHistoryCapacity - 1].kickKey);
	EXPECT_EQ(0ull, a.GetStepsLost(0x42));
}

// ---------------------------------------------------------------------------
// JobChunkAssembler
// ---------------------------------------------------------------------------
namespace
{
	JobChunk MakeChunk(JobChunkBuilder& b, JobTimingTable& t, uint32_t nJobs, uint16_t workerBase, double nowMs)
	{
		for (uint32_t i = 0; i < nJobs; ++i)
			t.Record(Rec(0xA, 0.5, 1.0, 1000000ull * (workerBase + i + 1), static_cast<uint16_t>(workerBase + i)));
		t.EndFrame(workerBase);
		return b.Take(t, nowMs);
	}
}

TEST(JobChunkAssembler, MergesChunksAndKeepsLatestTier1)
{
	JobTimingTable t; t.Init(256);
	JobChunkBuilder b; b.Begin(0x42);
	JobChunkAssembler a;
	a.Apply(MakeChunk(b, t, 5, 0, 100));
	a.Apply(MakeChunk(b, t, 3, 5, 200));

	ASSERT_EQ(1u, a.GetUnitCount());
	EXPECT_EQ(0x42u, a.GetUnitCrcAt(0));
	EXPECT_EQ(0u, a.GetGapCount(0x42));
	EXPECT_EQ(0u, a.GetRawLost(0x42));
	EXPECT_EQ(8u, a.GetRawCount(0x42));
	ASSERT_NE(nullptr, a.GetLatest(0x42));
	EXPECT_EQ(8u, a.GetLatest(0x42)->owners[0].count);

	JobRecordPacked out[16];
	bool trunc = true;
	ASSERT_EQ(8u, a.CopyRaw(0x42, 0, ~0ull, out, 16, &trunc));
	EXPECT_FALSE(trunc);
	for (uint32_t i = 0; i < 8; ++i) EXPECT_EQ(i, out[i].worker);

	JobFrameAggregate aggs[8];
	ASSERT_EQ(2u, a.CopyAggregates(0x42, aggs, 8));
	EXPECT_EQ(5u, aggs[0].count);
	EXPECT_EQ(3u, aggs[1].count);
	ASSERT_EQ(1u, a.CopyAggregates(0x42, aggs, 1));   // newest only
	EXPECT_EQ(3u, aggs[0].count);
}

TEST(JobChunkAssembler, SeqGapCountsLostRecords)
{
	JobTimingTable t; t.Init(256);
	JobChunkBuilder b; b.Begin(0x42);
	JobChunkAssembler a;
	a.Apply(MakeChunk(b, t, 4, 0, 100));
	MakeChunk(b, t, 6, 4, 200);                      // lost on the stream
	a.Apply(MakeChunk(b, t, 2, 10, 300));
	EXPECT_EQ(1u, a.GetGapCount(0x42));
	EXPECT_EQ(6ull, a.GetRawLost(0x42));
	EXPECT_EQ(1ull, a.GetAggLost(0x42));
	EXPECT_EQ(6u, a.GetRawCount(0x42));
	EXPECT_EQ(12u, a.GetLatest(0x42)->owners[0].count);   // tier 1 is cumulative: nothing lost there
}

TEST(JobChunkAssembler, LateAttachCountsNothingLost)
{
	JobTimingTable t; t.Init(256);
	JobChunkBuilder b; b.Begin(0x42);
	MakeChunk(b, t, 4, 0, 100);
	MakeChunk(b, t, 4, 4, 200);
	JobChunkAssembler a;
	a.Apply(MakeChunk(b, t, 2, 8, 300));   // first chunk this reader sees (seq 2, index 8)
	EXPECT_EQ(0u, a.GetGapCount(0x42));
	EXPECT_EQ(0ull, a.GetRawLost(0x42));
	EXPECT_EQ(0ull, a.GetAggLost(0x42));
	EXPECT_EQ(2u, a.GetRawCount(0x42));
	EXPECT_EQ(10u, a.GetLatest(0x42)->owners[0].count);
}

TEST(JobChunkAssembler, DuplicateAndStaleChunksIgnored)
{
	JobTimingTable t; t.Init(256);
	JobChunkBuilder b; b.Begin(0x42);
	const JobChunk c0 = MakeChunk(b, t, 3, 0, 100);
	const JobChunk c1 = MakeChunk(b, t, 3, 3, 200);
	JobChunkAssembler a;
	a.Apply(c0);
	a.Apply(c1);
	a.Apply(c0);   // stale
	a.Apply(c1);   // duplicate
	EXPECT_EQ(6u, a.GetRawCount(0x42));
	EXPECT_EQ(0u, a.GetGapCount(0x42));
}

TEST(JobChunkAssembler, RingBoundsDropOldest)
{
	JobTimingTable t; t.Init(JobChunk::kMaxRaw * 2);
	JobChunkBuilder b; b.Begin(0x42);
	JobChunkAssembler a;
	const uint32_t chunks = (JobChunkAssembler::kRawRingCapacity / JobChunk::kMaxRaw) + 4;
	for (uint32_t i = 0; i < chunks; ++i)
		a.Apply(MakeChunk(b, t, JobChunk::kMaxRaw, static_cast<uint16_t>(0), 100.0 * (i + 1)));
	EXPECT_EQ(JobChunkAssembler::kRawRingCapacity, a.GetRawCount(0x42));
	EXPECT_EQ(0ull, a.GetRawLost(0x42));
	JobRecordPacked out[4];
	bool trunc = false;
	EXPECT_EQ(4u, a.CopyRaw(0x42, 0, ~0ull, out, 4, &trunc));
	EXPECT_TRUE(trunc);
}

TEST(JobChunkAssembler, UnknownUnitBeyondCapacityDropped)
{
	JobChunkAssembler a;
	for (uint32_t u = 0; u < JobChunkAssembler::kMaxUnits + 1; ++u)
	{
		JobTimingTable t; t.Init(8);
		JobChunkBuilder b; b.Begin(0x100 + u);
		t.Record(Rec(0xA, 0, 1));
		a.Apply(b.Take(t, 100.0));
	}
	EXPECT_EQ(JobChunkAssembler::kMaxUnits, a.GetUnitCount());
	EXPECT_EQ(1u, a.GetDroppedUnknownUnitCount());
	EXPECT_EQ(nullptr, a.GetLatest(0x100 + JobChunkAssembler::kMaxUnits));
	EXPECT_EQ(0u, a.GetRawCount(0x999));
}

// ---------------------------------------------------------------------------
// ProcessingUnit integration
// ---------------------------------------------------------------------------
TEST(SLOW_JobChunkPU, ProcessingUnitDrainsJobFromRealJobSystem)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	ProcessingUnit pu(StringCRC("job_timing_pu"), 60.0f, false);
	EventStreamStore<JobChunk> store(StringCRC("test_perfjobs"), StringCRC("JobChunk"), 8, 1);
	const int reader = store.RegisterReader();
	ASSERT_GE(reader, 0);
	pu.SetPerfJobsSink(&store);

	JobObserver::InstallThreadInbox(&pu.GetJobInbox());   // Update() installs it only while it runs

	JobTag tag; tag.owner = 0xBEEF; tag.tag = 3; tag.range = 1; tag.rangeCount = 2;
	auto h = js.Submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }, tag);
	js.Wait(h);
	js.Shutdown();   // finish hooks have run

	pu.Update(1.0f / 60.0f);

	const JobTimingTable& t = pu.GetJobTimingTable();
	ASSERT_EQ(1u, t.GetOwnerCount());
	EXPECT_EQ(0xBEEFu, t.GetOwner(0).owner);
	EXPECT_EQ(1u, t.GetOwner(0).count);
	EXPECT_GT(t.GetOwner(0).busyNs, 1000000ull);
	ASSERT_EQ(1u, t.GetRawTotal());
	JobRecordPacked p{};
	ASSERT_TRUE(t.GetRaw(0, p));
	EXPECT_EQ(3u, p.tag);
	EXPECT_EQ(2u, p.rangeCount);

	// The chunk reaches the sink once the 100 ms throttle allows it.
	for (int i = 0; i < 300; ++i)
	{
		auto events = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<JobChunk>, 4>>();
		store.Consume(reader, *events);
		if (events->Size() > 0)
		{
			const JobChunk& c = (*events)[0].payload;
			EXPECT_EQ(StringCRC("job_timing_pu").Value(), c.unitCrc);
			EXPECT_EQ(1u, c.ownerCount);
			EXPECT_EQ(0xBEEFu, c.owners[0].owner);
			EXPECT_EQ(1u, c.rawCount);
			JobObserver::ClearThreadInbox();
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		pu.Update(1.0f / 60.0f);
	}
	JobObserver::ClearThreadInbox();
	FAIL() << "no JobChunk reached the sink";
}

TEST(SLOW_JobChunkPU, ProcessingUnitIdleSendsNothing)
{
	EventStreamStore<JobChunk> store(StringCRC("test_perfjobs_idle"), StringCRC("JobChunk"), 8, 1);
	const int reader = store.RegisterReader();
	ProcessingUnit pu(StringCRC("job_timing_idle"), 60.0f, false);
	pu.SetPerfJobsSink(&store);
	for (int i = 0; i < 50; ++i)
	{
		pu.Update(1.0f / 60.0f);
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
	auto events = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<JobChunk>, 4>>();
	store.Consume(reader, *events);
	EXPECT_EQ(0u, events->Size());   // nothing ran: an idle PU does not send
	EXPECT_EQ(0u, pu.GetJobTimingTable().GetOwnerCount());
}

TEST(SLOW_JobChunkPU, ProcessingUnitKillSwitchSkipsPerJobRecords)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);
	ProcessingUnit pu(StringCRC("job_timing_off"), 60.0f, false);
	pu.SetPerJobRecording(false);
	JobObserver::InstallThreadInbox(&pu.GetJobInbox());
	auto h = js.Submit([] {});
	js.Wait(h);
	js.Shutdown();
	pu.Update(1.0f / 60.0f);
	JobObserver::ClearThreadInbox();
	EXPECT_EQ(1u, pu.GetJobTimingTable().GetRecordsSeen());
	EXPECT_EQ(0u, pu.GetJobTimingTable().GetRawTotal());
	EXPECT_EQ(1u, pu.GetJobTimingTable().GetOwnerCount());
}

TEST(SLOW_JobChunkPU, ProcessingUnitLeavesNoInboxInstalledAfterUpdate)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);
	ProcessingUnit pu(StringCRC("job_timing_scope"), 60.0f, false);
	pu.Update(1.0f / 60.0f);
	auto h = js.Submit([] {});   // outside Update(): the thread has no inbox, so the record is only counted
	js.Wait(h);
	js.Shutdown();
	pu.Update(1.0f / 60.0f);
	EXPECT_EQ(1u, observer.UnownedCount());
	EXPECT_EQ(0u, pu.GetJobTimingTable().GetRecordsSeen());
}
