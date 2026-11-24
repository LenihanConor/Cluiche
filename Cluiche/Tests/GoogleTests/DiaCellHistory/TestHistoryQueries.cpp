#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/HistoryRows.h>
#include <DiaCellHistory/QueryObservation.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/ExportDecoder.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <json/json.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	const char* const kMeta = "cells.history.meta";
	const char* const kAgg = "cells.history.aggregates";
	const char* const kEvents = "cells.history.events";
	const char* const kCells = "cells.history.cells";
	const uint32_t kHostLimit = 16384;	// kSimPayloadBytes

	const char* kEventEntry = "{\"key\":\"cow.drought\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"Drought\",\"description\":\"d\","
		"\"payload\":[{\"name\":\"weeks\",\"type\":\"int\",\"description\":\"d\"}]}";

	uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }
	bool ErrorHas(const Json::Value& v, const char* text) { return IsError(v) && v["error"].asString().find(text) != std::string::npos; }
	uint64_t C(const char* n) { auto* c = MetricRegistry::Instance().FindCounter(StringCRC(n)); return c ? c->Value() : 0; }

	// Compact serialised size: the same measure the queries use.
	size_t Size(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v).size();
	}

	// Compares two values after a text round trip, so an integer built as UInt equals the same integer parsed from a file.
	bool JsonEq(const Json::Value& a, const Json::Value& b)
	{
		Json::StreamWriterBuilder w;
		w["indentation"] = "";
		Json::Value na, nb;
		Json::Reader r;
		return r.parse(Json::writeString(w, a), na, false) && r.parse(Json::writeString(w, b), nb, false) && na == nb;
	}

	Json::Value Obj() { return Json::Value(Json::objectValue); }
	Json::Value StrList(std::initializer_list<const char*> items)
	{
		Json::Value a(Json::arrayValue);
		for (const char* s : items) { a.append(s); }
		return a;
	}

	struct QFix
	{
		Rig r;
		explicit QFix(uint16_t width = 4, uint16_t height = 3) : r(width, height) {}
		TempFolder tmp;
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryQueries> q;
		ExportOptions opt;
		ExportResult res;
		DecodedExport dec;

		// temp (float 0..100, tracked), biome (category 3), region (category 3, grouping), one event type.
		void Setup(const RecorderBudgets* budgets = nullptr, bool trackTemp = true)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			r.grid.AddCategoryLayer("region", 0);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, trackTemp ? "{\"track\":true}" : nullptr), EnumEntry("biome", 3),
				EnumEntry("region", 3, "{\"group\":true}"), kEventEntry }));
			r.Start(budgets);
			q = std::make_unique<HistoryQueries>(*r.rec);
			ASSERT_TRUE(q->Register(registrar));
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
		// Sample i emits (i % 4) + 1 events; sample 2 emits 7 (so a cursor can resume mid-sample).
		void Run(uint32_t n, bool events = true)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				if (events)
				{
					const uint32_t k = (i == 2) ? 7u : (i % 4) + 1u;
					for (uint32_t j = 0; j < k; ++j)
					{
						HistoryEvent e;
						e.type = StringCRC("cow.drought");
						const std::string payload = "{\"weeks\":" + std::to_string(i * 10 + j) + "}";
						const std::string text = "t" + std::to_string(i) + "_" + std::to_string(j);
						e.payloadJson = payload.c_str();
						e.text = text.c_str();
						if (j % 2 == 0) { e.group.mLayer = StringCRC("region"); e.group.mValue = j % 3; }
						if (j % 3 == 0) { e.cell = (i + j) % 12; }
						r.rec->Emit(e);
					}
				}
				Vary(i);
				r.Take();
			}
		}
		Json::Value Call(const char* name, const Json::Value& args = Json::Value(Json::objectValue)) { return registrar.Execute(name, args); }
		bool Export(bool cells = false)
		{
			opt.cells = cells;
			opt.overwrite = true;
			const std::string path = tmp.File("run.cell_history.json");
			return HistoryExporter::WriteFile(*r.rec, opt, path.c_str(), res) && dec.Load(path.c_str());
		}
		void Budget(uint32_t bytes) { q->SetMaxReplyBytes(bytes); }
	};

	// ---- paging helpers ------------------------------------------------------------------------------------------

	struct Pages
	{
		std::vector<Json::Value> replies;
		size_t maxBytes = 0;
		bool failed = false;
		std::string error;
	};

	Pages PageAggregates(QFix& f, Json::Value args)
	{
		Pages p;
		for (int guard = 0; guard < 100000; ++guard)
		{
			const Json::Value rep = f.Call(kAgg, args);
			if (IsError(rep)) { p.failed = true; p.error = rep["error"].asString(); return p; }
			p.maxBytes = std::max(p.maxBytes, Size(rep));
			p.replies.push_back(rep);
			if (!rep.isMember("next")) { return p; }
			args["from"] = rep["next"]["from"];
		}
		p.failed = true;
		p.error = "runaway paging";
		return p;
	}

	Pages PageEvents(QFix& f, Json::Value args)
	{
		Pages p;
		for (int guard = 0; guard < 100000; ++guard)
		{
			const Json::Value rep = f.Call(kEvents, args);
			if (IsError(rep)) { p.failed = true; p.error = rep["error"].asString(); return p; }
			p.maxBytes = std::max(p.maxBytes, Size(rep));
			p.replies.push_back(rep);
			if (!rep.isMember("next")) { return p; }
			args["from"] = rep["next"]["from"];
			args["skip"] = rep["next"]["skip"];
		}
		p.failed = true;
		p.error = "runaway paging";
		return p;
	}

	Pages PageCells(QFix& f, Json::Value args)
	{
		Pages p;
		for (int guard = 0; guard < 100000; ++guard)
		{
			const Json::Value rep = f.Call(kCells, args);
			if (IsError(rep)) { p.failed = true; p.error = rep["error"].asString(); return p; }
			p.maxBytes = std::max(p.maxBytes, Size(rep));
			p.replies.push_back(rep);
			if (!rep.isMember("next")) { return p; }
			args["from"] = rep["next"]["from"];
			args["cellOffset"] = rep["next"]["cellOffset"];
		}
		p.failed = true;
		p.error = "runaway paging";
		return p;
	}

	// Smallest budget at which `countAt(budget)` reaches `target`; -1 when none up to maxBudget.
	template <class F>
	int SmallestBudgetFor(F countAt, int target, int lo, int hi)
	{
		for (int b = lo; b <= hi; ++b) { if (countAt(uint32_t(b)) >= target) { return b; } }
		return -1;
	}

	// Reassembled aggregate data keyed "layer|group|which" (floats) or "layer|group" (category counts), plus group sizes and samples.
	struct AggAssembly
	{
		uint32_t from = 0, total = 0;
		std::map<std::string, std::vector<uint32_t>> f32bits;
		std::map<std::string, std::vector<uint16_t>> u16;
		std::map<std::string, std::vector<uint16_t>> sizes;
		std::vector<Json::Value> samples;
		std::map<std::string, Json::Value> layerMeta;
	};

	void Assemble(const std::vector<Json::Value>& pages, AggAssembly& out)
	{
		bool firstPage = true;
		for (const Json::Value& p : pages)
		{
			const uint32_t n = p["count"].asUInt();
			if (firstPage) { out.from = p["from"].asUInt(); firstPage = false; }
			out.total += n;
			for (const Json::Value& s : p["samples"]) { out.samples.push_back(s); }
			for (const std::string& g : p["groupSizes"].getMemberNames())
			{
				std::vector<uint16_t> v;
				ASSERT_TRUE(DecodedExport::DecodeU16(p["groupSizes"][g], n, v));
				out.sizes[g].insert(out.sizes[g].end(), v.begin(), v.end());
			}
			for (const std::string& layer : p["rows"].getMemberNames())
			{
				const Json::Value& l = p["rows"][layer];
				out.layerMeta[layer] = l;
				for (const std::string& g : l["groups"].getMemberNames())
				{
					const Json::Value& gv = l["groups"][g];
					if (l["kind"].asString() == "float")
					{
						for (const char* which : { "min", "mean", "max" })
						{
							std::vector<float> v;
							ASSERT_TRUE(DecodedExport::DecodeF32(gv[which], n, v));
							std::vector<uint32_t>& dst = out.f32bits[layer + "|" + g + "|" + which];
							for (float x : v) { dst.push_back(Bits(x)); }
						}
					}
					else
					{
						std::vector<uint16_t> v;
						ASSERT_TRUE(DecodedExport::DecodeU16(gv, n * l["values"].size(), v));
						std::vector<uint16_t>& dst = out.u16[layer + "|" + g];
						dst.insert(dst.end(), v.begin(), v.end());
					}
				}
			}
		}
	}

	// Compares the assembled pages with the file's aggregates, group sizes and sample headers.
	void ExpectAssemblyEqualsFile(const AggAssembly& a, const DecodedExport& d)
	{
		EXPECT_EQ(a.from, d.AggregateFirst());
		EXPECT_EQ(a.from + a.total, d.SampleCount());
		for (const std::string& g : d.root["groupSizes"].getMemberNames())
		{
			std::vector<uint16_t> file;
			ASSERT_TRUE(d.GroupSizes(g.c_str(), file));
			ASSERT_TRUE(a.sizes.count(g)) << g;
			EXPECT_EQ(a.sizes.at(g), file) << g;
		}
		for (const std::string& layer : d.root["aggregates"].getMemberNames())
		{
			const Json::Value& fl = d.root["aggregates"][layer];
			ASSERT_TRUE(a.layerMeta.count(layer)) << layer;
			EXPECT_TRUE(JsonEq(a.layerMeta.at(layer)["kind"], fl["kind"])) << layer;
			EXPECT_TRUE(JsonEq(a.layerMeta.at(layer)["first"], fl["first"])) << layer;
			if (fl.isMember("values")) { EXPECT_TRUE(JsonEq(a.layerMeta.at(layer)["values"], fl["values"])) << layer; }
			const uint32_t first = fl["first"].asUInt();
			for (const std::string& g : fl["groups"].getMemberNames())
			{
				if (fl["kind"].asString() == "float")
				{
					for (const char* which : { "min", "mean", "max" })
					{
						std::vector<float> file;
						uint32_t ff = 0;
						ASSERT_TRUE(d.FloatSeries(layer.c_str(), g.c_str(), which, file, ff));
						const std::vector<uint32_t>& got = a.f32bits.at(layer + "|" + g + "|" + which);
						ASSERT_EQ(got.size(), a.total);
						for (size_t i = 0; i < file.size(); ++i) { ASSERT_EQ(got[first - a.from + i], Bits(file[i])) << layer << " " << g << " " << which << " " << i; }
					}
				}
				else
				{
					std::vector<uint16_t> file;
					uint32_t ff = 0, ec = 0;
					ASSERT_TRUE(d.CategorySeries(layer.c_str(), g.c_str(), file, ff, ec));
					const std::vector<uint16_t>& got = a.u16.at(layer + "|" + g);
					ASSERT_EQ(got.size(), size_t(a.total) * ec);
					for (size_t i = 0; i < file.size(); ++i) { ASSERT_EQ(got[size_t(first - a.from) * ec + i], file[i]) << layer << " " << g << " " << i; }
				}
			}
		}
		const Json::Value& fs = d.root["samples"];
		const uint32_t sf = d.Header()["sampleFirst"].asUInt();
		for (uint32_t s = d.AggregateFirst(); s < d.SampleCount(); ++s)
		{
			ASSERT_GE(s, sf);
			const Json::Value& want = fs[s - sf];
			const Json::Value& got = a.samples[s - a.from];
			EXPECT_TRUE(JsonEq(got["tick"], want["tick"])) << s;
			EXPECT_TRUE(JsonEq(got["label"], want["label"])) << s;
			EXPECT_TRUE(JsonEq(got["band"], want["band"])) << s;
		}
	}

	// Cells reassembly: sample -> full column vector (codes widened to uint16), following row-major cursors.
	void AssembleCells(const std::vector<Json::Value>& pages, std::map<uint32_t, std::vector<uint16_t>>& rows, uint32_t cellTotal)
	{
		for (const Json::Value& p : pages)
		{
			const uint32_t from = p["from"].asUInt();
			const uint32_t to = p["to"].asUInt();
			const uint32_t off = p["cellOffset"].asUInt();
			const uint32_t cnt = p["cellCount"].asUInt();
			ASSERT_EQ(p["data"].size(), to - from);
			for (uint32_t i = 0; i < to - from; ++i)
			{
				std::vector<uint16_t> v;
				if (p["kind"].asString() == "float") { ASSERT_TRUE(DecodedExport::DecodeU16(p["data"][i], cnt, v)); }
				else
				{
					std::vector<uint8_t> b;
					ASSERT_TRUE(DecodedExport::DecodeU8(p["data"][i], cnt, b));
					v.assign(b.begin(), b.end());
				}
				std::vector<uint16_t>& row = rows[from + i];
				row.resize(cellTotal);
				for (uint32_t c = 0; c < cnt; ++c) { row[off + c] = v[c]; }
			}
		}
	}
}

// ---- registration ---------------------------------------------------------------------------------------------------

TEST(HistoryQueries, RegistersFourQueriesWithHelp)
{
	QFix f; f.Setup();
	EXPECT_EQ(f.registrar.Count(), 4u);
	EXPECT_EQ(HistoryQueries::kNameCount, 4u);
	for (const char* n : { kMeta, kAgg, kEvents, kCells })
	{
		ASSERT_TRUE(f.registrar.Has(n)) << n;
		EXPECT_FALSE(f.registrar.IsCommand(n)) << n;
		const Dia::DebugServer::SimHelpStored* help = f.registrar.Help(n);
		ASSERT_NE(help, nullptr);
		EXPECT_GT(strlen(help->description), 0u);
		EXPECT_LE(strlen(help->description), 256u);
		EXPECT_STREQ(help->owner, "DiaCellHistory");
		EXPECT_STREQ(help->category, "cells.history");
		EXPECT_GE(help->paramCount, 2u);
	}
	EXPECT_EQ(f.registrar.Help(kCells)->paramCount, 5u);
	EXPECT_TRUE(f.registrar.Help(kCells)->params[0].required);
}

TEST(HistoryQueries, UnregisterRemovesAndRegisterIsIdempotent)
{
	QFix f; f.Setup();
	EXPECT_TRUE(f.q->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 4u);
	f.q->Unregister(f.registrar);
	EXPECT_EQ(f.registrar.Count(), 0u);
	f.q->Unregister(f.registrar);
	EXPECT_TRUE(f.q->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 4u);
}

TEST(HistoryQueries, RegisterRollsBackWhenANameIsTaken)
{
	QFix f; f.Setup();
	FakeMirrorRegistrar other;
	Dia::DebugServer::SimHelpInfo help;
	help.description = "taken";
	ASSERT_TRUE(other.Register(kEvents, help, [](const Json::Value&) { return Json::Value(); }));
	HistoryQueries second(*f.r.rec);
	EXPECT_FALSE(second.Register(other));
	EXPECT_EQ(other.Count(), 1u);
}

TEST(HistoryQueries, BudgetSeamIsClamped)
{
	QFix f; f.Setup();
	EXPECT_EQ(f.q->MaxReplyBytes(), HistoryQueries::kMaxReplyBytes);
	EXPECT_LT(HistoryQueries::kMaxReplyBytes, kHostLimit);
	f.q->SetMaxReplyBytes(1);
	EXPECT_EQ(f.q->MaxReplyBytes(), HistoryQueries::kMinReplyBytes);
	f.q->SetMaxReplyBytes(1000000);
	EXPECT_EQ(f.q->MaxReplyBytes(), HistoryQueries::kMaxReplyBytes);
	f.q->SetMaxEvents(0);
	EXPECT_EQ(f.q->MaxEvents(), 1u);
	f.q->SetMaxEvents(5000);
	EXPECT_EQ(f.q->MaxEvents(), 2000u);
}

// ---- structure hash -------------------------------------------------------------------------------------------------

TEST(HistoryQueriesStructure, ChangesWithLayerOrGroupingAndNotWithSamples)
{
	QFix a; a.Setup();
	a.Run(2);
	const uint64_t h0 = a.r.rec->StructureHash();
	EXPECT_NE(h0, 0u);
	a.Run(5);
	EXPECT_EQ(a.r.rec->StructureHash(), h0);	// new samples never change it

	QFix b;	// one more layer
	b.r.grid.AddFloatLayer("temp", 40.0f); b.r.grid.AddCategoryLayer("biome", 1); b.r.grid.AddCategoryLayer("region", 0);
	b.r.grid.AddFloatLayer("extra", 1.0f);
	ASSERT_TRUE(b.r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), EnumEntry("region", 3, "{\"group\":true}"), FloatEntry("extra", 0, 1), kEventEntry }));
	b.r.Start();
	b.q = std::make_unique<HistoryQueries>(*b.r.rec);
	b.Run(2);
	EXPECT_NE(b.r.rec->StructureHash(), h0);

	QFix c;	// one more grouping
	c.r.grid.AddFloatLayer("temp", 40.0f); c.r.grid.AddCategoryLayer("biome", 1); c.r.grid.AddCategoryLayer("region", 0);
	ASSERT_TRUE(c.r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3, "{\"group\":true}"), EnumEntry("region", 3, "{\"group\":true}"), kEventEntry }));
	c.r.Start();
	c.q = std::make_unique<HistoryQueries>(*c.r.rec);
	c.Run(2);
	EXPECT_NE(c.r.rec->StructureHash(), h0);
	EXPECT_NE(c.r.rec->StructureHash(), b.r.rec->StructureHash());
}

TEST(HistoryQueriesStructure, TwoIdenticalRecordersAgree)
{
	QFix a; a.Setup(); a.Run(3);
	QFix b; b.Setup(); b.Run(6);
	EXPECT_EQ(a.r.rec->StructureHash(), b.r.rec->StructureHash());
}

// ---- meta -----------------------------------------------------------------------------------------------------------

TEST(HistoryQueriesMeta, CoreMatchesTheFileSections)
{
	QFix f; f.Setup(); f.Run(10);
	f.r.rec->SetWorldIdProvider([](void*) -> const char* { return "unit_world"; }, nullptr);
	f.q->SetAppName("TestApp");
	f.q->SetBuildInfo("abc1234");
	f.opt.world = "";
	ASSERT_TRUE(f.Export()) << f.res.error;
	const Json::Value m = f.Call(kMeta);
	ASSERT_FALSE(IsError(m)) << m.toStyledString();
	EXPECT_EQ(m["part"].asString(), "core");
	EXPECT_FALSE(m.isMember("next"));
	EXPECT_EQ(m["header"]["world"].asString(), "unit_world");
	EXPECT_EQ(m["header"]["app"].asString(), "TestApp");
	EXPECT_EQ(m["header"]["git_commit"].asString(), "abc1234");
	for (const char* k : { "sampleCount", "sampleFirst", "aggregateFirst", "cellFirst" }) { EXPECT_TRUE(JsonEq(m["header"][k], f.dec.Header()[k])) << k; }
	EXPECT_TRUE(JsonEq(m["bands"], f.dec.root["bands"]));
	EXPECT_TRUE(JsonEq(m["groups"], f.dec.root["groups"]));
	EXPECT_TRUE(JsonEq(m["tracked"], f.dec.root["tracked"]));
	EXPECT_TRUE(JsonEq(m["gaps"], f.dec.root["gaps"]));
	EXPECT_TRUE(JsonEq(m["snapshots"], f.dec.root["snapshots"]));
	ASSERT_EQ(m["layers"].size(), f.dec.root["aggregates"].size());
	for (const Json::Value& l : m["layers"])
	{
		const Json::Value& fl = f.dec.root["aggregates"][l["id"].asString()];
		ASSERT_TRUE(fl.isObject()) << l["id"].asString();
		EXPECT_TRUE(JsonEq(l["kind"], fl["kind"]));
		EXPECT_TRUE(JsonEq(l["first"], fl["first"]));
		if (fl.isMember("values")) { EXPECT_TRUE(JsonEq(l["values"], fl["values"])); } else { EXPECT_FALSE(l.isMember("values")); }
	}
	char hex[24];
	snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)f.r.rec->StructureHash());
	EXPECT_EQ(m["structure"].asString(), hex);
	EXPECT_LE(Size(m), size_t(HistoryQueries::kMaxReplyBytes));
}

TEST(HistoryQueriesMeta, StructureIsStableAcrossSamplesAndChangesWithTheSetOfLayers)
{
	QFix f; f.Setup(); f.Run(3);
	const std::string s0 = f.Call(kMeta)["structure"].asString();
	f.Run(4);
	EXPECT_EQ(f.Call(kMeta)["structure"].asString(), s0);
	QFix g;
	g.r.grid.AddFloatLayer("temp", 40.0f); g.r.grid.AddFloatLayer("other", 1.0f);
	ASSERT_TRUE(g.r.schema.Load({ FloatEntry("temp", 0, 100), FloatEntry("other", 0, 1) }));
	g.r.Start();
	g.q = std::make_unique<HistoryQueries>(*g.r.rec);
	g.r.Take();
	EXPECT_NE(g.Call(kMeta)["structure"].asString(), s0);
}

TEST(HistoryQueriesMeta, SnapshotsComeFromTheManager)
{
	QFix f; f.Setup(); f.Run(3);
	FakeSnapshotStore store;
	SnapshotManager mgr(*f.r.rec, f.r.registry, f.r.schema.service.get(), store);
	TakeResult tr;
	ASSERT_TRUE(mgr.Take("alpha", false, tr));
	store.RunFrameBoundary();
	f.q->SetSnapshots(&mgr);
	f.opt.snapshots = &mgr;
	ASSERT_TRUE(f.Export());
	const Json::Value m = f.Call(kMeta);
	ASSERT_EQ(m["snapshots"].size(), 1u);
	EXPECT_TRUE(JsonEq(m["snapshots"], f.dec.root["snapshots"]));
	EXPECT_EQ(m["snapshots"][0]["name"].asString(), "alpha");
}

TEST(HistoryQueriesMeta, NoSamplesGivesAValidCore)
{
	QFix f; f.Setup();
	const Json::Value m = f.Call(kMeta);
	ASSERT_FALSE(IsError(m)) << m.toStyledString();
	EXPECT_EQ(m["header"]["sampleCount"].asUInt(), 0u);
	EXPECT_TRUE(m["groups"].isArray());
	EXPECT_TRUE(m["layers"].isArray());
}

TEST(HistoryQueriesMeta, SchemaPagesConcatenateToTheFilesSchema)
{
	QFix f; f.Setup(); f.Run(8);
	ASSERT_TRUE(f.Export());
	const Json::Value& fileSchema = f.dec.root["schema"];
	ASSERT_GT(fileSchema["entries"].size(), 3u);
	for (uint32_t budget : { 15000u, 900u })
	{
		f.Budget(budget);
		Json::Value entries(Json::arrayValue), ramps(Json::arrayValue);
		Json::Value args = Obj();
		args["part"] = "schema";
		uint32_t pages = 0;
		for (int guard = 0; guard < 1000; ++guard)
		{
			const Json::Value rep = f.Call(kMeta, args);
			ASSERT_FALSE(IsError(rep)) << rep.toStyledString();
			EXPECT_EQ(rep["part"].asString(), "schema");
			EXPECT_LE(Size(rep), size_t(budget));
			for (const Json::Value& e : rep["entries"]) { entries.append(e); }
			for (const Json::Value& e : rep["ramps"]) { ramps.append(e); }
			++pages;
			if (!rep.isMember("next")) { break; }
			EXPECT_EQ(rep["next"]["part"].asString(), "schema");
			args["offset"] = rep["next"]["offset"];
		}
		EXPECT_TRUE(JsonEq(entries, fileSchema["entries"])) << budget;
		EXPECT_TRUE(JsonEq(ramps, fileSchema["ramps"])) << budget;
		if (budget == 15000u) { EXPECT_EQ(pages, 1u); } else { EXPECT_GT(pages, 1u); }
	}
}

TEST(HistoryQueriesMeta, ErrorsForBadArguments)
{
	QFix f; f.Setup(); f.Run(3);
	auto with = [&](const char* k, const Json::Value& v) { Json::Value a = Obj(); a[k] = v; return f.Call(kMeta, a); };
	EXPECT_TRUE(ErrorHas(with("part", "bogus"), "part"));
	EXPECT_TRUE(ErrorHas(with("part", 5), "part"));
	EXPECT_TRUE(ErrorHas(with("offset", "x"), "offset"));
	EXPECT_TRUE(ErrorHas(with("offset", -1), "offset"));
	Json::Value a = Obj(); a["part"] = "schema"; a["offset"] = 100000;
	EXPECT_TRUE(ErrorHas(f.Call(kMeta, a), "cursor"));
	a["part"] = "groups";
	EXPECT_TRUE(ErrorHas(f.Call(kMeta, a), "cursor"));
	a["part"] = "layers";
	EXPECT_TRUE(ErrorHas(f.Call(kMeta, a), "cursor"));
	EXPECT_TRUE(IsError(f.Call(kMeta, Json::Value(Json::arrayValue))));
	EXPECT_FALSE(IsError(f.Call(kMeta, Json::Value())));	// null args = {}
}

namespace
{
	// 64 float layers + one grouping layer (aggregate:false) with 127 long-labelled values = 128 groups.
	void SetupBigMeta(QFix& f)
	{
		std::vector<std::string> entries;
		char name[16];
		for (int i = 0; i < 64; ++i)
		{
			snprintf(name, sizeof(name), "layer%02d", i);
			f.r.grid.AddFloatLayer(name, 1.0f);
			entries.push_back(FloatEntry(name, 0, 100));
		}
		f.r.grid.AddCategoryLayer("grp", 0);
		std::string e = "{\"key\":\"grp\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"enum\",\"values\":[";
		for (int i = 0; i < 127; ++i)
		{
			if (i) { e += ","; }
			e += "{\"id\":\"v" + std::to_string(i) + "\",\"label\":\"A deliberately long region label number " + std::to_string(i) + "\"}";
		}
		e += "],\"history\":{\"group\":true,\"aggregate\":false}}";
		entries.push_back(e);
		ASSERT_TRUE(f.r.schema.Load(entries));
		f.r.Start();
		f.q = std::make_unique<HistoryQueries>(*f.r.rec);
		ASSERT_TRUE(f.q->Register(f.registrar));
		for (uint32_t i = 0; i < 3; ++i) { f.r.grid.SetAll("grp", float(i)); f.r.grid.BumpVersion("grp"); f.r.Take(); }
	}
}

TEST(HistoryQueriesMeta, ManyLayersAndGroupsArePagedAndReassembleToTheFile)
{
	QFix f;
	SetupBigMeta(f);
	ASSERT_EQ(f.r.rec->RecordedLayerCount(), 64u);
	ASSERT_EQ(f.r.rec->GroupCount(), 128u);
	ASSERT_TRUE(f.Export());
	Json::Value groups(Json::arrayValue), layers(Json::arrayValue);
	Json::Value args = Obj();
	uint32_t pages = 0;
	size_t maxBytes = 0;
	bool first = true;
	for (int guard = 0; guard < 1000; ++guard)
	{
		const Json::Value rep = f.Call(kMeta, args);
		ASSERT_FALSE(IsError(rep)) << rep.toStyledString();
		maxBytes = std::max(maxBytes, Size(rep));
		if (first) { EXPECT_EQ(rep["part"].asString(), "core"); EXPECT_TRUE(rep.isMember("header")); EXPECT_EQ(rep["structure"].asString().size(), 16u); first = false; }
		for (const Json::Value& g : rep["groups"]) { groups.append(g); }
		for (const Json::Value& l : rep["layers"]) { layers.append(l); }
		++pages;
		if (!rep.isMember("next")) { break; }
		args = Obj();
		args["part"] = rep["next"]["part"];
		args["offset"] = rep["next"]["offset"];
	}
	EXPECT_GT(pages, 1u);
	EXPECT_LE(maxBytes, size_t(kHostLimit));
	EXPECT_TRUE(JsonEq(groups, f.dec.root["groups"]));
	ASSERT_EQ(layers.size(), 64u);
	for (uint32_t i = 0; i < 64; ++i) { EXPECT_EQ(layers[i]["id"].asString(), f.r.rec->RecordedLayerId(i).AsChar()); }
	printf("[ queries ] meta 64 layers / 128 groups: %u pages, max reply %zu bytes\n", pages, maxBytes);
}

// ---- aggregates -----------------------------------------------------------------------------------------------------

TEST(HistoryQueriesAggregates, DefaultRangeReturnsTheWholeWindowInTheFilesShape)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value a = f.Call(kAgg);
	ASSERT_FALSE(IsError(a)) << a.toStyledString();
	EXPECT_EQ(a["from"].asUInt(), 0u);
	EXPECT_EQ(a["count"].asUInt(), 10u);
	EXPECT_FALSE(a.isMember("next"));
	EXPECT_EQ(a["samples"].size(), 10u);
	EXPECT_EQ(a["rows"].size(), 3u);
	EXPECT_EQ(a["groupSizes"].size(), f.r.rec->GroupCount());
	EXPECT_EQ(a["structure"].asString().size(), 16u);
	EXPECT_EQ(a["rows"]["temp"]["kind"].asString(), "float");
	EXPECT_EQ(a["rows"]["biome"]["kind"].asString(), "category");
	EXPECT_TRUE(a["rows"]["temp"]["groups"]["world"].isMember("mean"));
}

TEST(HistoryQueriesAggregates, SingleReplyEqualsTheFile)
{
	QFix f; f.Setup(); f.Run(12);
	ASSERT_TRUE(f.Export());
	AggAssembly as;
	Assemble({ f.Call(kAgg) }, as);
	ExpectAssemblyEqualsFile(as, f.dec);
}

TEST(HistoryQueriesAggregates, ChunksReassembleToTheFile)
{
	QFix f; f.Setup(); f.Run(30);
	ASSERT_TRUE(f.Export());
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value r = f.Call(kAgg); return (IsError(r) || r.isMember("layerCount")) ? -1 : int(r["count"].asUInt()); };
	const int b1 = SmallestBudgetFor(countAt, 1, HistoryQueries::kMinReplyBytes, 15000);
	ASSERT_GT(b1, 0);
	for (uint32_t budget : { 15000u, 1400u, uint32_t(b1) + 8 })
	{
		f.Budget(budget);
		const Pages p = PageAggregates(f, Obj());
		ASSERT_FALSE(p.failed) << budget << " " << p.error;
		if (budget < 15000u) { EXPECT_GT(p.replies.size(), 1u) << budget; }
		EXPECT_LE(p.maxBytes, size_t(budget)) << budget;
		AggAssembly as;
		Assemble(p.replies, as);
		ExpectAssemblyEqualsFile(as, f.dec);
	}
}

TEST(HistoryQueriesAggregates, BoundaryBudgetsHoldExactlyOneThenTwoSamples)
{
	QFix f; f.Setup(); f.Run(12);
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value r = f.Call(kAgg); return (IsError(r) || r.isMember("layerCount")) ? -1 : int(r["count"].asUInt()); };
	const int b1 = SmallestBudgetFor(countAt, 1, HistoryQueries::kMinReplyBytes, 15000);
	ASSERT_GT(b1, 0);
	EXPECT_EQ(countAt(uint32_t(b1) - 1), -1);				// one byte under: not even a sample fits
	{
		f.Budget(uint32_t(b1));
		const Json::Value r = f.Call(kAgg);
		EXPECT_EQ(r["count"].asUInt(), 1u);
		EXPECT_EQ(Size(r), size_t(b1));						// exactly fits
		ASSERT_TRUE(r.isMember("next"));
		EXPECT_EQ(r["next"]["from"].asUInt(), 1u);
	}
	const int b2 = SmallestBudgetFor(countAt, 2, uint32_t(b1), 15000);
	ASSERT_GT(b2, b1);
	EXPECT_EQ(countAt(uint32_t(b2) - 1), 1);				// one byte under two samples: still one
	f.Budget(uint32_t(b2));
	EXPECT_EQ(Size(f.Call(kAgg)), size_t(b2));
	// A budget a few bytes over b1 (later cursors have wider numbers) pages to the end one sample at a time and covers every sample once.
	f.Budget(uint32_t(b1) + 8);
	const Pages p = PageAggregates(f, Obj());
	ASSERT_FALSE(p.failed) << p.error;
	EXPECT_EQ(p.replies.size(), 12u);
	for (uint32_t i = 0; i < p.replies.size(); ++i) { EXPECT_EQ(p.replies[i]["from"].asUInt(), i); }
}

TEST(HistoryQueriesAggregates, RangeAndLayerFiltersApply)
{
	QFix f; f.Setup(); f.Run(10);
	Json::Value a = Obj();
	a["from"] = 3; a["to"] = 7; a["layers"] = StrList({ "temp" });
	const Json::Value r = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["from"].asUInt(), 3u);
	EXPECT_EQ(r["count"].asUInt(), 4u);
	EXPECT_EQ(r["rows"].size(), 1u);
	EXPECT_TRUE(r["rows"].isMember("temp"));
	EXPECT_EQ(r["samples"].size(), 4u);
	// An empty range is valid.
	a["to"] = 3;
	const Json::Value e = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(e)) << e.toStyledString();
	EXPECT_EQ(e["count"].asUInt(), 0u);
}

TEST(HistoryQueriesAggregates, ErrorsChangeNothing)
{
	RecorderBudgets b;
	b.mAggregateBytes = AggregateRowBytes(1, 6, 4) * 5;	// 5 samples -> window [7, 12)
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 1 << 20;
	QFix f; f.Setup(&b); f.Run(12);
	ASSERT_EQ(f.r.rec->AggregateFirstRetained(), 7u);
	const uint32_t samples = f.r.rec->SampleCount();
	uint64_t hash = f.r.rec->StructureHash();
	auto call = [&](Json::Value a) { return f.Call(kAgg, a); };
	Json::Value a = Obj();
	a["from"] = 0;
	const Json::Value evicted = call(a);
	EXPECT_TRUE(ErrorHas(evicted, "outside the retained"));
	EXPECT_TRUE(ErrorHas(evicted, "7"));
	a = Obj(); a["to"] = 13;
	EXPECT_TRUE(ErrorHas(call(a), "outside the retained"));
	a = Obj(); a["from"] = 10; a["to"] = 9;
	EXPECT_TRUE(ErrorHas(call(a), "from"));
	a = Obj(); a["from"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "from"));
	a = Obj(); a["to"] = -2;
	EXPECT_TRUE(ErrorHas(call(a), "to"));
	a = Obj(); a["layers"] = "temp";
	EXPECT_TRUE(ErrorHas(call(a), "layers"));
	a = Obj(); a["layers"] = StrList({ "nope" });
	EXPECT_TRUE(ErrorHas(call(a), "nope"));
	a = Obj(); a["layers"] = Json::Value(Json::arrayValue); a["layers"].append(3);
	EXPECT_TRUE(ErrorHas(call(a), "layers"));
	EXPECT_TRUE(IsError(f.Call(kAgg, Json::Value(Json::arrayValue))));
	EXPECT_EQ(f.r.rec->SampleCount(), samples);
	EXPECT_EQ(f.r.rec->StructureHash(), hash);
	const Json::Value ok = f.Call(kAgg);
	EXPECT_EQ(ok["from"].asUInt(), 7u);	// default window start follows eviction
	EXPECT_EQ(ok["count"].asUInt(), 5u);
}

TEST(HistoryQueriesAggregates, CatchUpParamsAreValidated)
{
	QFix f; f.Setup(); f.Run(10);
	const uint32_t samples = f.r.rec->SampleCount();
	const uint64_t hash = f.r.rec->StructureHash();
	auto call = [&](Json::Value a) { return f.Call(kAgg, a); };
	Json::Value a = Obj();
	a["stride"] = 0;
	EXPECT_TRUE(ErrorHas(call(a), "param 'stride' must be an integer >= 1"));
	a = Obj(); a["stride"] = -1;
	EXPECT_TRUE(ErrorHas(call(a), "param 'stride' must be an integer >= 1"));
	a = Obj(); a["stride"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "param 'stride' must be an integer >= 1"));
	a = Obj(); a["groups"] = StrList({ "nope" });
	EXPECT_TRUE(ErrorHas(call(a), "unknown group 'nope'"));
	a = Obj(); a["groups"] = "world";
	EXPECT_TRUE(ErrorHas(call(a), "param 'groups' must be an array of group ids"));
	a = Obj(); a["groups"] = Json::Value(Json::arrayValue); a["groups"].append(3);
	EXPECT_TRUE(ErrorHas(call(a), "param 'groups' must be an array of group ids"));
	a = Obj(); a["layerOffset"] = 1000;
	EXPECT_TRUE(ErrorHas(call(a), "invalid cursor: layerOffset 1000 is past the last layer"));
	a = Obj(); a["layers"] = StrList({ "temp" }); a["layerOffset"] = 1;
	EXPECT_TRUE(ErrorHas(call(a), "invalid cursor: layerOffset 1 is past the last layer"));
	a = Obj(); a["layerOffset"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "layerOffset"));
	a = Obj(); a["structure"] = "0000000000000000";
	EXPECT_TRUE(ErrorHas(call(a), "structure changed: restart the catch-up"));
	EXPECT_EQ(f.r.rec->SampleCount(), samples);
	EXPECT_EQ(f.r.rec->StructureHash(), hash);

	// Valid values are accepted and, for now, do not change the reply (groups is applied separately, see GroupsFilter tests).
	const Json::Value base = f.Call(kAgg);
	Json::Value g = Obj();
	g["stride"] = 1; g["layerOffset"] = 0; g["structure"] = base["structure"];
	const Json::Value ok = call(g);
	ASSERT_FALSE(IsError(ok)) << ok.toStyledString();
	EXPECT_TRUE(JsonEq(ok, base));
}

TEST(HistoryQueriesAggregates, GroupsFilterKeepsOnlyTheNamedGroups)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	const std::vector<std::string> names = base["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 3u);
	for (const std::string& pick : { names[0], names[1] })
	{
		Json::Value a = Obj();
		a["groups"] = StrList({ pick.c_str() });
		const Json::Value r = f.Call(kAgg, a);
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		ASSERT_EQ(r["groupSizes"].size(), 1u);
		EXPECT_TRUE(JsonEq(r["groupSizes"][pick], base["groupSizes"][pick]));
		EXPECT_EQ(r["rows"].size(), base["rows"].size());
		for (const std::string& layer : r["rows"].getMemberNames())
		{
			const Json::Value& gs = r["rows"][layer]["groups"];
			ASSERT_EQ(gs.size(), 1u) << layer;
			EXPECT_TRUE(JsonEq(gs[pick], base["rows"][layer]["groups"][pick])) << layer;
		}
		EXPECT_EQ(r["rows"]["temp"]["kind"].asString(), "float");
		EXPECT_EQ(r["rows"]["biome"]["kind"].asString(), "category");
		EXPECT_TRUE(JsonEq(r["samples"], base["samples"]));
		EXPECT_EQ(r["structure"].asString(), base["structure"].asString());
	}
}

TEST(HistoryQueriesAggregates, GroupsFilterWithTwoGroupsIsOrderIrrelevant)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	const std::vector<std::string> names = base["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 3u);
	Json::Value a = Obj(); a["groups"] = StrList({ names[0].c_str(), names[2].c_str() });
	Json::Value b = Obj(); b["groups"] = StrList({ names[2].c_str(), names[0].c_str() });
	const Json::Value ra = f.Call(kAgg, a), rb = f.Call(kAgg, b);
	ASSERT_FALSE(IsError(ra)) << ra.toStyledString();
	EXPECT_EQ(ra["groupSizes"].size(), 2u);
	EXPECT_TRUE(ra["groupSizes"].isMember(names[0]));
	EXPECT_TRUE(ra["groupSizes"].isMember(names[2]));
	EXPECT_TRUE(JsonEq(ra, rb));
	for (const std::string& layer : ra["rows"].getMemberNames()) { EXPECT_EQ(ra["rows"][layer]["groups"].size(), 2u) << layer; }
}

TEST(HistoryQueriesAggregates, NoGroupsFieldIsIdenticalAtAFixedBudget)
{
	QFix f; f.Setup(); f.Run(10);
	f.Budget(3000);
	const Json::Value base = f.Call(kAgg);
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	Json::Value a = Obj(); a["layers"] = StrList({ "temp", "biome", "region" });
	EXPECT_TRUE(JsonEq(f.Call(kAgg, a), base));
	EXPECT_TRUE(JsonEq(f.Call(kAgg, Obj()), base));
}

TEST(HistoryQueriesAggregates, EmptyGroupsArrayIncludesNoGroupsAndSucceeds)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	Json::Value a = Obj(); a["groups"] = Json::Value(Json::arrayValue);
	const Json::Value r = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["groupSizes"].size(), 0u);
	EXPECT_EQ(r["rows"].size(), base["rows"].size());
	for (const std::string& layer : r["rows"].getMemberNames()) { EXPECT_EQ(r["rows"][layer]["groups"].size(), 0u) << layer; }
	EXPECT_EQ(r["count"].asUInt(), 10u);
}

TEST(HistoryQueriesAggregates, GroupsFilterShrinksTheReplyAndHonoursTheBudget)
{
	QFix f; f.Setup(); f.Run(30);
	const Json::Value base = f.Call(kAgg);
	const std::vector<std::string> names = base["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 2u);
	Json::Value a = Obj(); a["groups"] = StrList({ names[0].c_str() });
	const Json::Value filtered = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(filtered)) << filtered.toStyledString();
	EXPECT_LT(Size(filtered), Size(base));
	// Budget below the unfiltered size: the filtered reply must still fit and honour it (and cover at least as many samples).
	const uint32_t budget = uint32_t(Size(base)) / 2;
	f.Budget(budget);
	const Json::Value u = f.Call(kAgg);
	const Json::Value fl = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(u)) << u.toStyledString();
	ASSERT_FALSE(IsError(fl)) << fl.toStyledString();
	EXPECT_LE(Size(fl), size_t(budget));
	EXPECT_GE(fl["count"].asUInt(), u["count"].asUInt());
}

TEST(HistoryQueriesAggregates, CatchUpParamsAreInTheHelp)
{
	QFix f; f.Setup();
	const Dia::DebugServer::SimHelpStored* help = f.registrar.Help(kAgg);
	ASSERT_NE(help, nullptr);
	EXPECT_EQ(help->paramCount, 7u);
	std::string names;
	for (unsigned i = 0; i < help->paramCount; ++i) { names += help->params[i].name; names += ","; }
	for (const char* n : { "groups", "stride", "layerOffset", "structure" }) { EXPECT_NE(names.find(n), std::string::npos) << n; }
	for (unsigned i = 0; i < help->paramCount; ++i) { EXPECT_GT(strlen(help->params[i].description), 0u); }
	EXPECT_LE(strlen(help->description), 256u);
}

TEST(HistoryQueriesAggregates, OneLayerThatDoesNotFitIsAnErrorNotAnOversizedReply)
{
	QFix f; f.Setup(); f.Run(5);
	f.Budget(HistoryQueries::kMinReplyBytes);
	const Json::Value r = f.Call(kAgg);
	EXPECT_TRUE(ErrorHas(r, "reply too large"));
	EXPECT_TRUE(ErrorHas(r, "fewer groups"));
	Json::Value a = Obj(); a["layerOffset"] = 1;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "fewer groups"));
}

namespace
{
	// Budget that is one byte under the full single-sample reply: forces the slice path.
	uint32_t SliceBudget(QFix& f)
	{
		Json::Value a = Obj(); a["from"] = 0; a["to"] = 1;
		return uint32_t(Size(f.Call(kAgg, a))) - 1;
	}

	// Follows the layer cursor to the end, re-sending to/layers/groups and the `next` fields.
	Pages PageSlices(QFix& f, Json::Value args)
	{
		Pages p;
		for (int guard = 0; guard < 10000; ++guard)
		{
			const Json::Value rep = f.Call(kAgg, args);
			if (IsError(rep)) { p.failed = true; p.error = rep["error"].asString(); return p; }
			p.maxBytes = std::max(p.maxBytes, Size(rep));
			p.replies.push_back(rep);
			if (!rep.isMember("next")) { return p; }
			args["from"] = rep["next"]["from"];
			args["layerOffset"] = rep["next"]["layerOffset"];
			args["structure"] = rep["next"]["structure"];
		}
		p.failed = true;
		p.error = "runaway paging";
		return p;
	}
}

TEST(HistoryQueriesAggregates, SampleThatDoesNotFitReturnsALayerSliceWithACursor)
{
	QFix f; f.Setup(); f.Run(6);
	const uint32_t budget = SliceBudget(f);
	f.Budget(budget);
	const Json::Value r = f.Call(kAgg);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 1u);
	EXPECT_EQ(r["from"].asUInt(), 0u);
	EXPECT_EQ(r["layerOffset"].asUInt(), 0u);
	EXPECT_EQ(r["layerTotal"].asUInt(), 3u);
	EXPECT_GE(r["layerCount"].asUInt(), 1u);
	EXPECT_LT(r["layerCount"].asUInt(), 3u);
	EXPECT_EQ(r["rows"].size(), r["layerCount"].asUInt());
	ASSERT_TRUE(r.isMember("next"));
	EXPECT_EQ(r["next"]["from"].asUInt(), 0u);
	EXPECT_EQ(r["next"]["layerOffset"].asUInt(), r["layerCount"].asUInt());
	EXPECT_EQ(r["next"]["structure"].asString(), r["structure"].asString());
	EXPECT_LE(Size(r), size_t(budget));
	EXPECT_EQ(r["samples"].size(), 1u);

	// layerOffset > 0 returns exactly one sample (the rest of that sample's layers), even if more would otherwise fit.
	f.Budget(15000);
	Json::Value a = Obj(); a["layerOffset"] = 1;
	const Json::Value rest = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(rest)) << rest.toStyledString();
	EXPECT_EQ(rest["count"].asUInt(), 1u);
	EXPECT_EQ(rest["layerOffset"].asUInt(), 1u);
	EXPECT_EQ(rest["layerCount"].asUInt(), 2u);
	EXPECT_EQ(rest["rows"].size(), 2u);
	ASSERT_TRUE(rest.isMember("next"));
	EXPECT_EQ(rest["next"]["from"].asUInt(), 1u);
	EXPECT_EQ(rest["next"]["layerOffset"].asUInt(), 0u);
}

TEST(HistoryQueriesAggregates, FollowingTheLayerCursorReassemblesTheUnlimitedData)
{
	QFix f; f.Setup(); f.Run(6);
	const uint32_t budget = SliceBudget(f);
	f.Budget(budget);
	Json::Value args = Obj(); args["to"] = 4;
	const Pages p = PageSlices(f, args);
	ASSERT_FALSE(p.failed) << p.error;
	EXPECT_LE(p.maxBytes, size_t(budget));
	EXPECT_GT(p.replies.size(), 4u);
	// Per sample: the slices tile layers 0..3 in order with no gaps.
	std::map<uint32_t, std::map<std::string, Json::Value>> bySample;
	std::map<uint32_t, uint32_t> covered;
	for (const Json::Value& rep : p.replies)
	{
		const uint32_t s = rep["from"].asUInt();
		EXPECT_EQ(rep["count"].asUInt(), 1u);
		EXPECT_EQ(rep["layerOffset"].asUInt(), covered[s]);
		covered[s] += rep["layerCount"].asUInt();
		for (const std::string& l : rep["rows"].getMemberNames()) { bySample[s][l] = rep["rows"][l]; }
	}
	ASSERT_EQ(covered.size(), 4u);
	f.Budget(15000);
	for (uint32_t s = 0; s < 4; ++s)
	{
		EXPECT_EQ(covered[s], 3u);
		Json::Value ref = Obj(); ref["from"] = s; ref["to"] = s + 1;
		const Json::Value whole = f.Call(kAgg, ref);
		ASSERT_FALSE(IsError(whole));
		ASSERT_EQ(bySample[s].size(), whole["rows"].size()) << s;
		for (const std::string& l : whole["rows"].getMemberNames()) { EXPECT_TRUE(JsonEq(bySample[s][l], whole["rows"][l])) << s << " " << l; }
	}
	// The last reply has no cursor; every earlier one does.
	EXPECT_FALSE(p.replies.back().isMember("next"));
	EXPECT_EQ(p.replies.back()["from"].asUInt(), 3u);
}

TEST(HistoryQueriesAggregates, SliceCursorWithAStaleStructureIsRejectedAndAFreshRequestSucceeds)
{
	QFix f; f.Setup(); f.Run(4);
	f.Budget(SliceBudget(f));
	const Json::Value first = f.Call(kAgg);
	ASSERT_TRUE(first.isMember("next"));
	// A recorder's structure cannot change live; a cursor from another structure is simulated with a different hash.
	Json::Value a = Obj();
	a["from"] = first["next"]["from"]; a["layerOffset"] = first["next"]["layerOffset"]; a["structure"] = "0123456789abcdef";
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "structure changed"));
	a["structure"] = first["next"]["structure"];
	EXPECT_FALSE(IsError(f.Call(kAgg, a)));
	f.Budget(15000);
	EXPECT_FALSE(IsError(f.Call(kAgg)));
}

TEST(HistoryQueriesAggregates, SliceWithAGroupFilterHonoursTheBudget)
{
	QFix f; f.Setup(); f.Run(6);
	Json::Value g = Obj(); g["groups"] = StrList({ "world" });
	Json::Value one = g; one["from"] = 0; one["to"] = 1;
	const uint32_t budget = uint32_t(Size(f.Call(kAgg, one))) - 1;
	f.Budget(budget);
	const Pages p = PageSlices(f, g);
	ASSERT_FALSE(p.failed) << p.error;
	EXPECT_GT(p.replies.size(), 6u);
	EXPECT_LE(p.maxBytes, size_t(budget));
}

// ---- aggregates: stride ---------------------------------------------------------------------------------------------

TEST(HistoryRowsReduce, MinMaxAndWeightedMean)
{
	const float mins[3] = { 5.f, 2.f, 4.f }, means[3] = { 10.f, 20.f, 40.f }, maxs[3] = { 15.f, 30.f, 50.f };
	const uint16_t w[3] = { 1, 2, 5 };
	const bool ok[3] = { true, true, true };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 3);
	EXPECT_TRUE(r.valid);
	EXPECT_FLOAT_EQ(r.min, 2.f);
	EXPECT_FLOAT_EQ(r.max, 50.f);
	EXPECT_NEAR(r.mean, (10.f * 1 + 20.f * 2 + 40.f * 5) / 8.f, 1e-4f);
}

TEST(HistoryRowsReduce, InvalidSamplesAreSkipped)
{
	const float mins[3] = { -100.f, 2.f, -200.f }, means[3] = { 1000.f, 20.f, 1000.f }, maxs[3] = { 900.f, 30.f, 900.f };
	const uint16_t w[3] = { 9, 3, 9 };
	const bool ok[3] = { false, true, false };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 3);
	EXPECT_TRUE(r.valid);
	EXPECT_FLOAT_EQ(r.min, 2.f);
	EXPECT_FLOAT_EQ(r.mean, 20.f);
	EXPECT_FLOAT_EQ(r.max, 30.f);
}

TEST(HistoryRowsReduce, ZeroWeightsFallBackToTheUnweightedMean)
{
	const float mins[2] = { 1.f, 1.f }, means[2] = { 10.f, 20.f }, maxs[2] = { 3.f, 3.f };
	const uint16_t w[2] = { 0, 0 };
	const bool ok[2] = { true, true };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 2);
	EXPECT_TRUE(r.valid);
	EXPECT_FLOAT_EQ(r.mean, 15.f);
}

TEST(HistoryRowsReduce, NothingValidIsNaN)
{
	const float v[2] = { 1.f, 2.f };
	const uint16_t w[2] = { 1, 1 };
	const bool ok[2] = { false, false };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(v, v, v, w, ok, 2);
	EXPECT_FALSE(r.valid);
	EXPECT_TRUE(r.min != r.min); EXPECT_TRUE(r.mean != r.mean); EXPECT_TRUE(r.max != r.max);
	const Dia::CellHistory::Rows::WindowStats e = Dia::CellHistory::Rows::ReduceFloatWindow(v, v, v, w, ok, 0);
	EXPECT_FALSE(e.valid);
}

TEST(HistoryRowsReduce, NaNInputsAreSkippedPerField)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float mins[2] = { nan, 4.f }, means[2] = { nan, 8.f }, maxs[2] = { 6.f, nan };
	const uint16_t w[2] = { 5, 2 };
	const bool ok[2] = { true, true };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 2);
	EXPECT_TRUE(r.valid);
	EXPECT_FLOAT_EQ(r.min, 4.f);
	EXPECT_FLOAT_EQ(r.max, 6.f);
	EXPECT_FLOAT_EQ(r.mean, 8.f);
}

TEST(HistoryRowsReduce, SingleSampleWindowEqualsTheInput)
{
	const float mn = 1.5f, me = 2.5f, mx = 3.5f;
	const uint16_t w = 7;
	const bool ok = true;
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(&mn, &me, &mx, &w, &ok, 1);
	EXPECT_TRUE(r.valid);
	EXPECT_FLOAT_EQ(r.min, mn); EXPECT_FLOAT_EQ(r.mean, me); EXPECT_FLOAT_EQ(r.max, mx);
}

namespace
{
	Json::Value StrideArgs(uint32_t stride) { Json::Value a = Obj(); a["stride"] = stride; return a; }

	// Walks the strided reply `r` against the unstrided `base` (covering [baseFrom, ..)) over windows of `stride` from `from` to `to`.
	void ExpectStridedMatchesReference(const Json::Value& base, const Json::Value& r, uint32_t stride, uint32_t baseFrom, uint32_t from, uint32_t to)
	{
		const uint32_t windows = r["count"].asUInt();
		const uint32_t baseN = base["count"].asUInt();
		ASSERT_EQ(r["samples"].size(), windows);
		for (const std::string& g : r["groupSizes"].getMemberNames())
		{
			std::vector<float> rmn, rme, rmx;
			std::vector<uint16_t> rsize, baseCat, cat, sz;
			ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"][g]["min"], baseN, rmn));
			ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"][g]["mean"], baseN, rme));
			ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"][g]["max"], baseN, rmx));
			ASSERT_TRUE(DecodedExport::DecodeU16(base["groupSizes"][g], baseN, rsize));
			ASSERT_TRUE(DecodedExport::DecodeU16(base["rows"]["biome"]["groups"][g], baseN * 3u, baseCat));
			std::vector<float> mn, me, mx;
			ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"][g]["min"], windows, mn));
			ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"][g]["mean"], windows, me));
			ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"][g]["max"], windows, mx));
			ASSERT_TRUE(DecodedExport::DecodeU16(r["groupSizes"][g], windows, sz));
			ASSERT_TRUE(DecodedExport::DecodeU16(r["rows"]["biome"]["groups"][g], windows * 3u, cat));
			for (uint32_t w = 0; w < windows; ++w)
			{
				const uint32_t a = from + w * stride;
				const uint32_t b = std::min(a + stride, to);
				double lo = 1e30, hi = -1e30, num = 0, den = 0;
				for (uint32_t s = a; s < b; ++s)
				{
					const uint32_t i = s - baseFrom;
					lo = std::min<double>(lo, rmn[i]); hi = std::max<double>(hi, rmx[i]);
					num += double(rme[i]) * rsize[i]; den += rsize[i];
				}
				EXPECT_NEAR(mn[w], lo, 1e-3) << g << " w" << w;
				EXPECT_NEAR(mx[w], hi, 1e-3) << g << " w" << w;
				if (den > 0) { EXPECT_NEAR(me[w], num / den, 1e-2) << g << " w" << w; }
				const uint32_t last = b - 1 - baseFrom;
				EXPECT_EQ(sz[w], rsize[last]) << g << " w" << w;
				for (uint32_t k = 0; k < 3; ++k) { EXPECT_EQ(cat[w * 3 + k], baseCat[last * 3 + k]) << g << " w" << w << " k" << k; }
				EXPECT_TRUE(JsonEq(r["samples"][w], base["samples"][last])) << w;
			}
		}
	}
}

TEST(HistoryQueriesAggregatesStride, WindowsMatchAnIndependentReference)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	const Json::Value r = f.Call(kAgg, StrideArgs(4));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 3u);			// [0,4) [4,8) [8,10)
	EXPECT_EQ(r["stride"].asUInt(), 4u);
	EXPECT_EQ(r["from"].asUInt(), 0u);
	EXPECT_FALSE(r.isMember("next"));
	EXPECT_GE(r["groupSizes"].size(), 3u);
	ExpectStridedMatchesReference(base, r, 4, 0, 0, 10);
}

TEST(HistoryQueriesAggregatesStride, StrideLargerThanTheRangeGivesOneWindow)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	const Json::Value r = f.Call(kAgg, StrideArgs(1000));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 1u);
	EXPECT_FALSE(r.isMember("next"));
	ExpectStridedMatchesReference(base, r, 1000, 0, 0, 10);
}

TEST(HistoryQueriesAggregatesStride, StrideOneEqualsTheUnstridedReplyWithNoStrideField)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	const Json::Value r = f.Call(kAgg, StrideArgs(1));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_FALSE(r.isMember("stride"));
	EXPECT_TRUE(JsonEq(r, base));
}

TEST(HistoryQueriesAggregatesStride, WindowsAreRelativeToFromAndToCanCutTheLastWindow)
{
	QFix f; f.Setup(); f.Run(12);
	Json::Value ba = Obj(); ba["from"] = 3; ba["to"] = 10;
	const Json::Value base = f.Call(kAgg, ba);
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	Json::Value a = StrideArgs(2); a["from"] = 3; a["to"] = 10;
	const Json::Value r = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["from"].asUInt(), 3u);
	EXPECT_EQ(r["count"].asUInt(), 4u);			// [3,5) [5,7) [7,9) [9,10)
	ExpectStridedMatchesReference(base, r, 2, 3, 3, 10);
}

TEST(HistoryQueriesAggregatesStride, NextCursorFollowsTheWindowsAndReassemblesTheOneShotResult)
{
	QFix f; f.Setup(); f.Run(30);
	const Json::Value whole = f.Call(kAgg, StrideArgs(3));
	ASSERT_FALSE(IsError(whole)) << whole.toStyledString();
	ASSERT_EQ(whole["count"].asUInt(), 10u);
	Json::Value four = StrideArgs(3); four["to"] = 12;
	const uint32_t budget = uint32_t(Size(f.Call(kAgg, four))) + 20;	// about four windows
	f.Budget(budget);
	const Json::Value first = f.Call(kAgg, StrideArgs(3));
	ASSERT_FALSE(IsError(first)) << first.toStyledString();
	ASSERT_TRUE(first.isMember("next"));
	EXPECT_EQ(first["next"]["from"].asUInt(), first["count"].asUInt() * 3u);
	EXPECT_LE(Size(first), size_t(budget));
	Json::Value args = StrideArgs(3);
	std::vector<Json::Value> pages;
	for (int guard = 0; guard < 1000; ++guard)
	{
		const Json::Value rep = f.Call(kAgg, args);
		ASSERT_FALSE(IsError(rep)) << rep.toStyledString();
		EXPECT_LE(Size(rep), size_t(budget));
		pages.push_back(rep);
		if (!rep.isMember("next")) { break; }
		args["from"] = rep["next"]["from"];
	}
	EXPECT_GT(pages.size(), 2u);
	AggAssembly paged, one;
	Assemble(pages, paged);
	Assemble({ whole }, one);
	EXPECT_EQ(paged.total, 10u);
	EXPECT_EQ(paged.f32bits, one.f32bits);
	EXPECT_EQ(paged.u16, one.u16);
	EXPECT_EQ(paged.sizes, one.sizes);
	ASSERT_EQ(paged.samples.size(), one.samples.size());
	for (size_t i = 0; i < one.samples.size(); ++i) { EXPECT_TRUE(JsonEq(paged.samples[i], one.samples[i])) << i; }
}

TEST(HistoryQueriesAggregatesStride, GroupAndLayerFiltersComposeWithStride)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value full = f.Call(kAgg, StrideArgs(4));
	const std::vector<std::string> names = full["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 2u);
	Json::Value a = StrideArgs(4);
	a["groups"] = StrList({ names[1].c_str() });
	a["layers"] = StrList({ "temp" });
	const Json::Value r = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["groupSizes"].size(), 1u);
	EXPECT_EQ(r["rows"].size(), 1u);
	ASSERT_TRUE(r["rows"].isMember("temp"));
	EXPECT_TRUE(JsonEq(r["groupSizes"][names[1]], full["groupSizes"][names[1]]));
	EXPECT_TRUE(JsonEq(r["rows"]["temp"]["groups"][names[1]], full["rows"]["temp"]["groups"][names[1]]));
}

TEST(HistoryQueriesAggregatesStride, StridedRepliesFitTheBudget)
{
	QFix f; f.Setup(); f.Run(40);
	const Json::Value whole = f.Call(kAgg, StrideArgs(2));
	for (uint32_t budget : { uint32_t(Size(whole)) / 2, uint32_t(Size(whole)) / 4 })
	{
		f.Budget(budget);
		const Json::Value r = f.Call(kAgg, StrideArgs(2));
		ASSERT_FALSE(IsError(r)) << budget << " " << r.toStyledString();
		EXPECT_LE(Size(r), size_t(budget));
		EXPECT_TRUE(r.isMember("next"));
	}
}

namespace
{
	// Budget one byte under the full single-window reply for [0, stride): forces the slice path for stride windows.
	uint32_t StrideSliceBudget(QFix& f, uint32_t stride, const Json::Value& extra = Json::Value(Json::objectValue), uint32_t shrink = 1, uint32_t to = 0)
	{
		Json::Value a = extra; a["stride"] = stride; a["from"] = 0; a["to"] = to ? to : stride;
		return uint32_t(Size(f.Call(kAgg, a))) - shrink;
	}

	// Checks stride slices tile layers per window and equal the single-window reference computed at the default budget.
	void ExpectStrideSlicesMatch(QFix& f, const Pages& p, const Json::Value& baseArgs, uint32_t stride, uint32_t to, size_t budget)
	{
		ASSERT_FALSE(p.failed) << p.error;
		EXPECT_LE(p.maxBytes, budget);
		std::map<uint32_t, uint32_t> covered;
		std::map<uint32_t, std::map<std::string, Json::Value>> rows;
		std::map<uint32_t, Json::Value> samples;
		for (const Json::Value& rep : p.replies)
		{
			const uint32_t w = rep["from"].asUInt();
			EXPECT_EQ(rep["count"].asUInt(), 1u);
			EXPECT_EQ(rep["stride"].asUInt(), stride);
			EXPECT_EQ(rep["layerOffset"].asUInt(), covered[w]);
			covered[w] += rep["layerCount"].asUInt();
			samples[w] = rep["samples"];
			for (const std::string& l : rep["rows"].getMemberNames()) { rows[w][l] = rep["rows"][l]; }
			for (const std::string& g : rep["groupSizes"].getMemberNames()) { rows[w]["|sizes|" + g] = rep["groupSizes"][g]; }
		}
		f.Budget(15000);
		const uint32_t windows = (to + stride - 1) / stride;
		ASSERT_EQ(covered.size(), windows);
		for (uint32_t i = 0; i < windows; ++i)
		{
			const uint32_t a = i * stride;
			Json::Value ref = baseArgs; ref["stride"] = stride; ref["from"] = a; ref["to"] = std::min(a + stride, to);
			const Json::Value whole = f.Call(kAgg, ref);
			ASSERT_FALSE(IsError(whole)) << whole.toStyledString();
			ASSERT_EQ(whole["count"].asUInt(), 1u);
			EXPECT_EQ(covered[a], whole["rows"].size()) << a;
			EXPECT_TRUE(JsonEq(samples[a], whole["samples"])) << a;
			for (const std::string& l : whole["rows"].getMemberNames()) { EXPECT_TRUE(JsonEq(rows[a][l], whole["rows"][l])) << a << " " << l; }
			for (const std::string& g : whole["groupSizes"].getMemberNames()) { EXPECT_TRUE(JsonEq(rows[a]["|sizes|" + g], whole["groupSizes"][g])) << a << " " << g; }
		}
		EXPECT_FALSE(p.replies.back().isMember("next"));
	}
}

TEST(HistoryQueriesAggregatesStride, WindowThatDoesNotFitReturnsAStrideLayerSlice)
{
	QFix f; f.Setup(); f.Run(10);
	const uint32_t budget = StrideSliceBudget(f, 4);
	f.Budget(budget);
	const Json::Value r = f.Call(kAgg, StrideArgs(4));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 1u);
	EXPECT_EQ(r["stride"].asUInt(), 4u);
	EXPECT_EQ(r["from"].asUInt(), 0u);
	EXPECT_EQ(r["layerOffset"].asUInt(), 0u);
	EXPECT_EQ(r["layerTotal"].asUInt(), 3u);
	EXPECT_LT(r["layerCount"].asUInt(), 3u);
	EXPECT_EQ(r["samples"].size(), 1u);
	ASSERT_TRUE(r.isMember("next"));
	EXPECT_EQ(r["next"]["from"].asUInt(), 0u);
	EXPECT_EQ(r["next"]["layerOffset"].asUInt(), r["layerCount"].asUInt());
	EXPECT_EQ(r["next"]["structure"].asString(), r["structure"].asString());
	EXPECT_LE(Size(r), size_t(budget));

	// layerOffset > 0 with stride returns the rest of that one window only.
	f.Budget(15000);
	Json::Value a = StrideArgs(4); a["layerOffset"] = 1;
	const Json::Value rest = f.Call(kAgg, a);
	ASSERT_FALSE(IsError(rest)) << rest.toStyledString();
	EXPECT_EQ(rest["count"].asUInt(), 1u);
	EXPECT_EQ(rest["stride"].asUInt(), 4u);
	EXPECT_EQ(rest["layerCount"].asUInt(), 2u);
	ASSERT_TRUE(rest.isMember("next"));
	EXPECT_EQ(rest["next"]["from"].asUInt(), 4u);
	EXPECT_EQ(rest["next"]["layerOffset"].asUInt(), 0u);
}

TEST(HistoryQueriesAggregatesStride, FollowingTheStrideSliceCursorReassemblesEveryWindow)
{
	QFix f; f.Setup(); f.Run(10);
	const uint32_t budget = StrideSliceBudget(f, 4);
	f.Budget(budget);
	const Pages p = PageSlices(f, StrideArgs(4));
	ASSERT_FALSE(p.failed) << p.error;
	EXPECT_GT(p.replies.size(), 3u);
	EXPECT_EQ(p.replies.back()["from"].asUInt(), 8u);		// windows [0,4) [4,8) [8,10)
	ExpectStrideSlicesMatch(f, p, Obj(), 4, 10, budget);
}

TEST(HistoryQueriesAggregatesStride, GroupsLayersStrideAndSlicingCombine)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value full = f.Call(kAgg, StrideArgs(3));
	const std::vector<std::string> names = full["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 2u);
	Json::Value filt = Obj();
	filt["groups"] = StrList({ names[0].c_str(), names[1].c_str() });
	filt["layers"] = StrList({ "temp", "biome" });
	const uint32_t budget = StrideSliceBudget(f, 3, filt, 1);
	f.Budget(budget);
	Json::Value args = filt; args["stride"] = 3;
	const Pages p = PageSlices(f, args);
	ASSERT_FALSE(p.failed) << p.error;
	for (const Json::Value& rep : p.replies)
	{
		EXPECT_EQ(rep["layerTotal"].asUInt(), 2u);
		EXPECT_EQ(rep["groupSizes"].size(), 2u);
		EXPECT_TRUE(rep["groupSizes"].isMember(names[1]));
		for (const std::string& l : rep["rows"].getMemberNames()) { EXPECT_TRUE(l == "temp" || l == "biome") << l; }
	}
	EXPECT_GT(p.replies.size(), 4u);
	ExpectStrideSlicesMatch(f, p, filt, 3, 10, budget);
}

TEST(HistoryQueriesAggregatesStride, StrideSliceCursorWithAStaleStructureIsRejected)
{
	QFix f; f.Setup(); f.Run(10);
	f.Budget(StrideSliceBudget(f, 4));
	const Json::Value first = f.Call(kAgg, StrideArgs(4));
	ASSERT_TRUE(first.isMember("next"));
	Json::Value a = StrideArgs(4);
	a["from"] = first["next"]["from"]; a["layerOffset"] = first["next"]["layerOffset"]; a["structure"] = "0123456789abcdef";
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "structure changed"));
	a["structure"] = first["next"]["structure"];
	EXPECT_FALSE(IsError(f.Call(kAgg, a)));
}

TEST(HistoryQueriesAggregatesStride, StrideSliceWhereOneLayerCannotFitIsTheFewerGroupsError)
{
	QFix f; f.Setup(); f.Run(10);
	f.Budget(HistoryQueries::kMinReplyBytes);
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, StrideArgs(4)), "fewer groups"));
	Json::Value a = StrideArgs(4); a["layerOffset"] = 1;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "fewer groups"));
}

TEST(HistoryQueriesAggregatesStride, StrideLargerThanTheRangeWithSlicingIsOneWindowWithNoNextWindow)
{
	QFix f; f.Setup(); f.Run(10);
	const uint32_t budget = StrideSliceBudget(f, 1000, Obj(), 1, 10);
	f.Budget(budget);
	const Pages p = PageSlices(f, StrideArgs(1000));
	ASSERT_FALSE(p.failed) << p.error;
	EXPECT_GT(p.replies.size(), 1u);
	for (const Json::Value& rep : p.replies) { EXPECT_EQ(rep["from"].asUInt(), 0u); EXPECT_EQ(rep["stride"].asUInt(), 1000u); }
	EXPECT_FALSE(p.replies.back().isMember("next"));
	EXPECT_LE(p.maxBytes, size_t(budget));
}

TEST(HistoryQueriesAggregatesStride, LayerOffsetWithStrideNeedsASampleInsideTheRange)
{
	QFix f; f.Setup(); f.Run(10);
	Json::Value a = StrideArgs(4); a["layerOffset"] = 1; a["from"] = 10;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "invalid cursor"));
}

// ---- aggregates: exhaustive pass (Feature 6 task 7) -----------------------------------------------------------------

namespace
{
	const uint32_t kMaxU32 = 0xFFFFFFFFu;

	// A layer that is in the recorded layout but whose aggregate rows are unwritten for the first `lateBy` samples (its `first` is lateBy).
	// The other of temp/biome is tracked first so the layout is resolved before the late layer is unregistered.
	void SetupLate(QFix& f, const char* lateLayer, uint32_t lateBy, uint32_t total)
	{
		f.Setup(nullptr, false);
		const bool lateIsTemp = std::strcmp(lateLayer, "temp") == 0;
		const StringCRC other(lateIsTemp ? "biome" : "temp");
		CellHistoryRecorder::TrackResult tr;
		ASSERT_TRUE(f.r.rec->Track(&other, 1, tr)) << tr.mError;
		ASSERT_TRUE(f.r.registry.Unregister(StringCRC(lateLayer)));
		f.Run(lateBy);
		Dia::CellLayers::CellLayerDesc d;
		ASSERT_TRUE(f.r.grid.Describe(lateIsTemp ? 0u : 1u, d));
		d.schemaKey = StringCRC((std::string("cells/") + lateLayer).c_str());
		ASSERT_EQ(f.r.registry.Register(d), Dia::CellLayers::CellRegisterResult::Ok);
		f.Run(total - lateBy);
	}

	// Independent reference: `r` (a strided or unstrided reply, possibly a layer slice, whose windows start at r["from"]) against `base`,
	// the unstrided reply covering [baseFrom, ..). `to` is the request's exclusive end. Float: extrema over the non-NaN values, group-size
	// weighted mean over the non-NaN means (unweighted when the weights sum to 0); category: the counts of the window's last sample.
	void ExpectWindowsMatchBase(const Json::Value& base, const Json::Value& r, uint32_t stride, uint32_t baseFrom, uint32_t to)
	{
		const uint32_t from = r["from"].asUInt();
		const uint32_t windows = r["count"].asUInt();
		const uint32_t baseN = base["count"].asUInt();
		ASSERT_EQ(r["samples"].size(), windows);
		auto winStart = [&](uint32_t w) { return uint64_t(from) + uint64_t(w) * stride; };
		auto winEnd = [&](uint32_t w) { const uint64_t e = uint64_t(from) + uint64_t(w + 1) * stride; return e < to ? e : uint64_t(to); };
		const std::vector<std::string> groups = r["groupSizes"].getMemberNames();
		for (const std::string& g : groups)
		{
			ASSERT_TRUE(base["groupSizes"].isMember(g)) << g;
			std::vector<uint16_t> bsz, sz;
			ASSERT_TRUE(DecodedExport::DecodeU16(base["groupSizes"][g], baseN, bsz));
			ASSERT_TRUE(DecodedExport::DecodeU16(r["groupSizes"][g], windows, sz));
			for (uint32_t w = 0; w < windows; ++w)
			{
				const uint32_t last = uint32_t(winEnd(w) - 1) - baseFrom;
				EXPECT_EQ(sz[w], bsz[last]) << g << " w" << w;
				EXPECT_TRUE(JsonEq(r["samples"][w], base["samples"][last])) << g << " w" << w;
			}
		}
		for (const std::string& layer : r["rows"].getMemberNames())
		{
			ASSERT_TRUE(base["rows"].isMember(layer)) << layer;
			const Json::Value& bl = base["rows"][layer];
			const Json::Value& rl = r["rows"][layer];
			EXPECT_TRUE(JsonEq(rl["kind"], bl["kind"])) << layer;
			EXPECT_TRUE(JsonEq(rl["first"], bl["first"])) << layer;
			EXPECT_EQ(rl["groups"].getMemberNames(), groups) << layer;
			const bool isFloat = bl["kind"].asString() == "float";
			const uint32_t enumCount = isFloat ? 0u : bl["values"].size();
			for (const std::string& g : groups)
			{
				std::vector<uint16_t> bsz;
				ASSERT_TRUE(DecodedExport::DecodeU16(base["groupSizes"][g], baseN, bsz));
				if (isFloat)
				{
					std::vector<float> bmn, bme, bmx, mn, me, mx;
					ASSERT_TRUE(DecodedExport::DecodeF32(bl["groups"][g]["min"], baseN, bmn));
					ASSERT_TRUE(DecodedExport::DecodeF32(bl["groups"][g]["mean"], baseN, bme));
					ASSERT_TRUE(DecodedExport::DecodeF32(bl["groups"][g]["max"], baseN, bmx));
					ASSERT_TRUE(DecodedExport::DecodeF32(rl["groups"][g]["min"], windows, mn));
					ASSERT_TRUE(DecodedExport::DecodeF32(rl["groups"][g]["mean"], windows, me));
					ASSERT_TRUE(DecodedExport::DecodeF32(rl["groups"][g]["max"], windows, mx));
					for (uint32_t w = 0; w < windows; ++w)
					{
						bool haveLo = false, haveHi = false;
						float lo = 0.f, hi = 0.f;
						double num = 0, den = 0, plain = 0;
						uint32_t nMean = 0;
						for (uint64_t s = winStart(w); s < winEnd(w); ++s)
						{
							const uint32_t i = uint32_t(s) - baseFrom;
							if (bmn[i] == bmn[i] && (!haveLo || bmn[i] < lo)) { lo = bmn[i]; haveLo = true; }
							if (bmx[i] == bmx[i] && (!haveHi || bmx[i] > hi)) { hi = bmx[i]; haveHi = true; }
							if (bme[i] == bme[i]) { num += double(bme[i]) * bsz[i]; den += bsz[i]; plain += bme[i]; ++nMean; }
						}
						if (haveLo) { EXPECT_EQ(Bits(mn[w]), Bits(lo)) << layer << " " << g << " min w" << w; } else { EXPECT_TRUE(mn[w] != mn[w]) << layer << " " << g << " min w" << w; }
						if (haveHi) { EXPECT_EQ(Bits(mx[w]), Bits(hi)) << layer << " " << g << " max w" << w; } else { EXPECT_TRUE(mx[w] != mx[w]) << layer << " " << g << " max w" << w; }
						if (nMean > 0) { EXPECT_NEAR(me[w], den > 0 ? num / den : plain / nMean, 1e-3) << layer << " " << g << " mean w" << w; }
						else { EXPECT_TRUE(me[w] != me[w]) << layer << " " << g << " mean w" << w; }
					}
				}
				else
				{
					std::vector<uint16_t> bc, c;
					ASSERT_TRUE(DecodedExport::DecodeU16(bl["groups"][g], baseN * enumCount, bc));
					ASSERT_TRUE(DecodedExport::DecodeU16(rl["groups"][g], windows * enumCount, c));
					for (uint32_t w = 0; w < windows; ++w)
					{
						const uint32_t last = uint32_t(winEnd(w) - 1) - baseFrom;
						for (uint32_t k = 0; k < enumCount; ++k) { EXPECT_EQ(c[w * enumCount + k], bc[last * enumCount + k]) << layer << " " << g << " w" << w << " k" << k; }
					}
				}
			}
		}
	}

	// Follows `next` (from / layerOffset / structure when present), failing on a reply that does not advance the cursor or a runaway loop.
	Pages PageChecked(QFix& f, Json::Value args, uint32_t maxPages)
	{
		Pages p;
		bool haveKey = false;
		uint64_t lastKey = 0;
		for (uint32_t guard = 0; guard < maxPages; ++guard)
		{
			const Json::Value rep = f.Call(kAgg, args);
			if (IsError(rep)) { p.failed = true; p.error = rep["error"].asString(); return p; }
			const uint64_t key = (uint64_t(rep["from"].asUInt()) << 32) | (rep.isMember("layerOffset") ? rep["layerOffset"].asUInt() : 0u);
			if (haveKey && key <= lastKey) { p.failed = true; p.error = "cursor did not advance"; return p; }
			haveKey = true; lastKey = key;
			p.maxBytes = std::max(p.maxBytes, Size(rep));
			p.replies.push_back(rep);
			if (!rep.isMember("next")) { return p; }
			args["from"] = rep["next"]["from"];
			if (rep["next"].isMember("layerOffset")) { args["layerOffset"] = rep["next"]["layerOffset"]; } else { args.removeMember("layerOffset"); }
			if (rep["next"].isMember("structure")) { args["structure"] = rep["next"]["structure"]; } else { args.removeMember("structure"); }
		}
		p.failed = true;
		p.error = "runaway paging";
		return p;
	}

	// Every window of [from0, to) is delivered exactly once per layer, each page matches the reference and honours `budget`.
	void ExpectPagesCoverAndMatch(const Pages& p, const Json::Value& base, uint32_t stride, uint32_t from0, uint32_t to, size_t budget, size_t layerTotal)
	{
		ASSERT_FALSE(p.failed) << p.error;
		EXPECT_LE(p.maxBytes, budget);
		std::map<uint64_t, std::map<std::string, int>> seen;
		for (const Json::Value& rep : p.replies)
		{
			ExpectWindowsMatchBase(base, rep, stride, from0, to);
			const uint32_t from = rep["from"].asUInt();
			for (uint32_t w = 0; w < rep["count"].asUInt(); ++w)
			{
				for (const std::string& l : rep["rows"].getMemberNames()) { ++seen[uint64_t(from) + uint64_t(w) * stride][l]; }
			}
		}
		const uint64_t windows = to > from0 ? (uint64_t(to - from0) + stride - 1) / stride : 0u;
		ASSERT_EQ(seen.size(), windows);
		uint64_t expectStart = from0;
		for (const auto& kv : seen)
		{
			EXPECT_EQ(kv.first, expectStart);
			expectStart += stride;
			EXPECT_EQ(kv.second.size(), layerTotal) << kv.first;
			for (const auto& l : kv.second) { EXPECT_EQ(l.second, 1) << kv.first << " " << l.first; }
		}
		EXPECT_FALSE(p.replies.back().isMember("next"));
	}

	Json::Value WithField(Json::Value a, const char* k, const Json::Value& v) { a[k] = v; return a; }
	Json::Value FromTo(uint32_t from, uint32_t to) { Json::Value a = Obj(); a["from"] = from; a["to"] = to; return a; }

	// Smallest budget (below `hi`, which must succeed) at which `args` succeeds; binary search (success is monotonic in the budget).
	uint32_t MinSuccessBudget(QFix& f, const Json::Value& args, uint32_t hi)
	{
		uint32_t lo = HistoryQueries::kMinReplyBytes;
		while (lo < hi)
		{
			const uint32_t mid = lo + (hi - lo) / 2;
			f.Budget(mid);
			if (IsError(f.Call(kAgg, args))) { lo = mid + 1; } else { hi = mid; }
		}
		return lo;
	}
}

TEST(HistoryQueriesAggregatesEdge, EmptyRangeIsAnEmptyReplyWithAndWithoutStride)
{
	QFix f; f.Setup(); f.Run(6);
	for (uint32_t pos : { 0u, 3u, 6u })
	{
		for (uint32_t stride : { 1u, 4u, kMaxU32 })
		{
			Json::Value a = FromTo(pos, pos); a["stride"] = stride; a["layerOffset"] = 0;
			const Json::Value r = f.Call(kAgg, a);
			ASSERT_FALSE(IsError(r)) << pos << " " << stride << " " << r.toStyledString();
			EXPECT_EQ(r["count"].asUInt(), 0u);
			EXPECT_EQ(r["from"].asUInt(), pos);
			EXPECT_EQ(r["samples"].size(), 0u);
			EXPECT_FALSE(r.isMember("next"));
			EXPECT_FALSE(r.isMember("layerCount"));
			EXPECT_EQ(r["rows"].size(), 3u);
			EXPECT_EQ(r.isMember("stride"), stride > 1u);
		}
	}
	// No sample recorded at all: the default range is empty too.
	QFix g; g.Setup();
	for (uint32_t stride : { 1u, 5u })
	{
		const Json::Value r = g.Call(kAgg, StrideArgs(stride));
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		EXPECT_EQ(r["count"].asUInt(), 0u);
		EXPECT_FALSE(r.isMember("next"));
	}
}

TEST(HistoryQueriesAggregatesEdge, LayerOffsetNeedsASampleInsideTheRange)
{
	QFix f; f.Setup(); f.Run(6);
	for (uint32_t stride : { 1u, 4u })
	{
		for (uint32_t pos : { 3u, 6u })
		{
			Json::Value a = FromTo(pos, pos); a["stride"] = stride; a["layerOffset"] = 1;
			EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "invalid cursor: layerOffset needs a sample inside the range")) << stride << " " << pos;
		}
	}
	// One sample inside the range is enough.
	Json::Value ok = FromTo(3, 4); ok["layerOffset"] = 1;
	EXPECT_FALSE(IsError(f.Call(kAgg, ok)));
}

TEST(HistoryQueriesAggregatesEdge, LayerOffsetBoundsAndTheLastLayerSlice)
{
	QFix f; f.Setup(); f.Run(10);
	for (uint32_t stride : { 1u, 4u })
	{
		Json::Value a = StrideArgs(stride);
		a["layerOffset"] = 3;		// == layerTotal
		EXPECT_TRUE(ErrorHas(f.Call(kAgg, a), "invalid cursor: layerOffset 3 is past the last layer")) << stride;
		a["layerOffset"] = 2;		// the last layer index: a slice of exactly one layer
		const Json::Value r = f.Call(kAgg, a);
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		EXPECT_EQ(r["layerOffset"].asUInt(), 2u);
		EXPECT_EQ(r["layerCount"].asUInt(), 1u);
		EXPECT_EQ(r["layerTotal"].asUInt(), 3u);
		EXPECT_EQ(r["rows"].size(), 1u);
		EXPECT_TRUE(r["rows"].isMember("region"));
		ASSERT_TRUE(r.isMember("next"));
		EXPECT_EQ(r["next"]["from"].asUInt(), stride);
		EXPECT_EQ(r["next"]["layerOffset"].asUInt(), 0u);
		// The last window / sample: nothing follows.
		Json::Value tail = a; tail["from"] = stride == 1 ? 9u : 8u;
		const Json::Value t = f.Call(kAgg, tail);
		ASSERT_FALSE(IsError(t)) << t.toStyledString();
		EXPECT_FALSE(t.isMember("next"));
	}
	// With a layers filter the offset indexes the filtered list.
	Json::Value filt = Obj(); filt["layers"] = StrList({ "temp", "region" }); filt["layerOffset"] = 1;
	const Json::Value r = f.Call(kAgg, filt);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["layerTotal"].asUInt(), 2u);
	EXPECT_EQ(r["layerCount"].asUInt(), 1u);
	EXPECT_TRUE(r["rows"].isMember("region"));
	EXPECT_FALSE(r["rows"].isMember("temp"));
	filt["layerOffset"] = 2;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, filt), "layerOffset 2 is past the last layer"));
	filt["groups"] = StrList({ "world" }); filt["stride"] = 3;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, filt), "layerOffset 2 is past the last layer"));
	// No layers selected at all: any offset is past the (empty) list; offset 0 is an empty, valid reply.
	Json::Value none = Obj(); none["layers"] = Json::Value(Json::arrayValue);
	const Json::Value e = f.Call(kAgg, none);
	ASSERT_FALSE(IsError(e)) << e.toStyledString();
	EXPECT_EQ(e["rows"].size(), 0u);
	EXPECT_EQ(e["count"].asUInt(), 10u);
	none["layerOffset"] = 1;
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, none), "past the last layer"));
}

TEST(HistoryQueriesAggregatesEdge, RangeOutsideTheRetainedWindowIsAnErrorWithStrideAndGroups)
{
	RecorderBudgets b;
	b.mAggregateBytes = AggregateRowBytes(1, 6, 4) * 5;	// window [7, 12)
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 1 << 20;
	QFix f; f.Setup(&b); f.Run(12);
	ASSERT_EQ(f.r.rec->AggregateFirstRetained(), 7u);
	const uint32_t samples = f.r.rec->SampleCount();
	for (uint32_t stride : { 1u, 2u, 100u, kMaxU32 })
	{
		for (bool groups : { false, true })
		{
			for (uint32_t off : { 0u, 1u })
			{
				auto call = [&](uint32_t from, uint32_t to)
				{
					Json::Value a = FromTo(from, to); a["stride"] = stride; a["layerOffset"] = off;
					if (groups) { a["groups"] = StrList({ "world" }); }
					return f.Call(kAgg, a);
				};
				EXPECT_TRUE(ErrorHas(call(6, 12), "outside the retained")) << stride << groups << off;
				EXPECT_TRUE(ErrorHas(call(0, 3), "outside the retained")) << stride << groups << off;
				EXPECT_TRUE(ErrorHas(call(7, 13), "outside the retained")) << stride << groups << off;
				EXPECT_TRUE(ErrorHas(call(13, 13), "outside the retained")) << stride << groups << off;
			}
		}
	}
	EXPECT_EQ(f.r.rec->SampleCount(), samples);
	// Inside the window the same stride works, windows start at the retained first sample.
	Json::Value ok = StrideArgs(2); ok["groups"] = StrList({ "world" });
	const Json::Value r = f.Call(kAgg, ok);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["from"].asUInt(), 7u);
	EXPECT_EQ(r["count"].asUInt(), 3u);		// [7,9) [9,11) [11,12)
	Json::Value baseArgs = ok; baseArgs.removeMember("stride");
	ExpectWindowsMatchBase(f.Call(kAgg, baseArgs), r, 2, 7, 12);
}

TEST(HistoryQueriesAggregatesEdge, ARecorderWithOnlyTheWorldGroup)
{
	QFix f;
	f.r.grid.AddFloatLayer("temp", 40.0f);
	f.r.grid.AddFloatLayer("t2", 2.0f);
	f.r.grid.AddFloatLayer("t3", 3.0f);
	f.r.grid.AddCategoryLayer("biome", 1);
	ASSERT_TRUE(f.r.schema.Load({ FloatEntry("temp", 0, 100), FloatEntry("t2", 0, 100), FloatEntry("t3", 0, 100), EnumEntry("biome", 3) }));
	f.r.Start();
	f.q = std::make_unique<HistoryQueries>(*f.r.rec);
	ASSERT_TRUE(f.q->Register(f.registrar));
	for (uint32_t i = 0; i < 11; ++i)
	{
		for (uint32_t c = 0; c < 12; ++c)
		{
			f.r.grid.Set("temp", c, float((i * 5 + c * 3) % 97) + 0.5f);
			f.r.grid.Set("t2", c, float((i * 3 + c * 7) % 91) + 0.25f);
			f.r.grid.Set("t3", c, float((i * 11 + c) % 83) + 0.75f);
			f.r.grid.Set("biome", c, float((c + i) % 3));
		}
		f.r.grid.BumpVersion("temp"); f.r.grid.BumpVersion("t2"); f.r.grid.BumpVersion("t3"); f.r.grid.BumpVersion("biome");
		f.r.Take();
	}
	const Json::Value base = f.Call(kAgg, Obj());
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	ASSERT_EQ(base["groupSizes"].size(), 1u);
	ASSERT_TRUE(base["groupSizes"].isMember("world"));
	for (uint32_t stride : { 1u, 3u, 4u, 11u, 12u })
	{
		const Json::Value r = f.Call(kAgg, StrideArgs(stride));
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		EXPECT_EQ(r["count"].asUInt(), (11 + stride - 1) / stride);
		EXPECT_EQ(r["groupSizes"].size(), 1u);
		ExpectWindowsMatchBase(base, r, stride, 0, 11);
	}
	Json::Value w = StrideArgs(3); w["groups"] = StrList({ "world" });
	EXPECT_TRUE(JsonEq(f.Call(kAgg, w), f.Call(kAgg, StrideArgs(3))));
	// Slicing with a single group.
	const uint32_t whole = StrideSliceBudget(f, 3, Obj(), 0);
	const uint32_t budget = MinSuccessBudget(f, WithField(FromTo(0, 3), "stride", 3), whole) + 40;
	ASSERT_LT(budget, whole);
	f.Budget(budget);
	const Pages p = PageChecked(f, StrideArgs(3), 1000);
	EXPECT_GT(p.replies.size(), 4u);
	ExpectPagesCoverAndMatch(p, base, 3, 0, 11, budget, 4);
}

TEST(HistoryQueriesAggregatesEdge, GroupsListedTwiceEqualGroupsListedOnce)
{
	QFix f; f.Setup(); f.Run(10);
	const Json::Value base = f.Call(kAgg);
	const std::vector<std::string> names = base["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 2u);
	for (uint32_t stride : { 1u, 4u })
	{
		Json::Value once = StrideArgs(stride); once["groups"] = StrList({ names[1].c_str() });
		Json::Value twice = StrideArgs(stride); twice["groups"] = StrList({ names[1].c_str(), names[1].c_str() });
		Json::Value mixed = StrideArgs(stride); mixed["groups"] = StrList({ names[1].c_str(), names[0].c_str(), names[1].c_str() });
		Json::Value two = StrideArgs(stride); two["groups"] = StrList({ names[0].c_str(), names[1].c_str() });
		const Json::Value r1 = f.Call(kAgg, once), r2 = f.Call(kAgg, twice);
		ASSERT_FALSE(IsError(r2)) << r2.toStyledString();
		EXPECT_EQ(r2["groupSizes"].size(), 1u);
		EXPECT_TRUE(JsonEq(r1, r2));
		EXPECT_TRUE(JsonEq(f.Call(kAgg, mixed), f.Call(kAgg, two)));
	}
}

TEST(HistoryQueriesAggregatesEdge, AGroupWithNoMembersAtSomeSamplesStillAggregates)
{
	QFix f; f.Setup();
	// Region 0 holds every cell for samples 0..4 (groups 1 and 2 are empty), then the cells spread over all three.
	for (uint32_t i = 0; i < 11; ++i)
	{
		for (uint32_t c = 0; c < 12; ++c)
		{
			f.r.grid.Set("temp", c, float((i * 9 + c * 4) % 89) + 0.25f);
			f.r.grid.Set("biome", c, float((c + i) % 3));
			f.r.grid.Set("region", c, i < 5 ? 0.0f : float((c / 4 + i) % 3));
		}
		f.r.grid.BumpVersion("temp"); f.r.grid.BumpVersion("biome"); f.r.grid.BumpVersion("region");
		f.r.Take();
	}
	const Json::Value base = f.Call(kAgg);
	ASSERT_FALSE(IsError(base)) << base.toStyledString();
	bool sawEmpty = false;
	for (const std::string& g : base["groupSizes"].getMemberNames())
	{
		std::vector<uint16_t> sz;
		ASSERT_TRUE(DecodedExport::DecodeU16(base["groupSizes"][g], 11, sz));
		for (uint16_t s : sz) { if (s == 0) { sawEmpty = true; } }
	}
	ASSERT_TRUE(sawEmpty) << "fixture must contain an empty group";
	for (uint32_t stride : { 1u, 3u, 5u, 11u })
	{
		const Json::Value r = f.Call(kAgg, StrideArgs(stride));
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		ExpectWindowsMatchBase(base, r, stride, 0, 11);
		for (const std::string& g : base["groupSizes"].getMemberNames())
		{
			Json::Value a = StrideArgs(stride); a["groups"] = StrList({ g.c_str() });
			const Json::Value one = f.Call(kAgg, a);
			ASSERT_FALSE(IsError(one)) << g << " " << one.toStyledString();
			ExpectWindowsMatchBase(base, one, stride, 0, 11);
		}
	}
}

TEST(HistoryQueriesAggregatesEdge, StrideBoundaries)
{
	QFix f; f.Setup(); f.Run(12);
	const Json::Value base = f.Call(kAgg);
	ASSERT_FALSE(IsError(base));
	// stride exactly dividing the range: no partial window.
	for (uint32_t stride : { 1u, 2u, 3u, 4u, 6u, 12u })
	{
		const Json::Value r = f.Call(kAgg, StrideArgs(stride));
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		EXPECT_EQ(r["count"].asUInt(), 12u / stride);
		EXPECT_FALSE(r.isMember("next"));
		ExpectWindowsMatchBase(base, r, stride, 0, 12);
	}
	// range = stride * k + 1: the last window holds one sample.
	for (uint32_t stride : { 2u, 3u, 5u, 11u })
	{
		const uint32_t to = (11 / stride) * stride + 1;
		Json::Value a = StrideArgs(stride); a["to"] = to;
		const Json::Value r = f.Call(kAgg, a);
		ASSERT_FALSE(IsError(r)) << r.toStyledString();
		EXPECT_EQ(r["count"].asUInt(), (to + stride - 1) / stride);
		ExpectWindowsMatchBase(base, r, stride, 0, to);
	}
	{
		Json::Value a = StrideArgs(4); a["to"] = 9;		// [0,4) [4,8) [8,9)
		const Json::Value r = f.Call(kAgg, a);
		ASSERT_FALSE(IsError(r));
		ASSERT_EQ(r["count"].asUInt(), 3u);
		std::vector<float> mn, me, mx, bmn, bme, bmx;
		ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"]["world"]["min"], 3, mn));
		ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"]["world"]["mean"], 3, me));
		ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"]["world"]["max"], 3, mx));
		ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"]["world"]["min"], 12, bmn));
		ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"]["world"]["mean"], 12, bme));
		ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"]["world"]["max"], 12, bmx));
		EXPECT_EQ(Bits(mn[2]), Bits(bmn[8])); EXPECT_EQ(Bits(me[2]), Bits(bme[8])); EXPECT_EQ(Bits(mx[2]), Bits(bmx[8]));
		EXPECT_TRUE(JsonEq(r["samples"][2], base["samples"][8]));
	}
	// A huge stride never overflows from + w * stride: one window, no cursor, with or without slicing.
	for (uint32_t stride : { 1000u, 0x80000000u, 0xFFFFFFFEu, kMaxU32 })
	{
		f.Budget(15000);
		Json::Value a = StrideArgs(stride); a["from"] = 5;
		const Json::Value r = f.Call(kAgg, a);
		ASSERT_FALSE(IsError(r)) << stride << " " << r.toStyledString();
		EXPECT_EQ(r["count"].asUInt(), 1u);
		EXPECT_EQ(r["stride"].asUInt(), stride);
		EXPECT_FALSE(r.isMember("next"));
		ExpectWindowsMatchBase(base, r, stride, 0, 12);
		const uint32_t budget = StrideSliceBudget(f, stride, Obj(), 1, 12);
		f.Budget(budget);
		const Pages p = PageChecked(f, StrideArgs(stride), 1000);
		ExpectPagesCoverAndMatch(p, base, stride, 0, 12, budget, 3);
		EXPECT_GE(p.replies.size(), 2u);
	}
	f.Budget(15000);
	// stride beyond 32 bits is rejected, not wrapped.
	Json::Value big = Obj(); big["stride"] = Json::UInt64(4294967296ull);
	EXPECT_TRUE(ErrorHas(f.Call(kAgg, big), "param 'stride' must be an integer >= 1"));
}

TEST(HistoryQueriesAggregatesEdge, StrideOneWithEveryOtherFieldEqualsTheUnstridedRequest)
{
	QFix f; f.Setup(); f.Run(12);
	Json::Value plain = FromTo(2, 11);
	plain["layers"] = StrList({ "temp", "region" });
	plain["groups"] = StrList({ "world" });
	Json::Value one = plain; one["stride"] = 1;
	for (uint32_t budget : { 15000u, 700u, 450u })
	{
		f.Budget(budget);
		const Pages a = PageChecked(f, plain, 1000), b = PageChecked(f, one, 1000);
		ASSERT_FALSE(a.failed) << budget << " " << a.error;
		ASSERT_FALSE(b.failed) << budget << " " << b.error;
		ASSERT_EQ(a.replies.size(), b.replies.size()) << budget;
		for (size_t i = 0; i < a.replies.size(); ++i)
		{
			EXPECT_TRUE(JsonEq(a.replies[i], b.replies[i])) << budget << " page " << i;
			EXPECT_FALSE(b.replies[i].isMember("stride"));
		}
	}
}

TEST(HistoryQueriesAggregatesEdge, OldShapeRequestsKeepTheirReplyShape)
{
	QFix f; f.Setup(); f.Run(10);
	Json::Value req = FromTo(1, 8); req["layers"] = StrList({ "temp", "biome", "region" });
	const Json::Value r = f.Call(kAgg, req);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	std::vector<std::string> keys = r.getMemberNames();
	std::sort(keys.begin(), keys.end());
	EXPECT_EQ(keys, (std::vector<std::string>{ "count", "from", "groupSizes", "rows", "samples", "structure" }));
	EXPECT_EQ(r["from"].asUInt(), 1u);
	EXPECT_EQ(r["count"].asUInt(), 7u);
	// Listing every group is the same as no groups field.
	const Json::Value base = f.Call(kAgg, Obj());
	Json::Value all = Obj(); all["groups"] = Json::Value(Json::arrayValue);
	for (const std::string& g : base["groupSizes"].getMemberNames()) { all["groups"].append(g); }
	EXPECT_TRUE(JsonEq(f.Call(kAgg, all), base));
	// An old-shape cursor stays {from} only when whole samples fit.
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value x = f.Call(kAgg); return (IsError(x) || x.isMember("layerCount")) ? -1 : int(x["count"].asUInt()); };
	const int b1 = SmallestBudgetFor(countAt, 1, HistoryQueries::kMinReplyBytes, 15000);
	ASSERT_GT(b1, 0);
	f.Budget(uint32_t(b1) + 8);
	const Json::Value first = f.Call(kAgg, Obj());
	ASSERT_FALSE(IsError(first));
	ASSERT_TRUE(first.isMember("next"));
	EXPECT_FALSE(first.isMember("layerCount"));
	EXPECT_FALSE(first.isMember("stride"));
	EXPECT_EQ(first["next"].getMemberNames(), std::vector<std::string>{ "from" });
}

TEST(HistoryQueriesAggregatesEdge, LateLayerWithStrideAndSlicing)
{
	for (const char* late : { "temp", "biome" })
	{
		const uint32_t lateBy = std::strcmp(late, "temp") == 0 ? 3u : 6u;
		QFix f; SetupLate(f, late, lateBy, 10);
		const Json::Value base = f.Call(kAgg);
		ASSERT_FALSE(IsError(base)) << base.toStyledString();
		ASSERT_EQ(base["rows"][late]["first"].asUInt(), lateBy) << late;
		for (uint32_t stride : { 1u, 2u, 3u, 4u, 7u, 10u })
		{
			f.Budget(15000);
			const Json::Value r = f.Call(kAgg, StrideArgs(stride));
			ASSERT_FALSE(IsError(r)) << late << stride << r.toStyledString();
			ExpectWindowsMatchBase(base, r, stride, 0, 10);
			// Sliced: one byte under a whole window, and near the minimum slice budget.
			const uint32_t tight = StrideSliceBudget(f, stride, Obj(), 1, std::min(stride, 10u));
			const uint32_t minimum = MinSuccessBudget(f, WithField(FromTo(0, std::min(stride, 10u)), "stride", stride), tight) + 40;
			for (uint32_t budget : { tight, minimum })
			{
				f.Budget(budget);
				const Pages p = PageChecked(f, StrideArgs(stride), 1000);
				ExpectPagesCoverAndMatch(p, base, stride, 0, 10, budget, 3);
			}
		}
	}
	// Stated explicitly: a float window entirely before the layer's first is NaN; a partly covered window uses the covered samples only.
	{
		QFix f; SetupLate(f, "temp", 3, 10);
		const Json::Value base = f.Call(kAgg);
		const Json::Value r = f.Call(kAgg, StrideArgs(3));		// [0,3) all before first=3; [3,6) fully covered
		std::vector<float> mn, bmn;
		ASSERT_TRUE(DecodedExport::DecodeF32(r["rows"]["temp"]["groups"]["world"]["min"], 4, mn));
		ASSERT_TRUE(DecodedExport::DecodeF32(base["rows"]["temp"]["groups"]["world"]["min"], 10, bmn));
		EXPECT_TRUE(mn[0] != mn[0]);
		EXPECT_FALSE(mn[1] != mn[1]);
		const Json::Value r4 = f.Call(kAgg, StrideArgs(4));		// [0,4): only sample 3 is valid
		ASSERT_TRUE(DecodedExport::DecodeF32(r4["rows"]["temp"]["groups"]["world"]["min"], 3, mn));
		EXPECT_EQ(Bits(mn[0]), Bits(bmn[3]));
	}
	// Category layer: the window's last sample decides, also when it is before first (all zero counts).
	{
		QFix f; SetupLate(f, "biome", 6, 10);
		const Json::Value r = f.Call(kAgg, StrideArgs(4));		// [0,4) last=3 < 6; [4,8) last=7 >= 6; [8,10)
		std::vector<uint16_t> c;
		ASSERT_TRUE(DecodedExport::DecodeU16(r["rows"]["biome"]["groups"]["world"], 3 * 3, c));
		EXPECT_EQ(c[0] + c[1] + c[2], 0);
		EXPECT_EQ(c[3] + c[4] + c[5], 12);
		EXPECT_EQ(c[6] + c[7] + c[8], 12);
	}
}

TEST(HistoryQueriesAggregatesEdge, SliceBudgetBoundariesAreExactAndTheCursorAlwaysAdvances)
{
	for (uint32_t stride : { 1u, 4u })
	{
		QFix f; f.Setup(); f.Run(10);
		const Json::Value args = StrideArgs(stride);
		const Json::Value base = f.Call(kAgg);
		const Json::Value firstWindow = WithField(FromTo(0, stride), "stride", stride);
		const uint32_t whole = uint32_t(Size(f.Call(kAgg, firstWindow)));
		std::map<uint32_t, uint32_t> bestAt;		// k -> smallest budget giving k layers
		uint32_t prevK = 0, errors = 0;
		for (uint32_t b = HistoryQueries::kMinReplyBytes; b < whole; ++b)
		{
			f.Budget(b);
			const Json::Value r = f.Call(kAgg, firstWindow);
			if (IsError(r)) { ++errors; EXPECT_EQ(prevK, 0u) << "an error after a success at a smaller budget: " << b; continue; }
			ASSERT_TRUE(r.isMember("layerCount")) << stride << " " << b;
			EXPECT_LE(Size(r), size_t(b)) << stride << " " << b;
			const uint32_t k = r["layerCount"].asUInt();
			EXPECT_GE(k, prevK) << "layerCount shrank when the budget grew: " << b;
			prevK = k;
			if (!bestAt.count(k)) { bestAt[k] = b; }
		}
		EXPECT_GT(errors, 0u);
		ASSERT_TRUE(bestAt.count(1)) << stride;
		// Exactly at a threshold the reply has that many layers and (nearly) fills the budget; one byte less gives fewer.
		for (const auto& kv : bestAt)
		{
			f.Budget(kv.second);
			const Json::Value at = f.Call(kAgg, firstWindow);
			EXPECT_EQ(at["layerCount"].asUInt(), kv.first);
			EXPECT_LE(Size(at), size_t(kv.second));
			EXPECT_GE(Size(at) + 12, size_t(kv.second)) << "the budget should be nearly full at the threshold, k=" << kv.first;
			f.Budget(kv.second - 1);
			const Json::Value under = f.Call(kAgg, firstWindow);
			if (kv.first == 1) { EXPECT_TRUE(ErrorHas(under, "fewer groups")); }
			else { EXPECT_LT(under["layerCount"].asUInt(), kv.first); }
		}
		// Following the cursor over the whole run terminates, advances, fits, and covers every (window, layer, group) at the reference values.
		std::vector<uint32_t> budgets{ whole - 1, whole, whole + 40 };
		for (const auto& kv : bestAt) { budgets.push_back(kv.second + 40); }
		for (uint32_t b : budgets)
		{
			f.Budget(b);
			const Pages p = PageChecked(f, args, 10u * 3u * 2u + 10u);
			ExpectPagesCoverAndMatch(p, base, stride, 0, 10, b, 3);
		}
		f.Budget(15000);
	}
}

TEST(HistoryQueriesAggregatesEdge, EveryStrideBudgetAndFilterCombinationReassemblesToTheReference)
{
	QFix f; f.Setup(); f.Run(11);
	struct Filter { const char* name; Json::Value args; size_t layers; };
	std::vector<Filter> filters;
	filters.push_back({ "none", Obj(), 3 });
	filters.push_back({ "world", WithField(Obj(), "groups", StrList({ "world" })), 3 });
	filters.push_back({ "layers", WithField(Obj(), "layers", StrList({ "temp", "region" })), 2 });
	filters.push_back({ "both", WithField(WithField(Obj(), "layers", StrList({ "region", "temp" })), "groups", StrList({ "world" })), 2 });
	for (const Filter& flt : filters)
	{
		f.Budget(15000);
		const Json::Value base = f.Call(kAgg, flt.args);
		ASSERT_FALSE(IsError(base)) << flt.name << base.toStyledString();
		for (uint32_t stride : { 1u, 2u, 3u, 4u, 7u, 10u, 11u, 12u, kMaxU32 })
		{
			f.Budget(15000);
			const Json::Value a = WithField(flt.args, "stride", stride);
			const Json::Value firstWindow = WithField(WithField(a, "from", 0u), "to", std::min(stride, 11u));
			const uint32_t whole = uint32_t(Size(f.Call(kAgg, firstWindow)));
			const uint32_t minimum = MinSuccessBudget(f, firstWindow, whole) + 40;
			for (uint32_t budget : { minimum, std::max(whole - 1, minimum), std::max(whole + 30, minimum), 15000u })
			{
				f.Budget(budget);
				const Pages p = PageChecked(f, a, 11u * 3u + 11u);
				ASSERT_FALSE(p.failed) << flt.name << " stride " << stride << " budget " << budget << ": " << p.error;
				ExpectPagesCoverAndMatch(p, base, stride, 0, 11, budget, flt.layers);
			}
		}
	}
}

TEST(HistoryQueriesAggregatesEdge, TheCursorKeepsFiltersAndStrideAcrossASliceWithLayersAndGroups)
{
	QFix f; f.Setup(); f.Run(10);
	Json::Value filt = Obj();
	filt["layers"] = StrList({ "temp", "region" });
	const std::vector<std::string> names = f.Call(kAgg)["groupSizes"].getMemberNames();
	ASSERT_GE(names.size(), 4u);
	filt["groups"] = StrList({ names[0].c_str(), names[1].c_str(), names[2].c_str() });
	f.Budget(15000);
	const uint32_t whole = StrideSliceBudget(f, 3, filt, 0);
	filt["stride"] = 3;
	const uint32_t tight = MinSuccessBudget(f, WithField(WithField(filt, "from", 0u), "to", 3u), whole) + 8;
	ASSERT_LT(tight, whole);
	f.Budget(tight);
	const Json::Value first = f.Call(kAgg, filt);
	ASSERT_FALSE(IsError(first)) << first.toStyledString();
	ASSERT_TRUE(first.isMember("layerCount"));
	EXPECT_EQ(first["layerTotal"].asUInt(), 2u);
	const std::vector<std::string> expectGroups(names.begin(), names.begin() + 3);
	EXPECT_EQ(first["groupSizes"].getMemberNames(), expectGroups);
	// Repeating the same request with only the cursor fields changed continues the same windows.
	Json::Value next = filt;
	for (const char* k : { "from", "layerOffset", "structure" }) { next[k] = first["next"][k]; }
	const Json::Value second = f.Call(kAgg, next);
	ASSERT_FALSE(IsError(second)) << second.toStyledString();
	EXPECT_EQ(second["stride"].asUInt(), 3u);
	EXPECT_EQ(second["layerOffset"].asUInt(), first["layerCount"].asUInt());
	EXPECT_EQ(second["groupSizes"].getMemberNames(), first["groupSizes"].getMemberNames());
	EXPECT_EQ(second["layerTotal"].asUInt(), 2u);
}

TEST(HistoryRowsReduce, ExtremaAreRightForAllNegativeValues)
{
	const float mins[3] = { -3.f, -9.f, -5.f }, means[3] = { -2.f, -8.f, -4.f }, maxs[3] = { -1.f, -7.f, -6.f };
	const uint16_t w[3] = { 1, 1, 1 };
	const bool ok[3] = { true, true, true };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 3);
	EXPECT_FLOAT_EQ(r.min, -9.f);
	EXPECT_FLOAT_EQ(r.max, -1.f);
	EXPECT_FLOAT_EQ(r.mean, (-2.f - 8.f - 4.f) / 3.f);
	// A valid zero after an invalid sample seeds the extrema and the mean.
	const float z[2] = { 0.f, 0.f };
	const uint16_t zw[2] = { 3, 3 };
	const bool zok[2] = { false, true };
	const Dia::CellHistory::Rows::WindowStats s = Dia::CellHistory::Rows::ReduceFloatWindow(z, z, z, zw, zok, 2);
	EXPECT_TRUE(s.valid);
	EXPECT_EQ(Bits(s.min), Bits(0.f)); EXPECT_EQ(Bits(s.max), Bits(0.f)); EXPECT_EQ(Bits(s.mean), Bits(0.f));
}

TEST(HistoryRowsReduce, ZeroWeightSampleDoesNotPullTheMeanAndLargeWeightsDoNotOverflow)
{
	const float mins[3] = { 1.f, 1.f, 1.f }, means[3] = { 100.f, 20.f, 40.f }, maxs[3] = { 3.f, 3.f, 3.f };
	const uint16_t w[3] = { 0, 1, 3 };
	const bool ok[3] = { true, true, true };
	const Dia::CellHistory::Rows::WindowStats r = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 3);
	EXPECT_NEAR(r.mean, (20.f * 1 + 40.f * 3) / 4.f, 1e-4f);		// the zero-weight 100 does not count
	const uint16_t big[2] = { 65535, 65535 };
	const float m2[2] = { 10.f, 30.f };
	const bool ok2[2] = { true, true };
	const Dia::CellHistory::Rows::WindowStats b = Dia::CellHistory::Rows::ReduceFloatWindow(m2, m2, m2, big, ok2, 2);
	EXPECT_NEAR(b.mean, 20.f, 1e-4f);
}

TEST(HistoryRowsReduce, NaNMeansWithValidMinMaxAndMixedInvalidSamples)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	// Valid samples whose means are all NaN: min and max still reduce, the mean is NaN.
	const float mins[2] = { 2.f, 1.f }, means[2] = { nan, nan }, maxs[2] = { 5.f, 9.f };
	const uint16_t w[2] = { 4, 4 };
	const bool ok[2] = { true, true };
	const Dia::CellHistory::Rows::WindowStats a = Dia::CellHistory::Rows::ReduceFloatWindow(mins, means, maxs, w, ok, 2);
	EXPECT_TRUE(a.valid);
	EXPECT_FLOAT_EQ(a.min, 1.f); EXPECT_FLOAT_EQ(a.max, 9.f);
	EXPECT_TRUE(a.mean != a.mean);
	// All min/max NaN on valid samples: NaN extrema, a finite weighted mean.
	const float m3[2] = { nan, nan }, me3[2] = { 10.f, 40.f };
	const uint16_t w3[2] = { 3, 1 };
	const Dia::CellHistory::Rows::WindowStats b = Dia::CellHistory::Rows::ReduceFloatWindow(m3, me3, m3, w3, ok, 2);
	EXPECT_TRUE(b.valid);
	EXPECT_TRUE(b.min != b.min); EXPECT_TRUE(b.max != b.max);
	EXPECT_NEAR(b.mean, 17.5f, 1e-4f);
	// Invalid samples carrying NaN interleaved with valid ones: only the valid ones count.
	const float mm[4] = { nan, 4.f, nan, 8.f }, mean4[4] = { nan, 6.f, nan, 12.f }, mx4[4] = { nan, 7.f, nan, 15.f };
	const uint16_t w4[4] = { 9, 1, 9, 2 };
	const bool ok4[4] = { false, true, false, true };
	const Dia::CellHistory::Rows::WindowStats c = Dia::CellHistory::Rows::ReduceFloatWindow(mm, mean4, mx4, w4, ok4, 4);
	EXPECT_FLOAT_EQ(c.min, 4.f); EXPECT_FLOAT_EQ(c.max, 15.f);
	EXPECT_NEAR(c.mean, (6.f * 1 + 12.f * 2) / 3.f, 1e-4f);
}

// ---- events ---------------------------------------------------------------------------------------------------------

TEST(HistoryQueriesEvents, DefaultReturnsEveryEventInTheFilesShape)
{
	QFix f; f.Setup(); f.Run(12);
	ASSERT_TRUE(f.Export());
	const Json::Value r = f.Call(kEvents);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_FALSE(r.isMember("next"));
	EXPECT_TRUE(JsonEq(r["events"], f.dec.root["events"]));
	EXPECT_GT(r["events"].size(), 12u);
}

TEST(HistoryQueriesEvents, ChunksIncludingMidSampleCursorsReassembleToTheFile)
{
	QFix f; f.Setup(); f.Run(12);
	ASSERT_TRUE(f.Export());
	for (uint32_t budget : { 1500u, 600u, 300u })
	{
		f.Budget(budget);
		const Pages p = PageEvents(f, Obj());
		ASSERT_FALSE(p.failed) << budget << " " << p.error;
		EXPECT_GT(p.replies.size(), 1u);
		EXPECT_LE(p.maxBytes, size_t(budget));
		Json::Value all(Json::arrayValue);
		bool midSample = false;
		for (const Json::Value& rep : p.replies)
		{
			for (const Json::Value& e : rep["events"]) { all.append(e); }
			if (rep.isMember("next") && rep["next"]["skip"].asUInt() > 0) { midSample = true; }
		}
		EXPECT_TRUE(JsonEq(all, f.dec.root["events"])) << budget;
		if (budget == 300u) { EXPECT_TRUE(midSample); }	// sample 2 has 7 events: a cursor lands inside it
	}
}

TEST(HistoryQueriesEvents, BoundaryBudgetsHoldExactlyOneThenTwoEvents)
{
	QFix f; f.Setup(); f.Run(12);
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value r = f.Call(kEvents); return IsError(r) ? -1 : int(r["events"].size()); };
	const int b1 = SmallestBudgetFor(countAt, 1, HistoryQueries::kMinReplyBytes, 15000);
	ASSERT_GT(b1, 0);
	EXPECT_EQ(countAt(uint32_t(b1) - 1), -1);
	f.Budget(uint32_t(b1));
	EXPECT_EQ(Size(f.Call(kEvents)), size_t(b1));
	const int b2 = SmallestBudgetFor(countAt, 2, uint32_t(b1), 15000);
	ASSERT_GT(b2, b1);
	EXPECT_EQ(countAt(uint32_t(b2) - 1), 1);
	f.Budget(uint32_t(b2));
	EXPECT_EQ(Size(f.Call(kEvents)), size_t(b2));
}

TEST(HistoryQueriesEvents, EventCapPerReplyIsHonoured)
{
	QFix f; f.Setup(); f.Run(12);
	ASSERT_TRUE(f.Export());
	f.q->SetMaxEvents(3);
	const Pages p = PageEvents(f, Obj());
	ASSERT_FALSE(p.failed) << p.error;
	Json::Value all(Json::arrayValue);
	for (const Json::Value& rep : p.replies) { EXPECT_LE(rep["events"].size(), 3u); for (const Json::Value& e : rep["events"]) { all.append(e); } }
	EXPECT_TRUE(JsonEq(all, f.dec.root["events"]));
	EXPECT_EQ(HistoryQueries::kMaxEventsPerReply, 2000u);
}

TEST(HistoryQueriesEvents, RangeAndTypeFiltersApply)
{
	QFix f; f.Setup(); f.Run(12);
	Json::Value a = Obj();
	a["from"] = 4; a["to"] = 6;
	const Json::Value r = f.Call(kEvents, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	ASSERT_GT(r["events"].size(), 0u);
	for (const Json::Value& e : r["events"]) { EXPECT_GE(e["sample"].asUInt(), 4u); EXPECT_LT(e["sample"].asUInt(), 6u); }
	a = Obj(); a["types"] = StrList({ "cow.drought" });
	EXPECT_GT(f.Call(kEvents, a)["events"].size(), 0u);
	a["types"] = StrList({ "cow.other" });
	const Json::Value none = f.Call(kEvents, a);
	ASSERT_FALSE(IsError(none));
	EXPECT_EQ(none["events"].size(), 0u);
	EXPECT_FALSE(none.isMember("next"));
}

TEST(HistoryQueriesEvents, ErrorsChangeNothing)
{
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 4 * 100;	// room for a few events only
	QFix f; f.Setup(&b); f.Run(12);
	ASSERT_GT(f.r.rec->EventsLog().GapCount(), 0u);
	const uint32_t samples = f.r.rec->SampleCount();
	const uint32_t evCount = f.r.rec->EventsLog().EventCount();
	auto call = [&](Json::Value a) { return f.Call(kEvents, a); };
	Json::Value a = Obj(); a["from"] = 0; a["to"] = 3;
	EXPECT_TRUE(ErrorHas(call(a), "retained"));
	a = Obj(); a["to"] = 99;
	EXPECT_TRUE(ErrorHas(call(a), "outside the retained"));
	a = Obj(); a["from"] = 9; a["to"] = 8;
	EXPECT_TRUE(ErrorHas(call(a), "from"));
	a = Obj(); a["types"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "types"));
	a = Obj(); a["types"] = Json::Value(Json::arrayValue); a["types"].append(1);
	EXPECT_TRUE(ErrorHas(call(a), "types"));
	a = Obj(); a["skip"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "skip"));
	EXPECT_EQ(f.r.rec->SampleCount(), samples);
	EXPECT_EQ(f.r.rec->EventsLog().EventCount(), evCount);
}

TEST(HistoryQueriesEvents, InvalidCursorIsAnError)
{
	QFix f; f.Setup(); f.Run(6);
	Json::Value a = Obj(); a["from"] = 2; a["skip"] = 8;	// sample 2 has 7 events
	EXPECT_TRUE(ErrorHas(f.Call(kEvents, a), "cursor"));
	a["skip"] = 7;
	EXPECT_FALSE(IsError(f.Call(kEvents, a)));	// skipping all of a sample is a valid cursor (it resumes at the next event)
	a = Obj(); a["from"] = 100; a["skip"] = 1;
	EXPECT_TRUE(IsError(f.Call(kEvents, a)));
}

// ---- cells ----------------------------------------------------------------------------------------------------------

TEST(HistoryQueriesCells, HappyPathReturnsTheFilesQuantisedSeries)
{
	QFix f; f.Setup(); f.Run(10);
	ASSERT_TRUE(f.Export(true));
	Json::Value a = Obj(); a["layer"] = "temp";
	const Json::Value r = f.Call(kCells, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	const Json::Value& fl = f.dec.root["cells"]["temp"];
	EXPECT_EQ(r["layer"].asString(), "temp");
	EXPECT_EQ(r["kind"].asString(), "float");
	EXPECT_TRUE(JsonEq(r["min"], fl["min"]));
	EXPECT_TRUE(JsonEq(r["max"], fl["max"]));
	EXPECT_EQ(r["width"].asUInt(), 4u);
	EXPECT_EQ(r["height"].asUInt(), 3u);
	EXPECT_TRUE(JsonEq(r["from"], fl["first"]));
	EXPECT_EQ(r["to"].asUInt(), fl["first"].asUInt() + fl["count"].asUInt());
	EXPECT_TRUE(r["cells"].isNull());
	EXPECT_EQ(r["cellOffset"].asUInt(), 0u);
	EXPECT_EQ(r["cellCount"].asUInt(), 12u);
	EXPECT_EQ(r["cellTotal"].asUInt(), 12u);
	EXPECT_FALSE(r.isMember("next"));
	EXPECT_TRUE(JsonEq(r["data"], fl["data"]));
}

TEST(HistoryQueriesCells, ChunksIncludingSplitRowsReassembleToTheFile)
{
	QFix f; f.Setup(); f.Run(14);
	ASSERT_TRUE(f.Export(true));
	// A budget just under one full row forces column slices; a larger one packs several rows.
	Json::Value one = Obj(); one["layer"] = "temp"; one["from"] = 5; one["to"] = 6;
	const size_t rowReply = Size(f.Call(kCells, one));
	for (uint32_t budget : { uint32_t(rowReply) + 15, uint32_t(rowReply) + 70, 15000u })
	{
		f.Budget(budget);
		Json::Value a = Obj(); a["layer"] = "temp";
		const Pages p = PageCells(f, a);
		ASSERT_FALSE(p.failed) << budget << " " << p.error;
		EXPECT_LE(p.maxBytes, size_t(budget));
		bool partial = false;
		for (const Json::Value& rep : p.replies) { if (rep["cellCount"].asUInt() < 12u) { partial = true; } }
		if (budget < rowReply + 30) { EXPECT_TRUE(partial) << budget; }
		std::map<uint32_t, std::vector<uint16_t>> rows;
		AssembleCells(p.replies, rows, 12);
		const uint32_t first = f.dec.root["cells"]["temp"]["first"].asUInt();
		const uint32_t count = f.dec.root["cells"]["temp"]["count"].asUInt();
		ASSERT_EQ(rows.size(), count) << budget;
		for (uint32_t s = first; s < first + count; ++s)
		{
			std::vector<uint16_t> file;
			ASSERT_TRUE(f.dec.CellCodes("temp", s, file));
			EXPECT_EQ(rows[s], file) << budget << " sample " << s;
		}
	}
}

TEST(HistoryQueriesCells, BoundaryBudgetsHoldExactlyOneThenTwoRows)
{
	QFix f; f.Setup(); f.Run(10);
	auto rowsAt = [&](uint32_t b)
	{
		f.Budget(b);
		Json::Value a = Obj(); a["layer"] = "temp";
		const Json::Value r = f.Call(kCells, a);
		return (IsError(r) || r["cellCount"].asUInt() < 12u) ? 0 : int(r["data"].size());
	};
	const int b1 = SmallestBudgetFor(rowsAt, 1, HistoryQueries::kMinReplyBytes, 15000);
	ASSERT_GT(b1, 0);
	EXPECT_EQ(rowsAt(uint32_t(b1) - 1), 0);
	const int b2 = SmallestBudgetFor(rowsAt, 2, uint32_t(b1), 15000);
	ASSERT_GT(b2, b1);
	EXPECT_EQ(rowsAt(uint32_t(b2) - 1), 1);
	f.Budget(uint32_t(b2));
	Json::Value a = Obj(); a["layer"] = "temp";
	EXPECT_EQ(Size(f.Call(kCells, a)), size_t(b2));
}

TEST(HistoryQueriesCells, ACellListReturnsOnlyThoseCells)
{
	QFix f; f.Setup(); f.Run(8);
	ASSERT_TRUE(f.Export(true));
	Json::Value a = Obj(); a["layer"] = "temp";
	a["cells"] = Json::Value(Json::arrayValue);
	for (uint32_t c : { 11u, 0u, 5u }) { a["cells"].append(c); }
	const Json::Value r = f.Call(kCells, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["cellTotal"].asUInt(), 3u);
	EXPECT_EQ(r["cellCount"].asUInt(), 3u);
	EXPECT_TRUE(JsonEq(r["cells"], a["cells"]));
	const uint32_t first = r["from"].asUInt();
	for (uint32_t i = 0; i < r["data"].size(); ++i)
	{
		std::vector<uint16_t> got, file;
		ASSERT_TRUE(DecodedExport::DecodeU16(r["data"][i], 3, got));
		ASSERT_TRUE(f.dec.CellCodes("temp", first + i, file));
		EXPECT_EQ(got[0], file[11]);
		EXPECT_EQ(got[1], file[0]);
		EXPECT_EQ(got[2], file[5]);
	}
}

TEST(HistoryQueriesCells, CategoryLayersUseBytes)
{
	QFix f;
	f.r.grid.AddCategoryLayer("biome", 1);
	ASSERT_TRUE(f.r.schema.Load({ EnumEntry("biome", 3, "{\"track\":true}") }));
	f.r.Start();
	f.q = std::make_unique<HistoryQueries>(*f.r.rec);
	ASSERT_TRUE(f.q->Register(f.registrar));
	for (uint32_t i = 0; i < 5; ++i) { for (uint32_t c = 0; c < 12; ++c) { f.r.grid.Set("biome", c, float((c + i) % 3)); } f.r.grid.BumpVersion("biome"); f.r.Take(); }
	ASSERT_TRUE(f.Export(true));
	Json::Value a = Obj(); a["layer"] = "biome";
	const Json::Value r = f.Call(kCells, a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["kind"].asString(), "category");
	EXPECT_TRUE(JsonEq(r["data"], f.dec.root["cells"]["biome"]["data"]));
}

TEST(HistoryQueriesCells, ErrorsChangeNothing)
{
	RecorderBudgets b;
	b.mCellBytes = 24 * 5;	// window of 5 samples
	b.mAggregateBytes = 1 << 20;
	b.mEventBytes = 1 << 20;
	QFix f; f.Setup(&b); f.Run(12);
	ASSERT_EQ(f.r.rec->FirstRetained(), 7u);
	const uint32_t samples = f.r.rec->SampleCount();
	auto call = [&](Json::Value a) { return f.Call(kCells, a); };
	Json::Value a = Obj();
	EXPECT_TRUE(ErrorHas(call(a), "layer"));								// missing
	a["layer"] = 5;
	EXPECT_TRUE(ErrorHas(call(a), "layer"));								// not a string
	a["layer"] = "nope";
	EXPECT_TRUE(ErrorHas(call(a), "nope"));									// unknown layer
	a["layer"] = "biome";
	const Json::Value untracked = call(a);
	EXPECT_TRUE(ErrorHas(untracked, "not tracked"));
	EXPECT_TRUE(ErrorHas(untracked, "track it first"));
	a["layer"] = "temp";
	a["from"] = 0;
	const Json::Value evicted = call(a);
	EXPECT_TRUE(ErrorHas(evicted, "outside the retained"));
	EXPECT_TRUE(ErrorHas(evicted, "7"));
	a = Obj(); a["layer"] = "temp"; a["to"] = 13;
	EXPECT_TRUE(ErrorHas(call(a), "outside the retained"));
	a = Obj(); a["layer"] = "temp"; a["from"] = 10; a["to"] = 9;
	EXPECT_TRUE(ErrorHas(call(a), "from"));
	a = Obj(); a["layer"] = "temp"; a["from"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "from"));
	a = Obj(); a["layer"] = "temp"; a["cells"] = "all";
	EXPECT_TRUE(ErrorHas(call(a), "cells"));
	a["cells"] = Json::Value(Json::arrayValue);
	EXPECT_TRUE(ErrorHas(call(a), "cells"));								// empty list
	a["cells"].append(12);
	EXPECT_TRUE(ErrorHas(call(a), "out of range"));
	a["cells"] = Json::Value(Json::arrayValue);
	for (uint32_t i = 0; i < HistoryQueries::kMaxCellList + 1; ++i) { a["cells"].append(0); }
	EXPECT_TRUE(ErrorHas(call(a), "too many"));
	a = Obj(); a["layer"] = "temp"; a["cellOffset"] = 12;
	EXPECT_TRUE(ErrorHas(call(a), "cursor"));
	a["cellOffset"] = "x";
	EXPECT_TRUE(ErrorHas(call(a), "cellOffset"));
	EXPECT_EQ(f.r.rec->SampleCount(), samples);
	a = Obj(); a["layer"] = "temp";
	const Json::Value ok = call(a);
	ASSERT_FALSE(IsError(ok)) << ok.toStyledString();
	EXPECT_EQ(ok["from"].asUInt(), 7u);
	EXPECT_EQ(ok["data"].size(), 5u);
}

TEST(HistoryQueriesCells, NoSamplesYetIsAnErrorNamingTheReason)
{
	QFix f; f.Setup();
	Json::Value a = Obj(); a["layer"] = "temp";
	EXPECT_TRUE(IsError(f.Call(kCells, a)));
}

// ---- Large fixture: every reply under the host limit --------------------------------------------------------------

namespace
{
	// 104x64 grid, 20 float layers + a 26-value grouping layer (aggregate:false) = 27 groups; l00 is tracked.
	void SetupLarge(QFix& f, uint32_t samples)
	{
		std::vector<std::string> entries;
		char name[16];
		for (int i = 0; i < 20; ++i)
		{
			snprintf(name, sizeof(name), "l%02d", i);
			f.r.grid.AddFloatLayer(name, 1.0f);
			entries.push_back(FloatEntry(name, 0, 1000, i == 0 ? "{\"track\":true}" : nullptr));
		}
		f.r.grid.AddCategoryLayer("grp", 0);
		entries.push_back(EnumEntry("grp", 26, "{\"group\":true,\"aggregate\":false}"));
		entries.push_back(kEventEntry);
		ASSERT_TRUE(f.r.schema.Load(entries));
		f.r.Start(nullptr, false);
		f.q = std::make_unique<HistoryQueries>(*f.r.rec);
		ASSERT_TRUE(f.q->Register(f.registrar));
		for (uint32_t i = 0; i < samples; ++i)
		{
			for (int l = 0; l < 20; ++l)
			{
				snprintf(name, sizeof(name), "l%02d", l);
				f.r.grid.SetAll(name, float((i * 31 + l * 17) % 997));
				for (uint32_t c = 0; c < 40; ++c) { f.r.grid.Set(name, (c * 163 + i) % 6656, float((i + c + l) % 900)); }
				f.r.grid.BumpVersion(name);
			}
			for (uint32_t c = 0; c < 6656; ++c) { f.r.grid.Set("grp", c, float((c / 256 + i / 20) % 26)); }
			f.r.grid.BumpVersion("grp");
			if (i % 5 == 0) { HistoryEvent e; e.type = StringCRC("cow.drought"); e.payloadJson = "{\"weeks\":3}"; e.text = "dry spell"; e.group.mLayer = StringCRC("grp"); e.group.mValue = i % 26; f.r.rec->Emit(e); }
			f.r.Take();
		}
	}
}

TEST(HistoryQueriesLarge, EveryReplyStaysUnderTheHostLimitAndReassemblesToTheFile)
{
	QFix f(104, 64);
	SetupLarge(f, 120);
	ASSERT_EQ(f.r.rec->RecordedLayerCount(), 20u);
	ASSERT_EQ(f.r.rec->GroupCount(), 27u);
	f.opt.cells = true;
	ASSERT_TRUE(f.Export(true)) << f.res.error;
	size_t maxMeta = 0, maxAgg = 0, maxEv = 0, maxCells = 0;
	uint32_t aggPages = 0, cellPages = 0;

	// meta: core plus the schema pages.
	{
		const Json::Value m = f.Call(kMeta);
		ASSERT_FALSE(IsError(m)) << m.toStyledString();
		maxMeta = std::max(maxMeta, Size(m));
		EXPECT_FALSE(m.isMember("next"));
		Json::Value args = Obj(); args["part"] = "schema";
		Json::Value entries(Json::arrayValue);
		for (int guard = 0; guard < 1000; ++guard)
		{
			const Json::Value rep = f.Call(kMeta, args);
			ASSERT_FALSE(IsError(rep)) << rep.toStyledString();
			maxMeta = std::max(maxMeta, Size(rep));
			for (const Json::Value& e : rep["entries"]) { entries.append(e); }
			if (!rep.isMember("next")) { break; }
			args["offset"] = rep["next"]["offset"];
		}
		EXPECT_TRUE(JsonEq(entries, f.dec.root["schema"]["entries"]));
	}

	// The whole 20-layer row set of even one sample cannot fit in one reply: it is returned as a layer slice with a cursor.
	{
		const Json::Value slice = f.Call(kAgg);
		ASSERT_FALSE(IsError(slice)) << slice.toStyledString();
		EXPECT_EQ(slice["count"].asUInt(), 1u);
		EXPECT_EQ(slice["layerTotal"].asUInt(), 20u);
		EXPECT_LT(slice["layerCount"].asUInt(), 20u);
		ASSERT_TRUE(slice.isMember("next"));
		EXPECT_EQ(slice["next"]["layerOffset"].asUInt(), slice["layerCount"].asUInt());
		EXPECT_LE(Size(slice), size_t(kHostLimit));
	}

	// aggregates: one layer at a time (the catch-up pattern for a wide run), reassembled against the file.
	{
		AggAssembly merged;
		for (int l = 0; l < 20; ++l)
		{
			char name[16];
			snprintf(name, sizeof(name), "l%02d", l);
			Json::Value a = Obj(); a["layers"] = StrList({ name });
			const Pages p = PageAggregates(f, a);
			ASSERT_FALSE(p.failed) << name << " " << p.error;
			EXPECT_LE(p.maxBytes, size_t(HistoryQueries::kMaxReplyBytes));
			maxAgg = std::max(maxAgg, p.maxBytes);
			aggPages += uint32_t(p.replies.size());
			AggAssembly as;
			Assemble(p.replies, as);
			for (auto& kv : as.f32bits) { merged.f32bits[kv.first] = kv.second; }
			for (auto& kv : as.layerMeta) { merged.layerMeta[kv.first] = kv.second; }
			merged.sizes = as.sizes;
			merged.samples = as.samples;
			merged.from = as.from;
			merged.total = as.total;
		}
		for (const std::string& layer : f.dec.root["aggregates"].getMemberNames())
		{
			for (const char* g : { "world", "grp=a", "grp=z" })
			{
				for (const char* which : { "min", "mean", "max" })
				{
					std::vector<float> file;
					uint32_t ff = 0;
					ASSERT_TRUE(f.dec.FloatSeries(layer.c_str(), g, which, file, ff));
					const std::vector<uint32_t>& got = merged.f32bits.at(layer + "|" + g + "|" + which);
					for (size_t i = 0; i < file.size(); ++i) { ASSERT_EQ(got[ff - merged.from + i], Bits(file[i])) << layer << " " << g; }
				}
			}
		}
		for (const std::string& g : f.dec.root["groupSizes"].getMemberNames())
		{
			std::vector<uint16_t> file;
			ASSERT_TRUE(f.dec.GroupSizes(g.c_str(), file));
			EXPECT_EQ(merged.sizes.at(g), file) << g;
		}
	}

	// events
	{
		f.Budget(HistoryQueries::kMaxReplyBytes);
		const Pages p = PageEvents(f, Obj());
		ASSERT_FALSE(p.failed) << p.error;
		maxEv = p.maxBytes;
		Json::Value all(Json::arrayValue);
		for (const Json::Value& rep : p.replies) { for (const Json::Value& e : rep["events"]) { all.append(e); } }
		EXPECT_TRUE(JsonEq(all, f.dec.root["events"]));
	}

	// cells: a full row is 6,656 cells = 13.3 KB of codes = 17.8 KB of base64, so rows are split into column slices.
	{
		Json::Value a = Obj(); a["layer"] = "l00"; a["from"] = 0; a["to"] = 4;
		const Pages p = PageCells(f, a);
		ASSERT_FALSE(p.failed) << p.error;
		maxCells = p.maxBytes;
		cellPages = uint32_t(p.replies.size());
		EXPECT_LE(p.maxBytes, size_t(HistoryQueries::kMaxReplyBytes));
		EXPECT_GE(p.replies.size(), 4u * 2u);
		std::map<uint32_t, std::vector<uint16_t>> rows;
		AssembleCells(p.replies, rows, 6656);
		ASSERT_EQ(rows.size(), 4u);
		for (uint32_t s = 0; s < 4; ++s)
		{
			std::vector<uint16_t> file;
			ASSERT_TRUE(f.dec.CellCodes("l00", s, file));
			EXPECT_EQ(rows[s], file) << s;
		}
	}
	EXPECT_LE(maxMeta, size_t(kHostLimit));
	EXPECT_LE(maxEv, size_t(kHostLimit));
	EXPECT_LE(maxCells, size_t(kHostLimit));
	printf("[ queries ] Large 104x64, 20 layers x 27 groups, 120 samples: max reply bytes meta %zu, aggregates(1 layer) %zu (%u pages), events %zu, cells %zu (%u pages for 4 samples)\n",
		maxMeta, maxAgg, aggPages, maxEv, maxCells, cellPages);
}

// ---- observation ----------------------------------------------------------------------------------------------------

TEST(HistoryQueriesObservation, CountsRepliesByKindAndBytesServed)
{
	QFix f; f.Setup(); f.Run(6);
	QueryObservation obs;
	f.q->SetObservation(&obs);
	const uint64_t m0 = C("dia.cellhistory.query_meta"), a0 = C("dia.cellhistory.query_aggregates"), e0 = C("dia.cellhistory.query_events"),
		c0 = C("dia.cellhistory.query_cells"), b0 = C("dia.cellhistory.query_bytes");
	size_t bytes = 0;
	bytes += Size(f.Call(kMeta));
	bytes += Size(f.Call(kAgg));
	bytes += Size(f.Call(kAgg));
	bytes += Size(f.Call(kEvents));
	Json::Value a = Obj(); a["layer"] = "temp";
	bytes += Size(f.Call(kCells, a));
	EXPECT_EQ(C("dia.cellhistory.query_meta") - m0, 1u);
	EXPECT_EQ(C("dia.cellhistory.query_aggregates") - a0, 2u);
	EXPECT_EQ(C("dia.cellhistory.query_events") - e0, 1u);
	EXPECT_EQ(C("dia.cellhistory.query_cells") - c0, 1u);
	EXPECT_EQ(C("dia.cellhistory.query_bytes") - b0, bytes);
	// Errors are not counted.
	a["layer"] = "nope";
	f.Call(kCells, a);
	EXPECT_EQ(C("dia.cellhistory.query_cells") - c0, 1u);
}

TEST(HistoryQueriesObservation, ClampedRepliesWarnOncePerInstance)
{
	QFix f; f.Setup(); f.Run(30);
	LogCapture log("HIS");
	f.Budget(900);
	EXPECT_EQ(log.Warnings("clamped"), 0u);
	const Json::Value r1 = f.Call(kAgg);
	ASSERT_TRUE(r1.isMember("next"));
	EXPECT_EQ(log.Warnings("clamped"), 1u);
	f.Call(kAgg);
	f.Call(kEvents);
	EXPECT_EQ(log.Warnings("clamped"), 1u);
	// An unclamped reply does not warn.
	QFix g; g.Setup(); g.Run(3);
	LogCapture log2("HIS");
	g.Call(kAgg);
	EXPECT_EQ(log2.Warnings("clamped"), 0u);
}

TEST(HistoryQueriesObservation, LargestReplyGaugeTracksTheBiggestReplyServed)
{
	QFix f; f.Setup(); f.Run(6);
	QueryObservation obs;
	f.q->SetObservation(&obs);
	auto gauge = []() { auto* g = MetricRegistry::Instance().FindGauge(StringCRC("dia.cellhistory.query_max_reply_bytes")); return g ? g->Value() : -1.0; };
	ASSERT_GE(gauge(), 0.0);
	size_t biggest = 0;
	for (const char* name : { kEvents, kAgg, kMeta, kAgg })
	{
		biggest = std::max(biggest, Size(f.Call(name)));
		EXPECT_DOUBLE_EQ(gauge(), double(biggest));
	}
	// A smaller reply never lowers it; an error does not move it.
	Json::Value bad = Obj(); bad["layer"] = "nope";
	f.Call(kCells, bad);
	EXPECT_DOUBLE_EQ(gauge(), double(biggest));
}

TEST(HistoryQueriesObservation, CountsSlicedAndStridedAggregatesReplies)
{
	QFix f; f.Setup(); f.Run(10);
	const uint32_t sliceBudget = StrideSliceBudget(f, 4);
	QueryObservation obs;
	f.q->SetObservation(&obs);
	const uint64_t s0 = C("dia.cellhistory.query_aggregates_slices"), t0 = C("dia.cellhistory.query_aggregates_strided");
	auto slices = [&]() { return C("dia.cellhistory.query_aggregates_slices") - s0; };
	auto strided = [&]() { return C("dia.cellhistory.query_aggregates_strided") - t0; };
	// A plain whole-sample reply and the other query kinds count neither.
	ASSERT_FALSE(IsError(f.Call(kAgg)));
	f.Call(kMeta);
	f.Call(kEvents);
	EXPECT_EQ(slices(), 0u); EXPECT_EQ(strided(), 0u);
	// stride 1 is not a strided request.
	f.Call(kAgg, StrideArgs(1));
	EXPECT_EQ(strided(), 0u);
	// A strided request that fits whole.
	ASSERT_FALSE(IsError(f.Call(kAgg, StrideArgs(4))));
	EXPECT_EQ(strided(), 1u); EXPECT_EQ(slices(), 0u);
	// A strided slice counts as both.
	f.Budget(sliceBudget);
	const Json::Value ss = f.Call(kAgg, StrideArgs(4));
	ASSERT_TRUE(ss.isMember("layerCount"));
	EXPECT_EQ(strided(), 2u); EXPECT_EQ(slices(), 1u);
	// An unstrided slice (layerOffset > 0) counts as a slice only.
	f.Budget(15000);
	Json::Value a = Obj(); a["layerOffset"] = 1;
	ASSERT_FALSE(IsError(f.Call(kAgg, a)));
	EXPECT_EQ(strided(), 2u); EXPECT_EQ(slices(), 2u);
	// Errors are not counted.
	f.Call(kAgg, StrideArgs(0));
	a["layerOffset"] = 1000;
	f.Call(kAgg, a);
	f.Budget(HistoryQueries::kMinReplyBytes);
	f.Call(kAgg, StrideArgs(4));
	EXPECT_EQ(strided(), 2u); EXPECT_EQ(slices(), 2u);
}

#endif // DIA_DEBUG
