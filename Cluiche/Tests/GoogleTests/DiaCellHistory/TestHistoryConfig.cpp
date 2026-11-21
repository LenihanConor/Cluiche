#include <gtest/gtest.h>
#include <DiaCellHistory/HistoryConfig.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <string.h>

using namespace Dia::CellHistory;

namespace
{
	Json::Value Parse(const char* text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		EXPECT_TRUE(r->parse(text, text + strlen(text), &v, &errs)) << errs;
		return v;
	}

	HistoryConfigResult ParseText(const char* text, HistoryConfig& out) { return ParseHistoryConfig(Parse(text), out); }

	void ExpectRejected(const char* text, const char* keyName)
	{
		HistoryConfig cfg;
		cfg.mCellBudgetMB = 7;	// sentinel: a rejected block must not touch the output
		cfg.mBandCount = 1;
		HistoryConfigResult r = ParseText(text, cfg);
		EXPECT_FALSE(r.mOk) << text;
		EXPECT_NE(std::string(r.mError.AsCStr()).find(keyName), std::string::npos) << "error was: " << r.mError.AsCStr();
		EXPECT_EQ(cfg.mCellBudgetMB, 7u);
		EXPECT_EQ(cfg.mBandCount, 1u);
	}

	// Builds a one-entry schema with the given history block text and returns the loaded entry.
	Dia::ValueSchema::ValueInfo EntryWithHistory(const char* historyBlock)
	{
		std::string doc = R"({"format":"dia.value_schema","version":1,"for":"t","entries":[{"key":"k","kind":"parameter","label":"L","description":"D","type":"float")";
		if (historyBlock) { doc += std::string(",\"history\":") + historyBlock; }
		doc += "}]}";
		Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(doc.c_str(), "t.json");
		EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
		return r.mEntries[0];
	}
}

TEST(HistoryConfig, EmptyObjectGivesDefaults)
{
	HistoryConfig cfg;
	HistoryConfigResult r = ParseText("{}", cfg);
	ASSERT_TRUE(r.mOk) << r.mError.AsCStr();
	EXPECT_EQ(cfg.mBandCount, 0u);
	EXPECT_EQ(cfg.mTrackCount, 0u);
	EXPECT_STREQ(cfg.mGrid.AsCStr(), "");
	EXPECT_EQ(cfg.mCellBudgetMB, 64u);
	EXPECT_EQ(cfg.mAggregateBudgetMB, 64u);
	EXPECT_EQ(cfg.mEventBudgetMB, 16u);
	EXPECT_EQ(cfg.mMaxAggregateLayers, 64u);
}

TEST(HistoryConfig, PartialConfigKeepsOtherDefaults)
{
	HistoryConfig cfg;
	ASSERT_TRUE(ParseText(R"({"eventBudgetMB": 4, "grid": "world.hex"})", cfg).mOk);
	EXPECT_EQ(cfg.mEventBudgetMB, 4u);
	EXPECT_STREQ(cfg.mGrid.AsCStr(), "world.hex");
	EXPECT_EQ(cfg.mCellBudgetMB, 64u);
	EXPECT_EQ(cfg.mBandCount, 0u);
}

TEST(HistoryConfig, FullConfigRoundTrip)
{
	HistoryConfig cfg;
	HistoryConfigResult r = ParseText(R"({
		"bands": ["Day", "Week", "Season", "Year", "Decade", "Century"],
		"track": ["ecology.producers_total", "ecology.group.herbivore", "weather.precipitation"],
		"grid": "world.hex",
		"cellBudgetMB": 32, "aggregateBudgetMB": 48, "eventBudgetMB": 8,
		"maxAggregateLayers": 10 })", cfg);
	ASSERT_TRUE(r.mOk) << r.mError.AsCStr();
	ASSERT_EQ(cfg.mBandCount, 6u);
	EXPECT_STREQ(cfg.mBands[0].AsCStr(), "Day");
	EXPECT_STREQ(cfg.mBands[5].AsCStr(), "Century");
	ASSERT_EQ(cfg.mTrackCount, 3u);
	EXPECT_STREQ(cfg.mTrack[0].AsCStr(), "ecology.producers_total");
	EXPECT_STREQ(cfg.mTrack[2].AsCStr(), "weather.precipitation");
	EXPECT_STREQ(cfg.mGrid.AsCStr(), "world.hex");
	EXPECT_EQ(cfg.mCellBudgetMB, 32u);
	EXPECT_EQ(cfg.mAggregateBudgetMB, 48u);
	EXPECT_EQ(cfg.mEventBudgetMB, 8u);
	EXPECT_EQ(cfg.mMaxAggregateLayers, 10u);
}

TEST(HistoryConfig, PercentInTextIsVerbatim)
{
	HistoryConfig cfg;
	ASSERT_TRUE(ParseText(R"({"grid": "a%s%d", "track": ["x%n"]})", cfg).mOk);
	EXPECT_STREQ(cfg.mGrid.AsCStr(), "a%s%d");
	EXPECT_STREQ(cfg.mTrack[0].AsCStr(), "x%n");
}

TEST(HistoryConfig, NonObjectRejected)
{
	HistoryConfig cfg;
	EXPECT_FALSE(ParseHistoryConfig(Parse("[1]"), cfg).mOk);
	EXPECT_FALSE(ParseHistoryConfig(Parse("5"), cfg).mOk);
}

TEST(HistoryConfig, BadBudgetsRejectedNamingKey)
{
	for (const char* key : { "cellBudgetMB", "aggregateBudgetMB", "eventBudgetMB" })
	{
		for (const char* bad : { "0", "-1", "\"64\"", "1.5", "true", "null", "[]" })
		{
			std::string text = std::string("{\"") + key + "\": " + bad + "}";
			ExpectRejected(text.c_str(), key);
		}
	}
}

TEST(HistoryConfig, MaxAggregateLayersBounds)
{
	ExpectRejected(R"({"maxAggregateLayers": 0})", "maxAggregateLayers");
	ExpectRejected(R"({"maxAggregateLayers": 65})", "maxAggregateLayers");
	ExpectRejected(R"({"maxAggregateLayers": -3})", "maxAggregateLayers");
	ExpectRejected(R"({"maxAggregateLayers": "8"})", "maxAggregateLayers");
	HistoryConfig cfg;
	ASSERT_TRUE(ParseText(R"({"maxAggregateLayers": 64})", cfg).mOk);
	EXPECT_EQ(cfg.mMaxAggregateLayers, 64u);
	ASSERT_TRUE(ParseText(R"({"maxAggregateLayers": 1})", cfg).mOk);
	EXPECT_EQ(cfg.mMaxAggregateLayers, 1u);
}

TEST(HistoryConfig, BadGridRejected)
{
	ExpectRejected(R"({"grid": 5})", "grid");
	ExpectRejected(R"({"grid": ["a"]})", "grid");
	std::string longName(64, 'g');
	ExpectRejected((std::string("{\"grid\": \"") + longName + "\"}").c_str(), "grid");
}

TEST(HistoryConfig, BadArraysRejected)
{
	ExpectRejected(R"({"bands": "Day"})", "bands");
	ExpectRejected(R"({"bands": ["Day", 3]})", "bands");
	ExpectRejected(R"({"bands": ["Day", null]})", "bands");
	ExpectRejected(R"({"bands": [""]})", "bands");
	ExpectRejected(R"({"track": {"a": 1}})", "track");
	ExpectRejected(R"({"track": ["ok", false]})", "track");
	std::string longName(64, 't');
	ExpectRejected((std::string("{\"track\": [\"") + longName + "\"]}").c_str(), "track");
}

TEST(HistoryConfig, OverCapacityArraysRejectedExactCapacityOk)
{
	auto makeArray = [](const char* key, unsigned n)
	{
		std::string s = std::string("{\"") + key + "\": [";
		for (unsigned i = 0; i < n; ++i) { s += std::string(i ? ",\"n" : "\"n") + std::to_string(i) + "\""; }
		return s + "]}";
	};
	ExpectRejected(makeArray("bands", HistoryConfig::kMaxBands + 1).c_str(), "bands");
	ExpectRejected(makeArray("track", HistoryConfig::kMaxTrack + 1).c_str(), "track");
	HistoryConfig cfg;
	ASSERT_TRUE(ParseText(makeArray("bands", HistoryConfig::kMaxBands).c_str(), cfg).mOk);
	EXPECT_EQ(cfg.mBandCount, HistoryConfig::kMaxBands);
	ASSERT_TRUE(ParseText(makeArray("track", HistoryConfig::kMaxTrack).c_str(), cfg).mOk);
	EXPECT_EQ(cfg.mTrackCount, HistoryConfig::kMaxTrack);
	EXPECT_STREQ(cfg.mTrack[HistoryConfig::kMaxTrack - 1].AsCStr(), "n63");
}

TEST(HistoryConfig, UnknownKeyIgnoredAndRestApplied)
{
	HistoryConfig cfg;
	HistoryConfigResult r = ParseText(R"({"bogus": 1, "cellBudgetMB": 12})", cfg);
	ASSERT_TRUE(r.mOk) << r.mError.AsCStr();
	EXPECT_EQ(cfg.mCellBudgetMB, 12u);
}

TEST(HistoryConfig, ReparseResetsPreviousValues)
{
	HistoryConfig cfg;
	ASSERT_TRUE(ParseText(R"({"bands": ["A","B"], "track": ["x"], "grid": "g", "cellBudgetMB": 1})", cfg).mOk);
	ASSERT_TRUE(ParseText("{}", cfg).mOk);
	EXPECT_EQ(cfg.mBandCount, 0u);
	EXPECT_EQ(cfg.mTrackCount, 0u);
	EXPECT_STREQ(cfg.mGrid.AsCStr(), "");
	EXPECT_EQ(cfg.mCellBudgetMB, 64u);
}

TEST(HistorySchemaFlags, DefaultsWhenNoHistoryBlock)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory(nullptr);
	EXPECT_FALSE(HistoryTrack(v));
	EXPECT_FALSE(HistoryGroup(v));
	EXPECT_TRUE(HistoryAggregate(v));
}

TEST(HistorySchemaFlags, DefaultsWhenEmptyBlock)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory("{}");
	EXPECT_FALSE(HistoryTrack(v));
	EXPECT_FALSE(HistoryGroup(v));
	EXPECT_TRUE(HistoryAggregate(v));
}

TEST(HistorySchemaFlags, ExplicitTrue)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory(R"({"track": true, "group": true, "aggregate": true})");
	EXPECT_TRUE(HistoryTrack(v));
	EXPECT_TRUE(HistoryGroup(v));
	EXPECT_TRUE(HistoryAggregate(v));
}

TEST(HistorySchemaFlags, ExplicitFalse)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory(R"({"track": false, "group": false, "aggregate": false})");
	EXPECT_FALSE(HistoryTrack(v));
	EXPECT_FALSE(HistoryGroup(v));
	EXPECT_FALSE(HistoryAggregate(v));
}

TEST(HistorySchemaFlags, AggregateFalseAloneLeavesOthersDefault)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory(R"({"aggregate": false})");
	EXPECT_FALSE(HistoryAggregate(v));
	EXPECT_FALSE(HistoryTrack(v));
	EXPECT_FALSE(HistoryGroup(v));
}

TEST(HistorySchemaFlags, GroupNameCountsAsGroup)
{
	EXPECT_TRUE(HistoryGroup(EntryWithHistory(R"({"group": "growth"})")));
	EXPECT_FALSE(HistoryGroup(EntryWithHistory(R"({"group": ""})")));
}

TEST(HistorySchemaFlags, NonBoolValuesFallBackToDefaults)
{
	Dia::ValueSchema::ValueInfo v = EntryWithHistory(R"({"track": 1, "aggregate": "no"})");
	EXPECT_FALSE(HistoryTrack(v));
	EXPECT_TRUE(HistoryAggregate(v));
}
