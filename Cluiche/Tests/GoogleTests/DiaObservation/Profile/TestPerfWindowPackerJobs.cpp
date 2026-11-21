////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfWindowPackerJobs.cpp
// GoogleTest suite - Perf Inspector task 13 part B: tier 2 jobs section of perf.window and the
// perf.jobs record payload (pure packers).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Profile/PerfWindowPacker.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <json/json.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;

namespace
{
	std::vector<uint8_t> DecodeBase64(const std::string& in)
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
			if (bits >= 8)
			{
				bits -= 8;
				out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
			}
		}
		return out;
	}

	template <typename T>
	std::vector<T> Decode(const std::string& in)
	{
		const std::vector<uint8_t> raw = DecodeBase64(in);
		std::vector<T> out(raw.size() / sizeof(T));
		for (size_t i = 0; i < out.size(); ++i)
		{
			uint64_t v = 0;
			for (int b = static_cast<int>(sizeof(T)) - 1; b >= 0; --b)
				v = (v << 8) | raw[i * sizeof(T) + static_cast<size_t>(b)];
			out[i] = static_cast<T>(v);
		}
		return out;
	}

	Json::Value Parse(const std::string& s)
	{
		Json::Value root;
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		EXPECT_TRUE(r->parse(s.data(), s.data() + s.size(), &root, &errs)) << errs;
		return root;
	}

	// frames x owners aggregates, one per owner per frame: count = owner+1, busy = 1 ms, wait = (frame % 7) * 0.1 ms.
	std::vector<JobFrameAggregate> MakeAggs(uint32_t frames, uint32_t owners)
	{
		std::vector<JobFrameAggregate> aggs;
		for (uint32_t f = 0; f < frames; ++f)
			for (uint32_t o = 0; o < owners; ++o)
				aggs.push_back({ f, 0x500u + o, o + 1, 1.0f, static_cast<float>(f % 7) * 0.1f });
		return aggs;
	}

	const char* ResolveWeather(uint32_t crc, char* buf, size_t n)
	{
		if (crc != 0x42u) return nullptr;
		snprintf(buf, n, "Weather");
		return buf;
	}
}

TEST(PerfWindowPackerJobs, RoundTripBucketsCountBusyAndWorstWait)
{
	// 30 Hz -> 6 frames per 200 ms bucket. 60 frames x 2 owners.
	const std::vector<JobFrameAggregate> aggs = MakeAggs(60, 2);
	PerfJobsUnitInput in = { 0x11u, "Sim", 30.0f, true, aggs.data(), static_cast<uint32_t>(aggs.size()) };
	std::string json;
	PackPerfWindowJobs(&in, 1, 60.0f, nullptr, json);
	const Json::Value root = Parse(json);
	EXPECT_EQ(200u, root["bucketMs"].asUInt());
	EXPECT_TRUE(root["perJobRecording"].asBool());
	ASSERT_TRUE(root["steps"].isArray());
	EXPECT_EQ(0u, root["steps"].size());
	ASSERT_EQ(1u, root["units"].size());
	const Json::Value& u = root["units"][0];
	EXPECT_EQ("Sim", u["unit"].asString());
	EXPECT_EQ(6u, u["framesPerBucket"].asUInt());
	ASSERT_EQ(2u, u["owners"].size());
	EXPECT_EQ(0x500u, u["owners"][0]["crc"].asUInt());
	EXPECT_EQ("0x00000500", u["owners"][0]["name"].asString());
	const uint32_t buckets = u["bucketCount"].asUInt();
	EXPECT_EQ(300u, buckets);
	EXPECT_EQ(0u, u["firstFrame"].asUInt());
	const auto count = Decode<uint16_t>(u["frameAgg"]["count"].asString());
	const auto busy  = Decode<uint16_t>(u["frameAgg"]["busyMs"].asString());
	const auto wait  = Decode<uint16_t>(u["frameAgg"]["worstWaitMs"].asString());
	ASSERT_EQ(2u * buckets, count.size());
	ASSERT_EQ(count.size(), busy.size());
	ASSERT_EQ(count.size(), wait.size());
	// bucket 0 = frames 0..5: owner 0 count 6 x 1, busy 6 ms = 600, worst wait 0.5 ms = 50.
	EXPECT_EQ(6u, count[0]);
	EXPECT_EQ(600u, busy[0]);
	EXPECT_EQ(50u, wait[0]);
	// owner 1 (count 2 per frame) starts at index buckets.
	EXPECT_EQ(12u, count[buckets]);
	// bucket 30 (frames 180..185) is beyond the data.
	EXPECT_EQ(0u, count[30]);
}

TEST(PerfWindowPackerJobs, ShortWindowKeepsNewestFrames)
{
	const std::vector<JobFrameAggregate> aggs = MakeAggs(300, 1);   // 10 s at 30 Hz
	PerfJobsUnitInput in = { 0x11u, "Sim", 30.0f, true, aggs.data(), static_cast<uint32_t>(aggs.size()) };
	std::string json;
	PackPerfWindowJobs(&in, 1, 2.0f, nullptr, json);
	const Json::Value u = Parse(json)["units"][0];
	EXPECT_EQ(10u, u["bucketCount"].asUInt());
	EXPECT_EQ(240u, u["firstFrame"].asUInt());
	EXPECT_EQ(10u, Decode<uint16_t>(u["frameAgg"]["count"].asString()).size());
}

TEST(PerfWindowPackerJobs, KillSwitchAndEmptyPackNoAggregates)
{
	const std::vector<JobFrameAggregate> aggs = MakeAggs(30, 2);
	PerfJobsUnitInput off = { 0x11u, "Sim", 30.0f, false, aggs.data(), static_cast<uint32_t>(aggs.size()) };
	std::string json;
	PackPerfWindowJobs(&off, 1, 60.0f, nullptr, json);
	Json::Value root = Parse(json);
	EXPECT_FALSE(root["perJobRecording"].asBool());
	EXPECT_FALSE(root["units"][0]["perJobRecording"].asBool());
	EXPECT_EQ(0u, root["units"][0]["owners"].size());
	EXPECT_EQ(0u, root["units"][0]["bucketCount"].asUInt());
	EXPECT_TRUE(root["units"][0]["frameAgg"]["count"].asString().empty());

	PerfJobsUnitInput none = { 0x11u, "Sim", 30.0f, true, nullptr, 0 };
	PackPerfWindowJobs(&none, 1, 60.0f, nullptr, json);
	root = Parse(json);
	EXPECT_EQ(0u, root["units"][0]["owners"].size());

	PackPerfWindowJobs(nullptr, 0, 60.0f, nullptr, json);
	root = Parse(json);
	EXPECT_EQ(0u, root["units"].size());
	EXPECT_EQ(0u, root["steps"].size());
}

TEST(PerfWindowPackerJobs, SaturatesAndNamesOwnersViaResolver)
{
	const std::vector<JobFrameAggregate> aggs = { { 0, 0u, 100000, 9999.0f, 99999.0f }, { 0, 0x42u, 1, 0.5f, 0.0f } };
	PerfJobsUnitInput in = { 0x11u, "Sim", 60.0f, true, aggs.data(), 2 };
	std::string json;
	PackPerfWindowJobs(&in, 1, 1.0f, &ResolveWeather, json);
	const Json::Value u = Parse(json)["units"][0];
	EXPECT_EQ("(other)", u["owners"][0]["name"].asString());
	EXPECT_EQ("Weather", u["owners"][1]["name"].asString());
	const uint32_t buckets = u["bucketCount"].asUInt();
	const auto count = Decode<uint16_t>(u["frameAgg"]["count"].asString());
	EXPECT_EQ(65535u, count[0]);
	EXPECT_EQ(65535u, Decode<uint16_t>(u["frameAgg"]["busyMs"].asString())[0]);
	EXPECT_EQ(65535u, Decode<uint16_t>(u["frameAgg"]["worstWaitMs"].asString())[0]);
	EXPECT_EQ(1u, count[buckets]);
}

TEST(PerfWindowPackerJobs, Tier2For60sWith5OwnersIsUnder30KB)
{
	// Worst plausible: every owner active every frame for 60 s (1800 frames x 5 owners at 30 Hz).
	const std::vector<JobFrameAggregate> aggs = MakeAggs(1800, 5);
	PerfJobsUnitInput in = { 0x11u, "Sim", 30.0f, true, aggs.data(), static_cast<uint32_t>(aggs.size()) };
	std::string json;
	PackPerfWindowJobs(&in, 1, 60.0f, nullptr, json);
	std::printf("[perf] jobs section 60 s, 5 owners, 30 Hz: %zu bytes\n", json.size());
	EXPECT_LT(json.size(), 30000u);
	// Buckets are time based, so 60 Hz costs the same.
	const std::vector<JobFrameAggregate> aggs60 = MakeAggs(3600, 5);
	PerfJobsUnitInput in60 = { 0x11u, "Sim", 60.0f, true, aggs60.data(), static_cast<uint32_t>(aggs60.size()) };
	PackPerfWindowJobs(&in60, 1, 60.0f, nullptr, json);
	std::printf("[perf] jobs section 60 s, 5 owners, 60 Hz: %zu bytes\n", json.size());
	EXPECT_LT(json.size(), 30000u);
}

TEST(PerfWindowPackerJobs, WholeWindow3Units13ModulesPlus5OwnerJobsUnder120KB)
{
	struct UnitSpec { uint32_t crc; const char* name; float hz; uint32_t modules; };
	const UnitSpec specs[] = { { 0x11u, "Main", 30.0f, 4 }, { 0x22u, "Sim", 30.0f, 5 }, { 0x33u, "Render", 60.0f, 4 } };
	CoarseChunkAssembler assembler;
	for (const UnitSpec& spec : specs)
	{
		CoarseChunkBuilder builder;
		builder.Begin(spec.crc, spec.hz);
		const uint32_t entries = CoarseRing::CapacityFor(spec.hz, 60.0f);
		for (uint32_t e = 0; e < entries; ++e)
		{
			for (uint32_t m = 0; m < spec.modules; ++m)
				builder.RecordModule(m, 0x100u * (m + 1) + spec.crc, 0.5f + 0.01f * static_cast<float>((e + m) % 50));
			builder.RecordUnitTick(3.0f + 0.01f * static_cast<float>(e % 20));
			builder.CommitEntry(0.0);
			if ((e + 1) % CoarseChunk::kMaxEntries == 0)
				assembler.Apply(builder.Take(1000.0));
		}
		assembler.Apply(builder.Take(1000.0));
	}
	PerfWindowUnitInput inputs[3];
	for (uint32_t i = 0; i < 3; ++i)
		inputs[i] = { specs[i].crc, specs[i].name, specs[i].hz, assembler.GetRing(specs[i].crc) };
	std::string window;
	PackPerfWindow(inputs, 3, 60.0f, nullptr, window);

	// 5 owners for 60 s on the Sim unit.
	const std::vector<JobFrameAggregate> aggs = MakeAggs(1800, 5);
	PerfJobsUnitInput in = { 0x22u, "Sim", 30.0f, true, aggs.data(), static_cast<uint32_t>(aggs.size()) };
	std::string jobs;
	PackPerfWindowJobs(&in, 1, 60.0f, nullptr, jobs);
	std::printf("[perf] perf.window 60 s 3 units (13 modules total, as AC7): %zu bytes, jobs section %zu bytes, total %zu bytes\n",
		window.size(), jobs.size(), window.size() + jobs.size());
	EXPECT_LT(window.size() + jobs.size() + 10, 120000u);
}

TEST(PerfWindowPackerJobs, RecordsRoundTripAndTruncatedFlag)
{
	JobRecordPacked recs[2] = {};
	recs[0].startNs = 5000000000ull; recs[0].durNs = 1500; recs[0].owner = 0x42; recs[0].tag = 9;
	recs[0].range = 3; recs[0].rangeCount = 4; recs[0].worker = 2; recs[0].flags = 1; recs[0].waitUs = 77;
	recs[1].startNs = 6000000000ull; recs[1].durNs = 2500; recs[1].owner = 0x43; recs[1].worker = 1;
	std::string json;
	PackPerfJobsRecords("Sim", 0x11u, 1, 99, recs, 2, true, true, json);
	const Json::Value r = Parse(json);
	EXPECT_TRUE(r["truncated"].asBool());
	EXPECT_TRUE(r["perJobRecording"].asBool());
	EXPECT_EQ(2u, r["count"].asUInt());
	const auto start = Decode<uint64_t>(r["startNs"].asString());
	ASSERT_EQ(2u, start.size());
	EXPECT_EQ(5000000000ull, start[0]);
	EXPECT_EQ(6000000000ull, start[1]);
	EXPECT_EQ(1500u, Decode<uint32_t>(r["durNs"].asString())[0]);
	EXPECT_EQ(0x43u, Decode<uint32_t>(r["owner"].asString())[1]);
	EXPECT_EQ(9u, Decode<uint32_t>(r["tag"].asString())[0]);
	EXPECT_EQ(77u, Decode<uint32_t>(r["waitUs"].asString())[0]);
	EXPECT_EQ(2u, Decode<uint16_t>(r["worker"].asString())[0]);
	EXPECT_EQ(3u, Decode<uint16_t>(r["range"].asString())[0]);
	EXPECT_EQ(4u, Decode<uint16_t>(r["rangeCount"].asString())[0]);
	EXPECT_EQ(1u, Decode<uint16_t>(r["flags"].asString())[0]);
	PackPerfJobsRecords("Sim", 0x11u, 1, 99, nullptr, 0, false, false, json);
	const Json::Value e = Parse(json);
	EXPECT_FALSE(e["truncated"].asBool());
	EXPECT_FALSE(e["perJobRecording"].asBool());
	EXPECT_EQ(0u, e["count"].asUInt());
}
