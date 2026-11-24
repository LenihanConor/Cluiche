////////////////////////////////////////////////////////////////////////////////
// Filename: TestCoarseChunk.cpp
// GoogleTest suite — Perf Inspector task 1b: CoarseChunkBuilder (PU-side),
// CoarseChunkAssembler (Main-side), chunk packing, seq gap detection, entry
// gap handling, and ProcessingUnit integration.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>
#include <memory>

using namespace Dia::Observation::Profile;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

TEST(CoarseChunk, StructIsTriviallyCopyable)
{
	EXPECT_TRUE(std::is_trivially_copyable<CoarseChunk>::value);
}

TEST(CoarseChunk, ChunkPacking_ValuesAndCrcs)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x1234u, 60.0f);

	// Entry 0: 2 modules
	builder.RecordModule(0, 0xAABBu, 1.5f);
	builder.RecordModule(1, 0xCCDDu, 2.75f);
	builder.RecordUnitTick(5.0f);
	builder.CommitEntry(0.0);

	// Entry 1: 3 modules
	builder.RecordModule(0, 0xAABBu, 1.0f);
	builder.RecordModule(1, 0xCCDDu, 2.0f);
	builder.RecordModule(2, 0xEEFFu, 3.0f);
	builder.RecordUnitTick(6.5f);
	builder.CommitEntry(50.0);

	const CoarseChunk& chunk = builder.Take(100.0);
	EXPECT_EQ(0x1234u, chunk.unitCrc);
	EXPECT_EQ(60.0f, chunk.unitHz);
	EXPECT_EQ(2u, chunk.count);
	EXPECT_EQ(3u, chunk.moduleCount);
	EXPECT_EQ(0u, chunk.seq);
	EXPECT_EQ(0u, chunk.firstEntry);

	// Check CRCs
	EXPECT_EQ(0xAABBu, chunk.moduleCrcs[0]);
	EXPECT_EQ(0xCCDDu, chunk.moduleCrcs[1]);
	EXPECT_EQ(0xEEFFu, chunk.moduleCrcs[2]);

	// Check values (series-major layout: ms[series][entry])
	// Entry 0
	EXPECT_EQ(150u, chunk.ms[0][0]);  // module 0: 1.5 ms → 150
	EXPECT_EQ(275u, chunk.ms[1][0]);  // module 1: 2.75 ms → 275
	EXPECT_EQ(0u,   chunk.ms[2][0]);  // module 2: not recorded yet
	EXPECT_EQ(500u, chunk.ms[CoarseRing::kMaxModules][0]);  // unit tick: 5.0 ms → 500

	// Entry 1
	EXPECT_EQ(100u, chunk.ms[0][1]);
	EXPECT_EQ(200u, chunk.ms[1][1]);
	EXPECT_EQ(300u, chunk.ms[2][1]);
	EXPECT_EQ(650u, chunk.ms[CoarseRing::kMaxModules][1]);
}

TEST(CoarseChunkBuilder, FlushAt64Entries)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x1111u, 60.0f);

	for (uint32_t i = 0; i < 63; ++i)
	{
		builder.RecordModule(0, 0x2222u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(i));	// 1 ms per entry: stays under the 100 ms rule
		EXPECT_FALSE(builder.ShouldFlush(static_cast<double>(i) + 0.5));
	}

	builder.RecordModule(0, 0x2222u, 1.0f);
	builder.RecordUnitTick(2.0f);
	builder.CommitEntry(63.0);
	EXPECT_TRUE(builder.ShouldFlush(63.5));	// 64 entries, only 63.5 ms elapsed

	const CoarseChunk& chunk = builder.Take(64.0);
	EXPECT_EQ(64u, chunk.count);
	EXPECT_EQ(0u, chunk.seq);
	EXPECT_EQ(0u, chunk.firstEntry);
}

TEST(CoarseChunkBuilder, FlushAt100Ms)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x3333u, 30.0f);

	builder.RecordModule(0, 0x4444u, 1.0f);
	builder.RecordUnitTick(2.0f);
	builder.CommitEntry(0.0);

	EXPECT_FALSE(builder.ShouldFlush(50.0));   // 50 ms elapsed, count=1
	EXPECT_FALSE(builder.ShouldFlush(99.0));   // 99 ms elapsed
	EXPECT_TRUE(builder.ShouldFlush(100.0));   // 100 ms elapsed

	const CoarseChunk& chunk = builder.Take(100.0);
	EXPECT_EQ(1u, chunk.count);
	EXPECT_EQ(0u, chunk.seq);
}

TEST(CoarseChunkBuilder, NoFlushWhenEmpty)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x5555u, 60.0f);

	EXPECT_FALSE(builder.ShouldFlush(0.0));
	EXPECT_FALSE(builder.ShouldFlush(200.0));
}

TEST(CoarseChunkBuilder, SeqIncrementsAcrossChunks)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x6666u, 60.0f);

	for (int i = 0; i < 3; ++i)
	{
		for (uint32_t e = 0; e < 64; ++e)
		{
			builder.RecordModule(0, 0x7777u, 1.0f);
			builder.RecordUnitTick(2.0f);
			builder.CommitEntry(static_cast<double>(i * 640 + e * 10));
		}
		const CoarseChunk& chunk = builder.Take(static_cast<double>(i * 640 + 700));
		EXPECT_EQ(static_cast<uint32_t>(i), chunk.seq);
		EXPECT_EQ(static_cast<uint64_t>(i * 64), chunk.firstEntry);
		EXPECT_EQ(64u, chunk.count);
	}
}

TEST(CoarseChunkBuilder, FirstEntryContinuityAcrossChunks)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x8888u, 60.0f);

	// Chunk 0: entries 0..63
	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0x9999u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(i) * 10.0);
	}
	CoarseChunk chunk0 = builder.Take(640.0);
	EXPECT_EQ(0u, chunk0.firstEntry);

	// Chunk 1: entries 64..127
	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0x9999u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(640 + i * 10));
	}
	CoarseChunk chunk1 = builder.Take(1280.0);
	EXPECT_EQ(64u, chunk1.firstEntry);
	EXPECT_EQ(1u, chunk1.seq);
}

TEST(CoarseChunkAssembler, RebuildEqualsSourceValues)
{
	CoarseChunkBuilder builder;
	builder.Begin(0xAAAAu, 60.0f);

	for (uint32_t i = 0; i < 10; ++i)
	{
		builder.RecordModule(0, 0xBBBBu, static_cast<float>(i) * 0.5f);
		builder.RecordModule(1, 0xCCCCu, static_cast<float>(i) * 1.0f);
		builder.RecordUnitTick(static_cast<float>(i) * 2.0f);
		builder.CommitEntry(static_cast<double>(i) * 20.0);
	}

	const CoarseChunk& chunk = builder.Take(200.0);

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk);

	EXPECT_EQ(1u, assembler.GetUnitCount());
	const CoarseRing* ring = assembler.GetRing(0xAAAAu);
	ASSERT_NE(nullptr, ring);
	EXPECT_EQ(10u, ring->GetEntryCount());

	// Verify module 0 values
	uint16_t vals0[10];
	EXPECT_EQ(10u, ring->CopyRange(0, 0, 10, vals0));
	for (uint32_t i = 0; i < 10; ++i)
		EXPECT_EQ(static_cast<uint16_t>(i * 50), vals0[i]);

	// Verify module 1 values
	uint16_t vals1[10];
	EXPECT_EQ(10u, ring->CopyRange(1, 0, 10, vals1));
	for (uint32_t i = 0; i < 10; ++i)
		EXPECT_EQ(static_cast<uint16_t>(i * 100), vals1[i]);

	// Verify unit tick
	uint16_t valsU[10];
	EXPECT_EQ(10u, ring->CopyRange(CoarseRing::kUnitSlot, 0, 10, valsU));
	for (uint32_t i = 0; i < 10; ++i)
		EXPECT_EQ(static_cast<uint16_t>(i * 200), valsU[i]);
}

TEST(CoarseChunkAssembler, SeqGapDetection)
{
	CoarseChunkBuilder builder;
	builder.Begin(0xDDDDu, 60.0f);

	// Send chunk 0 (seq=0)
	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0xEEEEu, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(i) * 10.0);
	}
	CoarseChunk chunk0 = builder.Take(640.0);

	// Skip chunk 1 (seq=1), send chunk 2 (seq=2)
	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0xEEEEu, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(640 + i * 10));
	}
	CoarseChunk chunk1_skipped = builder.Take(1280.0);

	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0xEEEEu, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(1280 + i * 10));
	}
	CoarseChunk chunk2 = builder.Take(1920.0);

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk0);
	EXPECT_EQ(0u, assembler.GetGapCount(0xDDDDu));

	assembler.Apply(chunk2);  // seq gap: expected 1, got 2
	EXPECT_EQ(1u, assembler.GetGapCount(0xDDDDu));
}

TEST(CoarseChunkAssembler, EntryGapFilled)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x1111u, 60.0f);

	// Chunk 0: entries 0..63
	for (uint32_t i = 0; i < 64; ++i)
	{
		builder.RecordModule(0, 0x2222u, 10.0f);
		builder.RecordUnitTick(20.0f);
		builder.CommitEntry(static_cast<double>(i) * 10.0);
	}
	CoarseChunk chunk0 = builder.Take(640.0);

	// Manually create chunk with a gap: firstEntry=70 (skipping 64..69)
	CoarseChunk chunk_gap;
	std::memset(&chunk_gap, 0, sizeof(chunk_gap));
	chunk_gap.unitCrc = 0x1111u;
	chunk_gap.seq = 1;
	chunk_gap.firstEntry = 70;
	chunk_gap.count = 10;
	chunk_gap.moduleCount = 1;
	chunk_gap.unitHz = 60.0f;
	chunk_gap.moduleCrcs[0] = 0x2222u;
	for (uint32_t i = 0; i < 10; ++i)
	{
		chunk_gap.ms[0][i] = 500;  // 5.0 ms
		chunk_gap.ms[CoarseRing::kMaxModules][i] = 1000;  // 10.0 ms
	}

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk0);
	assembler.Apply(chunk_gap);

	const CoarseRing* ring = assembler.GetRing(0x1111u);
	ASSERT_NE(nullptr, ring);

	// Entry 64..69 should be filled with zeros and kFlagGap
	uint16_t vals[10];
	EXPECT_EQ(6u, ring->CopyRange(0, 64, 6, vals));
	for (uint32_t i = 0; i < 6; ++i)
		EXPECT_EQ(0u, vals[i]);

	uint8_t flags[6];
	EXPECT_EQ(6u, ring->CopyFlags(64, 6, flags));
	for (uint32_t i = 0; i < 6; ++i)
		EXPECT_NE(0, flags[i] & CoarseRing::kFlagGap);

	// Entry 70..79 should have the real values
	EXPECT_EQ(10u, ring->CopyRange(0, 70, 10, vals));
	for (uint32_t i = 0; i < 10; ++i)
		EXPECT_EQ(500u, vals[i]);
}

TEST(CoarseChunkAssembler, GapLargerThanCapacityResetsRing)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x3333u, 60.0f);

	// Chunk 0: entries 0..9
	for (uint32_t i = 0; i < 10; ++i)
	{
		builder.RecordModule(0, 0x4444u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(i) * 10.0);
	}
	CoarseChunk chunk0 = builder.Take(100.0);

	// Apply chunk 0
	CoarseChunkAssembler assembler;
	assembler.Apply(chunk0);

	const CoarseRing* ring = assembler.GetRing(0x3333u);
	ASSERT_NE(nullptr, ring);
	uint32_t capacityBefore = CoarseRing::CapacityFor(60.0f, 60.0f);
	EXPECT_EQ(10u, ring->GetEntryCount());

	// Create chunk with firstEntry way beyond capacity
	CoarseChunk chunk_huge_gap;
	std::memset(&chunk_huge_gap, 0, sizeof(chunk_huge_gap));
	chunk_huge_gap.unitCrc = 0x3333u;
	chunk_huge_gap.seq = 1;
	chunk_huge_gap.firstEntry = capacityBefore * 2;  // way beyond
	chunk_huge_gap.count = 5;
	chunk_huge_gap.moduleCount = 1;
	chunk_huge_gap.unitHz = 60.0f;
	chunk_huge_gap.moduleCrcs[0] = 0x4444u;
	for (uint32_t i = 0; i < 5; ++i)
	{
		chunk_huge_gap.ms[0][i] = 1000;
		chunk_huge_gap.ms[CoarseRing::kMaxModules][i] = 2000;
	}

	assembler.Apply(chunk_huge_gap);

	// Ring is reset to start at the chunk's firstEntry; indexes stay absolute (PU numbering).
	EXPECT_EQ(static_cast<uint64_t>(capacityBefore) * 2 + 5, ring->GetEntryCount());
	EXPECT_EQ(static_cast<uint64_t>(capacityBefore) * 2, ring->GetOldestAvailable());
	uint16_t values[5] = {};
	EXPECT_EQ(0u, ring->CopyRange(0, 0, 5, values));	// pre-reset entries are gone
	ASSERT_EQ(5u, ring->CopyRange(0, static_cast<uint64_t>(capacityBefore) * 2, 5, values));
	EXPECT_EQ(1000u, values[0]);
}

TEST(CoarseChunkAssembler, OlderChunkIgnored)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x5555u, 60.0f);

	// Chunk 0: entries 0..9
	for (uint32_t i = 0; i < 10; ++i)
	{
		builder.RecordModule(0, 0x6666u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(static_cast<double>(i) * 10.0);
	}
	CoarseChunk chunk0 = builder.Take(100.0);

	// Chunk 1: entries 10..19
	for (uint32_t i = 0; i < 10; ++i)
	{
		builder.RecordModule(0, 0x6666u, 2.0f);
		builder.RecordUnitTick(3.0f);
		builder.CommitEntry(static_cast<double>(100 + i * 10));
	}
	CoarseChunk chunk1 = builder.Take(200.0);

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk1);  // first chunk seen: ring starts at entry 10 (late attach)
	EXPECT_EQ(20u, assembler.GetRing(0x5555u)->GetEntryCount());   // absolute: entries 10..19
	EXPECT_EQ(10u, assembler.GetRing(0x5555u)->GetOldestAvailable());
	EXPECT_EQ(0u, assembler.GetGapCount(0x5555u));

	assembler.Apply(chunk0);  // older chunk, should be ignored
	EXPECT_EQ(20u, assembler.GetRing(0x5555u)->GetEntryCount());  // unchanged
}

TEST(CoarseChunkAssembler, UnknownUnitBeyondCapacityIgnored)
{
	CoarseChunkAssembler assembler;

	// Fill assembler to capacity (8 units)
	for (uint32_t u = 0; u < 8; ++u)
	{
		CoarseChunkBuilder builder;
		builder.Begin(0x1000u + u, 60.0f);
		builder.RecordModule(0, 0x2000u + u, 1.0f);
		builder.RecordUnitTick(2.0f);
		builder.CommitEntry(0.0);
		const CoarseChunk& chunk = builder.Take(10.0);
		assembler.Apply(chunk);
	}

	EXPECT_EQ(8u, assembler.GetUnitCount());

	// Try to add 9th unit
	CoarseChunkBuilder builder9;
	builder9.Begin(0x9999u, 60.0f);
	builder9.RecordModule(0, 0x8888u, 1.0f);
	builder9.RecordUnitTick(2.0f);
	builder9.CommitEntry(0.0);
	const CoarseChunk& chunk9 = builder9.Take(10.0);
	assembler.Apply(chunk9);

	EXPECT_EQ(8u, assembler.GetUnitCount());  // still 8, overflow ignored
	EXPECT_EQ(nullptr, assembler.GetRing(0x9999u));
	EXPECT_EQ(1u, assembler.GetDroppedUnknownUnitCount());
}

TEST(CoarseChunkAssembler, UnitHzZeroFallbackTo60)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x7777u, 0.0f);  // invalid Hz
	builder.RecordModule(0, 0x8888u, 1.0f);
	builder.RecordUnitTick(2.0f);
	builder.CommitEntry(0.0);
	const CoarseChunk& chunk = builder.Take(10.0);

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk);

	const CoarseRing* ring = assembler.GetRing(0x7777u);
	ASSERT_NE(nullptr, ring);
	// Should have used fallback Hz=60 for capacity calculation
	EXPECT_EQ(1u, ring->GetEntryCount());
}

// --- ProcessingUnit integration test ---
// A PU with no modules still commits one entry per forward pass (unit tick only), so after
// 64 passes the builder is full and the PU sends exactly one chunk on the sink.
TEST(CoarseChunk, ProcessingUnitSendsChunkToSink)
{
	EventStreamStore<CoarseChunk> store(StringCRC("test_perfcoarse"), StringCRC("CoarseChunk"), 8, 1);
	const int reader = store.RegisterReader();
	ASSERT_GE(reader, 0);

	ProcessingUnit pu(StringCRC("coarse_chunk_pu"), 60.0f, false);
	pu.SetPerfCoarseSink(&store);

	const uint64_t passesBefore = pu.GetUnitTimingTable().GetEntryCount();
	for (int i = 0; i < 1000 && pu.GetUnitTimingTable().GetEntryCount() - passesBefore < CoarseChunk::kMaxEntries; ++i)
		pu.Update(1.0f / 60.0f);
	ASSERT_GE(pu.GetUnitTimingTable().GetEntryCount() - passesBefore, static_cast<uint64_t>(CoarseChunk::kMaxEntries));

	auto events = std::make_unique<Dia::Core::Containers::DynamicArrayC<Event<CoarseChunk>, 4>>();
	store.Consume(reader, *events);
	ASSERT_GE(events->Size(), 1u);

	const CoarseChunk& chunk = (*events)[0].payload;
	EXPECT_EQ(StringCRC("coarse_chunk_pu").Value(), chunk.unitCrc);
	EXPECT_FLOAT_EQ(60.0f, chunk.unitHz);
	EXPECT_EQ(0u, chunk.seq);
	EXPECT_EQ(0u, chunk.firstEntry);
	EXPECT_GE(chunk.count, 1u);
	EXPECT_EQ(0u, chunk.moduleCount);
}

TEST(CoarseChunk, ProcessingUnitWithoutSinkSendsNothing)
{
	ProcessingUnit pu(StringCRC("coarse_chunk_nosink"), 60.0f, false);
	for (int i = 0; i < 200; ++i)
		pu.Update(1.0f / 60.0f);
	SUCCEED();	// no sink: must not crash or assert while the builder is full
}

TEST(CoarseChunkAssembler, ClampedFlagAndRawValueSurviveRebuild)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x5151u, 30.0f);
	builder.RecordModule(0, 0xAAu, 900.0f);	// over 655.35 ms: clamps
	builder.RecordUnitTick(1.23f);
	builder.CommitEntry(0.0);
	const CoarseChunk& chunk = builder.Take(100.0);

	CoarseChunkAssembler assembler;
	assembler.Apply(chunk);
	const CoarseRing* ring = assembler.GetRing(0x5151u);
	ASSERT_NE(nullptr, ring);

	uint16_t value = 0;
	uint8_t flags = 0;
	ASSERT_EQ(1u, ring->CopyRange(0, 0, 1, &value));
	ASSERT_EQ(1u, ring->CopyFlags(0, 1, &flags));
	EXPECT_EQ(CoarseRing::kClampedValue, value);
	EXPECT_NE(0, flags & CoarseRing::kFlagClamped);
	ASSERT_EQ(1u, ring->CopyRange(CoarseRing::kUnitSlot, 0, 1, &value));
	EXPECT_EQ(123u, value);
}

TEST(CoarseChunkAssembler, GetUnitHzReportsChunkHzAndZeroForUnknown)
{
	CoarseChunkBuilder builder;
	builder.Begin(0x6161u, 30.0f);
	builder.RecordUnitTick(1.0f);
	builder.CommitEntry(0.0);
	CoarseChunkAssembler assembler;
	assembler.Apply(builder.Take(10.0));
	EXPECT_FLOAT_EQ(30.0f, assembler.GetUnitHz(0x6161u));
	EXPECT_FLOAT_EQ(0.0f, assembler.GetUnitHz(0x9999u));
}

TEST(CoarseChunkAssembler, GetUnitCrcAtEnumeratesUnitsInArrivalOrder)
{
	CoarseChunkAssembler assembler;
	for (uint32_t crc : { 0x11u, 0x22u })
	{
		CoarseChunkBuilder builder;
		builder.Begin(crc, 60.0f);
		builder.RecordUnitTick(1.0f);
		builder.CommitEntry(0.0);
		assembler.Apply(builder.Take(10.0));
	}
	ASSERT_EQ(2u, assembler.GetUnitCount());
	EXPECT_EQ(0x11u, assembler.GetUnitCrcAt(0));
	EXPECT_EQ(0x22u, assembler.GetUnitCrcAt(1));
	EXPECT_EQ(0u, assembler.GetUnitCrcAt(2));
}
