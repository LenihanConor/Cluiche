#include "HistoryTestRig.h"
#include <DiaCellHistory/SnapshotCommands.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/SnapshotObservation.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <json/json.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	namespace fs = std::filesystem;

	int64_t FakeNow(void*) { return 1792510000; }
	uint64_t FakeSeed(void*) { return 5; }

	// Every NowMs() call advances 5 ms: a take reads the clock 4 times (capture 5 ms + write 5 ms), a restore twice (5 ms).
	double gClock = 0.0;
	double StepClock(void*) { gClock += 5.0; return gClock; }

	double G(const char* n) { auto* g = MetricRegistry::Instance().FindGauge(StringCRC(n)); return g ? g->Value() : -1.0; }
	uint64_t C(const char* n) { auto* c = MetricRegistry::Instance().FindCounter(StringCRC(n)); return c ? c->Value() : 0; }

	const char* const kSnapshots = "dia.cellhistory.snapshots";
	const char* const kTakeMs = "dia.cellhistory.snapshot_take_ms";
	const char* const kRestoreMs = "dia.cellhistory.snapshot_restore_ms";
	const char* const kBytes = "dia.cellhistory.snapshot_bytes";
	const char* const kFailures = "dia.cellhistory.snapshot_failures";
	const char* const kQueries = "dia.cellhistory.snapshot_queries_served";

	struct Fix
	{
		Rig r;
		FakeSnapshotStore store;
		Dia::CellLayers::Testing::FakeMirrorRegistrar registrar;
		std::unique_ptr<SnapshotObservation> obs;
		std::unique_ptr<SnapshotManager> mgr;
		std::unique_ptr<SnapshotCommands> cmds;

		void Setup()
		{
			gClock = 0.0;
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
			r.Start();
			r.Take(); r.Take();
			store.SetWriteStateFiles(true);
			obs = std::make_unique<SnapshotObservation>(&StepClock, nullptr);
			mgr = std::make_unique<SnapshotManager>(*r.rec, r.registry, r.schema.service.get(), store);
			mgr->SetTimeProvider(&FakeNow, nullptr);
			mgr->SetResetSeedProvider(&FakeSeed, nullptr);
			mgr->SetObservation(obs.get());
			cmds = std::make_unique<SnapshotCommands>(*r.rec, *mgr, store);
			cmds->SetObservation(obs.get());
			ASSERT_TRUE(cmds->Register(registrar));
		}
		Json::Value Run(const char* cmd, const char* name, const char* layer = nullptr)
		{
			Json::Value a(Json::objectValue);
			a["name"] = name;
			if (layer) { a["layer"] = layer; }
			return registrar.Execute(cmd, a);
		}
		Json::Value List() { return registrar.Execute("cells.snapshot.list", Json::Value(Json::objectValue)); }
		void Take(const char* name)
		{
			ASSERT_FALSE(Run("cells.snapshot.take", name).isMember("error"));
			store.RunFrameBoundary();
		}
	};
}

TEST(SnapshotObservation, RegistersAllSixMetricsAndReconstructionReusesTheHandles)
{
	{
		SnapshotObservation a;
		auto& m = MetricRegistry::Instance();
		for (const char* g : { kSnapshots, kTakeMs, kRestoreMs, kBytes }) { EXPECT_NE(m.FindGauge(StringCRC(g)), nullptr) << g; }
		for (const char* c : { kFailures, kQueries }) { EXPECT_NE(m.FindCounter(StringCRC(c)), nullptr) << c; }
	}
	auto* before = MetricRegistry::Instance().FindGauge(StringCRC(kTakeMs));
	{
		SnapshotObservation b;
		EXPECT_EQ(MetricRegistry::Instance().FindGauge(StringCRC(kTakeMs)), before);		// no duplicate registration after destruct
	}
	EXPECT_EQ(MetricRegistry::Instance().FindGauge(StringCRC(kTakeMs)), before);
}

TEST(SnapshotObservation, ConstructorZeroesTheGauges)
{
	SnapshotObservation a;
	a.SetSnapshotCount(3); a.OnTakeCompleted(1, 2, 99); a.OnRestoreCompleted(7);
	SnapshotObservation b;
	EXPECT_EQ(G(kSnapshots), 0.0); EXPECT_EQ(G(kTakeMs), 0.0); EXPECT_EQ(G(kRestoreMs), 0.0); EXPECT_EQ(G(kBytes), 0.0);
}

TEST(SnapshotObservation, DirectCallsSetGaugesAndBumpCounters)
{
	SnapshotObservation o;
	const uint64_t f = C(kFailures), q = C(kQueries);
	o.SetSnapshotCount(4);
	o.OnTakeCompleted(1.5, 2.0, 1234);
	o.OnRestoreCompleted(9.0);
	o.OnFailure(); o.OnFailure();
	o.OnQueryServed();
	EXPECT_EQ(G(kSnapshots), 4.0);
	EXPECT_DOUBLE_EQ(G(kTakeMs), 3.5);
	EXPECT_EQ(G(kBytes), 1234.0);
	EXPECT_EQ(G(kRestoreMs), 9.0);
	EXPECT_EQ(C(kFailures), f + 2);
	EXPECT_EQ(C(kQueries), q + 1);
}

TEST(SnapshotObservation, InjectedClockIsUsed)
{
	gClock = 100.0;
	SnapshotObservation o(&StepClock, nullptr);
	EXPECT_DOUBLE_EQ(o.NowMs(), 105.0);
	SnapshotObservation def;
	const double a = def.NowMs();
	EXPECT_GE(def.NowMs(), a);
}

TEST(SnapshotObservation, TakeSuccessSetsTakeMsBytesAndCount)
{
	Fix f; f.Setup();
	f.Take("s1");
	EXPECT_DOUBLE_EQ(G(kTakeMs), 10.0);		// capture 5 + write 5
	const SnapshotRecord* rec = nullptr;
	for (uint32_t i = 0; i < f.mgr->Count(); ++i) { if (f.mgr->Records()[i].mUsed) { rec = &f.mgr->Records()[i]; } }
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(G(kBytes), double(rec->mBytes));
	EXPECT_GT(G(kBytes), 0.0);
	EXPECT_EQ(G(kSnapshots), 1.0);
	f.Take("s2");
	EXPECT_EQ(G(kSnapshots), 2.0);
}

TEST(SnapshotObservation, TakeStoreFailureCountsAndLeavesGaugesAlone)
{
	Fix f; f.Setup();
	f.Take("good");
	const double bytes = G(kBytes);
	const uint64_t fails = C(kFailures);
	f.store.FailNext("disk full");
	f.Take("bad");
	EXPECT_EQ(C(kFailures), fails + 1);
	EXPECT_EQ(G(kBytes), bytes);
	EXPECT_EQ(G(kSnapshots), 1.0);
}

TEST(SnapshotObservation, RefusedTakeAndRestoreCountAsFailures)
{
	Fix f; f.Setup();
	const uint64_t fails = C(kFailures);
	EXPECT_TRUE(f.Run("cells.snapshot.take", "bad name!").isMember("error"));
	EXPECT_EQ(C(kFailures), fails + 1);
	EXPECT_TRUE(f.Run("cells.snapshot.restore", "missing").isMember("error"));
	EXPECT_EQ(C(kFailures), fails + 2);
}

TEST(SnapshotObservation, RestoreSuccessSetsRestoreMs)
{
	Fix f; f.Setup();
	f.Take("s1");
	const uint64_t fails = C(kFailures);
	ASSERT_FALSE(f.Run("cells.snapshot.restore", "s1").isMember("error"));
	f.store.RunFrameBoundary();
	EXPECT_DOUBLE_EQ(G(kRestoreMs), 5.0);
	EXPECT_EQ(C(kFailures), fails);
}

TEST(SnapshotObservation, RestoreStoreFailureCountsAndKeepsRestoreMs)
{
	Fix f; f.Setup();
	f.Take("s1");
	ASSERT_FALSE(f.Run("cells.snapshot.restore", "s1").isMember("error"));
	f.store.RunFrameBoundary();
	const double ms = G(kRestoreMs);
	const uint64_t fails = C(kFailures);
	f.store.FailNext("load failed");
	ASSERT_FALSE(f.Run("cells.snapshot.restore", "s1").isMember("error"));
	f.store.RunFrameBoundary();
	EXPECT_EQ(C(kFailures), fails + 1);
	EXPECT_EQ(G(kRestoreMs), ms);
}

TEST(SnapshotObservation, DeleteUpdatesCountAndFailureCounts)
{
	Fix f; f.Setup();
	f.Take("s1"); f.Take("s2");
	ASSERT_EQ(G(kSnapshots), 2.0);
	const uint64_t fails = C(kFailures);
	EXPECT_FALSE(f.Run("cells.snapshot.delete", "s1").isMember("error"));
	EXPECT_EQ(G(kSnapshots), 1.0);
	EXPECT_EQ(C(kFailures), fails);
	EXPECT_TRUE(f.Run("cells.snapshot.delete", "nope").isMember("error"));
	EXPECT_EQ(C(kFailures), fails + 1);
}

TEST(SnapshotObservation, ListScanRefreshesTheCount)
{
	Fix f; f.Setup();
	f.Take("s1");
	f.obs->SetSnapshotCount(0);
	f.List();
	EXPECT_EQ(G(kSnapshots), 1.0);
}

TEST(SnapshotObservation, QueriesServedCountsEveryCall)
{
	Fix f; f.Setup();
	f.Take("s1");
	const uint64_t q = C(kQueries);
	EXPECT_FALSE(f.Run("cells.history.snapshot", "s1", "temp").isMember("error"));
	EXPECT_EQ(C(kQueries), q + 1);
	EXPECT_TRUE(f.Run("cells.history.snapshot", "s1", "nolayer").isMember("error"));
	EXPECT_EQ(C(kQueries), q + 2);
}

TEST(SnapshotObservation, NullObservationIsSafe)
{
	Fix f; f.Setup();
	f.mgr->SetObservation(nullptr);
	f.cmds->SetObservation(nullptr);
	const uint64_t q = C(kQueries), fails = C(kFailures);
	f.Take("s1");
	ASSERT_FALSE(f.Run("cells.snapshot.restore", "s1").isMember("error"));
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.Run("cells.snapshot.take", "bad name!").isMember("error"));
	f.Run("cells.history.snapshot", "s1", "temp");
	f.List();
	EXPECT_FALSE(f.Run("cells.snapshot.delete", "s1").isMember("error"));
	EXPECT_EQ(C(kQueries), q);
	EXPECT_EQ(C(kFailures), fails);
}

TEST(SnapshotObservation, OrphanWarningLogsOncePerNameForTheLifetime)
{
	Fix f; f.Setup();
	f.Take("a"); f.Take("b");
	f.store.Delete("a");
	LogCapture log("HIS");
	for (int i = 0; i < 3; ++i) { f.List(); }
	EXPECT_EQ(log.Warnings("orphan"), 1u);
	f.store.Delete("b");
	f.List();
	f.List();
	EXPECT_EQ(log.Warnings("orphan"), 2u);
	EXPECT_EQ(log.Warnings("'a'"), 1u);
}

TEST(SnapshotObservation, LogsTakeRestoreAndDeleteAtInfoAndStoreFailureAtError)
{
	Fix f; f.Setup();
	LogCapture log("HIS");
	f.Take("s1");
	EXPECT_EQ(log.Count(LogCapture::Level::kInfo, "take requested"), 1u);
	EXPECT_EQ(log.Count(LogCapture::Level::kInfo, "taken at sample"), 1u);
	f.Run("cells.snapshot.restore", "s1"); f.store.RunFrameBoundary();
	EXPECT_EQ(log.Count(LogCapture::Level::kInfo, "restored"), 1u);
	f.Run("cells.snapshot.delete", "s1");
	EXPECT_EQ(log.Count(LogCapture::Level::kInfo, "deleted"), 1u);
	f.store.FailNext("boom");
	f.Take("s2");
	EXPECT_EQ(log.Count(LogCapture::Level::kError, "store failed: boom"), 1u);
}

// ---- AC 13: capture at Large with 64 recorded layers ---------------------------------------------------------

namespace
{
	struct PerfNumbers { double median = 0, max = 0; uint64_t bytes = 0; };

	PerfNumbers MeasureCapture(Rig& r, uint32_t floatLayers, uint32_t categoryLayers, const char* label)
	{
		std::vector<std::string> entries;
		for (uint32_t i = 0; i < floatLayers; ++i)
		{
			const std::string id = "f" + std::to_string(i);
			r.grid.AddFloatLayer(id.c_str(), float(i) * 0.5f);
			entries.push_back(FloatEntry(id.c_str(), 0, 100));
		}
		for (uint32_t i = 0; i < categoryLayers; ++i)
		{
			const std::string id = "c" + std::to_string(i);
			r.grid.AddCategoryLayer(id.c_str(), int(i % 3));
			entries.push_back(EnumEntry(id.c_str(), 3));
		}
		EXPECT_TRUE(r.schema.Load(entries));
		r.cfg.mMaxAggregateLayers = 64;
		r.Start();
		r.Take();

		PerfNumbers p;
		std::vector<double> ms;
		SnapshotCellData d;
		char err[256];
		for (int run = 0; run < 20; ++run)
		{
			d.Clear();
			const auto t0 = std::chrono::steady_clock::now();
			const bool ok = SnapshotCellFile::BuildFromLive(*r.rec, r.registry, r.schema.service.get(), "perf", 1, "w", d, err, sizeof(err));
			const auto t1 = std::chrono::steady_clock::now();
			EXPECT_TRUE(ok) << err;
			ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
		}
		EXPECT_EQ(d.LayerCount(), floatLayers + categoryLayers);
		std::sort(ms.begin(), ms.end());
		p.median = ms[ms.size() / 2];
		p.max = ms.back();

		FakeSnapshotStore store;
		char path[300];
		EXPECT_TRUE(SnapshotCellFile::Path(store.Folder(), "perf", path, sizeof(path)));
		EXPECT_TRUE(SnapshotCellFile::WriteAtomic(d, path, err, sizeof(err))) << err;
		p.bytes = fs::file_size(path);
		printf("[ INFO ] %s: capture+quantise median %.2f ms, max %.2f ms (20 runs), cell file %llu bytes\n",
			label, p.median, p.max, (unsigned long long)p.bytes);
		return p;
	}
}

TEST(SnapshotObservation, CaptureOf64FloatLayersAtLargeIsWithinAFrameBudgetAndUnderTheFileCap)
{
	Rig r(104, 64);
	const PerfNumbers p = MeasureCapture(r, 64, 0, "64 float layers at 104x64");
	EXPECT_LT(p.median, 50.0);
	EXPECT_LT(p.max, 100.0);
	EXPECT_LT(p.bytes, 1200000u);
}

TEST(SnapshotObservation, CaptureOfAMixOf48FloatAnd16CategoryLayersAtLargeIsWithinBudget)
{
	Rig r(104, 64);
	const PerfNumbers p = MeasureCapture(r, 48, 16, "48 float + 16 category layers at 104x64");
	EXPECT_LT(p.median, 50.0);
	EXPECT_LT(p.max, 100.0);
	EXPECT_LT(p.bytes, 1200000u);
}

#endif // DIA_DEBUG
