#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Panel transport for long queries: the JS bridge caps each argument string, so the page sends
// "query_part" {i, s} chunks and then query_check / query_run {n: partCount}.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct PartsRig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			t.mEnumValues.Reserve(5);
			for (const char* id : { "grass", "water", "rock", "sand", "unused" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			return &f.Publish();
		}

		PartsRig()
			: grid(8, 4)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			const size_t n = 32;
			terrain.values.resize(n);
			for (size_t i = 0; i < n; ++i) { terrain.values[i] = static_cast<float>(i % 4); }
			temp.values = std::vector<float>(n, 1.0f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			domain.Update(draw);
		}

		void Part(int i, const std::string& s)
		{
			Json::Value a(Json::objectValue);
			a["i"] = i;
			a["s"] = s;
			domain.OnCommand(StringCRC("query_part"), a);
		}

		void Send(const char* cmd, int n)
		{
			Json::Value a(Json::objectValue);
			a["n"] = n;
			domain.OnCommand(StringCRC(cmd), a);
		}

		Json::Value Query()
		{
			Json::Value out(Json::objectValue);
			domain.GetJSONState(out);
			return out["query"];
		}
	};

	struct QueryPanelPartsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; }
	};
}

TEST_F(QueryPanelPartsTest, PartsAssembleAndRun)
{
	PartsRig rig;
	rig.Part(0, "terrain ==");
	rig.Part(1, " water");
	rig.Send("query_run", 2);
	const Json::Value q = rig.Query();
	EXPECT_EQ(q["last"]["expr"].asString(), "terrain == water");
	EXPECT_TRUE(q["last"]["ok"].asBool());
	EXPECT_EQ(q["last"]["count"].asUInt(), 8u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 8u);
}

TEST_F(QueryPanelPartsTest, PartsFeedCheckWithoutTouchingSelection)
{
	PartsRig rig;
	rig.Part(0, "terrain == w");
	rig.Part(1, "ater");
	rig.Send("query_check", 2);
	const Json::Value q = rig.Query();
	EXPECT_EQ(q["check"]["expr"].asString(), "terrain == water");
	EXPECT_TRUE(q["check"]["ok"].asBool());
	EXPECT_EQ(q["check"]["count"].asUInt(), 8u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
}

TEST_F(QueryPanelPartsTest, ShortFormStillWorks)
{
	PartsRig rig;
	Json::Value a(Json::objectValue);
	a["e"] = "terrain == water";
	rig.domain.OnCommand(StringCRC("query_run"), a);
	const Json::Value q = rig.Query();
	EXPECT_EQ(q["last"]["expr"].asString(), "terrain == water");
	EXPECT_TRUE(q["last"]["ok"].asBool());
	EXPECT_EQ(q["last"]["count"].asUInt(), 8u);
}

TEST_F(QueryPanelPartsTest, OutOfOrderPartIsIgnored)
{
	PartsRig rig;
	rig.Part(0, "terrain");
	rig.Part(2, " == water");	// index 1 skipped: ignored
	rig.Send("query_check", 2);	// only one part arrived
	const Json::Value q = rig.Query();
	EXPECT_FALSE(q["check"]["ok"].asBool());
	EXPECT_EQ(q["check"]["error"]["message"].asString(), "incomplete query (panel transport)");
	rig.Send("query_check", 1);	// the one valid part is still usable
	const Json::Value q1 = rig.Query();
	EXPECT_EQ(q1["check"]["expr"].asString(), "terrain");
}

TEST_F(QueryPanelPartsTest, OverflowPartIsIgnored)
{
	PartsRig rig;
	rig.Part(0, std::string(300, 'a'));
	rig.Part(1, std::string(300, 'b'));	// 600 > 512: ignored
	rig.Send("query_check", 2);
	EXPECT_EQ(rig.Query()["check"]["error"]["message"].asString(), "incomplete query (panel transport)");
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), std::string(300, 'a'));
}

TEST_F(QueryPanelPartsTest, IncompleteCountGivesErrorRecord)
{
	PartsRig rig;
	rig.Part(0, "terrain ==");
	rig.Part(1, " water");
	rig.Send("query_run", 3);
	const Json::Value q = rig.Query();
	EXPECT_FALSE(q["last"]["ok"].asBool());
	EXPECT_EQ(q["last"]["error"]["message"].asString(), "incomplete query (panel transport)");
	EXPECT_EQ(q["last"]["error"]["column"].asUInt(), 0u);
	EXPECT_EQ(q["last"]["error"]["length"].asUInt(), 0u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
}

TEST_F(QueryPanelPartsTest, NewFirstPartResetsBuffer)
{
	PartsRig rig;
	rig.Part(0, "temp > 99");
	rig.Part(0, "terrain == water");
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), "terrain == water");
}

TEST_F(QueryPanelPartsTest, FullLengthQueryRoundTrips)
{
	PartsRig rig;
	std::string text = "temp >= 1";
	text.append(512 - text.size(), ' ');
	ASSERT_EQ(text.size(), 512u);
	int idx = 0;
	for (size_t off = 0; off < text.size(); off += 40) { rig.Part(idx++, text.substr(off, 40)); }
	rig.Send("query_check", idx);
	const Json::Value q = rig.Query();
	EXPECT_EQ(q["check"]["expr"].asString(), text);
	EXPECT_TRUE(q["check"]["ok"].asBool());
	EXPECT_EQ(q["check"]["count"].asUInt(), 32u);
}

// ---- Task 10a exhaustive pass (Feature 3) ----

TEST_F(QueryPanelPartsTest, ExactlyFiveTwelveAcrossTwoPartsIsAcceptedAndOneOverIsRejected)
{
	PartsRig rig;
	std::string a = "temp >= 1";
	a.append(300 - a.size(), ' ');
	rig.Part(0, a);
	rig.Part(1, std::string(212, ' '));	// 300 + 212 = 512: fits exactly
	rig.Send("query_check", 2);
	EXPECT_TRUE(rig.Query()["check"]["ok"].asBool());
	EXPECT_EQ(rig.Query()["check"]["expr"].asString().size(), 512u);

	rig.Part(0, a);
	rig.Part(1, std::string(213, ' '));	// 513: second part dropped
	rig.Send("query_check", 2);
	EXPECT_EQ(rig.Query()["check"]["error"]["message"].asString(), "incomplete query (panel transport)");
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), a);
}

TEST_F(QueryPanelPartsTest, PartsAfterAnOverflowAreAlsoIgnoredUntilANewFirstPart)
{
	PartsRig rig;
	rig.Part(0, std::string(400, 'a'));
	rig.Part(1, std::string(200, 'b'));	// overflow: dropped
	rig.Part(2, "c");					// index 2 != expected 1: dropped
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), std::string(400, 'a'));
	rig.Part(0, "temp >= 1");
	rig.Send("query_check", 1);
	EXPECT_TRUE(rig.Query()["check"]["ok"].asBool());
}

TEST_F(QueryPanelPartsTest, FirstPartMustBeIndexZero)
{
	PartsRig rig;
	rig.Part(1, "temp >= 1");	// nothing assembled yet: ignored
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["error"]["message"].asString(), "incomplete query (panel transport)");
}

TEST_F(QueryPanelPartsTest, MalformedPartArgumentsAreIgnoredWithoutChangingTheBuffer)
{
	PartsRig rig;
	rig.Part(0, "temp >= 1");
	Json::Value noIndex(Json::objectValue);
	noIndex["s"] = " and x";
	rig.domain.OnCommand(StringCRC("query_part"), noIndex);
	Json::Value negative(Json::objectValue);
	negative["i"] = -1;
	negative["s"] = " and x";
	rig.domain.OnCommand(StringCRC("query_part"), negative);
	Json::Value notString(Json::objectValue);
	notString["i"] = 1;
	notString["s"] = 5;
	rig.domain.OnCommand(StringCRC("query_part"), notString);
	rig.domain.OnCommand(StringCRC("query_part"), Json::Value("temp"));	// not an object
	rig.Send("query_check", 1);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), "temp >= 1");
	EXPECT_TRUE(rig.Query()["check"]["ok"].asBool());
}

TEST_F(QueryPanelPartsTest, EmptyPartCountsAsAPart)
{
	PartsRig rig;
	rig.Part(0, "temp >= 1");
	rig.Part(1, "");
	rig.Part(2, " ");
	rig.Send("query_check", 3);
	EXPECT_TRUE(rig.Query()["check"]["ok"].asBool());
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), "temp >= 1 ");
}

TEST_F(QueryPanelPartsTest, CountZeroOrMissingCountIsIncompleteOrEmpty)
{
	PartsRig rig;
	rig.Part(0, "temp >= 1");
	rig.Send("query_check", 0);
	EXPECT_EQ(rig.Query()["check"]["error"]["message"].asString(), "incomplete query (panel transport)");
	rig.domain.OnCommand(StringCRC("query_check"), Json::Value(Json::objectValue));	// no e / expr / n at all
	EXPECT_EQ(rig.Query()["check"]["error"]["message"].asString(), "empty query");
	rig.Send("query_run", 0);
	EXPECT_EQ(rig.Query()["last"]["error"]["message"].asString(), "incomplete query (panel transport)");
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
}

TEST_F(QueryPanelPartsTest, AssembledErrorColumnSpansPartBoundaries)
{
	PartsRig rig;
	rig.Part(0, "temp >= 1 and terr");
	rig.Part(1, "ian == water");	// "terrian" straddles the two parts
	rig.Send("query_check", 2);
	const Json::Value e = rig.Query()["check"]["error"];
	EXPECT_EQ(e["column"].asUInt(), 14u);
	EXPECT_EQ(e["length"].asUInt(), 7u);
	EXPECT_EQ(e["suggestion"].asString(), "terrain");
}

TEST_F(QueryPanelPartsTest, ReRunningTheSameAssembledPartsIsRepeatable)
{
	PartsRig rig;
	rig.Part(0, "terrain ==");
	rig.Part(1, " water");
	rig.Send("query_run", 2);
	const uint32_t v = rig.domain.ActiveSelectionView()->Version();
	rig.Send("query_run", 2);	// parts are still buffered; same result, no further bump
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), v);
	EXPECT_EQ(rig.Query()["last"]["count"].asUInt(), 8u);
	rig.Send("query_check", 2);
	EXPECT_EQ(rig.Query()["check"]["count"].asUInt(), 8u);
}

TEST_F(QueryPanelPartsTest, ExprKeyAndShortKeyBothFeedTheQueryAndShortKeyWinsWhenBothPresent)
{
	PartsRig rig;
	Json::Value a(Json::objectValue);
	a["expr"] = "temp >= 1";
	rig.domain.OnCommand(StringCRC("query_check"), a);
	EXPECT_EQ(rig.Query()["check"]["count"].asUInt(), 32u);
	a["e"] = "temp > 100";
	rig.domain.OnCommand(StringCRC("query_check"), a);
	EXPECT_EQ(rig.Query()["check"]["expr"].asString(), "temp > 100");
	EXPECT_EQ(rig.Query()["check"]["count"].asUInt(), 0u);
}

TEST_F(QueryPanelPartsTest, RunErrorViaPartsKeepsSelectionAndRecordsStructuredError)
{
	PartsRig rig;
	rig.domain.ActiveSelection()->Set(1, true);
	rig.Part(0, "terrain >");
	rig.Part(1, " 1");
	rig.Send("query_run", 2);
	const Json::Value last = rig.Query()["last"];
	EXPECT_FALSE(last["ok"].asBool());
	EXPECT_EQ(last["expr"].asString(), "terrain > 1");
	EXPECT_TRUE(last["error"]["column"].isUInt());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 1u);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(1));
}

#endif // DIA_DEBUG
