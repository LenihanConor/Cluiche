#include <gtest/gtest.h>
#include <DiaObservation/Job/JobInbox.h>

#include <atomic>
#include <thread>
#include <vector>

using Dia::Observation::Job::JobInbox;
using Dia::Threading::JobTimingRecord;

namespace
{
	JobTimingRecord Make(uint32_t owner, uint32_t tag)
	{
		JobTimingRecord r{};
		r.owner = owner;
		r.tag = tag;
		r.startNs = (uint64_t(owner) << 32) | tag;
		return r;
	}
}

TEST(JobInbox, CapacityRoundsUpToPowerOfTwo)
{
	EXPECT_EQ(JobInbox(5).Capacity(), 8u);
	EXPECT_EQ(JobInbox(8).Capacity(), 8u);
	EXPECT_EQ(JobInbox(9).Capacity(), 16u);
	EXPECT_GE(JobInbox(0).Capacity(), 2u);
	EXPECT_GE(JobInbox(1).Capacity(), 2u);
}

TEST(JobInbox, PushPopFifo)
{
	JobInbox inbox(8);
	for (uint32_t i = 0; i < 5; ++i) EXPECT_TRUE(inbox.TryPush(Make(1, i)));
	JobTimingRecord r;
	for (uint32_t i = 0; i < 5; ++i)
	{
		ASSERT_TRUE(inbox.TryPop(r));
		EXPECT_EQ(r.tag, i);
	}
	EXPECT_FALSE(inbox.TryPop(r));
	EXPECT_EQ(inbox.PushedCount(), 5u);
	EXPECT_EQ(inbox.DroppedCount(), 0u);
}

TEST(JobInbox, FullDropsNewestAndKeepsOldest)
{
	JobInbox inbox(4);
	for (uint32_t i = 0; i < 4; ++i) EXPECT_TRUE(inbox.TryPush(Make(1, i)));
	EXPECT_FALSE(inbox.TryPush(Make(1, 100)));
	EXPECT_FALSE(inbox.TryPush(Make(1, 101)));
	EXPECT_EQ(inbox.DroppedCount(), 2u);
	EXPECT_EQ(inbox.PushedCount(), 4u);
	JobTimingRecord r;
	for (uint32_t i = 0; i < 4; ++i)
	{
		ASSERT_TRUE(inbox.TryPop(r));
		EXPECT_EQ(r.tag, i);
	}
	EXPECT_FALSE(inbox.TryPop(r));
}

TEST(JobInbox, WorksAgainAfterDrainWithWraparound)
{
	JobInbox inbox(4);
	JobTimingRecord r;
	uint32_t next = 0, expect = 0;
	for (int round = 0; round < 1000; ++round)
	{
		for (int i = 0; i < 3; ++i) ASSERT_TRUE(inbox.TryPush(Make(2, next++)));
		for (int i = 0; i < 3; ++i)
		{
			ASSERT_TRUE(inbox.TryPop(r));
			EXPECT_EQ(r.tag, expect++);
		}
	}
	EXPECT_EQ(inbox.DroppedCount(), 0u);
}

TEST(JobInbox, DrainCallsBackInOrder)
{
	JobInbox inbox(8);
	for (uint32_t i = 0; i < 6; ++i) inbox.TryPush(Make(1, i));
	JobTimingRecord out[4];
	EXPECT_EQ(inbox.Drain(out, 4), 4u);
	EXPECT_EQ(out[3].tag, 3u);
	EXPECT_EQ(inbox.Drain(out, 4), 2u);
	EXPECT_EQ(out[1].tag, 5u);
	EXPECT_EQ(inbox.Drain(out, 4), 0u);
}

TEST(JobInbox, MultiProducerStress)
{
	constexpr uint32_t kThreads = 4;
	constexpr uint32_t kPerThread = 10000;
	JobInbox inbox(256);
	std::atomic<bool> go{false};
	std::atomic<uint32_t> done{0};
	std::vector<std::thread> producers;
	for (uint32_t t = 0; t < kThreads; ++t)
	{
		producers.emplace_back([&, t]
		{
			while (!go.load()) {}
			for (uint32_t s = 0; s < kPerThread; ++s) inbox.TryPush(Make(t + 1, s));
			done.fetch_add(1);
		});
	}
	std::vector<uint32_t> lastSeq(kThreads + 1, 0);
	std::vector<bool> seen(kThreads + 1, false);
	uint64_t popped = 0;
	bool corrupt = false;
	auto consume = [&]
	{
		JobTimingRecord r;
		while (inbox.TryPop(r))
		{
			++popped;
			if (r.owner < 1 || r.owner > kThreads || r.startNs != ((uint64_t(r.owner) << 32) | r.tag)) { corrupt = true; continue; }
			// per-producer order preserved, no duplicates
			if (seen[r.owner] && r.tag <= lastSeq[r.owner]) corrupt = true;
			seen[r.owner] = true;
			lastSeq[r.owner] = r.tag;
		}
	};
	go.store(true);
	while (done.load() < kThreads) consume();
	for (auto& th : producers) th.join();
	consume();
	EXPECT_FALSE(corrupt);
	EXPECT_EQ(popped + inbox.DroppedCount(), uint64_t(kThreads) * kPerThread);
	EXPECT_EQ(popped, inbox.PushedCount());
}
