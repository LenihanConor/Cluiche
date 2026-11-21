#include <gtest/gtest.h>

#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimPendingTable.h>

#include <set>

using Dia::DebugServer::SimPendingTable;

TEST(SimPendingTable, AddReturnsUniqueNonzeroIdsOverCapacity)
{
	SimPendingTable t;
	std::set<uint32_t> ids;
	for (uint32_t i = 0; i < SimPendingTable::kCapacity; ++i)
	{
		const uint32_t id = t.Add(1, 100 + i, 0);
		EXPECT_NE(id, 0u);
		ids.insert(id);
	}
	EXPECT_EQ(ids.size(), static_cast<size_t>(SimPendingTable::kCapacity));
	EXPECT_EQ(t.Count(), SimPendingTable::kCapacity);
}

TEST(SimPendingTable, AddThenCompleteReturnsSameEntry)
{
	SimPendingTable t;
	const uint32_t id = t.Add(7, 0xABCD, 1000);
	ASSERT_NE(id, 0u);
	SimPendingTable::Entry e;
	ASSERT_TRUE(t.Complete(id, e));
	EXPECT_EQ(e.requestId, id);
	EXPECT_EQ(e.connectionId, 7);
	EXPECT_EQ(e.commandId, 0xABCDu);
	EXPECT_EQ(e.deadlineMs, 1000u + SimPendingTable::kTimeoutMs);
	EXPECT_EQ(t.Count(), 0u);
}

TEST(SimPendingTable, CompleteTwiceFalse)
{
	SimPendingTable t;
	const uint32_t id = t.Add(1, 1, 0);
	SimPendingTable::Entry e;
	EXPECT_TRUE(t.Complete(id, e));
	EXPECT_FALSE(t.Complete(id, e));
}

TEST(SimPendingTable, CompleteUnknownFalse)
{
	SimPendingTable t;
	SimPendingTable::Entry e;
	EXPECT_FALSE(t.Complete(12345, e));
	EXPECT_FALSE(t.Complete(0, e));
}

TEST(SimPendingTable, ExpireExactlyAtDeadline)
{
	SimPendingTable t;
	t.Add(1, 1, 500);
	const uint64_t deadline = 500 + SimPendingTable::kTimeoutMs;
	SimPendingTable::Entry out[4];
	EXPECT_EQ(t.Expire(deadline - 1, out, 4), 0u);
	EXPECT_EQ(t.Count(), 1u);
	EXPECT_EQ(t.Expire(deadline, out, 4), 1u);
	EXPECT_EQ(out[0].connectionId, 1);
	EXPECT_EQ(t.Count(), 0u);
}

TEST(SimPendingTable, ExpireReturnsOnlyDueEntries)
{
	SimPendingTable t;
	const uint32_t early = t.Add(1, 10, 0);
	const uint32_t late = t.Add(2, 20, 1000);
	SimPendingTable::Entry out[4];
	EXPECT_EQ(t.Expire(SimPendingTable::kTimeoutMs, out, 4), 1u);
	EXPECT_EQ(out[0].requestId, early);
	EXPECT_EQ(t.Count(), 1u);
	SimPendingTable::Entry e;
	EXPECT_TRUE(t.Complete(late, e));
	EXPECT_FALSE(t.Complete(early, e));
}

TEST(SimPendingTable, FullTableReturnsZeroAndSlotReusable)
{
	SimPendingTable t;
	uint32_t first = 0;
	for (uint32_t i = 0; i < SimPendingTable::kCapacity; ++i)
	{
		const uint32_t id = t.Add(1, i, 0);
		if (i == 0) first = id;
	}
	EXPECT_EQ(t.Add(1, 999, 0), 0u);
	SimPendingTable::Entry e;
	ASSERT_TRUE(t.Complete(first, e));
	const uint32_t again = t.Add(1, 999, 0);
	EXPECT_NE(again, 0u);
	EXPECT_EQ(t.Count(), SimPendingTable::kCapacity);
}

TEST(SimPendingTable, DropConnectionRemovesOnlyThatConnection)
{
	SimPendingTable t;
	const uint32_t a1 = t.Add(1, 1, 0);
	const uint32_t b1 = t.Add(2, 2, 0);
	const uint32_t a2 = t.Add(1, 3, 0);
	t.DropConnection(1);
	EXPECT_EQ(t.Count(), 1u);
	SimPendingTable::Entry e;
	EXPECT_FALSE(t.Complete(a1, e));
	EXPECT_FALSE(t.Complete(a2, e));
	EXPECT_TRUE(t.Complete(b1, e));
}

TEST(SimPendingTable, CountTracksAddCompleteExpire)
{
	SimPendingTable t;
	EXPECT_EQ(t.Count(), 0u);
	const uint32_t a = t.Add(1, 1, 0);
	t.Add(1, 2, 0);
	EXPECT_EQ(t.Count(), 2u);
	SimPendingTable::Entry e;
	t.Complete(a, e);
	EXPECT_EQ(t.Count(), 1u);
	SimPendingTable::Entry out[2];
	t.Expire(SimPendingTable::kTimeoutMs, out, 2);
	EXPECT_EQ(t.Count(), 0u);
}

TEST(SimPendingTable, IdsNeverZeroOrLiveAcrossLongChurn)
{
	SimPendingTable t;
	const uint32_t keep = t.Add(9, 9, 0);
	ASSERT_NE(keep, 0u);
	SimPendingTable::Entry e;
	for (uint32_t i = 0; i < 70000; ++i)
	{
		const uint32_t id = t.Add(1, 1, 0);
		ASSERT_NE(id, 0u);
		ASSERT_NE(id, keep);
		ASSERT_TRUE(t.Complete(id, e));
	}
	EXPECT_TRUE(t.Complete(keep, e));
}

TEST(SimPendingTable, WrapAroundSkipsZeroAndLiveIds)
{
	SimPendingTable t;
	t.SetNextIdForTesting(0xFFFFFFFEu);
	const uint32_t a = t.Add(1, 1, 0);
	const uint32_t b = t.Add(1, 2, 0);
	EXPECT_EQ(a, 0xFFFFFFFEu);
	EXPECT_EQ(b, 0xFFFFFFFFu);
	// Next candidate wraps to 0 (skipped) then 1.
	const uint32_t c = t.Add(1, 3, 0);
	EXPECT_EQ(c, 1u);
	// Force a collision with live id 'c': must skip to 2.
	t.SetNextIdForTesting(1);
	const uint32_t d = t.Add(1, 4, 0);
	EXPECT_EQ(d, 2u);
}

TEST(SimHostedTypes, TriviallyCopyableAndLayout)
{
	using namespace Dia::DebugServer;
	EXPECT_TRUE(std::is_trivially_copyable_v<SimRequestEvent>);
	EXPECT_TRUE(std::is_trivially_copyable_v<SimReplyEvent>);
	EXPECT_EQ(kSimPayloadBytes, 16384u);
	EXPECT_EQ(sizeof(SimRequestEvent), 12u + kSimPayloadBytes);
	EXPECT_EQ(sizeof(SimReplyEvent), 8u + kSimPayloadBytes);
	EXPECT_EQ(static_cast<uint32_t>(SimKind::Query), 0u);
	EXPECT_EQ(static_cast<uint32_t>(SimKind::Command), 1u);
	EXPECT_EQ(static_cast<uint32_t>(SimReplyStatus::Ok), 0u);
	EXPECT_EQ(static_cast<uint32_t>(SimReplyStatus::UnknownName), 3u);
}
