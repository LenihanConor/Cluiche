#include <gtest/gtest.h>

#include <string>

#include <DiaAttribute/ParameterFile.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>
#include <Shared/Debug/ParamCommandHelper.h>
#include <Shared/Debug/Testing/FakeSimCommandHost.h>

namespace CoW {
namespace Debug {
namespace Testing {

namespace {

using namespace Dia::Attribute::Testing;

class ParamCommandHelperTest : public ::testing::Test
{
protected:
    ParamCommandHelperTest() : helper(World::kWeather, file) {}
    void SetUp() override
    {
        const std::string text = MakeSidecarText({
            SidecarFloat("rain.base", 0, 10, 5),
            SidecarInt("rain.days", 0, 10, 3),
            SidecarBool("wind.on", false),
            SidecarEnum("wind.mode", { "Slow", "Fast", "Turbo" }, "Slow"),
            SidecarFloat("zone", 0, 1, 0.5) }, nullptr, "test.params");
        Dia::Attribute::ParamError err;
        ASSERT_TRUE(file.Load(text.c_str(), "test.params.json", nullptr, err));
    }
    Json::Value Call(const char* action, const char* keyName = nullptr, const char* keyValue = nullptr)
    {
        Json::Value args(Json::objectValue);
        if (action != nullptr) { args["action"] = action; }
        if (keyName != nullptr) { args[keyName] = keyValue; }
        return helper.Handle(args);
    }
    Json::Value Pack(const char* body)
    {
        Json::Value v;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(std::string("{\"format\":\"test.params\",\"version\":1,") + body + "}", v));
        return v;
    }

    Dia::Attribute::ParameterFile file;
    ParamCommandHelper helper;
};

}  // namespace

TEST_F(ParamCommandHelperTest, CommandNameIsCowSysParams)
{
    EXPECT_STREQ("cow.weather.params", helper.CommandName());
}

TEST_F(ParamCommandHelperTest, GetReturnsEveryField)
{
    const Json::Value r = Call("get", "key", "rain.base");
    ASSERT_TRUE(r["success"].asBool());
    EXPECT_EQ("rain.base", r["key"].asString());
    EXPECT_DOUBLE_EQ(5.0, r["value"].asDouble());
    EXPECT_DOUBLE_EQ(5.0, r["default"].asDouble());
    EXPECT_DOUBLE_EQ(0.0, r["min"].asDouble());
    EXPECT_DOUBLE_EQ(10.0, r["max"].asDouble());
    EXPECT_EQ("float", r["kind"].asString());
    EXPECT_EQ("default", r["source"].asString());
}

TEST_F(ParamCommandHelperTest, GetRendersEachKind)
{
    EXPECT_TRUE(Call("get", "key", "rain.days")["value"].isIntegral());
    EXPECT_EQ("int", Call("get", "key", "rain.days")["kind"].asString());
    EXPECT_TRUE(Call("get", "key", "wind.on")["value"].isBool());
    EXPECT_EQ("bool", Call("get", "key", "wind.on")["kind"].asString());
    const Json::Value mode = Call("get", "key", "wind.mode");
    EXPECT_EQ("enum", mode["kind"].asString());
    EXPECT_EQ("Slow", mode["value"].asString());
    EXPECT_EQ("Slow", mode["default"].asString());
}

TEST_F(ParamCommandHelperTest, GetShowsTheCurrentValueNextToTheDefault)
{
    Dia::Attribute::ParamError err;
    ASSERT_TRUE(file.Set(Dia::Core::StringCRC("rain.base"), Json::Value(7.5), err));
    const Json::Value r = Call("get", "key", "rain.base");
    EXPECT_DOUBLE_EQ(7.5, r["value"].asDouble());
    EXPECT_DOUBLE_EQ(5.0, r["default"].asDouble());
}

TEST_F(ParamCommandHelperTest, GetUnknownKeyIsAnErrorNamingTheKey)
{
    const Json::Value r = Call("get", "key", "rain.nope");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("rain.nope"));
}

TEST_F(ParamCommandHelperTest, GetWithoutKeyIsAnError)
{
    EXPECT_FALSE(Call("get")["success"].asBool());
}

TEST_F(ParamCommandHelperTest, ListReturnsAllInSchemaOrder)
{
    const Json::Value r = Call("list");
    ASSERT_TRUE(r["success"].asBool());
    ASSERT_EQ(5u, r["params"].size());
    EXPECT_EQ("rain.base", r["params"][0u]["key"].asString());
    EXPECT_EQ("rain.days", r["params"][1u]["key"].asString());
    EXPECT_EQ("wind.on", r["params"][2u]["key"].asString());
    EXPECT_EQ("wind.mode", r["params"][3u]["key"].asString());
    EXPECT_EQ("zone", r["params"][4u]["key"].asString());
    EXPECT_EQ("default", r["params"][4u]["source"].asString());
}

TEST_F(ParamCommandHelperTest, ListFiltersByPrefixKeepingSchemaOrder)
{
    const Json::Value r = Call("list", "prefix", "wind.");
    ASSERT_EQ(2u, r["params"].size());
    EXPECT_EQ("wind.on", r["params"][0u]["key"].asString());
    EXPECT_EQ("wind.mode", r["params"][1u]["key"].asString());
}

TEST_F(ParamCommandHelperTest, ListWithAPrefixMatchingNothingIsAnEmptySuccess)
{
    const Json::Value r = Call("list", "prefix", "nothing.");
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(r["params"].isArray());
    EXPECT_EQ(0u, r["params"].size());
}

TEST_F(ParamCommandHelperTest, SourceIsPackIdForPackKeysAndLaterPackWins)
{
    const Json::Value p1 = Pack("\"rain.base\":6,\"zone\":0.25");
    const Json::Value p2 = Pack("\"rain.base\":7");
    Dia::Attribute::ParamWarnings warn;
    Dia::Attribute::ParamError err;
    ASSERT_TRUE(file.ApplyPack(p1, warn, err));
    helper.NotePackApplied("first", p1);
    ASSERT_TRUE(file.ApplyPack(p2, warn, err));
    helper.NotePackApplied("second", p2);
    EXPECT_EQ("pack:second", Call("get", "key", "rain.base")["source"].asString());
    EXPECT_EQ("pack:first", Call("get", "key", "zone")["source"].asString());
    EXPECT_EQ("default", Call("get", "key", "wind.on")["source"].asString());
    EXPECT_DOUBLE_EQ(7.0, Call("get", "key", "rain.base")["value"].asDouble());
}

TEST_F(ParamCommandHelperTest, ClearSourcesReturnsEverythingToDefault)
{
    const Json::Value p1 = Pack("\"zone\":0.25");
    helper.NotePackApplied("first", p1);
    helper.ClearSources();
    EXPECT_EQ("default", Call("get", "key", "zone")["source"].asString());
}

TEST_F(ParamCommandHelperTest, MissingOrUnknownActionIsAnError)
{
    EXPECT_FALSE(Call(nullptr)["success"].asBool());
    const Json::Value r = Call("explode");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("explode"));
}

TEST_F(ParamCommandHelperTest, LaterActionsReportNotImplementedYet)
{
    EXPECT_FALSE(Call("set")["success"].asBool());
    EXPECT_FALSE(Call("reload")["success"].asBool());
    EXPECT_FALSE(Call("save_defaults")["success"].asBool());
}

TEST_F(ParamCommandHelperTest, EntryRegistersAsACommandWithHelpOnTheRealHost)
{
    Dia::DebugServer::SimQueryHost real;
    SimQueryHostAdapter adapter(real);
    SimCommandSet set;
    const SimCommandEntry entry = helper.BuildEntry();
    EXPECT_EQ(EntryKind::kCommand, entry.kind);
    ASSERT_TRUE(set.RegisterAll(adapter, &entry, 1, "ParamCommandHelperTest"));
    EXPECT_TRUE(real.Has(Dia::Core::StringCRC("cow.weather.params")));
    const Dia::DebugServer::SimHelpStored* help = real.GetHelp(Dia::Core::StringCRC("cow.weather.params"));
    ASSERT_NE(nullptr, help);
    EXPECT_GT(strlen(help->description), 0u);
    Json::Value args(Json::objectValue);
    args["action"] = "get";
    args["key"] = "zone";
    EXPECT_TRUE(entry.handler(args)["success"].asBool());
}

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
