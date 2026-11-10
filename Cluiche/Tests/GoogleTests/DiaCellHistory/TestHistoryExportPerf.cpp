#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/HistoryTopics.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

#include <json/json.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Feature 5 perf proxies on the Large grid (104x64 = 6,656 cells, 20 float layers, 27 groups, 2,080 weekly samples = 40 years).
// SLOW_ suites only run with `dia run googletest --config Debug --all --filter="*SLOW_HistoryExportPerf*"`. These are Debug numbers with generous
// bounds; they print what they measured. The Release gate (AC 15: export without cells under 2 s) is checked by the S7 system task.

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	using Clock = std::chrono::steady_clock;
	double MsSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

	const uint32_t kSamples = 2080;
	const char* const kEventEntry = "{\"key\":\"cow.drought\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"Drought\",\"description\":\"d\","
		"\"payload\":[{\"name\":\"weeks\",\"type\":\"int\",\"description\":\"d\"}]}";

	struct FakePub : ITopicPublisher
	{
		uint32_t messages = 0;
		uint64_t maxMessageBytes = 0;
		bool Publish(StringCRC, const Json::Value& payload) override
		{
			++messages;
			Json::StreamWriterBuilder w;
			w["indentation"] = "";
			maxMessageBytes = std::max<uint64_t>(maxMessageBytes, Json::writeString(w, payload).size());
			return true;
		}
		uint32_t SubscriberCount() const override { return 1; }
	};

	struct StepClock
	{
		double t = 0.0;
		static double Read(void* user) { return static_cast<StepClock*>(user)->t; }
	};

	// One Large recorder shared by the tests of this file (building 2,080 samples takes a while in Debug). l00 is tracked.
	struct LargeRun
	{
		Rig r{ 104, 64 };
		double buildMs = 0.0;
		bool ok = false;

		void Sample(uint32_t i)
		{
			char name[16];
			for (int l = 0; l < 20; ++l)
			{
				snprintf(name, sizeof(name), "l%02d", l);
				r.grid.SetAll(name, float((i * 31 + l * 17) % 997));
				for (uint32_t c = 0; c < 40; ++c) { r.grid.Set(name, (c * 163 + i) % 6656, float((i + c + l) % 900)); }
				r.grid.BumpVersion(name);
			}
			for (uint32_t c = 0; c < 6656; ++c) { r.grid.Set("grp", c, float((c / 256 + i / 20) % 26)); }
			r.grid.BumpVersion("grp");
			if (i % 5 == 0)
			{
				HistoryEvent e;
				e.type = StringCRC("cow.drought"); e.payloadJson = "{\"weeks\":3}"; e.text = "dry spell";
				e.group.mLayer = StringCRC("grp"); e.group.mValue = i % 26;
				r.rec->Emit(e);
			}
			r.Take();
		}

		void Build()
		{
			std::vector<std::string> entries;
			char name[16];
			for (int i = 0; i < 20; ++i)
			{
				snprintf(name, sizeof(name), "l%02d", i);
				r.grid.AddFloatLayer(name, 1.0f);
				entries.push_back(FloatEntry(name, 0, 1000, i == 0 ? "{\"track\":true}" : nullptr));
			}
			r.grid.AddCategoryLayer("grp", 0);
			entries.push_back(EnumEntry("grp", 26, "{\"group\":true,\"aggregate\":false}"));
			entries.push_back(kEventEntry);
			ASSERT_TRUE(r.schema.Load(entries));
			r.Start(nullptr, false);
			const auto t0 = Clock::now();
			for (uint32_t i = 0; i < kSamples; ++i) { Sample(i); }
			buildMs = MsSince(t0);
			ok = r.rec->SampleCount() == kSamples && r.rec->RecordedLayerCount() == 20u && r.rec->GroupCount() == 27u;
		}
	};

	LargeRun& Large()
	{
		static LargeRun* run = nullptr;
		if (run == nullptr)
		{
			run = new LargeRun();		// intentionally leaked: avoids teardown order issues at process exit
			run->Build();
			printf("[ perf ] built the Large recorder: %u samples x 20 float layers x 6,656 cells, 27 groups, in %.0f ms (Debug)\n", kSamples, run->buildMs);
		}
		return *run;
	}
}

TEST(SLOW_HistoryExportPerf, LargeFortyYearExportWithoutCellsTimeAndSize)
{
	LargeRun& run = Large();
	ASSERT_TRUE(run.ok);
	TempFolder tmp;
	ExportOptions opt;
	opt.app = "Perf";
	opt.generatedAt = "2026-01-01T00:00:00Z";
	ExportResult res;
	const std::string path = tmp.File("large.cell_history.json");
	const auto t0 = Clock::now();
	ASSERT_TRUE(HistoryExporter::WriteFile(*run.r.rec, opt, path.c_str(), res)) << res.error;
	const double exportMs = MsSince(t0);
	printf("[ perf ] AC15 proxy (Debug): export without cells %.0f ms, %llu bytes (%.2f MB), %u samples\n", exportMs, (unsigned long long)res.bytes, double(res.bytes) / 1048576.0, res.samples);
	EXPECT_LT(res.bytes, 20ull * 1000 * 1000);		// AC 1 (the 4x3 and 104x64 files are the same size: the aggregates do not depend on the cell count)
	EXPECT_LT(exportMs, 30000.0);					// Debug is allowed to be slow; the < 2 s bound is the Release gate (S7)

	// Where the time goes: the row-by-row Aggregate() lookups the exporter makes (one per sample x layer x group),
	// timed on their own over the same data.
	const CellHistoryRecorder& rec = *run.r.rec;
	std::vector<StringCRC> groups;
	{
		for (uint32_t g = 0; g < rec.Aggregates().GroupCount(); ++g) { groups.push_back(g == 0 ? StringCRC("world") : rec.Aggregates().GroupAt(g)); }
	}
	char name[16];
	const auto t1 = Clock::now();
	uint64_t lookups = 0;
	float sink = 0.0f;
	for (uint32_t s = 0; s < kSamples; ++s)
	{
		for (int l = 0; l < 20; ++l)
		{
			snprintf(name, sizeof(name), "l%02d", l);
			const StringCRC layer(name);
			for (const StringCRC& g : groups)
			{
				AggregateStats st;
				if (rec.Aggregate(s, layer, g, st)) { sink += st.mMean; }
				++lookups;
			}
		}
	}
	const double lookupMs = MsSince(t1);
	printf("[ perf ] row-by-row Aggregate() lookups alone: %llu lookups in %.0f ms (%.0f%% of the export; sink %g)\n", (unsigned long long)lookups, lookupMs, 100.0 * lookupMs / exportMs, double(sink));
	EXPECT_EQ(lookups, uint64_t(kSamples) * 20u * 27u);
	if (exportMs > 10000.0) { ADD_FAILURE() << "Debug export over 10 s: add a bulk row accessor (spec note in the 5.2 report)"; }
}

TEST(SLOW_HistoryExportPerf, TopicBuildForATwentyLayerTwentySevenGroupSample)
{
	LargeRun& run = Large();
	ASSERT_TRUE(run.ok);
	// A fresh topic object over the Large recorder, then more samples: one message per sample (interval 0), per-Tick time measured.
	FakePub pub;
	StepClock clock;
	HistoryTopics topics(*run.r.rec, pub);
	topics.SetClock(&StepClock::Read, &clock);
	topics.SetMinAggregateInterval(0.0);
	topics.Tick();							// initialise the cursors at the head
	const uint32_t extra = 40;
	double totalMs = 0.0, worstMs = 0.0;
	for (uint32_t i = 0; i < extra; ++i)
	{
		run.Sample(kSamples + i);
		clock.t += 1.0;
		const auto t0 = Clock::now();
		topics.Tick();
		const double ms = MsSince(t0);
		totalMs += ms;
		worstMs = std::max(worstMs, ms);
	}
	EXPECT_GE(topics.AggregateMessages(), uint64_t(extra));
	printf("[ perf ] topic build (1 new sample, 20 layers x 27 groups, plus events): mean %.2f ms per Tick, worst %.2f ms, largest message %llu bytes, %u messages (Debug)\n",
		totalMs / extra, worstMs, (unsigned long long)pub.maxMessageBytes, pub.messages);
	EXPECT_LT(worstMs, 1000.0);
}

TEST(SLOW_HistoryExportPerf, WorstCaseQueryPageBuildTimes)
{
	LargeRun& run = Large();
	ASSERT_TRUE(run.ok);
	HistoryQueries q(*run.r.rec);

	// aggregates: one layer (all 27 groups), as many whole samples as fit in 15,000 bytes.
	{
		const int pages = 20;
		double total = 0.0, worst = 0.0;
		size_t maxBytes = 0;
		uint32_t count = 0;
		for (int p = 0; p < pages; ++p)
		{
			Json::Value args(Json::objectValue);
			args["from"] = Json::UInt(p * 97);
			args["layers"] = Json::Value(Json::arrayValue);
			args["layers"].append("l07");
			const auto t0 = Clock::now();
			const Json::Value rep = q.Aggregates(args);
			const double ms = MsSince(t0);
			ASSERT_FALSE(rep.isMember("error")) << rep["error"].asString();
			total += ms; worst = std::max(worst, ms);
			count = rep["count"].asUInt();
			Json::StreamWriterBuilder w; w["indentation"] = "";
			maxBytes = std::max(maxBytes, Json::writeString(w, rep).size());
		}
		printf("[ perf ] aggregates page (1 layer x 27 groups, %u samples per page, <= %zu bytes): mean %.2f ms, worst %.2f ms (Debug)\n", count, maxBytes, total / pages, worst);
		EXPECT_LT(worst, 2000.0);
	}
	// aggregates: all layers, one sample: the "reply too large" path still builds before it refuses.
	{
		Json::Value args(Json::objectValue);
		args["from"] = 0; args["to"] = 1;
		const auto t0 = Clock::now();
		const Json::Value rep = q.Aggregates(args);
		const double ms = MsSince(t0);
		printf("[ perf ] aggregates, all 20 layers, 1 sample: %.2f ms (%s)\n", ms, rep.isMember("error") ? "refused: reply too large" : "served");
		EXPECT_LT(ms, 2000.0);
	}
	// cells: tracked l00, a full 6,656-cell row does not fit, so the page is a column slice of one sample.
	{
		const int pages = 20;
		double total = 0.0, worst = 0.0;
		size_t maxBytes = 0;
		uint32_t cellCount = 0;
		const uint32_t first = run.r.rec->FirstRetained();
		for (int p = 0; p < pages; ++p)
		{
			Json::Value args(Json::objectValue);
			args["layer"] = "l00";
			args["from"] = Json::UInt(first + p);
			const auto t0 = Clock::now();
			const Json::Value rep = q.Cells(args);
			const double ms = MsSince(t0);
			ASSERT_FALSE(rep.isMember("error")) << rep["error"].asString();
			total += ms; worst = std::max(worst, ms);
			cellCount = rep["cellCount"].asUInt();
			Json::StreamWriterBuilder w; w["indentation"] = "";
			maxBytes = std::max(maxBytes, Json::writeString(w, rep).size());
		}
		printf("[ perf ] cells page (l00, %u cells per page, <= %zu bytes): mean %.2f ms, worst %.2f ms (Debug)\n", cellCount, maxBytes, total / pages, worst);
		EXPECT_LT(worst, 2000.0);
	}
}

#endif // DIA_DEBUG
