#include "HistoryTestRig.h"
#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <json/json.h>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	struct CmdRig : Rig
	{
		FakeMirrorRegistrar registrar;
		std::unique_ptr<CellHistoryCommands> commands;

		void Setup()
		{
			grid.AddFloatLayer("temp", 40.0f);
			grid.AddCategoryLayer("biome", 1);
			grid.AddFloatLayer("raw", 5.0f);
			other.AddFloatLayer("oth", 1.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), RangelessFloatEntry("raw"), FloatEntry("oth", 0, 10) }));
			Start();
			Take();
			commands = std::make_unique<CellHistoryCommands>(*rec);
			ASSERT_TRUE(commands->Register(registrar));
		}

		static Json::Value Layers(std::initializer_list<const char*> ids)
		{
			Json::Value args(Json::objectValue);
			Json::Value arr(Json::arrayValue);
			for (const char* id : ids) { arr.append(id); }
			args["layers"] = arr;
			return args;
		}
		Json::Value Run(const char* name, const Json::Value& args = Json::Value(Json::objectValue)) { return registrar.Execute(name, args); }
	};

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }
}

TEST(CellHistoryCommands, RegistersThreeCommandsWithHelp)
{
	CmdRig r;
	r.Setup();
	EXPECT_EQ(r.registrar.Count(), 3u);
	for (const char* name : { "cells.history.track", "cells.history.untrack", "cells.history.status" })
	{
		ASSERT_TRUE(r.registrar.Has(name)) << name;
		EXPECT_TRUE(r.registrar.IsCommand(name)) << name;
		const Dia::DebugServer::SimHelpStored* help = r.registrar.Help(name);
		ASSERT_NE(help, nullptr) << name;
		EXPECT_GT(strlen(help->description), 0u) << name;
		EXPECT_STREQ(help->category, "cells.history") << name;
		EXPECT_GT(strlen(help->owner), 0u) << name;
	}
	for (const char* name : { "cells.history.track", "cells.history.untrack" })
	{
		const Dia::DebugServer::SimHelpStored* help = r.registrar.Help(name);
		ASSERT_EQ(help->paramCount, 1u) << name;
		EXPECT_STREQ(help->params[0].name, "layers");
		EXPECT_TRUE(help->params[0].required);
		EXPECT_FALSE(help->noArgs);
	}
	const Dia::DebugServer::SimHelpStored* status = r.registrar.Help("cells.history.status");
	EXPECT_TRUE(status->noArgs);
	EXPECT_EQ(status->paramCount, 0u);
}

TEST(CellHistoryCommands, UnregisterRemovesAllAndRegisterIsIdempotent)
{
	CmdRig r;
	r.Setup();
	EXPECT_TRUE(r.commands->Register(r.registrar));	// already registered: no duplicate
	EXPECT_EQ(r.registrar.Count(), 3u);
	r.commands->Unregister(r.registrar);
	EXPECT_EQ(r.registrar.Count(), 0u);
	EXPECT_TRUE(r.commands->Register(r.registrar));
	EXPECT_EQ(r.registrar.Count(), 3u);
}

TEST(CellHistoryCommands, RegisterRollsBackWhenTheRegistrarRejectsAName)
{
	CmdRig r;
	r.Setup();
	r.commands->Unregister(r.registrar);
	FakeMirrorRegistrar taken;
	Dia::DebugServer::SimHelpInfo help;
	help.description = "taken";
	ASSERT_TRUE(taken.RegisterCommand("cells.history.status", help, [](const Json::Value&) { return Json::Value(); }));
	EXPECT_FALSE(r.commands->Register(taken));
	EXPECT_FALSE(taken.Has("cells.history.track"));	// rolled back
	EXPECT_FALSE(taken.Has("cells.history.untrack"));
	EXPECT_EQ(taken.Count(), 1u);
}

TEST(CellHistoryCommands, TrackReturnsTheNewWindowLengthAndTracks)
{
	CmdRig r;
	r.Setup();
	const Json::Value out = r.Run("cells.history.track", CmdRig::Layers({ "temp", "biome" }));
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["windowLength"].asUInt(), r.rec->Ring()->WindowLength());
	EXPECT_GT(out["windowLength"].asUInt(), 0u);
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("biome")));
}

TEST(CellHistoryCommands, TrackInvalidLayerIsAnErrorReplyAndChangesNothing)
{
	LogCapture log("HIS");
	CmdRig r;
	r.Setup();
	for (const char* bad : { "nonexistent", "oth", "raw" })
	{
		const Json::Value out = r.Run("cells.history.track", CmdRig::Layers({ "temp", bad }));
		EXPECT_TRUE(IsError(out)) << bad;
		EXPECT_NE(out["error"].asString().find(bad), std::string::npos) << out["error"].asString();
		EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp"))) << bad;
	}
	EXPECT_GE(log.Errors(), 3u);
}

TEST(CellHistoryCommands, BadArgumentsAreErrorRepliesWithNoStateChange)
{
	LogCapture log("HIS");
	CmdRig r;
	r.Setup();
	Json::Value notArray(Json::objectValue); notArray["layers"] = "temp";
	Json::Value emptyArr(Json::objectValue); emptyArr["layers"] = Json::Value(Json::arrayValue);
	Json::Value nonString(Json::objectValue); nonString["layers"] = Json::Value(Json::arrayValue); nonString["layers"].append(5);
	Json::Value emptyString(Json::objectValue); emptyString["layers"] = Json::Value(Json::arrayValue); emptyString["layers"].append("");
	const Json::Value cases[] = { Json::Value(Json::nullValue), Json::Value("temp"), Json::Value(Json::objectValue), notArray, emptyArr, nonString, emptyString };
	for (const char* name : { "cells.history.track", "cells.history.untrack" })
	{
		for (const Json::Value& c : cases)
		{
			EXPECT_TRUE(IsError(r.Run(name, c))) << name << " " << c.toStyledString();
		}
	}
	EXPECT_EQ(r.rec->Ring()->TrackedCount(), 0u);
	EXPECT_GE(log.Errors(), 14u);
}

TEST(CellHistoryCommands, MoreThanSixteenLayersIsRejected)
{
	CmdRig r;
	r.Setup();
	Json::Value args(Json::objectValue);
	args["layers"] = Json::Value(Json::arrayValue);
	for (int i = 0; i < 17; ++i) { args["layers"].append("temp"); }
	EXPECT_TRUE(IsError(r.Run("cells.history.track", args)));
	EXPECT_TRUE(IsError(r.Run("cells.history.untrack", args)));
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp")));
	// Exactly 16 is accepted (duplicates count once).
	Json::Value sixteen(Json::objectValue);
	sixteen["layers"] = Json::Value(Json::arrayValue);
	for (int i = 0; i < 16; ++i) { sixteen["layers"].append("temp"); }
	EXPECT_FALSE(IsError(r.Run("cells.history.track", sixteen)));
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));
}

TEST(CellHistoryCommands, UntrackFreesAndReturnsTheWindowLength)
{
	CmdRig r;
	r.Setup();
	ASSERT_FALSE(IsError(r.Run("cells.history.track", CmdRig::Layers({ "temp" }))));
	const Json::Value out = r.Run("cells.history.untrack", CmdRig::Layers({ "temp" }));
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_TRUE(out.isMember("windowLength"));
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_TRUE(IsError(r.Run("cells.history.untrack", CmdRig::Layers({ "temp" }))));	// no longer tracked
}

TEST(CellHistoryCommands, StatusReportsStateAndMatchesTheRecorder)
{
	CmdRig r;
	r.Setup();
	ASSERT_FALSE(IsError(r.Run("cells.history.track", CmdRig::Layers({ "biome" }))));
	r.Take();
	r.Take();
	const Json::Value s = r.Run("cells.history.status");
	ASSERT_TRUE(s.isObject());
	EXPECT_EQ(s["sampleCount"].asUInt(), 3u);
	EXPECT_EQ(s["runId"].asString().size(), 16u);
	EXPECT_EQ(s["recordedLayers"].asUInt(), r.rec->RecordedLayerCount());
	EXPECT_EQ(s["windowLength"].asUInt(), r.rec->Ring()->WindowLength());
	ASSERT_TRUE(s["tracked"].isArray());
	ASSERT_EQ(s["tracked"].size(), 1u);
	EXPECT_EQ(s["tracked"][0u].asString(), "biome");
	EXPECT_EQ(s["retained"]["cells"]["first"].asUInt(), 1u);	// tracked from sample 1
	EXPECT_EQ(s["retained"]["cells"]["latest"].asUInt(), 2u);
	EXPECT_EQ(s["retained"]["aggregates"]["latest"].asUInt(), 2u);
	EXPECT_GT(s["memory"]["cells"].asUInt64(), 0u);
	EXPECT_GT(s["memory"]["aggregates"].asUInt64(), 0u);
	EXPECT_GT(s["budgets"]["cells"].asUInt64(), 0u);
	EXPECT_GT(s["budgets"]["aggregates"].asUInt64(), 0u);
	EXPECT_GT(s["budgets"]["events"].asUInt64(), 0u);
	ASSERT_TRUE(s["gaps"]["cells"].isArray());
	EXPECT_TRUE(s["gaps"]["aggregates"].isArray());
	EXPECT_TRUE(s["gaps"]["events"].isArray());
	EXPECT_EQ(s["gapCap"].asUInt(), 64u);
}

TEST(CellHistoryCommands, StatusListsGapsAfterEviction)
{
	CmdRig r;
	r.grid.AddCategoryLayer("biome", 1);
	ASSERT_TRUE(r.schema.Load({ EnumEntry("biome", 3, "{\"track\":true}") }));
	RecorderBudgets b;
	b.mAggregateBytes = BudgetBytesForWindow(4, AggregateRowBytes(0, 3, 1));
	b.mCellBytes = BudgetBytesForWindow(2, BytesPerSample(0, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b, false);
	r.commands = std::make_unique<CellHistoryCommands>(*r.rec);
	ASSERT_TRUE(r.commands->Register(r.registrar));
	for (int i = 0; i < 7; ++i) { r.Take(); }
	const Json::Value s = r.Run("cells.history.status");
	ASSERT_GE(s["gaps"]["cells"].size(), 1u);
	EXPECT_EQ(s["gaps"]["cells"][0u]["first"].asUInt(), 0u);
	EXPECT_EQ(s["gaps"]["cells"][0u]["last"].asUInt(), 4u);
	ASSERT_GE(s["gaps"]["aggregates"].size(), 1u);
	EXPECT_EQ(s["gaps"]["aggregates"][0u]["last"].asUInt(), 2u);
	EXPECT_EQ(s["retained"]["cells"]["first"].asUInt(), 5u);
}

TEST(CellHistoryCommands, StatusIgnoresExtraArguments)
{
	CmdRig r;
	r.Setup();
	Json::Value junk(Json::objectValue);
	junk["x"] = 1;
	EXPECT_FALSE(IsError(r.Run("cells.history.status", junk)));
	EXPECT_FALSE(IsError(r.Run("cells.history.status", Json::Value(Json::nullValue))));
}

#endif // DIA_DEBUG
