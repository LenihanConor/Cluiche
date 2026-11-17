////////////////////////////////////////////////////////////////////////////////
// Filename: TestTripChunk.cpp
// GoogleTest suite - Perf Inspector task 3b: TripChunk POD, cumulative trip/marker counters on
// CoarseChunk (builder + assembler).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaObservation/Profile/TripChunk.h>

#include <cstring>
#include <type_traits>

using namespace Dia::Observation::Profile;

TEST(TripChunk, IsTriviallyCopyableAndBounded)
{
	EXPECT_TRUE(std::is_trivially_copyable<TripChunk>::value);
	EXPECT_LE(sizeof(TripChunk), 9000u);
	EXPECT_GE(sizeof(TripChunk), sizeof(TripRecord));
}

TEST(CoarseChunkBuilder, CumulativeCountersAppearInTakenChunk)
{
	CoarseChunkBuilder b;
	b.Begin(0x42u, 60.0f);
	b.RecordUnitTick(1.0f);
	b.CommitEntry(0.0);
	b.SetCumulativeCounters(3u, 17u);
	const CoarseChunk& c = b.Take(0.0);
	EXPECT_EQ(3u, c.tripCount);
	EXPECT_EQ(17u, c.markerDrops);
}

TEST(CoarseChunkBuilder, CountersDefaultToZero)
{
	CoarseChunkBuilder b;
	b.Begin(0x42u, 60.0f);
	b.RecordUnitTick(1.0f);
	b.CommitEntry(0.0);
	const CoarseChunk& c = b.Take(0.0);
	EXPECT_EQ(0u, c.tripCount);
	EXPECT_EQ(0u, c.markerDrops);
}

TEST(CoarseChunkAssembler, TracksLatestTripCountPerUnit)
{
	CoarseChunkAssembler a;
	EXPECT_EQ(0u, a.GetTripCount(0x42u));   // unknown unit

	CoarseChunk c;
	std::memset(&c, 0, sizeof(c));
	c.unitCrc = 0x42u;
	c.unitHz = 60.0f;
	c.count = 1;
	c.tripCount = 2;
	a.Apply(c);
	EXPECT_EQ(2u, a.GetTripCount(0x42u));

	c.seq = 1;
	c.firstEntry = 1;
	c.tripCount = 5;
	a.Apply(c);
	EXPECT_EQ(5u, a.GetTripCount(0x42u));

	// a late (older seq) chunk must not move the count back
	c.seq = 0;
	c.firstEntry = 0;
	c.tripCount = 2;
	a.Apply(c);
	EXPECT_EQ(5u, a.GetTripCount(0x42u));
}
