#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/SnapshotCommands.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

#include <json/json.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;

namespace
{
	namespace fs = std::filesystem;

	int64_t gNow = 1000;
	int64_t TickNow(void*) { return gNow++; }

	const char* const kAllNames[] = { "cells.snapshot.take", "cells.snapshot.restore", "cells.snapshot.list", "cells.snapshot.delete", "cells.history.snapshot" };

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }

	struct Fix
	{
		Rig r;
		FakeSnapshotStore store;
		FakeMirrorRegistrar registrar;
		std::unique_ptr<SnapshotManager> mgr;
		std::unique_ptr<SnapshotCommands> cmds;

		explicit Fix(uint16_t w = 4, uint16_t h = 3) : r(w, h) {}

		void Setup()
		{
			gNow = 1000;
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}"), EnumEntry("biome", 3) }));
			r.Start();
			store.SetWriteStateFiles(true);
			MakeManager();
			for (int i = 0; i < 3; ++i) { r.grid.SetAll("temp", float(r.rec->SampleCount())); r.grid.BumpVersion("temp"); r.Take(); }
		}
		void MakeManager()
		{
			if (cmds) { cmds->Unregister(registrar); }
			mgr = std::make_unique<SnapshotManager>(*r.rec, r.registry, r.schema.service.get(), store);
			mgr->SetTimeProvider(&TickNow, nullptr);
			cmds = std::make_unique<SnapshotCommands>(*r.rec, *mgr, store);
			ASSERT_TRUE(cmds->Register(registrar));
		}
		Json::Value Run(const char* name, const char* snap = nullptr, const char* extraKey = nullptr, const Json::Value& extra = Json::Value())
		{
			Json::Value a(Json::objectValue);
			if (snap) { a["name"] = snap; }
			if (extraKey) { a[extraKey] = extra; }
			return registrar.Execute(name, a);
		}
		void Take(const char* name)
		{
			Json::Value out = Run("cells.snapshot.take", name);
			ASSERT_FALSE(IsError(out)) << out.toStyledString();
			store.RunFrameBoundary();
		}
		std::string CellPath(const char* name)
		{
			char p[300];
			EXPECT_TRUE(SnapshotCellFile::Path(store.Folder(), name, p, sizeof(p)));
			return p;
		}
		Json::Value List() { return registrar.Execute("cells.snapshot.list", Json::Value(Json::objectValue)); }
		static const Json::Value* Find(const Json::Value& list, const char* name)
		{
			for (const Json::Value& e : list["snapshots"]) { if (e["name"].asString() == name) { return &e; } }
			return nullptr;
		}
		template <typename F> void Edit(const char* name, F mutate)
		{
			SnapshotCellData d; char err[256];
			ASSERT_TRUE(SnapshotCellFile::Read(CellPath(name).c_str(), d, err, sizeof(err))) << err;
			mutate(d);
			ASSERT_TRUE(SnapshotCellFile::WriteAtomic(d, CellPath(name).c_str(), err, sizeof(err))) << err;
		}
	};
}

// ---- registration -------------------------------------------------------------------------------------------

TEST(SnapshotCommands, RegistersFiveCommandsWithHelp)
{
	Fix f; f.Setup();
	EXPECT_EQ(f.registrar.Count(), 5u);
	for (const char* name : kAllNames)
	{
		ASSERT_TRUE(f.registrar.Has(name)) << name;
		EXPECT_TRUE(f.registrar.IsCommand(name)) << name;
		const Dia::DebugServer::SimHelpStored* help = f.registrar.Help(name);
		ASSERT_NE(help, nullptr) << name;
		EXPECT_GT(strlen(help->description), 0u) << name;
		EXPECT_GT(strlen(help->owner), 0u) << name;
		EXPECT_STREQ(help->owner, "DiaCellHistory") << name;
	}
	EXPECT_STREQ(f.registrar.Help("cells.snapshot.take")->category, "cells.snapshot");
	EXPECT_STREQ(f.registrar.Help("cells.history.snapshot")->category, "cells.history");
	EXPECT_TRUE(f.registrar.Help("cells.snapshot.list")->noArgs);
	EXPECT_EQ(f.registrar.Help("cells.snapshot.take")->paramCount, 2u);
	EXPECT_TRUE(f.registrar.Help("cells.snapshot.take")->params[0].required);
	EXPECT_FALSE(f.registrar.Help("cells.snapshot.take")->params[1].required);
	EXPECT_EQ(f.registrar.Help("cells.snapshot.restore")->paramCount, 1u);
	EXPECT_EQ(f.registrar.Help("cells.snapshot.delete")->paramCount, 1u);
	EXPECT_EQ(f.registrar.Help("cells.history.snapshot")->paramCount, 4u);
	EXPECT_TRUE(f.registrar.Help("cells.history.snapshot")->params[0].required);
	EXPECT_TRUE(f.registrar.Help("cells.history.snapshot")->params[1].required);
	EXPECT_FALSE(f.registrar.Help("cells.history.snapshot")->params[2].required);
}

TEST(SnapshotCommands, UnregisterRemovesAllAndRegisterIsIdempotent)
{
	Fix f; f.Setup();
	EXPECT_TRUE(f.cmds->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 5u);
	f.cmds->Unregister(f.registrar);
	EXPECT_EQ(f.registrar.Count(), 0u);
	f.cmds->Unregister(f.registrar);		// harmless twice
	EXPECT_TRUE(f.cmds->Register(f.registrar));
	EXPECT_EQ(f.registrar.Count(), 5u);
}

TEST(SnapshotCommands, RegisterRollsBackWhenTheRegistrarRejectsAName)
{
	Fix f; f.Setup();
	f.cmds->Unregister(f.registrar);
	for (const char* taken : kAllNames)
	{
		FakeMirrorRegistrar reg;
		Dia::DebugServer::SimHelpInfo help;
		help.description = "taken";
		ASSERT_TRUE(reg.RegisterCommand(taken, help, [](const Json::Value&) { return Json::Value(); }));
		EXPECT_FALSE(f.cmds->Register(reg)) << taken;
		EXPECT_EQ(reg.Count(), 1u) << taken;
		EXPECT_TRUE(reg.Has(taken));
	}
}

// ---- take / restore -----------------------------------------------------------------------------------------

TEST(SnapshotCommands, TakeReturnsPendingThenReady)
{
	Fix f; f.Setup();
	Json::Value out = f.Run("cells.snapshot.take", "a");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_TRUE(out["pending"].asBool());
	EXPECT_EQ(f.Find(f.List(), "a")->operator[]("status").asString(), "pending");
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find(f.List(), "a")->operator[]("status").asString(), "ready");
}

TEST(SnapshotCommands, TakeErrorsPassThrough)
{
	Fix f; f.Setup();
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.take")));					// missing name
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.take", "../evil")));		// invalid
	Json::Value bad(Json::objectValue); bad["name"] = 7;
	EXPECT_TRUE(IsError(f.registrar.Execute("cells.snapshot.take", bad)));	// non-string
	f.Take("a");
	Json::Value dup = f.Run("cells.snapshot.take", "a");				// exists, no overwrite
	EXPECT_TRUE(IsError(dup)) << dup.toStyledString();
	ASSERT_FALSE(IsError(f.Run("cells.snapshot.take", "b")));
	Json::Value busy = f.Run("cells.snapshot.take", "c");
	ASSERT_TRUE(IsError(busy));
	EXPECT_NE(busy["error"].asString().find("snapshot operation in progress"), std::string::npos);
	f.store.RunFrameBoundary();
}

TEST(SnapshotCommands, TakeOverwriteFlagIsHonoured)
{
	Fix f; f.Setup();
	f.Take("a");
	Json::Value ow = f.Run("cells.snapshot.take", "a", "overwrite", true);
	ASSERT_FALSE(IsError(ow)) << ow.toStyledString();
	f.store.RunFrameBoundary();
	Json::Value notBool = f.Run("cells.snapshot.take", "a", "overwrite", "yes");
	EXPECT_TRUE(IsError(notBool));
	EXPECT_EQ(f.store.PendingCount(), 0u);
}

TEST(SnapshotCommands, TakeWithNoSamplesIsAnError)
{
	Fix f;
	f.r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(f.r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	f.r.Start();
	f.mgr = std::make_unique<SnapshotManager>(*f.r.rec, f.r.registry, f.r.schema.service.get(), f.store);
	f.cmds = std::make_unique<SnapshotCommands>(*f.r.rec, *f.mgr, f.store);
	ASSERT_TRUE(f.cmds->Register(f.registrar));
	Json::Value out = f.Run("cells.snapshot.take", "a");
	ASSERT_TRUE(IsError(out));
	EXPECT_NE(out["error"].asString().find("no samples"), std::string::npos);
}

TEST(SnapshotCommands, RestoreReturnsPendingAndErrorsPassThrough)
{
	Fix f; f.Setup();
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.restore")));
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.restore", "bad name")));
	Json::Value missing = f.Run("cells.snapshot.restore", "nope");
	ASSERT_TRUE(IsError(missing));
	EXPECT_NE(missing["error"].asString().find("not found"), std::string::npos);
	f.Take("a");
	Json::Value out = f.Run("cells.snapshot.restore", "a");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_TRUE(out["pending"].asBool());
	EXPECT_TRUE(f.mgr->OperationInFlight());
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.restore", "a")));		// one at a time
	f.store.RunFrameBoundary();
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_TRUE(f.mgr->LastRestore().mDone);
}

// ---- list ---------------------------------------------------------------------------------------------------

TEST(SnapshotCommands, ListIsEmptyWithNothingTaken)
{
	Fix f; f.Setup();
	Json::Value l = f.List();
	ASSERT_TRUE(l["snapshots"].isArray());
	EXPECT_EQ(l["snapshots"].size(), 0u);
	EXPECT_FALSE(l.isMember("truncated"));
}

TEST(SnapshotCommands, ListShowsAReadyTakeWithItsFields)
{
	Fix f; f.Setup();
	f.mgr->SetWorld("Small");
	f.Take("a");
	const Json::Value l = f.List();
	ASSERT_EQ(l["snapshots"].size(), 1u);
	const Json::Value& e = l["snapshots"][0u];
	EXPECT_EQ(e["name"].asString(), "a");
	EXPECT_EQ(e["status"].asString(), "ready");
	EXPECT_FALSE(e.isMember("reason"));
	EXPECT_EQ(e["sample"].asUInt(), 2u);
	EXPECT_EQ(e["label"].asString(), std::string(f.r.rec->Header(2).mLabel.AsCStr()));
	EXPECT_EQ(e["createdUnix"].asInt64(), 1000);
	EXPECT_EQ(e["world"].asString(), "Small");
	EXPECT_EQ(e["bytes"].asUInt64(), uint64_t(fs::file_size(f.CellPath("a"))));
	EXPECT_GT(e["bytes"].asUInt64(), 0u);
	EXPECT_TRUE(e["sameRun"].asBool());
}

TEST(SnapshotCommands, ListShowsAFailedTakeWithItsReason)
{
	Fix f; f.Setup();
	f.store.FailNext("disk full");
	ASSERT_FALSE(IsError(f.Run("cells.snapshot.take", "a")));
	f.store.RunFrameBoundary();
	const Json::Value l = f.List();
	const Json::Value* e = f.Find(l, "a");
	ASSERT_NE(e, nullptr);
	EXPECT_EQ((*e)["status"].asString(), "failed");
	EXPECT_NE((*e)["reason"].asString().find("disk full"), std::string::npos);
	EXPECT_EQ(l["snapshots"].size(), 1u);
}

TEST(SnapshotCommands, ListFindsEarlierSessionSnapshotsAndFlagsForeignRuns)
{
	Fix f; f.Setup();
	f.Take("same"); f.Take("other");
	f.Edit("other", [](SnapshotCellData& d) { d.mHeader.mRunId ^= 0xABCDull; });
	f.MakeManager();		// a new session: no records, the files and store state remain
	const Json::Value l = f.List();
	ASSERT_EQ(l["snapshots"].size(), 2u);
	const Json::Value* same = f.Find(l, "same");
	const Json::Value* other = f.Find(l, "other");
	ASSERT_NE(same, nullptr); ASSERT_NE(other, nullptr);
	EXPECT_EQ((*same)["status"].asString(), "ready");
	EXPECT_TRUE((*same)["sameRun"].asBool());
	EXPECT_EQ((*other)["status"].asString(), "ready");
	EXPECT_FALSE((*other)["sameRun"].asBool());
	EXPECT_EQ((*same)["sample"].asUInt(), 2u);
	EXPECT_EQ((*same)["bytes"].asUInt64(), uint64_t(fs::file_size(f.CellPath("same"))));
}

TEST(SnapshotCommands, ACellFileWithoutStoreStateIsAFailedOrphan)
{
	Fix f; f.Setup();
	f.Take("a");
	f.MakeManager();
	ASSERT_TRUE(f.store.Delete("a"));		// state gone, cell file stays
	const Json::Value l = f.List();
	const Json::Value* e = f.Find(l, "a");
	ASSERT_NE(e, nullptr);
	EXPECT_EQ((*e)["status"].asString(), "failed");
	EXPECT_EQ((*e)["reason"].asString(), "state file missing");
}

TEST(SnapshotCommands, ARecordWhoseCellFileIsGoneIsFailed)
{
	Fix f; f.Setup();
	f.Take("a");
	fs::remove(f.CellPath("a"));
	const Json::Value l = f.List();
	const Json::Value* e = f.Find(l, "a");
	ASSERT_NE(e, nullptr);
	EXPECT_EQ(l["snapshots"].size(), 1u);
	EXPECT_EQ((*e)["status"].asString(), "failed");
	EXPECT_EQ((*e)["reason"].asString(), "cell file missing");
}

TEST(SnapshotCommands, ACorruptCellFileIsListedFailedWithTheNameFromTheFileName)
{
	Fix f; f.Setup();
	std::ofstream(f.CellPath("zz"), std::ios::binary) << "this is not json";
	const Json::Value l = f.List();
	const Json::Value* e = f.Find(l, "zz");
	ASSERT_NE(e, nullptr);
	EXPECT_EQ((*e)["status"].asString(), "failed");
	EXPECT_GT((*e)["reason"].asString().size(), 0u);
}

TEST(SnapshotCommands, ACorruptFileForARecordedNameIsFailedToo)
{
	Fix f; f.Setup();
	f.Take("a");
	std::ofstream(f.CellPath("a"), std::ios::binary | std::ios::trunc) << "garbage";
	const Json::Value l = f.List();
	EXPECT_EQ(l["snapshots"].size(), 1u);
	EXPECT_EQ((*f.Find(l, "a"))["status"].asString(), "failed");
}

TEST(SnapshotCommands, APendingRecordWinsOverAnExistingFileOfTheSameName)
{
	Fix f; f.Setup();
	f.Take("a");
	ASSERT_FALSE(IsError(f.Run("cells.snapshot.take", "a", "overwrite", true)));
	const Json::Value l = f.List();
	EXPECT_EQ(l["snapshots"].size(), 1u);
	EXPECT_EQ((*f.Find(l, "a"))["status"].asString(), "pending");
	f.store.RunFrameBoundary();
}

TEST(SnapshotCommands, ListIsSortedByCreatedThenName)
{
	Fix f; f.Setup();
	f.Take("zeta"); f.Take("alpha"); f.Take("mid");
	f.Edit("mid", [](SnapshotCellData& d) { d.mHeader.mCreatedUnix = 1001; });	// ties with "alpha"
	const Json::Value l = f.List();
	ASSERT_EQ(l["snapshots"].size(), 3u);
	EXPECT_EQ(l["snapshots"][0u]["name"].asString(), "zeta");		// 1000
	EXPECT_EQ(l["snapshots"][1u]["name"].asString(), "alpha");		// 1001
	EXPECT_EQ(l["snapshots"][2u]["name"].asString(), "mid");		// 1001
}

TEST(SnapshotCommands, ListIsCappedAndMarkedTruncated)
{
	Fix f; f.Setup();
	f.Take("base");
	const std::string src = f.CellPath("base");
	for (int i = 0; i < 40; ++i)
	{
		const std::string name = "n" + std::to_string(i);
		f.store.AddExisting(name.c_str());
		fs::copy_file(src, f.CellPath(name.c_str()));
	}
	f.MakeManager();
	const Json::Value l = f.List();
	EXPECT_EQ(l["snapshots"].size(), SnapshotCommands::kMaxListEntries);
	EXPECT_TRUE(l["truncated"].asBool());
}

TEST(SnapshotCommands, ExactlyTheCapIsNotTruncated)
{
	Fix f; f.Setup();
	f.Take("base");
	const std::string src = f.CellPath("base");
	for (uint32_t i = 1; i < SnapshotCommands::kMaxListEntries; ++i)
	{
		const std::string name = "n" + std::to_string(i);
		f.store.AddExisting(name.c_str());
		fs::copy_file(src, f.CellPath(name.c_str()));
	}
	f.MakeManager();
	const Json::Value l = f.List();
	EXPECT_EQ(l["snapshots"].size(), SnapshotCommands::kMaxListEntries);
	EXPECT_FALSE(l.isMember("truncated"));
}

// ---- delete -------------------------------------------------------------------------------------------------

TEST(SnapshotCommands, DeleteRemovesBothHalvesAndTheRecord)
{
	Fix f; f.Setup();
	f.Take("a"); f.Take("b");
	ASSERT_TRUE(fs::exists(f.CellPath("a")));
	ASSERT_TRUE(fs::exists(f.store.StatePath("a")));
	Json::Value out = f.Run("cells.snapshot.delete", "a");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_FALSE(fs::exists(f.CellPath("a")));
	EXPECT_FALSE(fs::exists(f.store.StatePath("a")));
	EXPECT_FALSE(f.store.Exists("a"));
	EXPECT_TRUE(fs::exists(f.CellPath("b")));
	EXPECT_TRUE(f.store.Exists("b"));
	const Json::Value l = f.List();
	EXPECT_EQ(f.Find(l, "a"), nullptr);
	EXPECT_NE(f.Find(l, "b"), nullptr);
	// a deleted name can be taken again
	f.Take("a");
	EXPECT_EQ((*f.Find(f.List(), "a"))["status"].asString(), "ready");
}

TEST(SnapshotCommands, DeleteRemovesAFailedRecord)
{
	Fix f; f.Setup();
	f.store.FailNext("boom");
	f.Run("cells.snapshot.take", "a");
	f.store.RunFrameBoundary();
	ASSERT_NE(f.Find(f.List(), "a"), nullptr);
	Json::Value out = f.Run("cells.snapshot.delete", "a");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(f.Find(f.List(), "a"), nullptr);
}

TEST(SnapshotCommands, DeleteRemovesWhateverHalfExists)
{
	Fix f; f.Setup();
	f.Take("cellsonly"); f.Take("stateonly");
	f.MakeManager();
	ASSERT_TRUE(f.store.Delete("cellsonly"));
	fs::remove(f.CellPath("stateonly"));
	EXPECT_FALSE(IsError(f.Run("cells.snapshot.delete", "cellsonly")));
	EXPECT_FALSE(fs::exists(f.CellPath("cellsonly")));
	EXPECT_FALSE(IsError(f.Run("cells.snapshot.delete", "stateonly")));
	EXPECT_FALSE(f.store.Exists("stateonly"));
	EXPECT_FALSE(fs::exists(f.store.StatePath("stateonly")));
}

TEST(SnapshotCommands, DeleteErrors)
{
	Fix f; f.Setup();
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.delete")));
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.delete", "../evil")));
	EXPECT_TRUE(IsError(f.Run("cells.snapshot.delete", "")));
	Json::Value nf = f.Run("cells.snapshot.delete", "unknown");
	ASSERT_TRUE(IsError(nf));
	EXPECT_NE(nf["error"].asString().find("not found"), std::string::npos);
}

TEST(SnapshotCommands, DeleteIsRefusedWhileAnOperationIsInFlight)
{
	Fix f; f.Setup();
	f.Take("a");
	ASSERT_FALSE(IsError(f.Run("cells.snapshot.take", "b")));
	Json::Value out = f.Run("cells.snapshot.delete", "a");
	ASSERT_TRUE(IsError(out));
	EXPECT_NE(out["error"].asString().find("snapshot operation in progress"), std::string::npos);
	EXPECT_TRUE(fs::exists(f.CellPath("a")));
	EXPECT_TRUE(f.store.Exists("a"));
	f.store.RunFrameBoundary();
	EXPECT_FALSE(IsError(f.Run("cells.snapshot.delete", "a")));
}

// ---- cells.history.snapshot ---------------------------------------------------------------------------------

namespace
{
	Json::Value Query(Fix& f, const char* name, const char* layer, int64_t offset = -1, int64_t count = -1)
	{
		Json::Value a(Json::objectValue);
		if (name) { a["name"] = name; }
		if (layer) { a["layer"] = layer; }
		if (offset >= 0) { a["offset"] = Json::Int64(offset); }
		if (count >= 0) { a["count"] = Json::Int64(count); }
		return f.registrar.Execute("cells.history.snapshot", a);
	}

	// Decodes one reply's data into uint8 bytes of the stored payload (uint16 codes as little-endian pairs).
	std::vector<uint8_t> DecodePayload(const Json::Value& v)
	{
		const std::string text = v["data"].asString();
		const uint32_t n = v["count"].asUInt();
		std::vector<uint8_t> out;
		if (v["kind"].asString() == "float")
		{
			std::vector<uint16_t> codes(n);
			EXPECT_TRUE(DecodeUInt16(text.c_str(), uint32_t(text.size()), codes.data(), n));
			for (uint16_t c : codes) { out.push_back(uint8_t(c & 0xFF)); out.push_back(uint8_t(c >> 8)); }
		}
		else
		{
			out.resize(n);
			EXPECT_TRUE(DecodeUInt8(text.c_str(), uint32_t(text.size()), out.data(), n));
		}
		return out;
	}

	void FillDistinct(Fix& f, uint32_t cells)
	{
		for (uint32_t c = 0; c < cells; ++c)
		{
			f.r.grid.Set("temp", c, float(c % 101));
			f.r.grid.Set("biome", c, float(c % 3));
		}
		f.r.grid.BumpVersion("temp"); f.r.grid.BumpVersion("biome");
		f.r.Take();
	}
}

TEST(SnapshotCommands, QueryReturnsAFloatLayerAsStoredQuantised)
{
	Fix f; f.Setup();
	FillDistinct(f, 12);
	f.Take("a");
	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("a").c_str(), d, err, sizeof(err))) << err;
	Json::Value out = Query(f, "a", "temp");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["name"].asString(), "a");
	EXPECT_EQ(out["layer"].asString(), "temp");
	EXPECT_EQ(out["kind"].asString(), "float");
	EXPECT_FLOAT_EQ(out["min"].asFloat(), 0.0f);
	EXPECT_FLOAT_EQ(out["max"].asFloat(), 100.0f);
	EXPECT_EQ(out["width"].asUInt(), 4u);
	EXPECT_EQ(out["height"].asUInt(), 3u);
	EXPECT_EQ(out["offset"].asUInt(), 0u);
	EXPECT_EQ(out["count"].asUInt(), 12u);
	EXPECT_EQ(out["total"].asUInt(), 12u);
	const SnapshotLayer* layer = d.FindLayer(Dia::Core::StringCRC("temp"));
	ASSERT_NE(layer, nullptr);
	const std::vector<uint8_t> bytes = DecodePayload(out);
	ASSERT_EQ(bytes.size(), size_t(layer->PayloadBytes()));
	EXPECT_EQ(0, std::memcmp(bytes.data(), layer->Payload(), bytes.size()));
	// dequantised values are the live values within the quantisation error
	std::vector<float> vals(12);
	ASSERT_TRUE(SnapshotCellFile::Dequantise(d, Dia::Core::StringCRC("temp"), vals.data(), 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(vals[c], float(c % 101), 100.0f / 65535.0f + 1e-4f); }
}

TEST(SnapshotCommands, QueryReturnsACategoryLayerAsUInt8)
{
	Fix f; f.Setup();
	FillDistinct(f, 12);
	f.Take("a");
	Json::Value out = Query(f, "a", "biome");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["kind"].asString(), "category");
	EXPECT_EQ(out["count"].asUInt(), 12u);
	const std::vector<uint8_t> ids = DecodePayload(out);
	ASSERT_EQ(ids.size(), 12u);
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_EQ(ids[c], uint8_t(c % 3)); }
}

TEST(SnapshotCommands, QueryErrors)
{
	Fix f; f.Setup();
	f.Take("a");
	EXPECT_TRUE(IsError(Query(f, nullptr, "temp")));
	EXPECT_TRUE(IsError(Query(f, "a", nullptr)));
	EXPECT_TRUE(IsError(Query(f, "../evil", "temp")));
	Json::Value nf = Query(f, "nope", "temp");
	ASSERT_TRUE(IsError(nf));
	EXPECT_NE(nf["error"].asString().find("not found"), std::string::npos);
	Json::Value nl = Query(f, "a", "nolayer");
	ASSERT_TRUE(IsError(nl));
	EXPECT_EQ(nl["error"].asString(), "layer 'nolayer' not in snapshot 'a'");
	std::ofstream(f.CellPath("bad"), std::ios::binary) << "junk";
	Json::Value bad = Query(f, "bad", "temp");
	ASSERT_TRUE(IsError(bad));
	EXPECT_NE(bad["error"].asString().find("unreadable"), std::string::npos);
}

TEST(SnapshotCommands, QueryDoesNotNeedTheStoreState)
{
	Fix f; f.Setup();
	f.Take("a");
	ASSERT_TRUE(f.store.Delete("a"));
	EXPECT_FALSE(IsError(Query(f, "a", "temp")));
}

TEST(SnapshotCommands, QueryChunksALargeLayerUnderTheSimHostedLimit)
{
	Fix f(104, 64);
	f.Setup();
	FillDistinct(f, 104 * 64);
	f.Take("big");
	const uint32_t total = 104 * 64;
	ASSERT_GT(total, SnapshotCommands::kMaxChunkCells);
	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("big").c_str(), d, err, sizeof(err))) << err;
	const SnapshotLayer* layer = d.FindLayer(Dia::Core::StringCRC("temp"));
	ASSERT_NE(layer, nullptr);

	std::vector<uint8_t> all;
	uint32_t offset = 0, calls = 0;
	while (offset < total)
	{
		Json::Value out = Query(f, "big", "temp", offset);
		ASSERT_FALSE(IsError(out)) << out.toStyledString();
		EXPECT_EQ(out["offset"].asUInt(), offset);
		EXPECT_EQ(out["total"].asUInt(), total);
		EXPECT_LE(out["count"].asUInt(), SnapshotCommands::kMaxChunkCells);
		EXPECT_LT(Json::FastWriter().write(out).size(), 16384u);
		const std::vector<uint8_t> part = DecodePayload(out);
		all.insert(all.end(), part.begin(), part.end());
		offset += out["count"].asUInt();
		ASSERT_LT(++calls, 10u);
	}
	EXPECT_EQ(calls, 2u);
	ASSERT_EQ(all.size(), size_t(layer->PayloadBytes()));
	EXPECT_EQ(0, std::memcmp(all.data(), layer->Payload(), all.size()));
}

TEST(SnapshotCommands, QueryChunkBoundaries)
{
	Fix f; f.Setup();
	FillDistinct(f, 12);
	f.Take("a");
	f.cmds->SetMaxChunk(5);
	EXPECT_EQ(f.cmds->MaxChunk(), 5u);
	Json::Value first = Query(f, "a", "biome");
	EXPECT_EQ(first["count"].asUInt(), 5u);							// default count = min(total-offset, chunk)
	Json::Value clamped = Query(f, "a", "biome", 0, 100);
	EXPECT_EQ(clamped["count"].asUInt(), 5u);						// larger count clamped
	Json::Value mid = Query(f, "a", "biome", 5, 3);
	EXPECT_EQ(mid["count"].asUInt(), 3u);
	const std::vector<uint8_t> midIds = DecodePayload(mid);
	for (uint32_t i = 0; i < 3; ++i) { EXPECT_EQ(midIds[i], uint8_t((5 + i) % 3)); }
	Json::Value last = Query(f, "a", "biome", 10);
	EXPECT_EQ(last["count"].asUInt(), 2u);							// clipped by total
	Json::Value atEnd = Query(f, "a", "biome", 12);
	ASSERT_FALSE(IsError(atEnd));
	EXPECT_EQ(atEnd["count"].asUInt(), 0u);
	EXPECT_EQ(atEnd["data"].asString(), "");
	EXPECT_TRUE(IsError(Query(f, "a", "biome", 13)));				// beyond total
	Json::Value neg(Json::objectValue); neg["name"] = "a"; neg["layer"] = "biome"; neg["offset"] = -1;
	EXPECT_TRUE(IsError(f.registrar.Execute("cells.history.snapshot", neg)));
	Json::Value txt(Json::objectValue); txt["name"] = "a"; txt["layer"] = "biome"; txt["count"] = "x";
	EXPECT_TRUE(IsError(f.registrar.Execute("cells.history.snapshot", txt)));
	f.cmds->SetMaxChunk(0);
	EXPECT_EQ(f.cmds->MaxChunk(), 1u);
	f.cmds->SetMaxChunk(1u << 30);
	EXPECT_EQ(f.cmds->MaxChunk(), SnapshotCommands::kMaxChunkCells);
}

// ---- ListSummaries ------------------------------------------------------------------------------------------

TEST(SnapshotManagerSummaries, ListsReadySnapshotsOnly)
{
	Fix f; f.Setup();
	f.Take("a");
	f.store.FailNext("nope");
	f.Run("cells.snapshot.take", "bad");
	f.store.RunFrameBoundary();
	f.Run("cells.snapshot.take", "c");		// pending
	SnapshotSummary out[SnapshotManager::kMaxRecords];
	const uint32_t n = f.mgr->ListSummaries(out, SnapshotManager::kMaxRecords);
	ASSERT_EQ(n, 1u);
	EXPECT_STREQ(out[0].mName, "a");
	EXPECT_EQ(out[0].mSample, 2u);
	EXPECT_STREQ(out[0].mLabel, f.r.rec->Header(2).mLabel.AsCStr());
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.mgr->ListSummaries(out, SnapshotManager::kMaxRecords), 2u);
}

TEST(SnapshotManagerSummaries, RespectsTheCapacityAndNullOutput)
{
	Fix f; f.Setup();
	f.Take("a"); f.Take("b"); f.Take("c");
	SnapshotSummary out[2];
	EXPECT_EQ(f.mgr->ListSummaries(out, 2), 2u);
	EXPECT_EQ(f.mgr->ListSummaries(out, 0), 0u);
	EXPECT_EQ(f.mgr->ListSummaries(nullptr, 5), 0u);
}

TEST(SnapshotManagerSummaries, DeletedSnapshotsDisappear)
{
	Fix f; f.Setup();
	f.Take("a"); f.Take("b");
	ASSERT_FALSE(IsError(f.Run("cells.snapshot.delete", "a")));
	SnapshotSummary out[4];
	ASSERT_EQ(f.mgr->ListSummaries(out, 4), 1u);
	EXPECT_STREQ(out[0].mName, "b");
}

#endif // DIA_DEBUG
