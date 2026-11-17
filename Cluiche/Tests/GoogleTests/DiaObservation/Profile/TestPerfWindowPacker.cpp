#include <gtest/gtest.h>
#include <DiaObservation/Profile/PerfWindowPacker.h>
#include <DiaObservation/Profile/CoarseRing.h>
#include <json/json.h>
#include <cstdio>
#include <iostream>
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

	std::vector<uint16_t> DecodeU16(const std::string& in)
	{
		std::vector<uint8_t> raw = DecodeBase64(in);
		std::vector<uint16_t> out(raw.size() / 2);
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = static_cast<uint16_t>(raw[i * 2] | (raw[i * 2 + 1] << 8));
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

	uint32_t gSeed = 12345;
	float NextMs()
	{
		gSeed = gSeed * 1664525u + 1013904223u;
		return 0.05f + (static_cast<float>(gSeed >> 8) / 16777216.0f) * 7.95f;
	}

	void Fill(CoarseRing& ring, uint32_t modules, uint32_t entries, float tickScale = 1.0f)
	{
		for (uint32_t e = 0; e < entries; ++e)
		{
			for (uint32_t m = 0; m < modules; ++m)
				ring.RecordModule(m, 1000 + m, NextMs());
			ring.RecordUnitTick(NextMs() * tickScale);
			ring.CommitEntry();
		}
	}

	struct Rig
	{
		CoarseRing main, sim, render;
		PerfWindowUnitInput units[3];
		Rig()
		{
			main.Init(CoarseRing::CapacityFor(30.0f, 60.0f), 4);
			sim.Init(CoarseRing::CapacityFor(30.0f, 60.0f), 5);
			render.Init(CoarseRing::CapacityFor(60.0f, 60.0f), 4);
			Fill(main, 4, 1800);
			Fill(sim, 5, 1800);
			Fill(render, 4, 3600);
			units[0] = { 1, "Main", 30.0f, &main };
			units[1] = { 2, "Sim", 30.0f, &sim };
			units[2] = { 3, "Render", 60.0f, &render };
		}
	};

	size_t PlainJsonBytes(const Rig& rig, uint32_t count)
	{
		// Same arrays as plain JSON numbers, comma separated.
		size_t bytes = 0;
		for (const PerfWindowUnitInput& u : rig.units)
		{
			const CoarseRing& r = *u.ring;
			const uint32_t modules = r.GetModuleCount();
			uint64_t first = r.GetEntryCount() > count ? r.GetEntryCount() - count : 0;
			std::vector<uint16_t> buf(count);
			for (uint32_t s = 0; s <= modules; ++s)
			{
				uint32_t series = (s == modules) ? CoarseRing::kUnitSlot : s;
				uint32_t n = r.CopyRange(series, first, count, buf.data());
				for (uint32_t i = 0; i < n; ++i)
					bytes += std::to_string(buf[i]).size() + 1;
			}
		}
		return bytes;
	}
}

TEST(PerfWindowPacker, SizeUnder120KbFor60sAt30HzWith3Units13Modules)
{
	Rig rig;
	std::string json60, json30;
	PackPerfWindow(rig.units, 3, 60.0f, nullptr, json60);
	PackPerfWindow(rig.units, 3, 30.0f, nullptr, json30);
	const size_t plain60 = PlainJsonBytes(rig, 3600) ;
	std::cout << "[PerfWindow] base64 60s=" << json60.size() << " base64 30s=" << json30.size()
		<< " plainJsonArrays(60s, Main/Sim 1800 + Render 3600 clipped to 3600)=" << plain60 << std::endl;
	RecordProperty("Base64Bytes60s", static_cast<int>(json60.size()));
	RecordProperty("Base64Bytes30s", static_cast<int>(json30.size()));
	RecordProperty("PlainJsonBytes60s", static_cast<int>(plain60));
	EXPECT_LT(json60.size(), 120000u);
	EXPECT_LT(json30.size(), 120000u);
}

TEST(PerfWindowPacker, RoundTripMatchesRingValues)
{
	Rig rig;
	std::string json;
	PackPerfWindow(rig.units, 3, 60.0f, nullptr, json);
	Json::Value root = Parse(json);
	ASSERT_EQ(1, root["v"].asInt());
	ASSERT_EQ(3u, root["units"].size());
	EXPECT_FALSE(root["truncated"].asBool());
	EXPECT_TRUE(root["markers"].isArray());
	EXPECT_TRUE(root["trips"].isArray());

	for (uint32_t i = 0; i < 3; ++i)
	{
		const Json::Value& u = root["units"][i];
		const CoarseRing& ring = *rig.units[i].ring;
		const uint32_t count = u["count"].asUInt();
		ASSERT_EQ(ring.GetEntryCount(), count);
		EXPECT_EQ(rig.units[i].name, u["unit"].asString());
		EXPECT_EQ(rig.units[i].unitCrc, u["unitCrc"].asUInt());
		const uint32_t modules = u["modules"].size();
		std::vector<uint16_t> unitMs = DecodeU16(u["unitMs"].asString());
		std::vector<uint16_t> moduleMs = DecodeU16(u["moduleMs"].asString());
		ASSERT_EQ(count, unitMs.size());
		ASSERT_EQ(static_cast<size_t>(count) * modules, moduleMs.size());
		std::vector<uint16_t> expect(count);
		ring.CopyRange(CoarseRing::kUnitSlot, u["firstEntry"].asUInt64(), count, expect.data());
		EXPECT_EQ(expect, unitMs);
		for (uint32_t m = 0; m < modules; ++m)
		{
			EXPECT_EQ(ring.GetModuleCrc(m), u["modules"][m]["crc"].asUInt());
			ring.CopyRange(m, u["firstEntry"].asUInt64(), count, expect.data());
			EXPECT_EQ(expect, std::vector<uint16_t>(moduleMs.begin() + m * count, moduleMs.begin() + (m + 1) * count));
		}
		EXPECT_FALSE(u.isMember("flags"));
	}
}

TEST(PerfWindowPacker, WindowShorterThanRingTakesNewest)
{
	Rig rig;
	std::string json;
	PackPerfWindow(rig.units, 1, 10.0f, nullptr, json);
	Json::Value root = Parse(json);
	const Json::Value& u = root["units"][0];
	EXPECT_EQ(300u, u["count"].asUInt());
	EXPECT_EQ(1500u, u["firstEntry"].asUInt());
}

TEST(PerfWindowPacker, SecondsClamped)
{
	Rig rig;
	std::string a, b;
	PackPerfWindow(rig.units, 1, 1000.0f, nullptr, a);
	PackPerfWindow(rig.units, 1, 60.0f, nullptr, b);
	EXPECT_EQ(a.find("\"seconds\":60"), b.find("\"seconds\":60"));
	EXPECT_NE(std::string::npos, a.find("\"seconds\":60"));
	std::string c;
	PackPerfWindow(rig.units, 1, 0.0f, nullptr, c);
	Json::Value root = Parse(c);
	EXPECT_EQ(30u, root["units"][0]["count"].asUInt());
	EXPECT_EQ(1, root["seconds"].asInt());
}

TEST(PerfWindowPacker, ClampedFlagSurvives)
{
	CoarseRing ring;
	ring.Init(10, 1);
	ring.RecordModule(0, 7, 1.0f);
	ring.RecordUnitTick(1.0f);
	ring.CommitEntry();
	ring.RecordModule(0, 7, 9999.0f);
	ring.RecordUnitTick(1.0f);
	ring.CommitEntry();
	PerfWindowUnitInput in = { 9, "U", 30.0f, &ring };
	std::string json;
	PackPerfWindow(&in, 1, 5.0f, nullptr, json);
	Json::Value root = Parse(json);
	const Json::Value& u = root["units"][0];
	ASSERT_TRUE(u.isMember("flags"));
	std::vector<uint8_t> flags = DecodeBase64(u["flags"].asString());
	ASSERT_EQ(2u, flags.size());
	EXPECT_EQ(0, flags[0]);
	EXPECT_EQ(CoarseRing::kFlagClamped, flags[1]);
	std::vector<uint16_t> mm = DecodeU16(u["moduleMs"].asString());
	ASSERT_EQ(2u, mm.size());
	EXPECT_EQ(65535u, mm[1]);
}

TEST(PerfWindowPacker, EmptyUnitHasCountZero)
{
	CoarseRing ring;
	ring.Init(10, 2);
	PerfWindowUnitInput units[2] = { { 5, "Empty", 30.0f, &ring }, { 6, "NoRing", 30.0f, nullptr } };
	std::string json;
	PackPerfWindow(units, 2, 5.0f, nullptr, json);
	Json::Value root = Parse(json);
	ASSERT_EQ(2u, root["units"].size());
	for (uint32_t i = 0; i < 2; ++i)
	{
		EXPECT_EQ(0u, root["units"][i]["count"].asUInt());
		EXPECT_EQ("", root["units"][i]["unitMs"].asString());
		EXPECT_EQ("", root["units"][i]["moduleMs"].asString());
		EXPECT_FALSE(root["units"][i].isMember("flags"));
	}
}

TEST(PerfWindowPacker, NameEscapingAndResolver)
{
	CoarseRing ring;
	ring.Init(4, 1);
	ring.RecordModule(0, 0xABCu, 1.0f);
	ring.CommitEntry();
	PerfWindowUnitInput in = { 1, "a\"b\\c\n", 30.0f, &ring };
	std::string json;
	PackPerfWindow(&in, 1, 5.0f, nullptr, json);
	Json::Value root = Parse(json);
	EXPECT_EQ("a\"b\\c\n", root["units"][0]["unit"].asString());
	EXPECT_EQ("0x00000abc", root["units"][0]["modules"][0]["name"].asString());

	struct R
	{
		static const char* Resolve(uint32_t crc, char* buf, size_t size)
		{
			snprintf(buf, size, "Mod\"%u", crc);
			return buf;
		}
	};
	std::string json2;
	PackPerfWindow(&in, 1, 5.0f, &R::Resolve, json2);
	Json::Value root2 = Parse(json2);
	EXPECT_EQ("Mod\"2748", root2["units"][0]["modules"][0]["name"].asString());
}

TEST(CoarseRing, GetModuleCountReportsInitAndCap)
{
	CoarseRing ring;
	EXPECT_EQ(0u, ring.GetModuleCount());
	ring.Init(4, 5);
	EXPECT_EQ(5u, ring.GetModuleCount());
	CoarseRing big;
	big.Init(4, 1000);
	EXPECT_EQ(CoarseRing::kMaxModules, big.GetModuleCount());
}

TEST(PerfWindowPacker, FrameUnavailable)
{
	std::string json;
	PackPerfFrameUnavailable(json);
	EXPECT_EQ("{\"v\":1,\"available\":false,\"reason\":\"profiling off\"}", json);
}

TEST(PerfWindowPacker, FrameSectionsInOrder)
{
	PerfFrameSection sections[2] = { { "Phys\"ics", 1.5f }, { "Render", 2.25f } };
	std::string json;
	PackPerfFrameSections("Sim", 42, sections, 2, json);
	Json::Value root = Parse(json);
	EXPECT_TRUE(root["available"].asBool());
	EXPECT_EQ("Sim", root["unit"].asString());
	EXPECT_EQ(42u, root["frame"].asUInt64());
	ASSERT_EQ(2u, root["sections"].size());
	EXPECT_EQ("Phys\"ics", root["sections"][0]["name"].asString());
	EXPECT_FLOAT_EQ(1.5f, root["sections"][0]["ms"].asFloat());
	EXPECT_EQ("Render", root["sections"][1]["name"].asString());
}
