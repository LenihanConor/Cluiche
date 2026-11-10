#include <cstring>
#include <gtest/gtest.h>
#include <DiaValueSchema/RampLegend.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <json/json.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Shared ramp conformance fixtures (Feature 5 task 5), data under Dia/DiaValueSchema/Testing/Conformance/ramps,
// located by walking up from this source file. Each file carries its own expectations for the TypeScript side.

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::MakeValueInfo;
namespace fs = std::filesystem;

namespace
{
	fs::path FindRoot()
	{
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		while (!dir.empty())
		{
			if (fs::exists(dir / "Dia" / "DiaValueSchema" / "Data" / "ramps.json", ec)) { return dir / "Dia" / "DiaValueSchema"; }
			const fs::path parent = dir.parent_path();
			if (parent == dir) { break; }
			dir = parent;
		}
		return fs::path();
	}

	bool ReadFile(const fs::path& p, std::string& out)
	{
		std::ifstream in(p, std::ios::binary);
		if (!in) { return false; }
		std::stringstream ss;
		ss << in.rdbuf();
		out = ss.str();
		return true;
	}

	bool LoadJson(const fs::path& p, Json::Value& out, std::string& err)
	{
		std::string text;
		if (!ReadFile(p, text)) { err = "cannot read " + p.string(); return false; }
		Json::Reader reader;
		if (!reader.parse(text, out)) { err = p.string() + ": " + reader.getFormattedErrorMessages(); return false; }
		return true;
	}

	std::string Hex(uint32_t rgba8)
	{
		char buf[16];
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", (rgba8 >> 24) & 0xFF, (rgba8 >> 16) & 0xFF, (rgba8 >> 8) & 0xFF);
		return buf;
	}

	unsigned Channel(const std::string& hex, int idx) { return static_cast<unsigned>(std::stoul(hex.substr(1 + idx * 2, 2), nullptr, 16)); }

	// Independent straightforward reference: double-precision lerp, round half up.
	std::string ReferenceSample(const Json::Value& ramp, double t)
	{
		if (std::isnan(t) || t < 0.0) { t = 0.0; }
		if (t > 1.0) { t = 1.0; }
		const Json::Value& stops = ramp["stops"];
		const Json::ArrayIndex n = stops.size();
		unsigned c[3];
		if (t <= stops[0u]["t"].asDouble())
		{
			for (int k = 0; k < 3; ++k) { c[k] = Channel(stops[0u]["colour"].asString(), k); }
		}
		else if (t >= stops[n - 1]["t"].asDouble())
		{
			for (int k = 0; k < 3; ++k) { c[k] = Channel(stops[n - 1]["colour"].asString(), k); }
		}
		else
		{
			for (Json::ArrayIndex i = 1; i < n; ++i)
			{
				const double hiT = stops[i]["t"].asDouble();
				if (t <= hiT)
				{
					const double loT = stops[i - 1]["t"].asDouble();
					const double f = (t - loT) / (hiT - loT);
					for (int k = 0; k < 3; ++k)
					{
						const double a = Channel(stops[i - 1]["colour"].asString(), k);
						const double b = Channel(stops[i]["colour"].asString(), k);
						c[k] = static_cast<unsigned>(std::floor(a + (b - a) * f + 0.5));
					}
					break;
				}
			}
		}
		char buf[16];
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c[0], c[1], c[2]);
		return buf;
	}

	struct Fixture
	{
		ValueSchemaRegistry	mSchema;
		RampRegistry		mRamps;
		ValueSchemaService*	mService = nullptr;

		explicit Fixture(const std::string& rampsFileText)
		{
			RampLoadResult r = RampLoader::LoadFromText(rampsFileText.c_str(), "ramps.json");
			EXPECT_TRUE(r.mOk);
			for (unsigned i = 0; i < r.mRamps.Size(); ++i) { mRamps.Register(r.mRamps[i], true, "ramps.json"); }
			mSchema.Publish();
			mRamps.Publish();
			mService = new ValueSchemaService(mSchema, &mRamps);
		}
		~Fixture() { delete mService; }
	};

	Json::Value ShippedRamps(const fs::path& root)
	{
		Json::Value v; std::string err;
		EXPECT_TRUE(LoadJson(root / "Data" / "ramps.json", v, err)) << err;
		return v["ramps"];
	}

	std::string Compact(const Json::Value& v) { Json::FastWriter w; return w.write(v); }
}

TEST(DiaValueSchemaRampConformance, SamplesMatchFixtureAndReference)
{
	const fs::path root = FindRoot();
	ASSERT_FALSE(root.empty()) << "could not locate Dia/DiaValueSchema from " << __FILE__;
	Json::Value doc; std::string err;
	ASSERT_TRUE(LoadJson(root / "Testing" / "Conformance" / "ramps" / "samples.json", doc, err)) << err;
	EXPECT_EQ("dia.ramps.conformance.samples", doc["format"].asString());
	EXPECT_EQ(1, doc["version"].asInt());

	// The embedded ramps are a verbatim copy of the shipped file.
	EXPECT_EQ(Compact(ShippedRamps(root)), Compact(doc["ramps"])) << "samples.json ramps drifted from Data/ramps.json";

	Json::Value file;
	file["format"] = "dia.ramps";
	file["version"] = 1;
	file["ramps"] = doc["ramps"];
	Fixture f(Compact(file));

	ASSERT_TRUE(doc["samples"].isArray());
	ASSERT_GE(doc["samples"].size(), 7u * 8u);
	for (const Json::Value& s : doc["samples"])
	{
		const std::string id = s["ramp"].asString();
		const double t = s["t"].asDouble();
		const std::string expected = s["expectedHex"].asString();
		SCOPED_TRACE(id + " t=" + std::to_string(t));

		const RampData* ramp = f.mService->FindRamp(Dia::Core::StringCRC(id.c_str()));
		ASSERT_NE(nullptr, ramp);
		EXPECT_EQ(expected, Hex(SampleRamp(*ramp, static_cast<float>(t))));
		EXPECT_EQ(0xFFu, SampleRamp(*ramp, static_cast<float>(t)) & 0xFFu);

		for (const Json::Value& r : doc["ramps"])
		{
			if (r["id"].asString() == id) { EXPECT_EQ(expected, ReferenceSample(r, t)) << "fixture disagrees with reference"; }
		}
	}
}

TEST(DiaValueSchemaRampConformance, LegendTicksMatchFixture)
{
	const fs::path root = FindRoot();
	ASSERT_FALSE(root.empty());
	Json::Value doc; std::string err;
	ASSERT_TRUE(LoadJson(root / "Testing" / "Conformance" / "ramps" / "legend-ticks.json", doc, err)) << err;
	EXPECT_EQ("dia.ramps.conformance.legend_ticks", doc["format"].asString());
	EXPECT_EQ(1, doc["version"].asInt());

	std::string rampsText;
	ASSERT_TRUE(ReadFile(root / "Data" / "ramps.json", rampsText));
	Fixture f(rampsText);
	RampResolver res(*f.mService);

	ASSERT_TRUE(doc["cases"].isArray());
	ASSERT_GE(doc["cases"].size(), 11u);
	for (const Json::Value& c : doc["cases"])
	{
		ValueInfo info = MakeValueInfo("test", "k");
		info.mFullKey = "test/k";
		info.mHasMin = true; info.mHasMax = true;
		info.mMin = c["min"].asDouble();
		info.mMax = c["max"].asDouble();
		// raw copy: String32(const char*) is printf-style, '%' would be a format
		if (c.isMember("unit")) { info.mUnit = Dia::Core::Containers::String32("%s", c["unit"].asCString()); }
		SCOPED_TRACE(Compact(c));

		const LegendData l = BuildLegend(res, info, false, 0, 0);
		const Json::Value& ticks = c["expectedTicks"];
		ASSERT_EQ(ticks.size(), l.mTickCount);
		for (Json::ArrayIndex i = 0; i < ticks.size(); ++i)
		{
			const double e = ticks[i].asDouble();
			EXPECT_NEAR(e, l.mTicks[i], std::fabs(e) * 1e-6 + 1e-12) << "tick " << i;
		}
		EXPECT_EQ(c["expectedUnit"].asString(), std::string(l.mUnit.AsCStr()));
	}
}

TEST(DiaValueSchemaRampConformance, PalettesMatchFixture)
{
	const fs::path root = FindRoot();
	ASSERT_FALSE(root.empty());
	Json::Value doc; std::string err;
	ASSERT_TRUE(LoadJson(root / "Testing" / "Conformance" / "ramps" / "palettes.json", doc, err)) << err;
	EXPECT_EQ("dia.ramps.conformance.palettes", doc["format"].asString());
	EXPECT_EQ(1, doc["version"].asInt());

	// categorical matches the shipped categorical.12
	const Json::Value shipped = ShippedRamps(root);
	for (const Json::Value& r : shipped)
	{
		if (r["id"].asString() == "categorical.12") { EXPECT_EQ(Compact(r["colours"]), Compact(doc["categorical"])); }
	}

	std::string rampsText;
	ASSERT_TRUE(ReadFile(root / "Data" / "ramps.json", rampsText));
	Fixture f(rampsText);

	ASSERT_TRUE(doc["cases"].isArray());
	ASSERT_GE(doc["cases"].size(), 5u);
	for (const Json::Value& c : doc["cases"])
	{
		SCOPED_TRACE(c["name"].asString());
		ValueInfo info = MakeValueInfo("test", "e");
		info.mFullKey = (std::string("test/") + c["name"].asString()).c_str();
		info.mType = ValueType::Enum;
		const Json::Value& values = c["values"];
		info.mEnumValues.Reserve(values.size());
		for (const Json::Value& v : values)
		{
			EnumValue ev;
			ev.mId = v["id"].asCString();
			ev.mLabel = v["label"].asCString();
			if (v.isMember("colour")) { ev.mColour = v["colour"].asCString(); ev.mHasColour = true; }
			info.mEnumValues.Add(ev);
		}

		RampResolver res(*f.mService);
		const ResolvedPalette pal = res.ResolvePalette(info);
		const Json::Value& expected = c["expectedColours"];
		ASSERT_EQ(expected.size(), pal.mEntries.Size());
		for (Json::ArrayIndex i = 0; i < expected.size(); ++i)
		{
			EXPECT_EQ(expected[i].asString(), Hex(pal.mEntries[i].mRgba8)) << "value " << i;
		}
		EXPECT_EQ(c["expectedWrapped"].asBool(), pal.mWrapped);

		// BuildLegend swatches use the same colours.
		const LegendData l = BuildLegend(res, info, false, 0, 0);
		ASSERT_EQ(expected.size(), l.mSwatches.Size());
		for (Json::ArrayIndex i = 0; i < expected.size(); ++i)
		{
			EXPECT_EQ(expected[i].asString(), Hex(l.mSwatches[i].mRgba8)) << "swatch " << i;
		}
	}
}
