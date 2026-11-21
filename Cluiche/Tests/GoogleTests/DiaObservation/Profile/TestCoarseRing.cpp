#include <gtest/gtest.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <cmath>

using Dia::Observation::Profile::CoarseRing;

TEST(CoarseRing, FixedPointRoundingToNearest)
{
	bool clamped = false;
	// 0.004 ms → 0 (< 0.005)
	EXPECT_EQ(0u, CoarseRing::ToFixed(0.004f, clamped));
	EXPECT_FALSE(clamped);
	// 0.005 ms → 1 (round half up)
	clamped = false;
	EXPECT_EQ(1u, CoarseRing::ToFixed(0.005f, clamped));
	EXPECT_FALSE(clamped);
	// 1.234 ms → 123
	clamped = false;
	EXPECT_EQ(123u, CoarseRing::ToFixed(1.234f, clamped));
	EXPECT_FALSE(clamped);
	// 1.235 ms → 124
	clamped = false;
	EXPECT_EQ(124u, CoarseRing::ToFixed(1.235f, clamped));
	EXPECT_FALSE(clamped);
	// 655.35 ms → 65535 (max representable)
	clamped = false;
	EXPECT_EQ(65535u, CoarseRing::ToFixed(655.35f, clamped));
	EXPECT_FALSE(clamped);
	// 655.36 ms → 65535 (clamped)
	clamped = false;
	EXPECT_EQ(65535u, CoarseRing::ToFixed(655.36f, clamped));
	EXPECT_TRUE(clamped);
	// 1000 ms → 65535 (clamped)
	clamped = false;
	EXPECT_EQ(65535u, CoarseRing::ToFixed(1000.0f, clamped));
	EXPECT_TRUE(clamped);
	// Negative → 0
	clamped = false;
	EXPECT_EQ(0u, CoarseRing::ToFixed(-5.0f, clamped));
	EXPECT_FALSE(clamped);
	// NaN → 0
	clamped = false;
	EXPECT_EQ(0u, CoarseRing::ToFixed(std::nanf(""), clamped));
	EXPECT_FALSE(clamped);
}

TEST(CoarseRing, FixedPointRoundTrip)
{
	EXPECT_FLOAT_EQ(0.0f, CoarseRing::FromFixed(0u));
	EXPECT_FLOAT_EQ(0.01f, CoarseRing::FromFixed(1u));
	EXPECT_FLOAT_EQ(1.23f, CoarseRing::FromFixed(123u));
	EXPECT_FLOAT_EQ(655.35f, CoarseRing::FromFixed(65535u));
}

TEST(CoarseRing, CapacityForReturnsCorrectSize)
{
	// 30 Hz, 60 s → 1800
	EXPECT_EQ(1800u, CoarseRing::CapacityFor(30.0f, 60.0f));
	// 60 Hz, 60 s → 3600
	EXPECT_EQ(3600u, CoarseRing::CapacityFor(60.0f, 60.0f));
	// Sim fixed step at 100 Hz, 60 s → 6000
	EXPECT_EQ(6000u, CoarseRing::CapacityFor(100.0f, 60.0f));
}

TEST(CoarseRing, MemoryBytesFor13ModulesAt30Hz)
{
	// 13 modules + 1 unit series = 14 series, 1800 entries, 2 bytes per value + 1 byte flags per entry
	// = 14 * 1800 * 2 + 1800 = 50400 + 1800 = 52200 bytes
	CoarseRing ring;
	ring.Init(1800, 13);
	uint32_t mem = ring.GetMemoryBytes();
	EXPECT_GE(mem, 50000u);
	EXPECT_LE(mem, 55000u);
}

TEST(CoarseRing, EmptyRingReturnsZeroCount)
{
	CoarseRing ring;
	ring.Init(100, 5);
	EXPECT_EQ(0u, ring.GetEntryCount());
	EXPECT_EQ(0u, ring.GetOldestAvailable());
}

TEST(CoarseRing, SingleEntryCommit)
{
	CoarseRing ring;
	ring.Init(100, 3);
	ring.RecordModule(0, 0x1234u, 1.5f);
	ring.RecordModule(2, 0x5678u, 2.75f);
	ring.RecordUnitTick(5.0f);
	ring.CommitEntry();
	EXPECT_EQ(1u, ring.GetEntryCount());
	EXPECT_EQ(0u, ring.GetOldestAvailable());
	EXPECT_EQ(0x1234u, ring.GetModuleCrc(0));
	EXPECT_EQ(0x5678u, ring.GetModuleCrc(2));
}

TEST(CoarseRing, UnrecordedSlotStoresZero)
{
	CoarseRing ring;
	ring.Init(100, 3);
	ring.RecordModule(0, 0xABCDu, 1.0f);
	// slot 1 not recorded
	ring.RecordModule(2, 0xBEEFu, 3.0f);
	ring.RecordUnitTick(10.0f);
	ring.CommitEntry();
	uint16_t out[4];
	EXPECT_EQ(1u, ring.CopyRange(1, 0, 1, out));
	EXPECT_EQ(0u, out[0]);
}

TEST(CoarseRing, OutOfRangeSlotRejected)
{
	CoarseRing ring;
	ring.Init(100, 5);
	EXPECT_FALSE(ring.RecordModule(5, 1u, 1.0f));  // slot 5 out of range (0-4)
	EXPECT_FALSE(ring.RecordModule(100, 1u, 1.0f));
}

TEST(CoarseRing, WrapOverwritesOldest)
{
	CoarseRing ring;
	const uint32_t capacity = 10;
	ring.Init(capacity, 2);
	// Commit capacity + 5 entries
	for (uint32_t i = 0; i < capacity + 5; ++i)
	{
		ring.RecordModule(0, 1u, static_cast<float>(i));
		ring.RecordUnitTick(static_cast<float>(i) * 2.0f);
		ring.CommitEntry();
	}
	EXPECT_EQ(15u, ring.GetEntryCount());
	EXPECT_EQ(5u, ring.GetOldestAvailable());  // entries 0-4 were overwritten
	// Try to read from entry 0 (should clip)
	uint16_t out[10];
	EXPECT_EQ(0u, ring.CopyRange(0, 0, 1, out));
	// Read from entry 5 onwards (available)
	EXPECT_EQ(10u, ring.CopyRange(0, 5, 10, out));
	bool clamped;
	for (uint32_t i = 0; i < 10; ++i)
		EXPECT_EQ(CoarseRing::ToFixed(static_cast<float>(5 + i), clamped), out[i]);
}

TEST(CoarseRing, CopyRangeReturnsAvailableCount)
{
	CoarseRing ring;
	ring.Init(100, 3);
	for (int i = 0; i < 5; ++i)
	{
		ring.RecordModule(1, 1u, static_cast<float>(i));
		ring.RecordUnitTick(0.0f);
		ring.CommitEntry();
	}
	uint16_t out[20];
	// Request 10, only 5 available
	EXPECT_EQ(5u, ring.CopyRange(1, 0, 10, out));
	// Request from middle
	EXPECT_EQ(3u, ring.CopyRange(1, 2, 10, out));
	// Request beyond end
	EXPECT_EQ(0u, ring.CopyRange(1, 10, 5, out));
}

TEST(CoarseRing, CopyFlagsReturnsFlags)
{
	CoarseRing ring;
	ring.Init(100, 2);
	// Entry with clamped value
	ring.RecordModule(0, 1u, 1000.0f);  // will clamp
	ring.RecordUnitTick(1.0f);
	ring.CommitEntry();
	// Entry without clamp
	ring.RecordModule(0, 1u, 1.0f);
	ring.RecordUnitTick(2.0f);
	ring.CommitEntry();
	uint8_t flags[2];
	EXPECT_EQ(2u, ring.CopyFlags(0, 2, flags));
	EXPECT_NE(0u, flags[0]);  // first entry has clamp flag
	EXPECT_EQ(0u, flags[1]);  // second entry has no flags
}

TEST(CoarseRing, SimFixedStepSemantics)
{
	// Multiple commits in one "frame" produce multiple entries
	CoarseRing ring;
	ring.Init(100, 2);
	ring.RecordModule(0, 1u, 1.0f);
	ring.RecordUnitTick(10.0f);
	ring.CommitEntry();
	ring.RecordModule(0, 1u, 2.0f);
	ring.RecordUnitTick(11.0f);
	ring.CommitEntry();
	ring.RecordModule(0, 1u, 3.0f);
	ring.RecordUnitTick(12.0f);
	ring.CommitEntry();
	EXPECT_EQ(3u, ring.GetEntryCount());
	uint16_t out[3];
	EXPECT_EQ(3u, ring.CopyRange(0, 0, 3, out));
	bool clamped;
	EXPECT_EQ(CoarseRing::ToFixed(1.0f, clamped), out[0]);
	EXPECT_EQ(CoarseRing::ToFixed(2.0f, clamped), out[1]);
	EXPECT_EQ(CoarseRing::ToFixed(3.0f, clamped), out[2]);
}

TEST(CoarseRing, UnitSlotIndependent)
{
	CoarseRing ring;
	ring.Init(100, 3);
	ring.RecordModule(0, 1u, 5.0f);
	ring.RecordModule(1, 2u, 10.0f);
	ring.RecordUnitTick(25.0f);
	ring.CommitEntry();
	uint16_t outMod[1], outUnit[1];
	EXPECT_EQ(1u, ring.CopyRange(0, 0, 1, outMod));
	EXPECT_EQ(1u, ring.CopyRange(CoarseRing::kUnitSlot, 0, 1, outUnit));
	bool clamped;
	EXPECT_EQ(CoarseRing::ToFixed(5.0f, clamped), outMod[0]);
	EXPECT_EQ(CoarseRing::ToFixed(25.0f, clamped), outUnit[0]);
}

TEST(CoarseRing, ModuleCrcRetained)
{
	CoarseRing ring;
	ring.Init(100, 4);
	ring.RecordModule(0, 0x1111u, 1.0f);
	ring.RecordModule(1, 0x2222u, 2.0f);
	ring.RecordModule(3, 0x4444u, 4.0f);
	ring.CommitEntry();
	EXPECT_EQ(0x1111u, ring.GetModuleCrc(0));
	EXPECT_EQ(0x2222u, ring.GetModuleCrc(1));
	EXPECT_EQ(0u, ring.GetModuleCrc(2));  // not recorded
	EXPECT_EQ(0x4444u, ring.GetModuleCrc(3));
}

TEST(CoarseRing, CopyFromEmptyRingCopiesNothing)
{
	CoarseRing ring;
	ring.Init(8, 2);
	uint16_t values[4] = { 7, 7, 7, 7 };
	uint8_t flags[4] = { 7, 7, 7, 7 };
	EXPECT_EQ(0u, ring.CopyRange(0, 0, 4, values));
	EXPECT_EQ(0u, ring.CopyRange(CoarseRing::kUnitSlot, 0, 4, values));
	EXPECT_EQ(0u, ring.CopyFlags(0, 4, flags));
	EXPECT_EQ(7u, values[0]);
}

TEST(CoarseRing, CopyZeroCountCopiesNothing)
{
	CoarseRing ring;
	ring.Init(8, 1);
	ring.RecordModule(0, 1u, 1.0f);
	ring.CommitEntry();
	uint16_t value = 0;
	EXPECT_EQ(0u, ring.CopyRange(0, 0, 0, &value));
}

TEST(CoarseRing, ZeroCapacityIsInert)
{
	CoarseRing ring;
	ring.Init(0, 2);
	ring.RecordModule(0, 1u, 1.0f);
	ring.RecordUnitTick(2.0f);
	ring.CommitEntry();
	EXPECT_EQ(0u, ring.GetEntryCount());
	EXPECT_EQ(0u, ring.GetMemoryBytes());
	EXPECT_EQ(0u, CoarseRing::CapacityFor(0.0f, 60.0f));
	EXPECT_EQ(0u, CoarseRing::CapacityFor(30.0f, -1.0f));
}

TEST(CoarseRing, ModuleCountCappedAtMax)
{
	CoarseRing ring;
	ring.Init(4, CoarseRing::kMaxModules + 10);
	EXPECT_TRUE(ring.RecordModule(CoarseRing::kMaxModules - 1, 1u, 1.0f));
	EXPECT_FALSE(ring.RecordModule(CoarseRing::kMaxModules, 1u, 1.0f));
}

TEST(CoarseRing, ResetAtStartsAtNewBase)
{
	CoarseRing ring;
	ring.Init(4, 1);
	ring.ResetAt(100);
	EXPECT_EQ(100u, ring.GetEntryCount());
	EXPECT_EQ(100u, ring.GetOldestAvailable());
	ring.RecordModule(0, 1u, 2.0f);
	ring.CommitEntry();
	uint16_t value = 0;
	EXPECT_EQ(0u, ring.CopyRange(0, 99, 1, &value));
	ASSERT_EQ(1u, ring.CopyRange(0, 100, 1, &value));
	EXPECT_EQ(200u, value);
	for (int i = 0; i < 5; ++i)
		ring.CommitEntry();
	EXPECT_EQ(102u, ring.GetOldestAvailable());	// 106 committed, capacity 4
}
