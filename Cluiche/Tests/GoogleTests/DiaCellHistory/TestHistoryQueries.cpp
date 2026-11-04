#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryQueries.h>
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
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value r = f.Call(kAgg); return IsError(r) ? -1 : int(r["count"].asUInt()); };
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
	auto countAt = [&](uint32_t b) { f.Budget(b); const Json::Value r = f.Call(kAgg); return IsError(r) ? -1 : int(r["count"].asUInt()); };
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

TEST(HistoryQueriesAggregates, OneSampleThatDoesNotFitIsAnErrorNotAnOversizedReply)
{
	QFix f; f.Setup(); f.Run(5);
	f.Budget(HistoryQueries::kMinReplyBytes);
	const Json::Value r = f.Call(kAgg);
	EXPECT_TRUE(ErrorHas(r, "reply too large"));
	EXPECT_TRUE(ErrorHas(r, "fewer layers"));
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

	// The whole 20-layer row set of even one sample cannot fit in one reply: the error says to request fewer layers.
	EXPECT_TRUE(ErrorHas(f.Call(kAgg), "fewer layers"));

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

#endif // DIA_DEBUG
