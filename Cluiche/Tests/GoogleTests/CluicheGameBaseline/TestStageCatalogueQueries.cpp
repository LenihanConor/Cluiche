#include <gtest/gtest.h>

#include <Modules/StageCatalogueModule.h>
#include <Modules/StageCatalogueQueries.h>
#include <Modules/StageCatalogueState.h>
#include <Testing/StageCatalogueFixture.h>
#include <DiaDebugServer/QueryRegistry.h>

#include <memory>
#include <string>
#include <vector>

using namespace Cluiche::AppFlow;
using Dia::Core::StringCRC;
namespace Q = Cluiche::AppFlow::StageCatalogueQueries;

namespace {

std::string DiagamePath(const char* fixture)
{
    return (Cluiche::AppFlow::Testing::FindStageCatalogueFixtures(__FILE__) / fixture / "game.diagame").string();
}

StageResultEvent MakeEvent(const char* stage, StageResultSeverity sev, uint64_t utcMs, const char* sessionId, const char* logPath)
{
    StageResultEvent e;
    e.stage = StringCRC(stage);
    e.result.severity = sev;
    e.result.SetLabel("PASS");
    e.result.utcMs = utcMs;
    e.result.SetSessionId(sessionId);
    e.result.SetLogPath(logPath);
    return e;
}

std::vector<std::string> Names(const Json::Value& list)
{
    std::vector<std::string> out;
    for (Json::ArrayIndex i = 0; i < list.size(); ++i) { out.push_back(list[i]["name"].asString()); }
    return out;
}

Json::Value Args(const char* json)
{
    Json::Value v;
    Json::Reader r;
    r.parse(json, v);
    return v;
}

struct Fixture {
    StageCatalogueState state;
    explicit Fixture(const char* name = "basic")
    {
        EXPECT_EQ(state.LoadCatalogue(DiagamePath(name).c_str()), StageCatalogueResult::kOk);
        state.ApplyEvent(MakeEvent("ShowTest", StageResultSeverity::kOk, 5, "sess-1", "C:/s/log.jsonl"), "", "");
    }
};

class TestableModule : public StageCatalogueModule {
public:
    TestableModule() : StageCatalogueModule(StringCRC("TestStageCatalogueQ")) {}
    void Configure(const std::string& diagame)
    {
        Json::Value cfg(Json::objectValue);
        cfg["diagame_path"] = diagame;
        Json::FastWriter w;
        OnConfigure(w.write(cfg).c_str());
    }
    bool Start() { return DoStart() == Dia::ApplicationFlow::StartResult::kReady; }
    void Stop() { DoStop(); }
};

} // namespace

TEST(StageCatalogueQueries, ListAll)
{
    Fixture f;
    const Json::Value r = Q::List(f.state, Json::Value(Json::objectValue));
    ASSERT_TRUE(r.isArray());
    EXPECT_EQ(r.size(), 7u);
    EXPECT_TRUE(r[0].isMember("name") && r[0].isMember("kinds") && r[0].isMember("tags") && r[0].isMember("description")
        && r[0].isMember("presets") && r[0].isMember("last_result"));
}

TEST(StageCatalogueQueries, ListNullArgsIsAll)
{
    Fixture f;
    EXPECT_EQ(Q::List(f.state, Json::Value()).size(), 7u);
}

TEST(StageCatalogueQueries, ListByTagAndByTags)
{
    Fixture f;
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"tag\":[\"test\"]}"))), (std::vector<std::string>{ "TestRig", "ShowTest" }));
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"tag\":[\"test\",\"showcase\"]}"))), (std::vector<std::string>{ "ShowTest" }));
    EXPECT_EQ(Q::List(f.state, Args("{\"tag\":[\"nope\"]}")).size(), 0u);
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"tag\":\"tool\"}"))), (std::vector<std::string>{ "ToolBench" }));
}

TEST(StageCatalogueQueries, ListByKind)
{
    Fixture f;
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"kind\":\"tool\"}"))), (std::vector<std::string>{ "ToolBench" }));
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"kind\":\"other\"}"))), (std::vector<std::string>{ "NoImport", "Misc", "Plain" }));
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"kind\":\"test\"}"))), (std::vector<std::string>{ "TestRig", "ShowTest" }));
}

TEST(StageCatalogueQueries, ListByTagAndKind)
{
    Fixture f;
    EXPECT_EQ(Names(Q::List(f.state, Args("{\"tag\":[\"test\"],\"kind\":\"showcase\"}"))), (std::vector<std::string>{ "ShowTest" }));
    EXPECT_EQ(Q::List(f.state, Args("{\"tag\":[\"tool\"],\"kind\":\"test\"}")).size(), 0u);
}

TEST(StageCatalogueQueries, ListUnknownKindIsError)
{
    Fixture f;
    const Json::Value r = Q::List(f.state, Args("{\"kind\":\"bogus\"}"));
    EXPECT_TRUE(r.isObject() && r.isMember("error"));
}

TEST(StageCatalogueQueries, ListCarriesLastResult)
{
    Fixture f;
    const Json::Value r = Q::List(f.state, Args("{\"tag\":[\"test\",\"showcase\"]}"));
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0]["last_result"]["label"].asString(), "PASS");
    EXPECT_EQ(r[0]["last_result"]["severity"].asString(), "ok");
    const Json::Value plain = Q::List(f.state, Args("{\"kind\":\"tool\"}"));
    EXPECT_TRUE(plain[0]["last_result"].isNull());
    EXPECT_EQ(plain[0]["kinds"][0].asString(), "tool");
}

TEST(StageCatalogueQueries, GetKnownStage)
{
    Fixture f;
    const Json::Value r = Q::Get(f.state, Args("{\"stage\":\"ShowTest\"}"));
    ASSERT_TRUE(r.isObject());
    EXPECT_EQ(r["name"].asString(), "ShowTest");
    ASSERT_EQ(r["kinds"].size(), 2u);
    EXPECT_EQ(r["kinds"][0].asString(), "showcase");
    EXPECT_EQ(r["kinds"][1].asString(), "test");
    EXPECT_EQ(r["tags"].size(), 2u);
    EXPECT_TRUE(r["proves"].isArray());
    EXPECT_TRUE(r["presets"].isArray());
    EXPECT_EQ(r["last_result"]["log_path"].asString(), "C:/s/log.jsonl");
}

TEST(StageCatalogueQueries, GetUnknownOrMissingStageIsError)
{
    Fixture f;
    EXPECT_TRUE(Q::Get(f.state, Args("{\"stage\":\"Nope\"}")).isMember("error"));
    EXPECT_TRUE(Q::Get(f.state, Json::Value(Json::objectValue)).isMember("error"));
}

TEST(StageCatalogueQueries, ResultsAll)
{
    Fixture f;
    f.state.ApplyEvent(MakeEvent("Plain", StageResultSeverity::kFail, 9, "sess-2", "C:/p/log.jsonl"), "", "");
    const Json::Value r = Q::Results(f.state, Json::Value(Json::objectValue));
    ASSERT_TRUE(r.isObject());
    EXPECT_EQ(r.getMemberNames().size(), 2u);
    EXPECT_EQ(r["ShowTest"]["session_id"].asString(), "sess-1");
    EXPECT_EQ(r["ShowTest"]["log_path"].asString(), "C:/s/log.jsonl");
    EXPECT_EQ(r["Plain"]["severity"].asString(), "fail");
    EXPECT_EQ(r["Plain"]["utc"].asUInt64(), 9u);
}

TEST(StageCatalogueQueries, ResultsSingleStage)
{
    Fixture f;
    const Json::Value r = Q::Results(f.state, Args("{\"stage\":\"ShowTest\"}"));
    EXPECT_EQ(r["label"].asString(), "PASS");
    EXPECT_EQ(r["log_path"].asString(), "C:/s/log.jsonl");
    // Known stage without a result: null.
    EXPECT_TRUE(Q::Results(f.state, Args("{\"stage\":\"Plain\"}")).isNull());
}

TEST(StageCatalogueQueries, ResultsUnknownStageIsError)
{
    Fixture f;
    EXPECT_TRUE(Q::Results(f.state, Args("{\"stage\":\"Nope\"}")).isMember("error"));
}

TEST(StageCatalogueQueries, EmptyCatalogueIsEmptyList)
{
    StageCatalogueState state;
    EXPECT_EQ(Q::List(state, Json::Value(Json::objectValue)).size(), 0u);
}

TEST(StageCatalogueQueries, FullCatalogueListFitsSixteenKB)
{
    Fixture f("over_cap");
    ASSERT_EQ(f.state.Catalogue().Size(), StageCatalogue::kMaxStages);
    const std::string text = Json::FastWriter().write(Q::List(f.state, Json::Value(Json::objectValue)));
    EXPECT_LT(text.size(), 16u * 1024u);
}

TEST(StageCatalogueQueries, HelpTextNonEmpty)
{
    EXPECT_GT(std::string(Q::kListHelp).size(), 0u);
    EXPECT_GT(std::string(Q::kGetHelp).size(), 0u);
    EXPECT_GT(std::string(Q::kResultsHelp).size(), 0u);
}

TEST(StageCatalogueModuleQueries, RegistersAndAnswersThroughRegistry)
{
    Dia::DebugServer::QueryRegistry registry;
    TestableModule module;
    module.SetQueryRegistry(&registry);
    module.Configure(DiagamePath("basic"));
    ASSERT_TRUE(module.Start());
    EXPECT_TRUE(module.QueriesRegistered());
    EXPECT_TRUE(registry.Has(StringCRC("stages.list")));
    EXPECT_TRUE(registry.Has(StringCRC("stages.get")));
    EXPECT_TRUE(registry.Has(StringCRC("stages.results")));
    EXPECT_EQ(registry.Execute(StringCRC("stages.list"), Json::Value(Json::objectValue)).size(), 7u);
    EXPECT_TRUE(registry.Execute(StringCRC("stages.get"), Args("{\"stage\":\"Nope\"}")).isMember("error"));
    module.Stop();
    EXPECT_FALSE(module.QueriesRegistered());
    EXPECT_FALSE(registry.Has(StringCRC("stages.list")));
}

TEST(StageCatalogueModuleQueries, AbsentServiceRunsWithoutQueries)
{
    TestableModule module;
    module.Configure(DiagamePath("basic"));
    ASSERT_TRUE(module.Start());
    EXPECT_FALSE(module.QueriesRegistered());
    EXPECT_EQ(module.State().Catalogue().Size(), 7u);
    module.Stop();
}
