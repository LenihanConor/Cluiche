#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaExporter.h>
#include <DiaValueSchema/ValueSchemaQuery.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ComponentFieldCollector.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include "RampTestSupport.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// DiaValueSchema Feature 6 task 1: ValueSchemaExporter builds the dia.value_schema_export v1 document (AC 2, 3, 6).

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::FakeCodeSideSource;
using Dia::ValueSchema::Testing::MakeValueInfo;

namespace
{
	const Dia::API::ValueParam kSaveParams[] = {
		{ "slot", "string", true, nullptr },						// no description: undescribed arg
		{ "mode", "string", false, "How to save" },
	};
	const Dia::API::ValueParam kLoadParams[] = {
		{ "slot", "string", true, "Which slot" },
	};

	struct ExportFixture : ::testing::Test
	{
		ExportFixture() : mRamps(true, false) {}

		void SetUp() override
		{
			ValueInfo side = MakeValueInfo("cow.ecology_params", "grazing.rate", ValueKind::Parameter, "Global\\Ecology/eco.schema.json");
			SetText(side.mLabel, "Grazing 100% \"fast\" \xC3\xA9");
			SetText(side.mDescription, "Rate %s %d \\ \xE2\x82\xAC");
			ASSERT_EQ(RegisterResult::Ok, mRamps.mSchema.Register(side));

			FakeCodeSideSource src;
			src.AddMetric("cow.ecology.herbivores_total", "Herbivores", "All herbivores", "animals");
			src.AddMetric("cow.weather.ticks");
			src.AddCommand("cow.save", "CoW", "Save the game", kSaveParams, 2);
			src.AddCommand("cow.load", "CoW", "Load the game", kLoadParams, 1);
			src.AddCommand("cow.ping", "CoW", "Ping", nullptr, 0, true);
			src.AddCommand("cow.bare", "CoW", "Bare", nullptr, 0, false);
			mReport = CodeSideCollector().Collect(src, mRamps.mSchema);

			ValueInfo field = MakeValueInfo("Transform2D", "rotation", ValueKind::ComponentField, "code:ComponentRegistry");
			ASSERT_EQ(RegisterResult::Ok, mRamps.mSchema.Register(field));
			UndescribedItem item;
			SetText(item.mName, "Transform2D/scale");
			SetText(item.mOwner, "Transform2D");
			mFieldReport.mUndescribedFields.Reserve(1);
			mFieldReport.mUndescribedFields.Add(item);

			mRamps.Finish();
		}

		Json::Value Build() const
		{
			ExportHeader h; h.mApp = "cow"; h.mConfig = "Debug";
			return ValueSchemaExporter().Build(mRamps.Svc(), &mReport, &mFieldReport, h);
		}

		static const Json::Value* FindEntry(const Json::Value& doc, const char* key)
		{
			for (const Json::Value& e : doc["entries"])
			{
				if ((e["for"].asString() + "/" + e["key"].asString()) == key) { return &e; }
			}
			return nullptr;
		}
		static const Json::Value* FindByName(const Json::Value& list, const char* name)
		{
			for (const Json::Value& e : list) { if (e["name"].asString() == name) { return &e; } }
			return nullptr;
		}

		VsTest::RampFixture	mRamps;
		CodeSideReport			mReport;
		ComponentFieldReport	mFieldReport;
	};

	bool IsIsoUtc(const std::string& s)
	{
		if (s.size() != 20) { return false; }
		for (size_t i = 0; i < s.size(); ++i)
		{
			const char c = s[i];
			if (i == 4 || i == 7) { if (c != '-') return false; }
			else if (i == 10) { if (c != 'T') return false; }
			else if (i == 13 || i == 16) { if (c != ':') return false; }
			else if (i == 19) { if (c != 'Z') return false; }
			else if (c < '0' || c > '9') { return false; }
		}
		return true;
	}
}

TEST_F(ExportFixture, EnvelopeAndHeader)
{
	const Json::Value doc = Build();
	EXPECT_EQ("dia.value_schema_export", doc["format"].asString());
	EXPECT_EQ(1, doc["version"].asInt());
	const Json::Value& h = doc["header"];
	EXPECT_EQ("cow", h["app"].asString());
	EXPECT_EQ("Debug", h["config"].asString());
	EXPECT_TRUE(IsIsoUtc(h["generated_at"].asString())) << h["generated_at"].asString();
	EXPECT_TRUE(h["code_side"].isBool());
	EXPECT_TRUE(h["code_side"].asBool());
	EXPECT_FALSE(h.isMember("git_commit"));
	EXPECT_FALSE(h.isMember("dirty"));
}

TEST_F(ExportFixture, EveryEntryHasTheRightOrigin)
{
	const Json::Value doc = Build();
	ASSERT_TRUE(doc["entries"].isArray());
	const Json::Value* e = FindEntry(doc, "cow.ecology_params/grazing.rate");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("sidecar:Global/Ecology/eco.schema.json", (*e)["origin"].asString());
	e = FindEntry(doc, "dia.metric/cow.ecology.herbivores_total");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("code:metric", (*e)["origin"].asString());
	EXPECT_EQ("animals", (*e)["unit"].asString());
	e = FindEntry(doc, "dia.command/cow.save");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("code:command", (*e)["origin"].asString());
	e = FindEntry(doc, "dia.command/cow.save.slot");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("code:arg", (*e)["origin"].asString());
	e = FindEntry(doc, "Transform2D/rotation");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("code:component_field", (*e)["origin"].asString());
	for (const Json::Value& x : doc["entries"]) { EXPECT_TRUE(x["origin"].isString()) << x["key"].asString(); }
}

TEST_F(ExportFixture, OriginOfHelper)
{
	EXPECT_STREQ("sidecar:a/b.json", OriginOf(MakeValueInfo("f", "k", ValueKind::Parameter, "a\\b.json")).AsCStr());
	EXPECT_STREQ("code:metric", OriginOf(MakeValueInfo("f", "k", ValueKind::Metric, "code:MetricRegistry")).AsCStr());
	EXPECT_STREQ("code:arg", OriginOf(MakeValueInfo("f", "k", ValueKind::Arg, "code:CommandRegistry")).AsCStr());
}

TEST_F(ExportFixture, LiveQueryOutputUnchanged)
{
	const ValueInfo* info = mRamps.Svc().At(0);
	ASSERT_NE(nullptr, info);
	EXPECT_FALSE(ValueInfoToJson(*info).isMember("origin"));
}

TEST_F(ExportFixture, EntriesInRegistrationOrder)
{
	const Json::Value doc = Build();
	ASSERT_EQ(mRamps.Svc().Count(), doc["entries"].size());
	for (unsigned int i = 0; i < mRamps.Svc().Count(); ++i)
	{
		EXPECT_EQ(std::string(mRamps.Svc().At(i)->mKey.AsCStr()), doc["entries"][i]["key"].asString());
	}
}

TEST_F(ExportFixture, RampsEqualQueryRamps)
{
	const Json::Value doc = Build();
	const Json::Value live = ValueSchemaQueryHandler(mRamps.Svc()).Handle(Json::Value(Json::objectValue));
	ASSERT_GT(live["ramps"].size(), 0u);
	EXPECT_EQ(live["ramps"], doc["ramps"]);
}

TEST_F(ExportFixture, UndescribedListsUseObjectShapes)
{
	const Json::Value doc = Build();
	const Json::Value& u = doc["undescribed"];
	const Json::Value* m = FindByName(u["metrics"], "cow.weather.ticks");
	ASSERT_NE(nullptr, m);
	EXPECT_EQ("cow", (*m)["owner"].asString());
	EXPECT_EQ(nullptr, FindByName(u["metrics"], "cow.ecology.herbivores_total"));

	const Json::Value* save = FindByName(u["commands"], "cow.save");
	ASSERT_NE(nullptr, save);
	EXPECT_EQ("CoW", (*save)["owner"].asString());
	ASSERT_EQ(1u, (*save)["args"].size());
	EXPECT_EQ("slot", (*save)["args"][0].asString());
	const Json::Value* bare = FindByName(u["commands"], "cow.bare");
	ASSERT_NE(nullptr, bare);
	ASSERT_TRUE((*bare)["args"].isArray());
	EXPECT_EQ(0u, (*bare)["args"].size());
	EXPECT_EQ(nullptr, FindByName(u["commands"], "cow.load"));
	EXPECT_EQ(nullptr, FindByName(u["commands"], "cow.ping"));	// noArgs is described by definition

	const Json::Value* f = FindByName(u["component_fields"], "Transform2D/scale");
	ASSERT_NE(nullptr, f);
	EXPECT_EQ("Transform2D", (*f)["owner"].asString());
}

TEST_F(ExportFixture, NoArgsCommandEntryCarriesUiNoArgs)
{
	const Json::Value doc = Build();
	const Json::Value* e = FindEntry(doc, "dia.command/cow.ping");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("code:command", (*e)["origin"].asString());
	EXPECT_TRUE((*e)["ui"]["no_args"].asBool());
	const Json::Value* other = FindEntry(doc, "dia.command/cow.save");
	ASSERT_NE(nullptr, other);
	EXPECT_FALSE((*other)["ui"].isMember("no_args"));
}

TEST_F(ExportFixture, DeterministicOutput)
{
	Json::Value a = Build();
	Json::Value b = Build();
	a["header"].removeMember("generated_at");
	b["header"].removeMember("generated_at");
	EXPECT_EQ(a, b);
}

TEST_F(ExportFixture, TextWithPercentQuotesAndUnicodeSurvives)
{
	const Json::Value doc = Build();
	const Json::Value* e = FindEntry(doc, "cow.ecology_params/grazing.rate");
	ASSERT_NE(nullptr, e);
	EXPECT_EQ("Grazing 100% \"fast\" \xC3\xA9", (*e)["label"].asString());
	EXPECT_EQ("Rate %s %d \\ \xE2\x82\xAC", (*e)["description"].asString());
}

TEST(ValueSchemaExporter, EmptyRegistryGivesValidEmptyArrays)
{
	VsTest::RampFixture f(false, true);
	ExportHeader h; h.mApp = "x"; h.mConfig = "Release";
	const Json::Value doc = ValueSchemaExporter().Build(f.Svc(), nullptr, nullptr, h);
	EXPECT_TRUE(doc["entries"].isArray());
	EXPECT_EQ(0u, doc["entries"].size());
	EXPECT_TRUE(doc["ramps"].isArray());
	EXPECT_EQ(0u, doc["ramps"].size());
	for (const char* k : { "metrics", "commands", "component_fields" })
	{
		EXPECT_TRUE(doc["undescribed"][k].isArray()) << k;
		EXPECT_EQ(0u, doc["undescribed"][k].size()) << k;
	}
}

TEST(ValueSchemaExporter, WriteFileWritesAtomicallyAndRoundTrips)
{
	const std::filesystem::path dir = std::filesystem::temp_directory_path() / "vs_export_test";
	std::filesystem::create_directories(dir);
	const std::string path = (dir / "export.json").string();
	Json::Value v(Json::objectValue);
	v["text"] = "100% \xC3\xA9 \"q\"";
	ASSERT_TRUE(ValueSchemaExporter::WriteFile(path.c_str(), v));
	ASSERT_TRUE(ValueSchemaExporter::WriteFile(path.c_str(), v));	// overwrite
	EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
	Json::Value back;
	{
		std::ifstream in(path, std::ios::binary);
		std::stringstream ss; ss << in.rdbuf();
		Json::Reader reader;
		ASSERT_TRUE(reader.parse(ss.str(), back));
	}
	EXPECT_EQ(v, back);
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST(ValueSchemaExporter, WriteFileFailsOnUnwritablePath)
{
	Json::Value v(Json::objectValue);
	EXPECT_FALSE(ValueSchemaExporter::WriteFile("Z:\\no_such_dir_vs\\deeper\\export.json", v));
	EXPECT_FALSE(ValueSchemaExporter::WriteFile("", v));
	EXPECT_FALSE(ValueSchemaExporter::WriteFile(nullptr, v));
}
