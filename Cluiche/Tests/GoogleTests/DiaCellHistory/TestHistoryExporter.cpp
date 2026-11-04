#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/ExportDecoder.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	const char* kEventEntry = "{\"key\":\"cow.drought\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"Drought\",\"description\":\"d\","
		"\"payload\":[{\"name\":\"weeks\",\"type\":\"int\",\"description\":\"d\"}]}";

	uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

	struct ExpFix
	{
		Rig r;
		TempFolder tmp;
		ExportOptions opt;
		ExportResult res;
		DecodedExport dec;

		// temp (float 0..100, tracked by schema or not), biome (category 3), region (category 3, grouping).
		void Setup(bool trackTemp = true, const RecorderBudgets* budgets = nullptr, bool startRecorder = true)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			r.grid.AddCategoryLayer("region", 0);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, trackTemp ? "{\"track\":true}" : nullptr), EnumEntry("biome", 3),
				EnumEntry("region", 3, "{\"group\":true}"), kEventEntry }));
			if (startRecorder) { r.Start(budgets); }
		}
		void Vary(uint32_t i)
		{
			for (uint32_t c = 0; c < 12; ++c)
			{
				r.grid.Set("temp", c, float((i * 7 + c * 3) % 101) + 0.37f);
				r.grid.Set("biome", c, float((c + i) % 3));
				r.grid.Set("region", c, float((c / 4 + i) % 3));
			}
			r.grid.BumpVersion("temp");
			r.grid.BumpVersion("biome");
			r.grid.BumpVersion("region");
		}
		void Run(uint32_t n)
		{
			for (uint32_t i = 0; i < n; ++i) { Vary(i); r.Take(); }
		}
		std::string Path(const char* name = "run.cell_history.json") const { return tmp.File(name); }
		bool Export(const char* name = "run.cell_history.json")
		{
			const bool ok = HistoryExporter::WriteFile(*r.rec, opt, Path(name).c_str(), res);
			return ok && dec.Load(Path(name).c_str());
		}
	};
}

TEST(HistoryExporter, EverySectionIsPresentAndWellFormed)
{
	ExpFix f;
	f.Setup();
	f.Run(6);
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	EXPECT_TRUE(f.res.ok);
	EXPECT_EQ(f.res.samples, 6u);
	EXPECT_GT(f.res.bytes, 0u);
	EXPECT_EQ(f.res.bytes, std::filesystem::file_size(f.Path()));
	const Json::Value& root = f.dec.root;
	EXPECT_EQ(root["format"].asString(), "dia.cell_history");
	EXPECT_EQ(root["version"].asInt(), 1);
	for (const char* k : { "header", "bands", "samples", "groups", "groupSizes", "aggregates", "events", "snapshots", "tracked", "gaps" })
	{
		EXPECT_TRUE(root.isMember(k)) << k;
	}
	EXPECT_TRUE(root["samples"].isArray());
	EXPECT_TRUE(root["groups"].isArray());
	EXPECT_TRUE(root["aggregates"].isObject());
	EXPECT_TRUE(root["gaps"]["aggregates"].isArray());
	EXPECT_TRUE(root["gaps"]["cells"].isArray());
	EXPECT_TRUE(root["gaps"]["events"].isArray());
	EXPECT_FALSE(root.isMember("cells"));
}

TEST(HistoryExporter, HeaderFieldsComeFromOptionsAndTheRecorder)
{
	ExpFix f;
	f.Setup(true, nullptr, false);
	f.r.cfg.mBands[0] = Dia::Core::Containers::String32("Day");
	f.r.cfg.mBands[1] = Dia::Core::Containers::String32("Week");
	f.r.cfg.mBandCount = 2;
	f.r.Start();
	f.Run(4);
	f.opt.app = "cow";
	f.opt.gitCommit = "8f88d1aa";
	f.opt.generatedAt = "2026-10-20T14:03:00Z";
	f.opt.world = "coast_range";
	ASSERT_TRUE(f.Export()) << f.res.error;
	const Json::Value& h = f.dec.Header();
	EXPECT_EQ(h["app"].asString(), "cow");
	EXPECT_EQ(h["git_commit"].asString(), "8f88d1aa");
	EXPECT_EQ(h["generated_at"].asString(), "2026-10-20T14:03:00Z");
	EXPECT_EQ(h["world"].asString(), "coast_range");
	EXPECT_EQ(h["sampleCount"].asUInt(), 4u);
	EXPECT_EQ(h["aggregateFirst"].asUInt(), 0u);
	EXPECT_EQ(h["cellFirst"].asUInt(), f.r.rec->FirstRetained());
	EXPECT_EQ(h["sampleFirst"].asUInt(), 0u);
	ASSERT_EQ(f.dec.root["bands"].size(), 2u);
	EXPECT_EQ(f.dec.root["bands"][0].asString(), "Day");
	EXPECT_EQ(f.dec.root["bands"][1].asString(), "Week");
}

TEST(HistoryExporter, SamplesListTickLabelAndBand)
{
	ExpFix f;
	f.Setup();
	f.Run(5);
	ASSERT_TRUE(f.Export());
	const Json::Value& s = f.dec.root["samples"];
	ASSERT_EQ(s.size(), 5u);
	for (uint32_t i = 0; i < 5; ++i)
	{
		const SampleHeader& h = f.r.rec->Header(i);
		EXPECT_EQ(s[i]["tick"].asUInt64(), h.mTick) << i;
		EXPECT_EQ(s[i]["label"].asString(), h.mLabel.AsCStr()) << i;
		EXPECT_EQ(s[i]["band"].asUInt(), unsigned(h.mBand)) << i;
	}
}

TEST(HistoryExporter, GroupsIncludeWorldAndOnePerGroupingValue)
{
	ExpFix f;
	f.Setup();
	f.Run(3);
	ASSERT_TRUE(f.Export());
	const Json::Value& g = f.dec.root["groups"];
	ASSERT_EQ(g.size(), 4u);
	EXPECT_EQ(g[0]["id"].asString(), "world");
	EXPECT_EQ(g[0]["label"].asString(), "World");
	EXPECT_EQ(g[1]["id"].asString(), "region=a");
	EXPECT_EQ(g[1]["layer"].asString(), "region");
	EXPECT_EQ(g[1]["value"].asString(), "a");
	EXPECT_EQ(g[1]["label"].asString(), "V");	// the schema enum label
	EXPECT_EQ(g[3]["id"].asString(), "region=c");
	for (uint32_t i = 0; i < g.size(); ++i) { EXPECT_TRUE(f.dec.root["groupSizes"].isMember(g[i]["id"].asString())) << i; }
}

TEST(HistoryExporter, GroupLabelsFallBackToIdsWithoutASchema)
{
	ExpFix f;
	f.Setup(true, nullptr, false);
	f.r.Start();
	f.Run(2);
	// A recorder with a null schema has no groups; the export still succeeds with just the world group.
	CellHistoryRecorder noSchema(f.r.registry, nullptr, f.r.cfg, 5);
	f.Vary(0);
	SampleInfo si; si.tick = 1; si.label = "x";
	noSchema.Sample(si);
	ExportResult res;
	ASSERT_TRUE(HistoryExporter::WriteFile(noSchema, f.opt, f.Path("ns.json").c_str(), res)) << res.error;
	DecodedExport d;
	ASSERT_TRUE(d.Load(f.Path("ns.json").c_str()));
	ASSERT_EQ(d.root["groups"].size(), 1u);
	EXPECT_EQ(d.root["groups"][0]["id"].asString(), "world");
}

TEST(HistoryExporter, AggregatesDecodeExactlyAgainstTheRecorder)
{
	ExpFix f;
	f.Setup();
	f.Run(9);
	ASSERT_TRUE(f.Export());
	const char* groups[] = { "world", "region=a", "region=b", "region=c" };
	const uint32_t n = f.r.rec->SampleCount();
	for (const char* g : groups)
	{
		std::vector<uint16_t> sizes;
		ASSERT_TRUE(f.dec.GroupSizes(g, sizes)) << g;
		ASSERT_EQ(sizes.size(), n);
		for (uint32_t s = 0; s < n; ++s)
		{
			uint16_t expect = 0;
			ASSERT_TRUE(f.r.rec->Aggregates().GroupSize(s, StringCRC(g), expect));
			EXPECT_EQ(sizes[s], expect) << g << " " << s;
		}

		uint32_t first = 99;
		std::vector<float> mn, mean, mx;
		ASSERT_TRUE(f.dec.FloatSeries("temp", g, "min", mn, first)) << g;
		ASSERT_TRUE(f.dec.FloatSeries("temp", g, "mean", mean, first));
		ASSERT_TRUE(f.dec.FloatSeries("temp", g, "max", mx, first));
		EXPECT_EQ(first, 0u);
		for (uint32_t s = 0; s < n; ++s)
		{
			AggregateStats st;
			ASSERT_TRUE(f.r.rec->Aggregate(s, StringCRC("temp"), StringCRC(g), st));
			EXPECT_EQ(Bits(mn[s]), Bits(st.mMin)) << g << " " << s;
			EXPECT_EQ(Bits(mean[s]), Bits(st.mMean)) << g << " " << s;
			EXPECT_EQ(Bits(mx[s]), Bits(st.mMax)) << g << " " << s;
		}

		std::vector<uint16_t> counts;
		uint32_t enumCount = 0;
		ASSERT_TRUE(f.dec.CategorySeries("biome", g, counts, first, enumCount)) << g;
		ASSERT_EQ(enumCount, 3u);
		for (uint32_t s = 0; s < n; ++s)
		{
			uint16_t expect[3] = {};
			ASSERT_TRUE(f.r.rec->CategoryCounts(s, StringCRC("biome"), StringCRC(g), expect, 3));
			for (uint32_t v = 0; v < 3; ++v) { EXPECT_EQ(counts[s * 3 + v], expect[v]) << g << " " << s << " " << v; }
		}
	}
	const Json::Value& biome = f.dec.root["aggregates"]["biome"];
	EXPECT_EQ(biome["kind"].asString(), "category");
	EXPECT_EQ(biome["values"][0].asString(), "a");
	EXPECT_EQ(biome["values"][2].asString(), "c");
	EXPECT_EQ(f.dec.root["aggregates"]["temp"]["kind"].asString(), "float");
}

TEST(HistoryExporter, EventsRoundTripWithPayloadTextGroupAndCell)
{
	ExpFix f;
	f.Setup();
	f.Run(2);
	HistoryEvent e;
	e.type = StringCRC("cow.drought");
	e.group.mLayer = StringCRC("region");
	e.group.mValue = 1;
	e.cell = 5;
	e.payloadJson = "{\"weeks\":4}";
	e.text = "4 dry weeks";
	f.r.rec->Emit(e);
	HistoryEvent e2;
	e2.type = StringCRC("cow.drought");
	f.r.rec->Emit(e2);
	f.Vary(2);
	f.r.Take();	// sample 2 carries both events
	ASSERT_TRUE(f.Export());
	const Json::Value& ev = f.dec.root["events"];
	ASSERT_EQ(ev.size(), 2u);
	EXPECT_EQ(ev[0]["sample"].asUInt(), 2u);
	EXPECT_EQ(ev[0]["type"].asString(), "cow.drought");
	EXPECT_EQ(ev[0]["group"].asString(), "region=b");
	EXPECT_EQ(ev[0]["cell"].asUInt(), 5u);
	EXPECT_EQ(ev[0]["payload"]["weeks"].asInt(), 4);
	EXPECT_EQ(ev[0]["text"].asString(), "4 dry weeks");
	EXPECT_TRUE(ev[1]["group"].isNull());
	EXPECT_TRUE(ev[1]["cell"].isNull());
	EXPECT_TRUE(ev[1]["payload"].isNull() || ev[1]["payload"].isObject());
}

TEST(HistoryExporter, UnresolvedEventTypeIsWrittenAsItsCrc)
{
	ExpFix f;
	f.Setup();
	f.Run(1);
	HistoryEvent e;
	e.type = StringCRC("not.in.schema");
	f.r.rec->Emit(e);
	f.Vary(1);
	f.r.Take();
	ASSERT_TRUE(f.Export());
	ASSERT_EQ(f.dec.root["events"].size(), 1u);
	char expect[24];
	snprintf(expect, sizeof(expect), "crc:%08x", StringCRC("not.in.schema").Value());
	EXPECT_EQ(f.dec.root["events"][0]["type"].asString(), expect);
}

TEST(HistoryExporter, LayersAppearingAfterEarlySamplesHaveAFirstAfterZero)
{
	ExpFix f;
	f.Setup(true, nullptr, false);
	// Three samples before any layer is registered: headers only.
	f.r.rec = std::make_unique<CellHistoryRecorder>(f.r.registry, f.r.schema.service.get(), f.r.cfg, 99);
	for (int i = 0; i < 3; ++i) { f.r.Take(); }
	RegisterWithKeys(f.r.grid, f.r.registry);
	f.Run(4);
	ASSERT_EQ(f.r.rec->SampleCount(), 7u);
	ASSERT_TRUE(f.Export());
	EXPECT_EQ(f.dec.AggregateFirst(), 3u);
	EXPECT_EQ(f.dec.root["samples"].size(), 7u);
	uint32_t first = 0;
	std::vector<float> mean;
	ASSERT_TRUE(f.dec.FloatSeries("temp", "world", "mean", mean, first));
	EXPECT_EQ(first, 3u);
	EXPECT_EQ(mean.size(), 4u);
	std::vector<uint16_t> sizes;
	ASSERT_TRUE(f.dec.GroupSizes("world", sizes));
	EXPECT_EQ(sizes.size(), 4u);
	EXPECT_EQ(sizes[0], 12u);
}

TEST(HistoryExporter, EvictedAggregateWindowGivesAFirstAndGaps)
{
	RecorderBudgets b;
	b.mAggregateBytes = AggregateRowBytes(1, 6, 4) * 5;	// 5 samples
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 1 << 20;
	ExpFix f;
	f.Setup(true, &b);
	f.Run(12);
	ASSERT_EQ(f.r.rec->AggregateFirstRetained(), 7u);
	ASSERT_TRUE(f.Export());
	EXPECT_EQ(f.dec.AggregateFirst(), 7u);
	EXPECT_EQ(f.dec.SampleCount(), 12u);
	ASSERT_GE(f.dec.root["gaps"]["aggregates"].size(), 1u);
	EXPECT_EQ(f.dec.root["gaps"]["aggregates"][0][0].asUInt(), 0u);
	EXPECT_EQ(f.dec.root["gaps"]["aggregates"][0][1].asUInt(), 6u);
	std::vector<float> mean;
	uint32_t first = 0;
	ASSERT_TRUE(f.dec.FloatSeries("temp", "world", "mean", mean, first));
	EXPECT_EQ(first, 7u);
	ASSERT_EQ(mean.size(), 5u);
	for (uint32_t s = 7; s < 12; ++s)
	{
		AggregateStats st;
		ASSERT_TRUE(f.r.rec->Aggregate(s, StringCRC("temp"), StringCRC("world"), st));
		EXPECT_EQ(Bits(mean[s - 7]), Bits(st.mMean));
	}
}

TEST(HistoryExporter, NoSamplesGivesAValidMinimalFile)
{
	ExpFix f;
	f.Setup();
	ASSERT_TRUE(f.Export()) << f.res.error;
	EXPECT_EQ(f.dec.SampleCount(), 0u);
	EXPECT_EQ(f.dec.root["samples"].size(), 0u);
	EXPECT_EQ(f.dec.root["events"].size(), 0u);
	EXPECT_EQ(f.dec.root["groups"].size(), 1u);
	EXPECT_EQ(f.res.samples, 0u);
}

TEST(HistoryExporter, EventsAreExportedWithEvictedRangeAsGap)
{
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 4 * 40;	// room for a few small events only
	ExpFix f;
	f.Setup(true, &b);
	for (uint32_t i = 0; i < 12; ++i)
	{
		HistoryEvent e;
		e.type = StringCRC("cow.drought");
		e.payloadJson = "{\"weeks\":1}";
		f.r.rec->Emit(e);
		f.Vary(i);
		f.r.Take();
	}
	ASSERT_GT(f.r.rec->EventsLog().GapCount(), 0u);
	ASSERT_TRUE(f.Export());
	EXPECT_EQ(f.dec.root["gaps"]["events"].size(), f.r.rec->EventsLog().GapCount());
	EXPECT_EQ(f.dec.root["events"].size(), f.r.rec->EventsLog().EventCount());
}

TEST(HistoryExporter, TrackedListsTheTrackedLayers)
{
	ExpFix f;
	f.Setup(true);
	f.Run(2);
	ASSERT_TRUE(f.Export());
	ASSERT_EQ(f.dec.root["tracked"].size(), 1u);
	EXPECT_EQ(f.dec.root["tracked"][0].asString(), "temp");
}

TEST(HistoryExporter, SnapshotsListComesFromTheManager)
{
	ExpFix f;
	f.Setup();
	f.Run(3);
	FakeSnapshotStore store;
	SnapshotManager mgr(*f.r.rec, f.r.registry, f.r.schema.service.get(), store);
	TakeResult tr;
	ASSERT_TRUE(mgr.Take("alpha", false, tr));
	store.RunFrameBoundary();
	f.opt.snapshots = &mgr;
	ASSERT_TRUE(f.Export());
	ASSERT_EQ(f.dec.root["snapshots"].size(), 1u);
	EXPECT_EQ(f.dec.root["snapshots"][0]["name"].asString(), "alpha");
	EXPECT_EQ(f.dec.root["snapshots"][0]["sample"].asUInt(), 2u);
	EXPECT_TRUE(f.dec.root["snapshots"][0].isMember("label"));
}

TEST(HistoryExporter, WriteFailureReturnsFalseWithAMessage)
{
	ExpFix f;
	f.Setup();
	f.Run(2);
	const std::string bad = f.tmp.File("no_such_folder/run.json");
	EXPECT_FALSE(HistoryExporter::WriteFile(*f.r.rec, f.opt, bad.c_str(), f.res));
	EXPECT_FALSE(f.res.ok);
	EXPECT_GT(std::strlen(f.res.error), 0u);
	EXPECT_FALSE(HistoryExporter::WriteFile(*f.r.rec, f.opt, "", f.res));
	EXPECT_FALSE(HistoryExporter::WriteFile(*f.r.rec, f.opt, nullptr, f.res));
}

// ---- --cells (task 5.3) ----

TEST(HistoryExporterCells, SectionIsAbsentWithoutTheOption)
{
	ExpFix f;
	f.Setup();
	f.Run(4);
	ASSERT_TRUE(f.Export());
	EXPECT_FALSE(f.dec.root.isMember("cells"));
}

TEST(HistoryExporterCells, TrackedLayerDataMatchesTheRingWithinOneQuantisationStep)
{
	ExpFix f;
	f.Setup(true);
	f.Run(6);
	f.opt.cells = true;
	ASSERT_TRUE(f.Export());
	const Json::Value& c = f.dec.root["cells"];
	ASSERT_TRUE(c.isObject());
	ASSERT_TRUE(c.isMember("temp"));
	EXPECT_EQ(c["temp"]["kind"].asString(), "float");
	EXPECT_EQ(c["temp"]["width"].asUInt(), 4u);
	EXPECT_EQ(c["temp"]["height"].asUInt(), 3u);
	EXPECT_DOUBLE_EQ(c["temp"]["min"].asDouble(), 0.0);
	EXPECT_DOUBLE_EQ(c["temp"]["max"].asDouble(), 100.0);
	EXPECT_EQ(c["temp"]["first"].asUInt(), 0u);
	EXPECT_EQ(c["temp"]["count"].asUInt(), 6u);
	EXPECT_EQ(c["temp"]["data"].size(), 6u);
	EXPECT_EQ(f.dec.CellFirst(), 0u);
	for (uint32_t s = 0; s < 6; ++s)
	{
		std::vector<float> got;
		ASSERT_TRUE(f.dec.CellValues("temp", s, got)) << s;
		float expect[12];
		ASSERT_TRUE(f.r.rec->CellValues(s, StringCRC("temp"), expect, 12));
		for (uint32_t i = 0; i < 12; ++i) { EXPECT_NEAR(got[i], expect[i], 100.0f / 65535.0f) << s << " " << i; }
	}
}

TEST(HistoryExporterCells, ExportedCodesAreTheRingsRawCodes)
{
	// The ring's dequantise -> requantise round trip is exact, so the exported codes equal what the ring stores.
	ExpFix f;
	f.Setup(true);
	f.Run(5);
	f.r.grid.Set("temp", 0, std::numeric_limits<float>::quiet_NaN());
	f.r.grid.BumpVersion("temp");
	f.r.Take();
	f.opt.cells = true;
	ASSERT_TRUE(f.Export());
	for (uint32_t s = 0; s < 6; ++s)
	{
		std::vector<uint16_t> codes;
		ASSERT_TRUE(f.dec.CellCodes("temp", s, codes));
		float v[12];
		ASSERT_TRUE(f.r.rec->CellValues(s, StringCRC("temp"), v, 12));
		for (uint32_t i = 0; i < 12; ++i) { EXPECT_EQ(codes[i], QuantiseCellFloat(v[i], 0.f, 100.f)) << s << " " << i; }
	}
	std::vector<uint16_t> last;
	ASSERT_TRUE(f.dec.CellCodes("temp", 5, last));
	EXPECT_EQ(last[0], 0xFFFFu);	// NaN code survives
}

TEST(HistoryExporterCells, LayerTrackedMidRunStartsLate)
{
	ExpFix f;
	f.Setup(false);
	f.Run(3);
	CellHistoryRecorder::TrackResult tr;
	const StringCRC temp("temp");
	ASSERT_TRUE(f.r.rec->Track(&temp, 1, tr)) << tr.mError;
	f.Run(4);
	f.opt.cells = true;
	ASSERT_TRUE(f.Export());
	const Json::Value& t = f.dec.root["cells"]["temp"];
	EXPECT_EQ(t["first"].asUInt(), 3u);
	EXPECT_EQ(t["count"].asUInt(), 4u);
	EXPECT_EQ(t["data"].size(), 4u);
	EXPECT_EQ(f.dec.CellFirst(), 3u);
}

TEST(HistoryExporterCells, EvictedRangeIsAGapAndFirstMovesOn)
{
	RecorderBudgets b;
	b.mCellBytes = 24 * 5;	// temp: 12 cells x 2 bytes = 24 bytes per sample, window 5
	b.mAggregateBytes = 1 << 20;
	b.mEventBytes = 1 << 20;
	ExpFix f;
	f.Setup(true, &b);
	f.Run(12);
	ASSERT_EQ(f.r.rec->FirstRetained(), 7u);
	f.opt.cells = true;
	ASSERT_TRUE(f.Export());
	const Json::Value& t = f.dec.root["cells"]["temp"];
	EXPECT_EQ(t["first"].asUInt(), 7u);
	EXPECT_EQ(t["count"].asUInt(), 5u);
	ASSERT_GE(f.dec.root["gaps"]["cells"].size(), 1u);
	EXPECT_EQ(f.dec.root["gaps"]["cells"][0][0].asUInt(), 0u);
	EXPECT_EQ(f.dec.root["gaps"]["cells"][0][1].asUInt(), 6u);
	EXPECT_EQ(f.dec.CellFirst(), 7u);
	std::vector<uint16_t> codes;
	EXPECT_FALSE(f.dec.CellCodes("temp", 6, codes));
	EXPECT_TRUE(f.dec.CellCodes("temp", 11, codes));
}

TEST(HistoryExporterCells, UntrackedLayersAreAbsentAndCategoryLayersUseBytes)
{
	ExpFix f;
	f.Setup(true);
	f.Run(2);
	CellHistoryRecorder::TrackResult tr;
	const StringCRC biome("biome");
	ASSERT_TRUE(f.r.rec->Track(&biome, 1, tr)) << tr.mError;
	f.Run(3);
	f.opt.cells = true;
	ASSERT_TRUE(f.Export());
	const Json::Value& c = f.dec.root["cells"];
	EXPECT_TRUE(c.isMember("temp"));
	EXPECT_TRUE(c.isMember("biome"));
	EXPECT_FALSE(c.isMember("region"));
	EXPECT_EQ(c["biome"]["kind"].asString(), "category");
	EXPECT_EQ(c["biome"]["first"].asUInt(), 2u);
	EXPECT_EQ(c["biome"]["count"].asUInt(), 3u);
	// u8 payload: 12 cells -> 12 bytes -> 16 base64 chars.
	EXPECT_EQ(c["biome"]["data"][0].asString().size(), 16u);
	for (uint32_t s = 2; s < 5; ++s)
	{
		std::vector<float> got;
		ASSERT_TRUE(f.dec.CellValues("biome", s, got));
		float expect[12];
		ASSERT_TRUE(f.r.rec->CellValues(s, biome, expect, 12));
		for (uint32_t i = 0; i < 12; ++i) { EXPECT_EQ(got[i], expect[i]) << s << " " << i; }
	}
}

// ---- size (AC 1) ----

TEST(HistoryExporterSize, FortyYearWeeklyTwentyLayersTwentySevenGroupsIsUnderTwentyMegabytes)
{
	ExpFix f;
	std::vector<std::string> entries;
	char name[16];
	for (int i = 0; i < 20; ++i)
	{
		snprintf(name, sizeof(name), "l%02d", i);
		f.r.grid.AddFloatLayer(name, 1.0f);
		entries.push_back(FloatEntry(name, 0, 1000));
	}
	f.r.grid.AddCategoryLayer("grp", 0);
	entries.push_back(EnumEntry("grp", 26, "{\"group\":true,\"aggregate\":false}"));	// 26 values + world = 27 groups
	ASSERT_TRUE(f.r.schema.Load(entries));
	f.r.Start();
	const uint32_t samples = 2080;
	for (uint32_t i = 0; i < samples; ++i)
	{
		for (int l = 0; l < 20; ++l)
		{
			snprintf(name, sizeof(name), "l%02d", l);
			for (uint32_t c = 0; c < 12; ++c) { f.r.grid.Set(name, c, float((i * 31 + l * 17 + c * 5) % 997) + 0.123f * float(c)); }
			f.r.grid.BumpVersion(name);
		}
		for (uint32_t c = 0; c < 12; ++c) { f.r.grid.Set("grp", c, float((c + i / 50) % 26)); }
		f.r.grid.BumpVersion("grp");
		f.r.Take();
	}
	ASSERT_EQ(f.r.rec->GroupCount(), 27u);
	ASSERT_EQ(f.r.rec->RecordedLayerCount(), 20u);
	const auto t0 = std::chrono::steady_clock::now();
	ASSERT_TRUE(f.Export()) << f.res.error;
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	const double perCell = double(f.res.bytes) / (double(samples) * 20 * 27);
	printf("[ size ] 2080 samples x 20 layers x 27 groups: %llu bytes (%.2f MB), %.2f bytes per sample-layer-group, export+load %.0f ms\n",
		(unsigned long long)f.res.bytes, double(f.res.bytes) / 1048576.0, perCell, ms);
	EXPECT_LT(f.res.bytes, 20ull * 1000 * 1000);
	EXPECT_LT(perCell, 20.0);	// 12 raw bytes -> 16 base64 chars, plus group sizes and JSON framing
}
