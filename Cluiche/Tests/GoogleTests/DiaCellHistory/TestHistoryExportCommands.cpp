#include "HistoryTestRig.h"
#include <DiaCellHistory/ExportObservation.h>
#include <DiaCellHistory/HistoryExportCommands.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/ExportDecoder.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <json/json.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::Core::StringCRC;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	namespace fs = std::filesystem;

	const char* const kName = "cells.history.export";

	int64_t FakeNow(void*) { return 1792510000; }		// 2026-10-20T15:26:40Z
	double gClock = 0.0;
	double StepClock(void*) { gClock += 7.0; return gClock; }
	const char* WorldFn(void*) { return "unit_world"; }

	double G(const char* n) { auto* g = MetricRegistry::Instance().FindGauge(StringCRC(n)); return g ? g->Value() : -1.0; }
	uint64_t C(const char* n) { auto* c = MetricRegistry::Instance().FindCounter(StringCRC(n)); return c ? c->Value() : 0; }

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }

	struct Fix
	{
		Rig r;
		TempFolder tmp;
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryExportCommands> cmds;

		void Setup()
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}"), EnumEntry("biome", 3) }));
			r.Start();
			for (uint32_t i = 0; i < 4; ++i) { r.grid.SetAll("temp", float(i * 10)); r.grid.BumpVersion("temp"); r.Take(); }
			r.rec->SetWorldIdProvider(&WorldFn, nullptr);
			cmds = std::make_unique<HistoryExportCommands>(*r.rec);
			cmds->SetAppName("TestApp");
			cmds->SetBuildInfo("abc1234");
			cmds->SetOutDir(tmp.Path().c_str());
			cmds->SetTimeProvider(&FakeNow, nullptr);
			ASSERT_TRUE(cmds->Register(registrar));
		}
		Json::Value Run(const char* path, const char* extraKey = nullptr, const Json::Value& extra = Json::Value())
		{
			Json::Value a(Json::objectValue);
			if (path) { a["path"] = path; }
			if (extraKey) { a[extraKey] = extra; }
			return registrar.Execute(kName, a);
		}
		std::string HistoryFile(const char* name) const { return (fs::path(tmp.Path()) / "TestApp" / "history" / name).string(); }
	};
}

TEST(HistoryExportCommands, RegistersOneCommandWithHelp)
{
	Fix f; f.Setup();
	EXPECT_EQ(f.registrar.Count(), 1u);
	ASSERT_TRUE(f.registrar.Has(kName));
	EXPECT_TRUE(f.registrar.IsCommand(kName));
	const Dia::DebugServer::SimHelpStored* help = f.registrar.Help(kName);
	ASSERT_NE(help, nullptr);
	EXPECT_GT(strlen(help->description), 0u);
	EXPECT_LE(strlen(help->description), 256u);
	EXPECT_STREQ(help->owner, "DiaCellHistory");
	EXPECT_STREQ(help->category, "cells.history");
	ASSERT_EQ(help->paramCount, 3u);
	EXPECT_TRUE(help->params[0].required);
	EXPECT_FALSE(help->params[1].required);
	EXPECT_FALSE(help->params[2].required);
}

TEST(HistoryExportCommands, UnregisterRemovesAndRegisterIsIdempotent)
{
	Fix f; f.Setup();
	EXPECT_TRUE(f.cmds->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 1u);
	f.cmds->Unregister(f.registrar);
	EXPECT_EQ(f.registrar.Count(), 0u);
	f.cmds->Unregister(f.registrar);
	EXPECT_TRUE(f.cmds->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 1u);
}

TEST(HistoryExportCommands, RegisterFailsWhenTheNameIsTaken)
{
	Fix f; f.Setup();
	FakeMirrorRegistrar other;
	Dia::DebugServer::SimHelpInfo help;
	help.description = "taken";
	ASSERT_TRUE(other.RegisterCommand(kName, help, [](const Json::Value&) { return Json::Value(); }));
	HistoryExportCommands second(*f.r.rec);
	EXPECT_FALSE(second.Register(other));
	EXPECT_EQ(other.Count(), 1u);
}

TEST(HistoryExportCommands, RelativePathWritesUnderTheHistoryFolderAndReturnsPathBytesSamples)
{
	Fix f; f.Setup();
	const Json::Value out = f.Run("run.cell_history.json");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(fs::path(out["path"].asString()).lexically_normal(), fs::path(f.HistoryFile("run.cell_history.json")).lexically_normal());
	EXPECT_EQ(out["samples"].asUInt(), 4u);
	ASSERT_TRUE(fs::exists(out["path"].asString()));
	EXPECT_EQ(out["bytes"].asUInt64(), uint64_t(fs::file_size(out["path"].asString())));
}

TEST(HistoryExportCommands, HeaderCarriesAppCommitIsoTimeAndWorldProvider)
{
	Fix f; f.Setup();
	ASSERT_FALSE(IsError(f.Run("h.json")));
	DecodedExport d;
	ASSERT_TRUE(d.Load(f.HistoryFile("h.json").c_str())) << d.error;
	EXPECT_EQ(d.Header()["app"].asString(), "TestApp");
	EXPECT_EQ(d.Header()["git_commit"].asString(), "abc1234");
	EXPECT_EQ(d.Header()["generated_at"].asString(), "2026-10-20T15:26:40Z");
	EXPECT_EQ(d.Header()["world"].asString(), "unit_world");
	EXPECT_TRUE(d.root["schema"].isObject());
}

TEST(HistoryExportCommands, AbsolutePathIsUsedAsIs)
{
	Fix f; f.Setup();
	const std::string abs = (fs::path(f.tmp.Path()) / "elsewhere.json").string();
	const Json::Value out = f.Run(abs.c_str());
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(fs::exists(abs));
	EXPECT_FALSE(fs::exists(f.HistoryFile("elsewhere.json")));
}

TEST(HistoryExportCommands, CellsFlagAddsTheCellsSection)
{
	Fix f; f.Setup();
	ASSERT_FALSE(IsError(f.Run("nocells.json")));
	ASSERT_FALSE(IsError(f.Run("cells.json", "cells", Json::Value(true))));
	DecodedExport a, b;
	ASSERT_TRUE(a.Load(f.HistoryFile("nocells.json").c_str()));
	ASSERT_TRUE(b.Load(f.HistoryFile("cells.json").c_str()));
	EXPECT_FALSE(a.root.isMember("cells"));
	EXPECT_TRUE(b.root.isMember("cells"));
}

TEST(HistoryExportCommands, SnapshotsAreListedWhenAManagerIsSet)
{
	Fix f; f.Setup();
	FakeSnapshotStore store;
	store.SetWriteStateFiles(true);
	SnapshotManager mgr(*f.r.rec, f.r.registry, f.r.schema.service.get(), store);
	TakeResult tr;
	ASSERT_TRUE(mgr.Take("snapA", false, tr)) << tr.error;
	store.RunFrameBoundary();
	f.cmds->SetSnapshots(&mgr);
	ASSERT_FALSE(IsError(f.Run("snaps.json")));
	DecodedExport d;
	ASSERT_TRUE(d.Load(f.HistoryFile("snaps.json").c_str()));
	ASSERT_EQ(d.root["snapshots"].size(), 1u);
	EXPECT_EQ(d.root["snapshots"][0]["name"].asString(), "snapA");
	f.cmds->SetSnapshots(nullptr);
}

TEST(HistoryExportCommands, ExistingFileIsRefusedUnlessOverwrite)
{
	Fix f; f.Setup();
	ASSERT_FALSE(IsError(f.Run("twice.json")));
	const Json::Value refused = f.Run("twice.json");
	ASSERT_TRUE(IsError(refused));
	EXPECT_NE(refused["error"].asString().find("--overwrite"), std::string::npos);
	const Json::Value ok = f.Run("twice.json", "overwrite", Json::Value(true));
	EXPECT_FALSE(IsError(ok)) << ok.toStyledString();
}

TEST(HistoryExportCommands, BadArgumentsReturnErrorObjects)
{
	Fix f; f.Setup();
	EXPECT_TRUE(IsError(f.Run(nullptr)));												// missing path
	EXPECT_TRUE(IsError(f.registrar.Execute(kName, Json::Value(Json::nullValue))));				// not an object
	Json::Value a(Json::objectValue);
	a["path"] = 5;
	EXPECT_TRUE(IsError(f.registrar.Execute(kName, a)));								// non-string path
	EXPECT_TRUE(IsError(f.Run("")));													// empty path
	EXPECT_TRUE(IsError(f.Run("x.json", "cells", Json::Value("yes"))));					// non-bool cells
	EXPECT_TRUE(IsError(f.Run("x.json", "overwrite", Json::Value(1))));					// non-bool overwrite
	EXPECT_TRUE(IsError(f.Run("../escape.json")));										// traversal
	EXPECT_FALSE(fs::exists(f.HistoryFile("x.json")));
}

TEST(HistoryExportCommands, RelativePathWithNoOutDirIsAnError)
{
	Fix f; f.Setup();
	HistoryExportCommands bare(*f.r.rec);
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(bare.Register(reg));
	Json::Value a(Json::objectValue);
	a["path"] = "x.json";
	EXPECT_TRUE(IsError(reg.Execute(kName, a)));
}

TEST(HistoryExportCommands, FormatUtcMatchesKnownInstants)
{
	char buf[32];
	HistoryExportCommands::FormatUtc(0, buf, sizeof(buf));
	EXPECT_STREQ(buf, "1970-01-01T00:00:00Z");
	HistoryExportCommands::FormatUtc(1792510000, buf, sizeof(buf));
	EXPECT_STREQ(buf, "2026-10-20T15:26:40Z");
	HistoryExportCommands::FormatUtc(951782400, buf, sizeof(buf));
	EXPECT_STREQ(buf, "2000-02-29T00:00:00Z");
	HistoryExportCommands::FormatUtc(1709210096, buf, sizeof(buf));
	EXPECT_STREQ(buf, "2024-02-29T12:34:56Z");
	char tiny[8] = { 'x' };
	HistoryExportCommands::FormatUtc(0, tiny, sizeof(tiny));
	EXPECT_STREQ(tiny, "");
}

TEST(ExportObservation, MetricsFollowSuccessfulExportsWithASteppingClock)
{
	Fix f; f.Setup();
	gClock = 0.0;
	ExportObservation obs(&StepClock, nullptr);
	f.cmds->SetObservation(&obs);
	const uint64_t before = C("dia.cellhistory.exports");
	const Json::Value out = f.Run("m.json");
	ASSERT_FALSE(IsError(out));
	EXPECT_EQ(C("dia.cellhistory.exports"), before + 1);
	EXPECT_DOUBLE_EQ(G("dia.cellhistory.export_ms"), 7.0);			// two clock reads, 7 ms apart
	EXPECT_DOUBLE_EQ(G("dia.cellhistory.export_bytes"), double(out["bytes"].asUInt64()));
	// A refused export changes nothing.
	ASSERT_TRUE(IsError(f.Run("m.json")));
	EXPECT_EQ(C("dia.cellhistory.exports"), before + 1);
	f.cmds->SetObservation(nullptr);
}

TEST(ExportObservation, DefaultClockIsMonotonicAndOnExportedSetsGauges)
{
	ExportObservation obs;
	const double a = obs.NowMs();
	const double b = obs.NowMs();
	EXPECT_GE(b, a);
	const uint64_t before = C("dia.cellhistory.exports");
	obs.OnExported(12.5, 4096);
	EXPECT_EQ(C("dia.cellhistory.exports"), before + 1);
	EXPECT_DOUBLE_EQ(G("dia.cellhistory.export_ms"), 12.5);
	EXPECT_DOUBLE_EQ(G("dia.cellhistory.export_bytes"), 4096.0);
}

TEST(ExportObservation, FailedExportsAreCountedAndSuccessesAreNot)
{
	Fix f; f.Setup();
	ExportObservation obs(&StepClock, nullptr);
	f.cmds->SetObservation(&obs);
	const uint64_t fails0 = C("dia.cellhistory.export_failures");
	ASSERT_FALSE(IsError(f.Run("fail.json")));
	EXPECT_EQ(C("dia.cellhistory.export_failures"), fails0);
	ASSERT_TRUE(IsError(f.Run("fail.json")));		// refused: the file exists and overwrite was not given
	EXPECT_EQ(C("dia.cellhistory.export_failures"), fails0 + 1);
	obs.OnFailed();
	EXPECT_EQ(C("dia.cellhistory.export_failures"), fails0 + 2);
	f.cmds->SetObservation(nullptr);
}

#endif // DIA_DEBUG
