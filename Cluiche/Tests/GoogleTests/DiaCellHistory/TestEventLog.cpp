#include <gtest/gtest.h>
#include <DiaCellHistory/EventLog.h>
#include <cstring>
#include <string>
#include <vector>

using namespace Dia::CellHistory;
using Dia::Core::StringCRC;

namespace
{
	constexpr uint64_t kBig = 1u << 20;

	struct Seen
	{
		uint32_t type, sample, seq, cell, groupValue, groupLayer;
		std::string payload, text;
	};
	struct Collector { std::vector<Seen> mSeen; };

	void Collect(const EventView& e, void* ctx)
	{
		Seen s{ e.mType.Value(), e.mSample, e.mSequence, e.mCell, e.mGroupValue, e.mGroupLayer.Value(), e.mPayloadJson, e.mText };
		static_cast<Collector*>(ctx)->mSeen.push_back(s);
	}
	std::vector<Seen> Read(const EventLog& log, uint32_t from, uint32_t to)
	{
		Collector c;
		uint32_t n = log.ForEachEvent(from, to, &Collect, &c);
		EXPECT_EQ(n, c.mSeen.size());
		return c.mSeen;
	}
	HistoryEvent Ev(const char* type, const char* payload = nullptr, const char* text = nullptr)
	{
		HistoryEvent e;
		e.type = StringCRC(type);
		e.payloadJson = payload;
		e.text = text;
		return e;
	}
}

TEST(EventLog, AssignsPendingToNextCommittedSample)
{
	EventLog log(kBig);
	EXPECT_EQ(log.Emit(Ev("a", "{\"i\":1}")), EmitResult::Ok);
	EXPECT_EQ(log.Emit(Ev("b", "{\"i\":2}")), EmitResult::Ok);
	EXPECT_EQ(log.Emit(Ev("c", "{\"i\":3}")), EmitResult::Ok);
	EXPECT_EQ(log.PendingCount(), 3u);
	EXPECT_EQ(log.EventCount(), 0u);
	EXPECT_TRUE(Read(log, 0, 100).empty());
	ASSERT_TRUE(log.CommitSample(5));
	EXPECT_EQ(log.PendingCount(), 0u);
	auto v = Read(log, 5, 5);
	ASSERT_EQ(v.size(), 3u);
	EXPECT_EQ(v[0].type, StringCRC("a").Value());
	EXPECT_EQ(v[1].type, StringCRC("b").Value());
	EXPECT_EQ(v[2].type, StringCRC("c").Value());
	for (auto& s : v) { EXPECT_EQ(s.sample, 5u); }
	EXPECT_LT(v[0].seq, v[1].seq);
	EXPECT_LT(v[1].seq, v[2].seq);
	EXPECT_EQ(v[1].payload, "{\"i\":2}");

	log.Emit(Ev("d"));
	log.Emit(Ev("e"));
	ASSERT_TRUE(log.CommitSample(6));
	EXPECT_EQ(Read(log, 6, 6).size(), 2u);
	EXPECT_EQ(Read(log, 5, 6).size(), 5u);

	log.Emit(Ev("f"));
	EXPECT_EQ(log.PendingCount(), 1u);
	EXPECT_EQ(Read(log, 0, 100).size(), 5u);	// pending not returned
	ASSERT_TRUE(log.CommitSample(7));
	EXPECT_EQ(Read(log, 7, 7).size(), 1u);
}

TEST(EventLog, NonContiguousAndNonIncreasingCommits)
{
	EventLog log(kBig);
	log.Emit(Ev("a"));
	ASSERT_TRUE(log.CommitSample(3));
	log.Emit(Ev("b"));
	ASSERT_TRUE(log.CommitSample(10));
	EXPECT_EQ(Read(log, 4, 9).size(), 0u);
	EXPECT_EQ(Read(log, 10, 10).size(), 1u);
	log.Emit(Ev("c"));
	EXPECT_FALSE(log.CommitSample(10));
	EXPECT_FALSE(log.CommitSample(9));
	EXPECT_EQ(log.PendingCount(), 1u);
	EXPECT_TRUE(log.CommitSample(11));
	EXPECT_EQ(log.LatestSample(), 11u);
}

TEST(EventLog, CommitWithNoPendingIsFine)
{
	EventLog log(kBig);
	EXPECT_TRUE(log.CommitSample(1));
	EXPECT_EQ(log.EventCount(), 0u);
}

TEST(EventLog, PayloadCap)
{
	EventLog log(kBig);
	std::string ok(1024, 'x');
	std::string bad(1025, 'x');
	EXPECT_EQ(log.Emit(Ev("a", ok.c_str())), EmitResult::Ok);
	EXPECT_EQ(log.Emit(Ev("a", bad.c_str())), EmitResult::PayloadTooLarge);
	EXPECT_EQ(log.PendingCount(), 1u);
	log.CommitSample(1);
	auto v = Read(log, 1, 1);
	ASSERT_EQ(v.size(), 1u);
	EXPECT_EQ(v[0].payload, ok);
}

TEST(EventLog, TextTruncatedTo64)
{
	EventLog log(kBig);
	std::string longText(100, 'a');
	std::string exact(64, 'b');
	EXPECT_EQ(log.Emit(Ev("a", nullptr, longText.c_str())), EmitResult::Ok);
	EXPECT_EQ(log.Emit(Ev("a", nullptr, exact.c_str())), EmitResult::Ok);
	log.CommitSample(1);
	auto v = Read(log, 1, 1);
	ASSERT_EQ(v.size(), 2u);
	EXPECT_EQ(v[0].text, std::string(64, 'a'));
	EXPECT_EQ(v[1].text, exact);
}

TEST(EventLog, NullPayloadAndTextReadAsEmpty)
{
	EventLog log(kBig);
	log.Emit(Ev("a"));
	log.CommitSample(1);
	auto v = Read(log, 1, 1);
	ASSERT_EQ(v.size(), 1u);
	EXPECT_EQ(v[0].payload, "");
	EXPECT_EQ(v[0].text, "");
}

TEST(EventLog, StringsAreCopied)
{
	EventLog log(kBig);
	{
		char* p = new char[16];
		char* t = new char[16];
		std::strcpy(p, "{\"k\":1}");
		std::strcpy(t, "hello");
		log.Emit(Ev("a", p, t));
		std::memset(p, 'Z', 15); p[15] = 0;
		std::memset(t, 'Z', 15); t[15] = 0;
		delete[] p;
		delete[] t;
	}
	log.CommitSample(1);
	auto v = Read(log, 1, 1);
	ASSERT_EQ(v.size(), 1u);
	EXPECT_EQ(v[0].payload, "{\"k\":1}");
	EXPECT_EQ(v[0].text, "hello");
}

TEST(EventLog, GroupAndCellRoundTrip)
{
	EventLog log(kBig);
	HistoryEvent e = Ev("a");
	e.group.mLayer = StringCRC("region");
	e.group.mValue = 7;
	e.cell = 1234;
	log.Emit(e);
	log.Emit(Ev("b"));
	log.CommitSample(1);
	auto v = Read(log, 1, 1);
	ASSERT_EQ(v.size(), 2u);
	EXPECT_EQ(v[0].groupLayer, StringCRC("region").Value());
	EXPECT_EQ(v[0].groupValue, 7u);
	EXPECT_EQ(v[0].cell, 1234u);
	EXPECT_EQ(v[1].groupValue, uint32_t(GroupRef::kNoValue));
	EXPECT_EQ(v[1].cell, uint32_t(HistoryEvent::kNoCell));
	EXPECT_TRUE(HistoryEvent().group.IsNone());
}

TEST(EventLog, RecordBytesIsSizedAndAligned)
{
	HistoryEvent a = Ev("a");
	HistoryEvent b = Ev("a", "{}", "hi");
	EXPECT_EQ(EventLog::RecordBytes(a) % 4, 0u);
	EXPECT_GE(EventLog::RecordBytes(a), 28u);
	EXPECT_GE(EventLog::RecordBytes(b), EventLog::RecordBytes(a));
	EXPECT_GE(EventLog::RecordBytes(b), 28u + 2u + 2u);
	EventLog log(kBig);
	log.Emit(a);
	EXPECT_EQ(log.BytesUsed(), EventLog::RecordBytes(a));
}

TEST(EventLog, BudgetEvictsOldestFirstWithGapAndOneWarning)
{
	HistoryEvent e = Ev("a", "{\"v\":1}");
	const uint32_t rec = EventLog::RecordBytes(e);
	EventLog log(uint64_t(rec) * 4);	// holds 4
	for (uint32_t s = 1; s <= 4; ++s) { log.Emit(e); log.CommitSample(s); }
	EXPECT_EQ(log.EventCount(), 4u);
	EXPECT_EQ(log.EvictionEpisodeCount(), 0u);
	EXPECT_EQ(log.GapCount(), 0u);
	log.Emit(e);	// evicts sample 1
	EXPECT_EQ(log.EvictionEpisodeCount(), 1u);
	log.CommitSample(5);
	log.Emit(e);	// evicts sample 2, same episode
	log.CommitSample(6);
	EXPECT_EQ(log.EvictionEpisodeCount(), 1u);
	EXPECT_EQ(log.DroppedEventCount(), 2u);
	ASSERT_EQ(log.GapCount(), 1u);
	EXPECT_EQ(log.Gap(0).mFirstDropped, 1u);
	EXPECT_EQ(log.Gap(0).mLastDropped, 2u);
	EXPECT_EQ(log.FirstRetainedSample(), 3u);
	auto v = Read(log, 0, 100);
	ASSERT_EQ(v.size(), 4u);
	EXPECT_EQ(v[0].sample, 3u);
	EXPECT_EQ(v[3].sample, 6u);
	EXPECT_LE(log.BytesUsed(), log.Budget());
	EXPECT_DOUBLE_EQ(log.UsedFraction(), 1.0);
}

TEST(EventLog, NewEpisodeAfterEmitWithoutEviction)
{
	HistoryEvent e = Ev("a");
	const uint32_t rec = EventLog::RecordBytes(e);
	EventLog log(uint64_t(rec) * 2);
	log.Emit(e); log.CommitSample(1);
	log.Emit(e); log.CommitSample(2);
	log.Emit(e); log.CommitSample(3);	// evicts 1
	EXPECT_EQ(log.EvictionEpisodeCount(), 1u);
	log.TruncateAfter(2);				// frees room
	log.Emit(e);						// no eviction -> episode ends
	log.CommitSample(3);
	log.Emit(e); log.CommitSample(4);	// evicts again -> episode 2
	EXPECT_EQ(log.EvictionEpisodeCount(), 2u);
	EXPECT_GE(log.GapCount(), 2u);
}

TEST(EventLog, PendingNeverEvicted)
{
	HistoryEvent e = Ev("a");
	const uint32_t rec = EventLog::RecordBytes(e);
	EventLog log(uint64_t(rec) * 3);
	log.Emit(e); log.CommitSample(1);
	EXPECT_EQ(log.Emit(e), EmitResult::Ok);
	EXPECT_EQ(log.Emit(e), EmitResult::Ok);	// full: 1 committed + 2 pending
	EXPECT_EQ(log.Emit(e), EmitResult::Ok);	// evicts the committed one
	EXPECT_EQ(log.EventCount(), 0u);
	EXPECT_EQ(log.PendingCount(), 3u);
	EXPECT_EQ(log.Emit(e), EmitResult::PendingFull);
	EXPECT_EQ(log.PendingCount(), 3u);
	log.CommitSample(2);
	EXPECT_EQ(Read(log, 2, 2).size(), 3u);
}

TEST(EventLog, TooLargeForBudget)
{
	HistoryEvent e = Ev("a", "{\"payload\":\"0123456789\"}");
	EventLog log(EventLog::RecordBytes(e) - 1);
	EXPECT_EQ(log.Emit(e), EmitResult::TooLargeForBudget);
	EXPECT_EQ(log.PendingCount(), 0u);
	EXPECT_EQ(log.BytesUsed(), 0u);
}

TEST(EventLog, TruncateAfterDropsLaterAndPending)
{
	EventLog log(kBig);
	for (uint32_t s = 1; s <= 5; ++s) { log.Emit(Ev("a")); log.Emit(Ev("b")); log.CommitSample(s); }
	log.Emit(Ev("p"));
	log.TruncateAfter(3);
	EXPECT_EQ(log.PendingCount(), 0u);
	EXPECT_EQ(log.EventCount(), 6u);
	EXPECT_EQ(log.LatestSample(), 3u);
	EXPECT_EQ(Read(log, 0, 100).size(), 6u);
	log.Emit(Ev("c"));
	ASSERT_TRUE(log.CommitSample(4));
	auto v = Read(log, 4, 4);
	ASSERT_EQ(v.size(), 1u);
	EXPECT_EQ(v[0].type, StringCRC("c").Value());
	log.TruncateAfter(0);
	EXPECT_EQ(log.EventCount(), 0u);
	EXPECT_EQ(log.BytesUsed(), 0u);
}

TEST(EventLog, PartialRangeReads)
{
	EventLog log(kBig);
	for (uint32_t s = 1; s <= 10; ++s) { log.Emit(Ev("a")); log.CommitSample(s); }
	EXPECT_EQ(Read(log, 3, 6).size(), 4u);
	EXPECT_EQ(Read(log, 10, 50).size(), 1u);
	EXPECT_EQ(Read(log, 11, 50).size(), 0u);
	EXPECT_EQ(Read(log, 6, 3).size(), 0u);
	EXPECT_EQ(Read(log, 0, 0).size(), 0u);
}

TEST(EventLog, CoversSamplesAroundGaps)
{
	HistoryEvent e = Ev("a");
	const uint32_t rec = EventLog::RecordBytes(e);
	EventLog log(uint64_t(rec) * 3);
	for (uint32_t s = 1; s <= 3; ++s) { log.Emit(e); log.CommitSample(s); }
	EXPECT_TRUE(log.CoversSamples(1, 3));
	log.Emit(e); log.CommitSample(4);	// drops 1
	log.Emit(e); log.CommitSample(5);	// drops 2
	EXPECT_FALSE(log.CoversSamples(1, 5));
	EXPECT_FALSE(log.CoversSamples(2, 2));
	EXPECT_FALSE(log.CoversSamples(0, 3));
	EXPECT_TRUE(log.CoversSamples(3, 5));
	EXPECT_TRUE(log.CoversSamples(4, 4));
}

TEST(EventLog, GapCapDiscardsOldest)
{
	HistoryEvent e = Ev("a");
	const uint32_t rec = EventLog::RecordBytes(e);
	EventLog log{uint64_t(rec)};	// holds 1
	// Each iteration: emit w/o eviction needs room, so alternate truncate to split episodes.
	uint32_t s = 1;
	for (int i = 0; i < 100; ++i)
	{
		log.Emit(e); log.CommitSample(s++);
		log.Emit(e); log.CommitSample(s++);	// evicts previous
		log.TruncateAfter(s - 2);			// leave one event; ends episode on next no-evict Emit
		log.TruncateAfter(0);
		log.Emit(e); log.CommitSample(s++);	// no eviction -> episode over
		log.Emit(e); log.CommitSample(s++);	// evicts -> new episode
	}
	EXPECT_LE(log.GapCount(), uint32_t(EventLog::kMaxGaps));
	EXPECT_EQ(log.GapCount(), uint32_t(EventLog::kMaxGaps));
}

TEST(EventLog, StressRingWrapStaysConsistent)
{
	HistoryEvent proto = Ev("a", "{\"v\":123456}", "stress");
	const uint32_t rec = EventLog::RecordBytes(proto);
	EventLog log(uint64_t(rec) * 50);
	uint32_t sample = 0;
	for (int i = 0; i < 10000; ++i)
	{
		std::string payload = "{\"i\":" + std::to_string(i) + "}";
		HistoryEvent e = Ev("a", payload.c_str(), (i % 3) ? "t" : nullptr);
		e.cell = uint32_t(i);
		if (log.Emit(e) != EmitResult::Ok) { continue; }
		if (i % 4 == 3) { log.CommitSample(++sample); }
	}
	auto v = Read(log, 0, 0xFFFFFFFFu);
	ASSERT_FALSE(v.empty());
	EXPECT_EQ(v.size(), log.EventCount());
	EXPECT_LE(log.BytesUsed(), log.Budget());
	for (size_t i = 0; i < v.size(); ++i)
	{
		EXPECT_EQ(v[i].payload, "{\"i\":" + std::to_string(v[i].cell) + "}");
		if (i > 0)
		{
			EXPECT_GT(v[i].seq, v[i - 1].seq);
			EXPECT_GE(v[i].sample, v[i - 1].sample);
		}
	}
	EXPECT_GT(log.DroppedEventCount(), 0u);
}

TEST(EventLog, ClearDropsCommittedAndPendingAndRestartsFromSampleZero)
{
	EventLog log(kBig);
	HistoryEvent e; e.type = StringCRC("t");
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);
	ASSERT_TRUE(log.CommitSample(0));
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);
	ASSERT_TRUE(log.CommitSample(1));
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);	// pending
	log.Clear();
	EXPECT_EQ(log.EventCount(), 0u);
	EXPECT_EQ(log.PendingCount(), 0u);
	EXPECT_EQ(log.GapCount(), 0u);
	EXPECT_EQ(log.BytesUsed(), 0u);
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);
	ASSERT_TRUE(log.CommitSample(0));		// not refused as "<= latest"
	EXPECT_EQ(Read(log, 0, 10).size(), 1u);
}

TEST(EventLog, TruncateAfterAlsoDropsPendingEventsNotYetAttached)
{
	EventLog log(kBig);
	HistoryEvent e; e.type = StringCRC("t");
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);
	ASSERT_TRUE(log.CommitSample(0));
	ASSERT_EQ(log.Emit(e), EmitResult::Ok);	// pending for sample 1
	log.TruncateAfter(0);
	EXPECT_EQ(log.PendingCount(), 0u);
	ASSERT_TRUE(log.CommitSample(1));
	EXPECT_EQ(Read(log, 0, 10).size(), 1u);	// the pending event did not attach to sample 1
}
