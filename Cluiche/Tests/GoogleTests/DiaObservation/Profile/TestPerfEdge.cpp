////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfEdge.cpp
// GoogleTest suite - Perf Inspector task 10: edge tests for CoarseChunkAssembler, PerfWindowPacker
// (window, frame, jobs) and JobChunkAssembler. Chunk-level edge cases that the per-feature test files
// do not cover: module set changes, retention, duplicate/out-of-order chunks, garbage hz/seconds,
// escaping, saturation.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/PerfWindowPacker.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkAssembler.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <DiaObservation/Profile/PerfTimingStats.h>
#include <json/json.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;

namespace
{
	std::vector<uint8_t> DecodeB64(const std::string& in)
	{
		std::vector<uint8_t> out;
		uint32_t acc = 0;
		int bits = 0;
		for (char c : in)
		{
			int v;
			if (c >= 'A' && c <= 'Z') v = c - 'A';
			else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
			else if (c >= '0' && c <= '9') v = c - '0' + 52;
			else if (c == '+') v = 62;
			else if (c == '/') v = 63;
			else break;
			acc = (acc << 6) | static_cast<uint32_t>(v);
			bits += 6;
			if (bits >= 8) { bits -= 8; out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF)); }
		}
		return out;
	}

	std::vector<uint16_t> DecodeU16(const std::string& in)
	{
		const std::vector<uint8_t> raw = DecodeB64(in);
		std::vector<uint16_t> out(raw.size() / 2);
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
		return out;
	}

	bool TryParse(const std::string& s, Json::Value& root)
	{
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		return r->parse(s.data(), s.data() + s.size(), &root, &errs);
	}

	Json::Value Parse(const std::string& s)
	{
		Json::Value root;
		EXPECT_TRUE(TryParse(s, root)) << s;
		return root;
	}

	struct Slot { uint32_t slot; uint32_t crc; float ms; };

	// One chunk of n entries, every entry has the given module slots at the given ms and the unit tick.
	CoarseChunk MakeChunk(uint32_t unitCrc, float hz, uint32_t seq, uint64_t firstEntry, uint32_t n,
		const std::vector<Slot>& slots, float unitMs = 2.0f)
	{
		CoarseChunk c;
		std::memset(&c, 0, sizeof(c));
		c.unitCrc = unitCrc;
		c.unitHz = hz;
		c.seq = seq;
		c.firstEntry = firstEntry;
		c.count = n;
		for (const Slot& s : slots)
		{
			c.moduleCrcs[s.slot] = s.crc;
			if (s.slot + 1 > c.moduleCount) c.moduleCount = s.slot + 1;
			bool clamped = false;
			for (uint32_t i = 0; i < n; ++i)
				c.ms[s.slot][i] = CoarseRing::ToFixed(s.ms, clamped);
		}
		for (uint32_t i = 0; i < n; ++i)
		{
			bool clamped = false;
			c.ms[CoarseRing::kMaxModules][i] = CoarseRing::ToFixed(unitMs, clamped);
		}
		return c;
	}

	std::string PackOne(const CoarseChunkAssembler& a, uint32_t unitCrc, const char* name, float seconds, float hz)
	{
		PerfWindowUnitInput in = { unitCrc, name, hz, a.GetRing(unitCrc) };
		std::string out;
		PackPerfWindow(&in, 1, seconds, nullptr, out);
		return out;
	}
}

// ---- CoarseChunkAssembler ----

TEST(CoarseChunkEdge, DuplicateChunkDoesNotDoubleApply)
{
	CoarseChunkAssembler a;
	const CoarseChunk c0 = MakeChunk(1, 30.0f, 0, 0, 10, { {0, 0xA, 1.0f} });
	a.Apply(c0);
	a.Apply(c0);
	EXPECT_EQ(10u, a.GetRing(1)->GetEntryCount());
	EXPECT_EQ(0u, a.GetRing(1)->GetOldestAvailable());
}

TEST(CoarseChunkEdge, StaleChunkDoesNotCreateSpuriousSeqGapLater)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 5, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 1, 5, 5, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 2, 10, 5, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 5, { {0, 0xA, 1.0f} }));   // late duplicate of seq 0
	a.Apply(MakeChunk(1, 30.0f, 3, 15, 5, { {0, 0xA, 1.0f} }));  // in order again
	EXPECT_EQ(0u, a.GetGapCount(1));
	EXPECT_EQ(20u, a.GetRing(1)->GetEntryCount());
}

TEST(CoarseChunkEdge, OutOfOrderChunkLeavesHonestGapFlagsAndLateOneIsDropped)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 5, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 2, 10, 5, { {0, 0xA, 3.0f} }));   // seq 1 skipped
	a.Apply(MakeChunk(1, 30.0f, 1, 5, 5, { {0, 0xA, 2.0f} }));    // arrives late: ignored
	const CoarseRing* ring = a.GetRing(1);
	ASSERT_NE(nullptr, ring);
	EXPECT_EQ(15u, ring->GetEntryCount());
	EXPECT_EQ(1u, a.GetGapCount(1));
	uint8_t flags[15] = {};
	ASSERT_EQ(15u, ring->CopyFlags(0, 15, flags));
	for (int i = 0; i < 5; ++i) EXPECT_EQ(0, flags[i]);
	for (int i = 5; i < 10; ++i) EXPECT_EQ(CoarseRing::kFlagGap, flags[i]);
	for (int i = 10; i < 15; ++i) EXPECT_EQ(0, flags[i]);
	uint16_t v[15] = {};
	ring->CopyRange(0, 0, 15, v);
	EXPECT_EQ(100, v[0]);
	EXPECT_EQ(0, v[7]);
	EXPECT_EQ(300, v[12]);
}

TEST(CoarseChunkEdge, WindowLongerThanRetentionKeepsNewestAndDropsOverwritten)
{
	CoarseChunkAssembler a;
	// 30 Hz -> 1800 entry ring. Feed 2000 entries (32 chunks of 64 = 2048 -> use 2000 via last short chunk).
	uint64_t next = 0;
	uint32_t seq = 0;
	while (next < 2000)
	{
		const uint32_t n = static_cast<uint32_t>(std::min<uint64_t>(64, 2000 - next));
		a.Apply(MakeChunk(1, 30.0f, seq++, next, n, { {0, 0xA, 1.0f} }));
		next += n;
	}
	const CoarseRing* ring = a.GetRing(1);
	EXPECT_EQ(2000u, ring->GetEntryCount());
	EXPECT_EQ(200u, ring->GetOldestAvailable());   // first 200 entries overwritten
	Json::Value root = Parse(PackOne(a, 1, "Sim", 60.0f, 30.0f));
	EXPECT_EQ(1800u, root["units"][0]["count"].asUInt());
	EXPECT_EQ(200u, root["units"][0]["firstEntry"].asUInt());
	EXPECT_EQ(1800u, DecodeU16(root["units"][0]["unitMs"].asString()).size());
	// seconds beyond 60 is clamped, never asks for more than retained
	root = Parse(PackOne(a, 1, "Sim", 100000.0f, 30.0f));
	EXPECT_EQ(1800u, root["units"][0]["count"].asUInt());
}

TEST(CoarseChunkEdge, GapBeyondCapacityResetsAndWindowStartsAtNewChunk)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 64, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 9, 100000, 10, { {0, 0xA, 1.0f} }));
	const CoarseRing* ring = a.GetRing(1);
	EXPECT_EQ(100010u, ring->GetEntryCount());
	EXPECT_EQ(100000u, ring->GetOldestAvailable());
	Json::Value root = Parse(PackOne(a, 1, "Sim", 60.0f, 30.0f));
	EXPECT_EQ(10u, root["units"][0]["count"].asUInt());
	EXPECT_EQ(100000u, root["units"][0]["firstEntry"].asUInt());
}

TEST(CoarseChunkEdge, ModuleAddedMidWindowKeepsPackedSeriesAligned)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 4, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(1, 30.0f, 1, 4, 4, { {0, 0xA, 1.0f}, {1, 0xB, 5.0f} }));   // slot 1 appears
	a.Apply(MakeChunk(1, 30.0f, 2, 8, 4, { {0, 0xA, 1.0f} }));                   // later chunk omits slot 1 again
	Json::Value root = Parse(PackOne(a, 1, "Sim", 60.0f, 30.0f));
	const Json::Value& u = root["units"][0];
	ASSERT_EQ(2u, u["modules"].size());
	EXPECT_EQ(0xAu, u["modules"][0]["crc"].asUInt());   // slot order = stable colour/lane order
	EXPECT_EQ(0xBu, u["modules"][1]["crc"].asUInt());
	const std::vector<uint16_t> ms = DecodeU16(u["moduleMs"].asString());
	ASSERT_EQ(24u, ms.size());                          // 2 modules x 12 entries, module-major
	for (int i = 0; i < 12; ++i) EXPECT_EQ(100, ms[i]);
	for (int i = 0; i < 4; ++i) EXPECT_EQ(0, ms[12 + i]);
	for (int i = 4; i < 8; ++i) EXPECT_EQ(500, ms[12 + i]);
	for (int i = 8; i < 12; ++i) EXPECT_EQ(0, ms[12 + i]);
	// the single-frame view agrees with the window at the entry where the module is live
	PerfWindowUnitInput in = { 1, "Sim", 30.0f, a.GetRing(1) };
	std::string frame;
	PackPerfFrame(in, 5, nullptr, frame);
	Json::Value f = Parse(frame);
	ASSERT_EQ(2u, f["modules"].size());
	EXPECT_EQ(0xBu, f["modules"][1]["crc"].asUInt());
	EXPECT_NEAR(5.0, f["modules"][1]["ms"].asDouble(), 0.01);
}

TEST(CoarseChunkEdge, UnitWithNoModulesStillPacksUnitTicks)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, 30.0f, 0, 0, 6, {}, 3.0f));
	Json::Value root = Parse(PackOne(a, 1, "Idle", 60.0f, 30.0f));
	const Json::Value& u = root["units"][0];
	EXPECT_EQ(0u, u["modules"].size());
	EXPECT_EQ(6u, u["count"].asUInt());
	const std::vector<uint16_t> ms = DecodeU16(u["unitMs"].asString());
	ASSERT_EQ(6u, ms.size());
	EXPECT_EQ(300, ms[0]);
	EXPECT_EQ(0u, DecodeU16(u["moduleMs"].asString()).size());
}

TEST(CoarseChunkEdge, ClampedValueInTheMiddleKeepsFlagAndValue)
{
	CoarseChunkAssembler a;
	CoarseChunk c = MakeChunk(1, 30.0f, 0, 0, 5, { {0, 0xA, 1.0f} });
	c.ms[0][2] = 65535;
	c.flags[2] = CoarseRing::kFlagClamped;
	a.Apply(c);
	Json::Value root = Parse(PackOne(a, 1, "Sim", 60.0f, 30.0f));
	const std::vector<uint16_t> ms = DecodeU16(root["units"][0]["moduleMs"].asString());
	EXPECT_EQ(65535, ms[2]);
	EXPECT_EQ(100, ms[1]);
	EXPECT_EQ(100, ms[3]);
	const std::vector<uint8_t> flags = DecodeB64(root["units"][0]["flags"].asString());
	ASSERT_EQ(5u, flags.size());
	EXPECT_EQ(CoarseRing::kFlagClamped, flags[2]);
	EXPECT_EQ(0, flags[1]);
}

TEST(CoarseChunkEdge, NineUnitsCountOverflowAndKeepFirstEight)
{
	CoarseChunkAssembler a;
	for (uint32_t u = 0; u < 12; ++u)
		a.Apply(MakeChunk(100 + u, 30.0f, 0, 0, 2, { {0, 0xA, 1.0f} }));
	EXPECT_EQ(8u, a.GetUnitCount());
	EXPECT_EQ(4u, a.GetDroppedUnknownUnitCount());
	// a dropped unit keeps being dropped, a known one keeps working
	a.Apply(MakeChunk(111, 30.0f, 1, 2, 2, { {0, 0xA, 1.0f} }));
	EXPECT_EQ(5u, a.GetDroppedUnknownUnitCount());
	a.Apply(MakeChunk(100, 30.0f, 1, 2, 2, { {0, 0xA, 1.0f} }));
	EXPECT_EQ(4u, a.GetRing(100)->GetEntryCount());
}

TEST(CoarseChunkEdge, InfiniteOrNegativeUnitHzDoesNotBreakAssembler)
{
	CoarseChunkAssembler a;
	a.Apply(MakeChunk(1, std::numeric_limits<float>::infinity(), 0, 0, 3, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(2, -30.0f, 0, 0, 3, { {0, 0xA, 1.0f} }));
	a.Apply(MakeChunk(3, std::numeric_limits<float>::quiet_NaN(), 0, 0, 3, { {0, 0xA, 1.0f} }));
	for (uint32_t u = 1; u <= 3; ++u)
	{
		ASSERT_NE(nullptr, a.GetRing(u));
		EXPECT_EQ(3u, a.GetRing(u)->GetEntryCount()) << u;
	}
}

// ---- PerfWindowPacker ----

TEST(PerfWindowEdge, GarbageUnitHzStillProducesValidJson)
{
	CoarseRing ring;
	ring.Init(100, 2);
	for (int i = 0; i < 10; ++i) { ring.RecordModule(0, 0xA, 1.0f); ring.RecordUnitTick(2.0f); ring.CommitEntry(); }
	const float hzs[] = { 0.0f, -5.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
	for (float hz : hzs)
	{
		PerfWindowUnitInput in = { 1, "U", hz, &ring };
		std::string out;
		PackPerfWindow(&in, 1, 10.0f, nullptr, out);
		Json::Value root;
		ASSERT_TRUE(TryParse(out, root)) << "hz=" << hz << " json=" << out;
		EXPECT_EQ(0u, root["units"][0]["count"].asUInt()) << "hz=" << hz;
	}
}

TEST(PerfWindowEdge, SecondsNanNegativeInfAreClamped)
{
	CoarseRing ring;
	ring.Init(CoarseRing::CapacityFor(30.0f, 60.0f), 2);
	for (int i = 0; i < 100; ++i) { ring.RecordModule(0, 0xA, 1.0f); ring.RecordUnitTick(2.0f); ring.CommitEntry(); }
	PerfWindowUnitInput in = { 1, "U", 30.0f, &ring };
	struct Case { float seconds; int expectSeconds; uint32_t expectCount; };
	const Case cases[] = {
		{ std::numeric_limits<float>::quiet_NaN(), 1, 30 },
		{ -10.0f, 1, 30 },
		{ -std::numeric_limits<float>::infinity(), 1, 30 },
		{ std::numeric_limits<float>::infinity(), 60, 100 },
		{ 1e30f, 60, 100 },
	};
	for (const Case& c : cases)
	{
		std::string out;
		PackPerfWindow(&in, 1, c.seconds, nullptr, out);
		Json::Value root;
		ASSERT_TRUE(TryParse(out, root)) << out;
		EXPECT_EQ(c.expectSeconds, root["seconds"].asInt());
		EXPECT_EQ(c.expectCount, root["units"][0]["count"].asUInt());
	}
}

TEST(PerfWindowEdge, EmptyWindowAndNullRingUnit)
{
	std::string out;
	PackPerfWindow(nullptr, 0, 10.0f, nullptr, out);
	Json::Value root = Parse(out);
	EXPECT_EQ(0u, root["units"].size());

	PerfWindowUnitInput in = { 1, nullptr, 30.0f, nullptr };   // null name and null ring
	PackPerfWindow(&in, 1, 10.0f, nullptr, out);
	root = Parse(out);
	ASSERT_EQ(1u, root["units"].size());
	EXPECT_EQ(0u, root["units"][0]["count"].asUInt());
	EXPECT_EQ("", root["units"][0]["unit"].asString());
}

TEST(PerfWindowEdge, NamesWithControlCharsQuotesAndUtf8RoundTrip)
{
	CoarseRing ring;
	ring.Init(10, 1);
	ring.RecordModule(0, 0xA, 1.0f); ring.RecordUnitTick(1.0f); ring.CommitEntry();
	const std::string unitName = std::string("a\"b\\c\n\t\x01") + "\xC3\xA9 \xE2\x82\xAC";
	PerfWindowUnitInput in = { 1, unitName.c_str(), 30.0f, &ring };
	std::string out;
	PackPerfWindow(&in, 1, 5.0f,
		[](uint32_t, char* buf, size_t n) -> const char* { snprintf(buf, n, "m\"od\\\x1f"); return buf; }, out);
	Json::Value root;
	ASSERT_TRUE(TryParse(out, root)) << out;
	EXPECT_EQ(unitName, root["units"][0]["unit"].asString());
	EXPECT_EQ(std::string("m\"od\\\x1f"), root["units"][0]["modules"][0]["name"].asString());

	std::string frame;
	PackPerfFrameSections(unitName.c_str(), 3, nullptr, 0, frame);
	root = Parse(frame);
	EXPECT_EQ(unitName, root["unit"].asString());
}

TEST(PerfWindowEdge, FrameOutOfRangeOldestAndHuge)
{
	CoarseChunkAssembler a;
	uint64_t next = 0;
	uint32_t seq = 0;
	while (next < 2000)
	{
		a.Apply(MakeChunk(1, 30.0f, seq++, next, 50, { {0, 0xA, 1.0f} }));
		next += 50;
	}
	PerfWindowUnitInput in = { 1, "Sim", 30.0f, a.GetRing(1) };
	std::string out;
	PackPerfFrame(in, 199, nullptr, out);                       // overwritten
	EXPECT_TRUE(Parse(out).isMember("error"));
	PackPerfFrame(in, 200, nullptr, out);                       // oldest retained
	EXPECT_FALSE(Parse(out).isMember("error"));
	PackPerfFrame(in, 1999, nullptr, out);                      // newest
	EXPECT_FALSE(Parse(out).isMember("error"));
	PackPerfFrame(in, 2000, nullptr, out);                      // one past the end
	EXPECT_TRUE(Parse(out).isMember("error"));
	PackPerfFrame(in, std::numeric_limits<uint64_t>::max(), nullptr, out);
	EXPECT_TRUE(Parse(out).isMember("error"));
}

TEST(PerfWindowEdge, SectionsAreEscapedAndNonFiniteMsStaysParsable)
{
	PerfFrameSection s[2] = { { "a\"b", 1.5f }, { "c\\d", 2.0f } };
	std::string out;
	PackPerfFrameSections("u", 7, s, 2, out);
	Json::Value root = Parse(out);
	EXPECT_EQ("a\"b", root["sections"][0]["name"].asString());
	EXPECT_EQ("c\\d", root["sections"][1]["name"].asString());
}

// ---- Jobs packing ----

TEST(PerfJobsEdge, WindowJobsGarbageHzSecondsAndUnsortedFrames)
{
	JobFrameAggregate aggs[4] = {
		{ 50, 0x42, 3, 1.0f, 0.5f },
		{ 10, 0x42, 2, 1.0f, 0.5f },     // frame goes backwards
		{ 10, 0x43, 1, 1.0f, 0.5f },
		{ 60, 0x42, 1, 1.0f, 0.5f },
	};
	const float hzs[] = { 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN() };
	const float secs[] = { std::numeric_limits<float>::quiet_NaN(), -3.0f, 1e30f };
	for (float hz : hzs)
		for (float sec : secs)
		{
			PerfJobsUnitInput in = { 1, "U", hz, true, aggs, 4 };
			std::string out;
			PackPerfWindowJobs(&in, 1, sec, nullptr, out);
			Json::Value root;
			ASSERT_TRUE(TryParse(out, root)) << out;
			EXPECT_EQ(60.0, root["units"][0]["hz"].asDouble());
			EXPECT_GE(root["units"][0]["bucketCount"].asUInt(), 1u);
		}
}

TEST(PerfJobsEdge, WindowJobsSaturatesCountAndBusyAtU16)
{
	JobFrameAggregate aggs[2] = {
		{ 5, 0x42, 4000000000u, 1.0e9f, 1.0e9f },
		{ 5, 0x42, 4000000000u, 1.0e9f, 1.0e9f },
	};
	PerfJobsUnitInput in = { 1, "U", 60.0f, true, aggs, 2 };
	std::string out;
	PackPerfWindowJobs(&in, 1, 10.0f, nullptr, out);
	Json::Value root = Parse(out);
	const Json::Value& agg = root["units"][0]["frameAgg"];
	const std::vector<uint16_t> count = DecodeU16(agg["count"].asString());
	const std::vector<uint16_t> busy = DecodeU16(agg["busyMs"].asString());
	const std::vector<uint16_t> wait = DecodeU16(agg["worstWaitMs"].asString());
	ASSERT_FALSE(count.empty());
	uint16_t maxCount = 0, maxBusy = 0, maxWait = 0;
	for (uint16_t v : count) maxCount = std::max(maxCount, v);
	for (uint16_t v : busy) maxBusy = std::max(maxBusy, v);
	for (uint16_t v : wait) maxWait = std::max(maxWait, v);
	EXPECT_EQ(65535, maxCount);
	EXPECT_EQ(65535, maxBusy);
	EXPECT_EQ(65535, maxWait);
}

TEST(PerfJobsEdge, WindowJobsEmptyAndKillSwitchUnitsAreValid)
{
	std::string out;
	PackPerfWindowJobs(nullptr, 0, 10.0f, nullptr, out);
	EXPECT_TRUE(Parse(out)["perJobRecording"].asBool());       // no units: assumed on
	PerfJobsUnitInput off = { 1, "U", 60.0f, false, nullptr, 0 };
	PackPerfWindowJobs(&off, 1, 10.0f, nullptr, out);
	Json::Value root = Parse(out);
	EXPECT_FALSE(root["perJobRecording"].asBool());
	EXPECT_EQ(0u, root["units"][0]["owners"].size());
	EXPECT_EQ(0u, root["units"][0]["bucketCount"].asUInt());
}

TEST(PerfJobsEdge, RecordsPackedWithEqualAndExtremeRange)
{
	JobRecordPacked rec = {};
	rec.startNs = std::numeric_limits<uint64_t>::max();
	rec.durNs = 0xFFFFFFFFu;
	std::string out;
	PackPerfJobsRecords("u\"x", 7, 5, 5, &rec, 1, false, true, out);
	Json::Value root = Parse(out);
	EXPECT_EQ("u\"x", root["unit"].asString());
	EXPECT_EQ(1u, root["count"].asUInt());
	EXPECT_EQ(8u, DecodeB64(root["startNs"].asString()).size());
	PackPerfJobsRecords("u", 7, std::numeric_limits<uint64_t>::max(), 0, nullptr, 0, true, false, out);
	root = Parse(out);
	EXPECT_EQ(0u, root["count"].asUInt());
	EXPECT_TRUE(root["truncated"].asBool());
	EXPECT_FALSE(root["perJobRecording"].asBool());
}

// ---- JobChunkAssembler ----

namespace
{
	std::unique_ptr<JobChunk> MakeJobChunk(uint32_t unitCrc, uint32_t seq, uint64_t rawFirst, uint32_t rawCount,
		uint64_t aggFirst = 0, uint32_t aggCount = 0)
	{
		auto c = std::make_unique<JobChunk>();
		std::memset(c.get(), 0, sizeof(JobChunk));
		c->unitCrc = unitCrc;
		c->seq = seq;
		c->flags = JobChunk::kFlagPerJob;
		c->rawFirst = rawFirst;
		c->rawCount = rawCount;
		c->aggFirst = aggFirst;
		c->aggCount = aggCount;
		for (uint32_t i = 0; i < rawCount && i < JobChunk::kMaxRaw; ++i)
		{
			c->raw[i].startNs = (rawFirst + i) * 1000;
			c->raw[i].durNs = 100;
		}
		return c;
	}
}

TEST(JobChunkEdge, DuplicateStaleAndOverlappingChunksDoNotDuplicateRecords)
{
	JobChunkAssembler a;
	a.Apply(*MakeJobChunk(1, 0, 0, 10));
	a.Apply(*MakeJobChunk(1, 0, 0, 10));     // duplicate
	a.Apply(*MakeJobChunk(1, 1, 5, 10));     // overlaps entries 5..9, adds 10..14
	EXPECT_EQ(15u, a.GetRawCount(1));
	EXPECT_EQ(0u, a.GetGapCount(1));
	a.Apply(*MakeJobChunk(1, 3, 20, 5));     // seq 2 lost and records 15..19 lost
	EXPECT_EQ(1u, a.GetGapCount(1));
	EXPECT_EQ(5u, a.GetRawLost(1));
	a.Apply(*MakeJobChunk(1, 2, 15, 5));     // late: dropped as stale
	EXPECT_EQ(20u, a.GetRawCount(1));
}

TEST(JobChunkEdge, NineUnitsCountOverflow)
{
	JobChunkAssembler a;
	for (uint32_t u = 0; u < 10; ++u)
		a.Apply(*MakeJobChunk(200 + u, 0, 0, 1));
	EXPECT_EQ(8u, a.GetUnitCount());
	EXPECT_EQ(2u, a.GetDroppedUnknownUnitCount());
}

TEST(JobChunkEdge, CopyRawEqualFromToAndHugeRanges)
{
	JobChunkAssembler a;
	a.Apply(*MakeJobChunk(1, 0, 0, 10));     // record i: [i*1000, i*1000+100]
	JobRecordPacked out[16];
	bool truncated = true;
	EXPECT_EQ(1u, a.CopyRaw(1, 3050, 3050, out, 16, &truncated));    // equal from/to inside a record
	EXPECT_FALSE(truncated);
	EXPECT_EQ(1u, a.CopyRaw(1, 3000, 3000, out, 16, &truncated));    // touches start edge
	EXPECT_EQ(1u, a.CopyRaw(1, 3100, 3100, out, 16, &truncated));    // touches end edge
	EXPECT_EQ(0u, a.CopyRaw(1, 3101, 3101, out, 16, &truncated));    // in the gap
	EXPECT_EQ(10u, a.CopyRaw(1, 0, std::numeric_limits<uint64_t>::max(), out, 16, &truncated));
	EXPECT_EQ(0u, a.CopyRaw(1, std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max(), out, 16, &truncated));
	EXPECT_EQ(3u, a.CopyRaw(1, 0, std::numeric_limits<uint64_t>::max(), out, 3, &truncated));
	EXPECT_TRUE(truncated);
	EXPECT_EQ(0u, a.CopyRaw(99, 0, 10, out, 16, &truncated));
}

// ---- PerfTimingStats / JobTimingTable ----

TEST(PerfTimingEdge, RingStatsSurviveGarbageSecondsAndHz)
{
	CoarseRing ring;
	ring.Init(100, 1);
	for (int i = 0; i < 50; ++i) { ring.RecordModule(0, 0xA, 1.0f); ring.RecordUnitTick(2.0f); ring.CommitEntry(); }
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const float hzs[] = { 0.0f, -1.0f, nan, inf, 60.0f };
	const float secs[] = { 0.0f, -5.0f, nan, inf, 1e30f, 10.0f };
	for (float hz : hzs)
		for (float sec : secs)
		{
			PerfSeriesStats st;
			PerfTimingScratch scratch;
			EXPECT_TRUE(ComputePerfRingSeriesStats(ring, CoarseRing::kUnitSlot, hz, sec, st, scratch)) << hz << " " << sec;
			EXPECT_GE(st.samples, 1u);
			EXPECT_LE(st.samples, 50u);
			EXPECT_TRUE(std::isfinite(st.ewmaMs));
			EXPECT_NEAR(2.0f, st.p95Ms, 1e-4f);
		}
}

TEST(PerfTimingEdge, RingStatsWindowLongerThanRetentionUsesWhatIsRetained)
{
	CoarseChunkAssembler a;
	uint64_t next = 0;
	uint32_t seq = 0;
	while (next < 2000)
	{
		a.Apply(MakeChunk(1, 30.0f, seq++, next, 50, { {0, 0xA, 1.0f} }, 4.0f));
		next += 50;
	}
	PerfSeriesStats st;
	PerfTimingScratch scratch;
	ASSERT_TRUE(ComputePerfRingSeriesStats(*a.GetRing(1), CoarseRing::kUnitSlot, 30.0f, 60.0f, st, scratch));
	EXPECT_EQ(kPerfTimingMaxEntries, st.samples);   // capped at 1024 entries even though 1800 are retained
	EXPECT_NEAR(4.0f, st.p95Ms, 1e-4f);
}

TEST(JobTimingEdge, ReversedAndZeroLengthRecordsDoNotUnderflow)
{
	JobTimingTable t;
	t.Init(8);
	Dia::Threading::JobTimingRecord rec = {};
	rec.owner = 5;
	rec.submitNs = 5000;
	rec.startNs = 1000;     // start before submit
	rec.endNs = 500;        // end before start
	t.Record(rec);
	t.EndFrame(0);
	const JobOwnerTier1 o = t.GetOwner(0);
	EXPECT_EQ(1u, o.count);
	EXPECT_EQ(0u, o.busyNs);
	EXPECT_FLOAT_EQ(0.0f, o.worstWaitMs);
	JobRecordPacked raw;
	ASSERT_TRUE(t.GetRaw(0, raw));
	EXPECT_EQ(0u, raw.durNs);
	EXPECT_EQ(0u, raw.waitUs);
}

TEST(JobTimingEdge, CopyRawWithEqualFromToAndExtremeBounds)
{
	JobTimingTable t;
	t.Init(8);
	Dia::Threading::JobTimingRecord rec = {};
	rec.owner = 5;
	rec.submitNs = 1000;
	rec.startNs = 1000;
	rec.endNs = 1100;
	t.Record(rec);
	JobRecordPacked out[4];
	bool truncated = true;
	EXPECT_EQ(1u, t.CopyRaw(1050, 1050, out, 4, &truncated));
	EXPECT_FALSE(truncated);
	EXPECT_EQ(1u, t.CopyRaw(0, std::numeric_limits<uint64_t>::max(), out, 4, &truncated));
	EXPECT_EQ(0u, t.CopyRaw(2000, 3000, out, 4, &truncated));
	EXPECT_EQ(0u, t.CopyRaw(0, 10, out, 4, nullptr));
}
