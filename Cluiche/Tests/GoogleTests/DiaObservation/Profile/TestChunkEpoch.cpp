////////////////////////////////////////////////////////////////////////////////
// Filename: TestChunkEpoch.cpp
// GoogleTest suite - Perf Inspector row 10b: chunk epoch. A restarted PU builder (new epoch,
// seq 0 / entry 0) must not be ignored by the assemblers' stale-chunk rule.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkBuilder.h>
#include <DiaObservation/Profile/JobChunkAssembler.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaCore/CRC/StringCRC.h>
#include <memory>
#include <type_traits>

using namespace Dia::Observation::Profile;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using Dia::Threading::JobTimingRecord;

namespace
{
	const uint32_t kUnit = 0xE0E0u;

	CoarseChunk CoarseTake(CoarseChunkBuilder& b, double& t)
	{
		for (uint32_t i = 0; i < 8; ++i)
		{
			b.RecordUnitTick(1.0f);
			b.CommitEntry(t);
			t += 10.0;
		}
		return b.Take(t);
	}

	JobTimingRecord Rec()
	{
		JobTimingRecord r{};
		r.startNs = 1000000000ull;
		r.submitNs = r.startNs;
		r.endNs = r.startNs + 1000;
		r.owner = 0xA;
		return r;
	}

	JobChunk JobTake(JobChunkBuilder& b, JobTimingTable& t, uint32_t frame, double now)
	{
		t.Record(Rec());
		t.EndFrame(frame);
		return b.Take(t, now);
	}
}

TEST(ChunkEpoch, StructsStayTriviallyCopyableAndUnderBudget)
{
	EXPECT_TRUE(std::is_trivially_copyable<CoarseChunk>::value);
	EXPECT_TRUE(std::is_trivially_copyable<JobChunk>::value);
	EXPECT_LE(sizeof(CoarseChunk), 9000u);
	EXPECT_LE(sizeof(JobChunk), 9000u);
	EXPECT_EQ(0u, CoarseChunk::kNoEpoch);
	EXPECT_EQ(0u, JobChunk::kNoEpoch);
}

TEST(ChunkEpoch, BuildersStampEveryChunk)
{
	CoarseChunkBuilder cb;
	cb.Begin(kUnit, 60.0f, 77u);
	double t = 0.0;
	EXPECT_EQ(77u, CoarseTake(cb, t).epoch);
	EXPECT_EQ(77u, CoarseTake(cb, t).epoch);

	JobTimingTable table;
	table.Init(64);
	JobChunkBuilder jb;
	jb.Begin(kUnit, 88u);
	EXPECT_EQ(88u, JobTake(jb, table, 1, 100.0).epoch);
	EXPECT_EQ(88u, JobTake(jb, table, 2, 200.0).epoch);

	CoarseChunkBuilder legacy;
	legacy.Begin(kUnit, 60.0f);
	EXPECT_EQ(CoarseChunk::kNoEpoch, CoarseTake(legacy, t).epoch);
}

TEST(ChunkEpoch, CoarseRestartedBuilderIsAccepted)
{
	CoarseChunkAssembler a;
	CoarseChunkBuilder old;
	old.Begin(kUnit, 60.0f, 5u);
	double t = 0.0;
	for (int i = 0; i < 10; ++i)
		a.Apply(CoarseTake(old, t));
	ASSERT_EQ(80u, a.GetRing(kUnit)->GetEntryCount());

	CoarseChunkBuilder fresh;
	fresh.Begin(kUnit, 60.0f, 6u);
	t = 0.0;
	const CoarseChunk c = CoarseTake(fresh, t);
	ASSERT_EQ(0u, c.seq);
	ASSERT_EQ(0u, c.firstEntry);
	a.Apply(c);
	EXPECT_EQ(1u, a.GetEpochResets(kUnit));
	EXPECT_EQ(8u, a.GetRing(kUnit)->GetEntryCount());
	a.Apply(CoarseTake(fresh, t));
	EXPECT_EQ(16u, a.GetRing(kUnit)->GetEntryCount());
	EXPECT_EQ(1u, a.GetEpochResets(kUnit));
	EXPECT_EQ(0u, a.GetGapCount(kUnit));
}

TEST(ChunkEpoch, CoarseSameEpochStaleStillIgnored)
{
	CoarseChunkAssembler a;
	CoarseChunkBuilder b;
	b.Begin(kUnit, 60.0f, 5u);
	double t = 0.0;
	const CoarseChunk c0 = CoarseTake(b, t);
	a.Apply(c0);
	a.Apply(CoarseTake(b, t));
	a.Apply(c0);   // duplicate
	EXPECT_EQ(16u, a.GetRing(kUnit)->GetEntryCount());
	EXPECT_EQ(0u, a.GetEpochResets(kUnit));
	EXPECT_EQ(0u, a.GetGapCount(kUnit));
}

TEST(ChunkEpoch, CoarseNoEpochBehavesAsBefore)
{
	CoarseChunkAssembler a;
	CoarseChunkBuilder old;
	old.Begin(kUnit, 60.0f);
	double t = 0.0;
	const CoarseChunk first = CoarseTake(old, t);
	a.Apply(first);
	a.Apply(CoarseTake(old, t));
	a.Apply(first);   // restarted-looking chunk without an epoch stays ignored
	EXPECT_EQ(16u, a.GetRing(kUnit)->GetEntryCount());
	EXPECT_EQ(0u, a.GetEpochResets(kUnit));
}

TEST(ChunkEpoch, CoarseNoEpochToRealEpochAdoptsWithoutReset)
{
	CoarseChunkAssembler a;
	CoarseChunkBuilder legacy;
	legacy.Begin(kUnit, 60.0f);
	double t = 0.0;
	a.Apply(CoarseTake(legacy, t));
	CoarseChunkBuilder real;
	real.Begin(kUnit, 60.0f, 9u);
	a.Apply(CoarseTake(real, t));   // seq 0 again, now stale: wildcard adopts, no reset
	EXPECT_EQ(0u, a.GetEpochResets(kUnit));
	EXPECT_EQ(8u, a.GetRing(kUnit)->GetEntryCount());
}

TEST(ChunkEpoch, JobRestartedBuilderIsAccepted)
{
	JobChunkAssembler a;
	JobTimingTable table;
	table.Init(64);
	JobChunkBuilder old;
	old.Begin(kUnit, 5u);
	for (uint32_t i = 0; i < 5; ++i)
		a.Apply(JobTake(old, table, i + 1, 100.0 * (i + 1)));
	ASSERT_EQ(5u, a.GetRawCount(kUnit));
	ASSERT_EQ(5u, a.GetAggCount(kUnit));

	JobTimingTable table2;
	table2.Init(64);
	JobChunkBuilder fresh;
	fresh.Begin(kUnit, 6u);
	const JobChunk c = JobTake(fresh, table2, 1, 100.0);
	ASSERT_EQ(0u, c.seq);
	ASSERT_EQ(0u, c.rawFirst);
	a.Apply(c);
	EXPECT_EQ(1u, a.GetEpochResets(kUnit));
	EXPECT_EQ(1u, a.GetRawCount(kUnit));
	EXPECT_EQ(1u, a.GetAggCount(kUnit));
	ASSERT_NE(nullptr, a.GetLatest(kUnit));
	EXPECT_EQ(6u, a.GetLatest(kUnit)->epoch);
	a.Apply(JobTake(fresh, table2, 2, 200.0));
	EXPECT_EQ(2u, a.GetRawCount(kUnit));
	EXPECT_EQ(0u, a.GetGapCount(kUnit));
}

TEST(ChunkEpoch, JobSameEpochStaleStillIgnoredAndNoEpochUnchanged)
{
	JobChunkAssembler a;
	JobTimingTable table;
	table.Init(64);
	JobChunkBuilder b;
	b.Begin(kUnit);   // kNoEpoch
	const JobChunk c0 = JobTake(b, table, 1, 100.0);
	a.Apply(c0);
	a.Apply(JobTake(b, table, 2, 200.0));
	a.Apply(c0);
	EXPECT_EQ(2u, a.GetRawCount(kUnit));
	EXPECT_EQ(0u, a.GetEpochResets(kUnit));
	EXPECT_EQ(0u, a.GetEpochResets(0xDEADu));
}

TEST(ChunkEpoch, ProcessingUnitStampsNonzeroEpoch)
{
	EventStreamStore<CoarseChunk> store(StringCRC("test_chunkepoch"), StringCRC("CoarseChunk"), 8, 1);
	const int reader = store.RegisterReader();
	ASSERT_GE(reader, 0);

	ProcessingUnit pu(StringCRC("chunk_epoch_pu"), 60.0f, false);
	pu.SetPerfCoarseSink(&store);
	for (int i = 0; i < 1000 && pu.GetUnitTimingTable().GetEntryCount() < CoarseChunk::kMaxEntries; ++i)
		pu.Update(1.0f / 60.0f);

	auto events = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<CoarseChunk>, 4>>();
	store.Consume(reader, *events);
	ASSERT_GE(events->Size(), 1u);
	EXPECT_NE(CoarseChunk::kNoEpoch, (*events)[0].payload.epoch);
}
