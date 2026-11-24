#include <gtest/gtest.h>

#include <string>

#include <DiaAttribute/ParameterFile.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>
#include <Shared/Debug/ParamCommandHelper.h>
#include <Shared/Debug/Testing/FakeSimCommandHost.h>
#include <Shared/WorldStep/MutationQueue.h>

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

    // Wires the helper to a small queue standing in for the stage's mutation queue; Capture drains it.
    void Wire(uint32_t capacity = 8, uint32_t effectiveDay = 41, uint32_t lag = 0)
    {
        queueCapacity = capacity;
        helper.SetMutationEnqueue([this](const ParamMutation& m)
        {
            if (queue.size() >= queueCapacity) { return false; }
            queue.push_back(m);
            return true;
        });
        helper.SetDayProvider([effectiveDay, lag]() { ParamDayInfo d; d.effectiveDay = effectiveDay; d.deferLag = lag; return d; });
    }
    // The owner's next Capture: drain in order through the stage hook.
    void Capture()
    {
        for (const ParamMutation& m : queue) { helper.ApplyMutation(m); }
        queue.clear();
    }
    Json::Value Set(const char* key, const Json::Value& value)
    {
        Json::Value args(Json::objectValue);
        args["action"] = "set";
        args["key"] = key;
        args["value"] = value;
        return helper.Handle(args);
    }
    double Base(const char* key) { return Call("get", "key", key)["value"].asDouble(); }

    Dia::Attribute::ParameterFile file;
    ParamCommandHelper helper;
    std::vector<ParamMutation> queue;
    uint32_t queueCapacity = 8;
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

TEST_F(ParamCommandHelperTest, SaveDefaultsWithoutSidecarIoIsAnError)
{
    const Json::Value r = Call("save_defaults");
    EXPECT_FALSE(r["success"].asBool());
}

namespace {

// Hand-written sidecar with odd whitespace, comments and a map entry; the bytes outside the defaults must survive.
const char* kOddSidecar =
    "// header comment\n"
    "{  \"format\" : \"dia.value_schema\" ,\n"
    "\t\"version\":1, \"for\":\"test.params\",\n"
    "  \"entries\" : [\n"
    "    { \"key\":\"rain.base\", \"kind\":\"parameter\", \"label\":\"l\", \"description\":\"d\", \"type\":\"float\", \"min\":0, \"max\":10, \"default\"  :  5.0 },  // c1\n"
    "    { \"key\":\"rain.days\", \"kind\":\"parameter\", \"label\":\"l\", \"description\":\"d\", \"type\":\"int\", \"min\":0, \"max\":10,\n"
    "       \"default\":3 },\n"
    "    { \"key\":\"wind.on\", \"kind\":\"parameter\", \"label\":\"l\", \"description\":\"d\", \"type\":\"bool\", \"default\":false },\n"
    "    { \"key\":\"wind.mode\", \"kind\":\"parameter\", \"label\":\"l\", \"description\":\"d\", \"type\":\"enum\",\n"
    "      \"values\":[{\"id\":\"Slow\",\"label\":\"Slow\"},{\"id\":\"Fast\",\"label\":\"Fast\"}], \"default\" : \"Slow\" },\n"
    "    { \"key\":\"yield\", \"kind\":\"parameter\", \"label\":\"l\", \"description\":\"d\", \"type\":\"map\",\n"
    "      \"keyRef\":\"cow.biomes/biomes[].id\", \"valueType\":\"float\", \"min\":0, \"max\":1,\n"
    "      \"default\": { \"TemperateForest\": 1.0,   \"Desert\" : 0.5, \"Tundra\":0.25 } }\n"
    "  ]\n"
    "}\n";

class ParamCommandHelperSaveDefaultsTest : public ::testing::Test
{
protected:
    ParamCommandHelperSaveDefaultsTest() : helper(World::kWeather, file) {}
    void SetUp() override
    {
        Dia::Attribute::ParamError err;
        ASSERT_TRUE(file.Load(kOddSidecar, "odd.params.json", &keys, err)) << err.reason.AsCStr();
        stored = kOddSidecar;
        helper.SetSidecarIO(
            [this](std::string& text, std::string& path) { text = stored; path = "odd.params.json"; return true; },
            [this](const std::string& text) { ++writes; if (failWrite) { return false; } stored = text; return true; });
    }
    void SetBase(const char* key, const Json::Value& v)
    {
        Dia::Attribute::ParamError err;
        ASSERT_TRUE(file.Set(Dia::Core::StringCRC(key), v, err)) << key << ": " << err.reason.AsCStr();
    }
    Json::Value Save()
    {
        Json::Value args(Json::objectValue);
        args["action"] = "save_defaults";
        return helper.Handle(args);
    }
    static std::vector<std::string> Changed(const Json::Value& r)
    {
        std::vector<std::string> out;
        for (const Json::Value& v : r["changed"]) { out.push_back(v.asString()); }
        return out;
    }

    FixtureMapKeySource keys;
    Dia::Attribute::ParameterFile file;
    ParamCommandHelper helper;
    std::string stored;
    int writes = 0;
    bool failWrite = false;
};

std::string Replace(std::string s, const std::string& from, const std::string& to)
{
    const size_t at = s.find(from);
    EXPECT_NE(std::string::npos, at) << from;
    if (at != std::string::npos) { s.replace(at, from.size(), to); }
    return s;
}

}  // namespace

TEST_F(ParamCommandHelperSaveDefaultsTest, OnlyChangedKeysAreListedAndOnlyDefaultBytesChange)
{
    SetBase("rain.base", Json::Value(7.5));
    SetBase("rain.days", Json::Value(4));
    SetBase("wind.on", Json::Value(true));
    SetBase("wind.mode", Json::Value("Fast"));
    SetBase("yield.Desert", Json::Value(0.75));
    const Json::Value r = Save();
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ("odd.params.json", r["written"].asString());
    const std::vector<std::string> changed = Changed(r);
    EXPECT_EQ((std::vector<std::string>{ "rain.base", "rain.days", "wind.on", "wind.mode", "yield.Desert" }), changed);

    std::string expect = kOddSidecar;
    expect = Replace(expect, "\"default\"  :  5.0 }", "\"default\"  :  7.5 }");
    expect = Replace(expect, "\"default\":3 }", "\"default\":4 }");
    expect = Replace(expect, "\"default\":false }", "\"default\":true }");
    expect = Replace(expect, "\"default\" : \"Slow\" }", "\"default\" : \"Fast\" }");
    expect = Replace(expect, "\"Desert\" : 0.5,", "\"Desert\" : 0.75,");
    EXPECT_EQ(expect, stored);
    EXPECT_EQ(1, writes);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, MapColumnKeyIsListedAsKeyDotMapKey)
{
    SetBase("yield.Tundra", Json::Value(0.5));
    const Json::Value r = Save();
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ((std::vector<std::string>{ "yield.Tundra" }), Changed(r));
    EXPECT_EQ(Replace(kOddSidecar, "\"Tundra\":0.25", "\"Tundra\":0.5"), stored);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, NothingChangedIsEmptyListAndByteIdentical)
{
    const Json::Value r = Save();
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["changed"].isArray());
    EXPECT_EQ(0u, r["changed"].size());
    EXPECT_EQ(std::string(kOddSidecar), stored);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, ShippingBuildRefusesAndWritesNothing)
{
    SetBase("rain.base", Json::Value(7.5));
    helper.SetDevBuildFn([]() { return false; });
    const Json::Value r = Save();
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("save_defaults"));
    EXPECT_EQ(0, writes);
    EXPECT_EQ(std::string(kOddSidecar), stored);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, DefaultDevFlagMatchesTheBuildMacro)
{
#ifdef COW_SHIPPING_BUILD
    EXPECT_FALSE(ParamCommandHelper::DefaultIsDevBuild());
#else
    EXPECT_TRUE(ParamCommandHelper::DefaultIsDevBuild());
#endif
}

TEST_F(ParamCommandHelperSaveDefaultsTest, WriteFailureNamesThePathAndChangesNothing)
{
    SetBase("rain.base", Json::Value(7.5));
    failWrite = true;
    const Json::Value r = Save();
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("odd.params.json"));
    EXPECT_EQ(std::string(kOddSidecar), stored);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, ReadFailureIsAnError)
{
    helper.SetSidecarIO([](std::string&, std::string&) { return false; }, [](const std::string&) { return true; });
    EXPECT_FALSE(Save()["success"].asBool());
}

TEST_F(ParamCommandHelperSaveDefaultsTest, SystemActionReachesTheHandlerAndItsReplyIsReturned)
{
    std::string seen;
    helper.SetSystemActionHandler([&seen](const std::string& action, const Json::Value& args)
    {
        seen = action + ":" + args["arg"].asString();
        Json::Value r(Json::objectValue);
        r["success"] = true;
        r["cycled"] = 3;
        return r;
    });
    Json::Value args(Json::objectValue);
    args["action"] = "cycle";
    args["arg"] = "x";
    const Json::Value r = helper.Handle(args);
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(3, r["cycled"].asInt());
    EXPECT_EQ("cycle:x", seen);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, BuiltInActionsAreNotRoutedToTheHandler)
{
    bool called = false;
    helper.SetSystemActionHandler([&called](const std::string&, const Json::Value&) { called = true; return Json::Value(Json::objectValue); });
    Json::Value args(Json::objectValue);
    args["action"] = "list";
    EXPECT_TRUE(helper.Handle(args)["success"].asBool());
    EXPECT_FALSE(called);
}

TEST_F(ParamCommandHelperSaveDefaultsTest, EntryHelpAcceptsSystemActionsOnTheRealHost)
{
    helper.SetSystemActionHandler([](const std::string&, const Json::Value&) { Json::Value r(Json::objectValue); r["success"] = true; return r; });
    const SimCommandEntry entry = helper.BuildEntry();
    for (uint32_t i = 0; i < entry.help.paramCount; ++i)
    {
        if (std::string(entry.help.params[i].name) == "action") { EXPECT_STREQ("string", entry.help.params[i].type); }
    }
}

TEST_F(ParamCommandHelperTest, SetWithoutKeyOrValueIsAnError)
{
    Wire();
    EXPECT_FALSE(Call("set")["success"].asBool());
    EXPECT_FALSE(Call("set", "key", "zone")["success"].asBool());
    EXPECT_TRUE(queue.empty());
}

TEST_F(ParamCommandHelperTest, SetBadKeyNamesTheKeyAndQueuesNothing)
{
    Wire();
    const Json::Value r = Set("rain.nope", Json::Value(1.0));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("rain.nope"));
    EXPECT_TRUE(queue.empty());
}

TEST_F(ParamCommandHelperTest, SetOutOfRangeNamesTheKeyAndQueuesNothing)
{
    Wire();
    const Json::Value r = Set("rain.base", Json::Value(11.0));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("rain.base"));
    EXPECT_FALSE(Set("rain.base", Json::Value(-0.5))["success"].asBool());
    EXPECT_TRUE(queue.empty());
}

TEST_F(ParamCommandHelperTest, SetWrongKindNamesTheKeyAndQueuesNothing)
{
    Wire();
    const char* keys[] = { "rain.days", "wind.on", "wind.mode", "rain.base" };
    const Json::Value bad[] = { Json::Value(2.5), Json::Value("maybe"), Json::Value("Warp"), Json::Value("abc") };
    for (int i = 0; i < 4; ++i)
    {
        const Json::Value r = Set(keys[i], bad[i]);
        EXPECT_FALSE(r["success"].asBool()) << keys[i];
        EXPECT_NE(std::string::npos, r["error"].asString().find(keys[i])) << keys[i];
    }
    EXPECT_TRUE(queue.empty());
}

TEST_F(ParamCommandHelperTest, SetBreakingAConstraintNamesTheKeyAndQueuesNothing)
{
    // Constrained sidecar: lo < hi.
    Dia::Attribute::ParameterFile cfile;
    const std::string text = MakeSidecarText({ SidecarFloat("lo", 0, 10, 2), SidecarFloat("hi", 0, 10, 8) }, nullptr, "test.params");
    Json::Value root;
    ASSERT_TRUE(Json::Reader().parse(text, root));
    Json::Value c(Json::objectValue);
    c["a"] = "lo"; c["op"] = "<"; c["b"] = "hi";
    Json::Value arr(Json::arrayValue);
    arr.append(c);
    root["constraints"] = arr;
    Dia::Attribute::ParamError err;
    const std::string ctext = Json::FastWriter().write(root);
    if (!cfile.Load(ctext.c_str(), "c.params.json", nullptr, err)) { GTEST_SKIP() << "constraint syntax: " << err.reason.AsCStr(); }
    ASSERT_GT(cfile.Schema().GetConstraintCount(), 0u);
    ParamCommandHelper h(World::kWeather, cfile);
    std::vector<ParamMutation> q;
    h.SetMutationEnqueue([&q](const ParamMutation& m) { q.push_back(m); return true; });
    Json::Value args(Json::objectValue);
    args["action"] = "set"; args["key"] = "lo"; args["value"] = 9.0;
    const Json::Value r = h.Handle(args);
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("lo"));
    EXPECT_TRUE(q.empty());
    args["value"] = 5.0;
    EXPECT_TRUE(h.Handle(args)["success"].asBool());
}

TEST_F(ParamCommandHelperTest, GoodSetQueuesAndAppliesOnlyAtTheNextCapture)
{
    Wire();
    const Json::Value r = Set("rain.base", Json::Value(7.5));
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["queued"].asBool());
    EXPECT_EQ("rain.base", r["key"].asString());
    EXPECT_DOUBLE_EQ(7.5, r["value"].asDouble());
    EXPECT_EQ(41u, r["effectiveDay"].asUInt());
    EXPECT_FALSE(r.isMember("visibleDay"));
    EXPECT_EQ(1u, queue.size());
    EXPECT_DOUBLE_EQ(5.0, Base("rain.base"));
    EXPECT_EQ("default", Call("get", "key", "rain.base")["source"].asString());
    Capture();
    EXPECT_DOUBLE_EQ(7.5, Base("rain.base"));
    EXPECT_EQ("set", Call("get", "key", "rain.base")["source"].asString());
}

TEST_F(ParamCommandHelperTest, SetAcceptsStringFormsAndEachKind)
{
    Wire();
    EXPECT_TRUE(Set("rain.base", Json::Value("3.25"))["success"].asBool());
    EXPECT_TRUE(Set("rain.days", Json::Value(4))["success"].asBool());
    EXPECT_TRUE(Set("wind.on", Json::Value("true"))["success"].asBool());
    EXPECT_TRUE(Set("wind.mode", Json::Value("Turbo"))["success"].asBool());
    Capture();
    EXPECT_DOUBLE_EQ(3.25, Base("rain.base"));
    EXPECT_EQ(4, Call("get", "key", "rain.days")["value"].asInt());
    EXPECT_TRUE(Call("get", "key", "wind.on")["value"].asBool());
    EXPECT_EQ("Turbo", Call("get", "key", "wind.mode")["value"].asString());
}

TEST_F(ParamCommandHelperTest, DeferredStepReportsVisibleDay)
{
    Wire(8, 41, 3);
    const Json::Value r = Set("zone", Json::Value(0.75));
    ASSERT_TRUE(r["success"].asBool());
    EXPECT_EQ(41u, r["effectiveDay"].asUInt());
    EXPECT_EQ(44u, r["visibleDay"].asUInt());
}

TEST_F(ParamCommandHelperTest, QueueFullRefusesNamingTheKeyAndChangesNothing)
{
    Wire(1);
    ASSERT_TRUE(Set("zone", Json::Value(0.1))["success"].asBool());
    const Json::Value r = Set("rain.base", Json::Value(2.0));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("rain.base"));
    Capture();
    EXPECT_DOUBLE_EQ(5.0, Base("rain.base"));
}

TEST_F(ParamCommandHelperTest, SetWithoutAnEnqueueSeamIsAnError)
{
    EXPECT_FALSE(Set("zone", Json::Value(0.1))["success"].asBool());
    EXPECT_FALSE(Call("reload")["success"].asBool());
}

TEST_F(ParamCommandHelperTest, SetDrainsThroughAMutationQueueAtCapture)
{
    struct Entry { ParamMutation m; };
    CoW::WorldStep::MutationQueue<Entry, 4> q;
    helper.SetMutationEnqueue([&q](const ParamMutation& m) { Entry e; e.m = m; return q.Enqueue(e); });
    ASSERT_TRUE(Set("zone", Json::Value(0.9))["success"].asBool());
    EXPECT_DOUBLE_EQ(0.5, Base("zone"));
    Entry e;
    while (q.Dequeue(e)) { helper.ApplyMutation(e.m); }
    EXPECT_NEAR(0.9, Base("zone"), 1e-6);
}

TEST_F(ParamCommandHelperTest, ApplyMutationRefusesAStaleSetAndKeepsTheValue)
{
    ParamMutation m;
    m.kind = ParamMutation::Kind::kSet;
    snprintf(m.key, sizeof(m.key), "%s", "zone");
    m.value = Json::Value(5.0);   // out of range now
    EXPECT_FALSE(helper.ApplyMutation(m));
    EXPECT_DOUBLE_EQ(0.5, Base("zone"));
}

TEST_F(ParamCommandHelperTest, ReloadQueuesAndReportsPacksAndWarnings)
{
    Wire(8, 12, 2);
    int applies = 0;
    const Json::Value body = Pack("\"zone\":0.25");
    helper.SetReloadFn([&](bool apply, ParamReloadResult& res)
    {
        if (apply) { ++applies; }
        res.packIds.push_back("alpha");
        res.packBodies.push_back(body);
        res.warnings.push_back("basedOn missing");
        return true;
    });
    const Json::Value r = Call("reload");
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["queued"].asBool());
    ASSERT_EQ(1u, r["packs"].size());
    EXPECT_EQ("alpha", r["packs"][0u].asString());
    ASSERT_EQ(1u, r["warnings"].size());
    EXPECT_EQ(12u, r["effectiveDay"].asUInt());
    EXPECT_EQ(14u, r["visibleDay"].asUInt());
    EXPECT_EQ(0, applies);
    ASSERT_EQ(1u, queue.size());
    helper.NotePackApplied("old", Pack("\"rain.base\":6"));
    Capture();
    EXPECT_EQ(1, applies);
    EXPECT_EQ("pack:alpha", Call("get", "key", "zone")["source"].asString());
    EXPECT_EQ("default", Call("get", "key", "rain.base")["source"].asString());
}

TEST_F(ParamCommandHelperTest, ReloadFailureOrQueueFullQueuesNothing)
{
    Wire(1);
    helper.SetReloadFn([](bool, ParamReloadResult& res) { res.error = "bad pack"; return false; });
    const Json::Value r = Call("reload");
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(std::string::npos, r["error"].asString().find("bad pack"));
    EXPECT_TRUE(queue.empty());
    helper.SetReloadFn([](bool, ParamReloadResult&) { return true; });
    ASSERT_TRUE(Call("reload")["success"].asBool());
    EXPECT_FALSE(Call("reload")["success"].asBool());   // queue full
    EXPECT_EQ(1u, queue.size());
}

namespace {

// Wires a reload callback that feeds the layer's ordered packs to ParameterFile::Reload (what the owner does).
void WirePresetReload(ParamCommandHelper& helper, Dia::Attribute::ParameterFile& file, const PresetPackLayer& layer, const std::string& sidecar)
{
    helper.SetReloadFn([&file, &layer, sidecar](bool apply, ParamReloadResult& res)
    {
        layer.Compose(res);
        if (!apply) { return true; }
        std::vector<const Json::Value*> ptrs;
        for (const Json::Value& b : res.packBodies) { ptrs.push_back(&b); }
        Dia::Attribute::ParamWarnings warn;
        Dia::Attribute::ParamError err;
        return file.Reload(sidecar.c_str(), ptrs.data(), static_cast<uint32_t>(ptrs.size()), warn, err);
    });
}

}  // namespace

TEST_F(ParamCommandHelperTest, PresetLayerIsAppliedLastAndReplacedByTheNextPreset)
{
    Wire();
    const std::string sidecar = MakeSidecarText({
        SidecarFloat("rain.base", 0, 10, 5), SidecarInt("rain.days", 0, 10, 3), SidecarBool("wind.on", false),
        SidecarEnum("wind.mode", { "Slow", "Fast", "Turbo" }, "Slow"), SidecarFloat("zone", 0, 1, 0.5) }, nullptr, "test.params");
    PresetPackLayer layer(World::kWeather);
    ASSERT_TRUE(layer.AddUserPack("user", Pack("\"rain.base\":6,\"zone\":0.25")));
    WirePresetReload(helper, file, layer, sidecar);

    ASSERT_TRUE(layer.ApplyPreset("stormy", Pack("\"rain.base\":9,\"rain.days\":7")));
    ASSERT_TRUE(Call("reload")["success"].asBool());
    Capture();
    EXPECT_DOUBLE_EQ(9.0, Base("rain.base"));
    EXPECT_DOUBLE_EQ(7.0, Base("rain.days"));
    EXPECT_EQ("pack:stormy", Call("get", "key", "rain.base")["source"].asString());
    EXPECT_EQ("pack:stormy", Call("get", "key", "rain.days")["source"].asString());
    EXPECT_EQ("pack:user", Call("get", "key", "zone")["source"].asString());

    // B replaces A: A-only keys fall back (rain.days to default), A/user overlap falls back to the user value.
    ASSERT_TRUE(layer.ApplyPreset("calm", Pack("\"wind.on\":true")));
    ASSERT_TRUE(Call("reload")["success"].asBool());
    Capture();
    EXPECT_DOUBLE_EQ(3.0, Base("rain.days"));
    EXPECT_EQ("default", Call("get", "key", "rain.days")["source"].asString());
    EXPECT_DOUBLE_EQ(6.0, Base("rain.base"));
    EXPECT_EQ("pack:user", Call("get", "key", "rain.base")["source"].asString());
    EXPECT_EQ(1.0, Base("wind.on"));
    EXPECT_EQ("pack:calm", Call("get", "key", "wind.on")["source"].asString());
    EXPECT_DOUBLE_EQ(0.25, Base("zone"));
    EXPECT_EQ("pack:user", Call("get", "key", "zone")["source"].asString());

    // ClearPreset removes the slot entirely.
    layer.ClearPreset();
    ASSERT_TRUE(Call("reload")["success"].asBool());
    Capture();
    EXPECT_EQ(0.0, Base("wind.on"));
    EXPECT_EQ("default", Call("get", "key", "wind.on")["source"].asString());
}

TEST_F(ParamCommandHelperTest, PresetLayerComposeKeepsUserPacksOrderedBeforeThePreset)
{
    PresetPackLayer layer(World::kWeather);
    ASSERT_TRUE(layer.AddUserPack("u1", Pack("\"zone\":0.1")));
    ASSERT_TRUE(layer.ApplyPreset("a", Pack("\"zone\":0.2")));
    ASSERT_TRUE(layer.AddUserPack("u2", Pack("\"zone\":0.3")));   // added after the preset: still before it
    ASSERT_TRUE(layer.ApplyPreset("b", Pack("\"zone\":0.4")));
    EXPECT_TRUE(layer.HasPreset());
    EXPECT_STREQ("b", layer.PresetId());
    ParamReloadResult r;
    layer.Compose(r);
    ASSERT_EQ(3u, r.packIds.size());
    ASSERT_EQ(3u, r.packBodies.size());
    EXPECT_EQ("u1", r.packIds[0]);
    EXPECT_EQ("u2", r.packIds[1]);
    EXPECT_EQ("b", r.packIds[2]);
    layer.ClearPreset();
    EXPECT_FALSE(layer.HasPreset());
    ParamReloadResult r2;
    layer.Compose(r2);
    EXPECT_EQ(2u, r2.packIds.size());
}

TEST_F(ParamCommandHelperTest, PresetLayerRefusesBadIdsAndBodies)
{
    PresetPackLayer layer(World::kWeather);
    EXPECT_FALSE(layer.ApplyPreset("", Pack("\"zone\":0.2")));
    EXPECT_FALSE(layer.ApplyPreset(nullptr, Pack("\"zone\":0.2")));
    EXPECT_FALSE(layer.ApplyPreset("a", Json::Value(3)));
    EXPECT_FALSE(layer.HasPreset());
    PresetPackLayer unknown("nosuchsystem");
    EXPECT_FALSE(unknown.ApplyPreset("a", Pack("\"zone\":0.2")));
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

TEST_F(ParamCommandHelperSaveDefaultsTest, SetOnAMapKeyQueuesAppliesAndRangeChecksTheMapValue)
{
    std::vector<ParamMutation> q;
    helper.SetMutationEnqueue([&q](const ParamMutation& m) { q.push_back(m); return true; });
    helper.SetDayProvider([]() { ParamDayInfo d; d.effectiveDay = 7; d.deferLag = 0; return d; });
    auto set = [this](const char* key, const Json::Value& v)
    {
        Json::Value args(Json::objectValue);
        args["action"] = "set"; args["key"] = key; args["value"] = v;
        return helper.Handle(args);
    };
    const Json::Value ok = set("yield.Desert", Json::Value(0.75));
    ASSERT_TRUE(ok["success"].asBool()) << ok.toStyledString();
    EXPECT_EQ("yield.Desert", ok["key"].asString());
    ASSERT_EQ(1u, q.size());
    Json::Value get(Json::objectValue);
    get["action"] = "get"; get["key"] = "yield.Desert";
    EXPECT_DOUBLE_EQ(0.5, helper.Handle(get)["value"].asDouble()) << "not visible before Capture";
    helper.ApplyMutation(q[0]);
    EXPECT_DOUBLE_EQ(0.75, helper.Handle(get)["value"].asDouble());
    // Out of range and unknown map key: named, nothing queued.
    const Json::Value hi = set("yield.Desert", Json::Value(2.0));
    EXPECT_FALSE(hi["success"].asBool());
    EXPECT_NE(std::string::npos, hi["error"].asString().find("yield.Desert"));
    const Json::Value bad = set("yield.Swamp", Json::Value(0.5));
    EXPECT_FALSE(bad["success"].asBool());
    EXPECT_NE(std::string::npos, bad["error"].asString().find("yield.Swamp"));
    EXPECT_EQ(1u, q.size());
}

TEST_F(ParamCommandHelperTest, TwoStagesOnOneSimCommandSetRegisterTogetherAndRollBackTogether)
{
    Dia::Attribute::ParameterFile hydroFile;
    const std::string text = MakeSidecarText({ SidecarFloat("flow", 0, 1, 0.5) }, nullptr, "test.params");
    Dia::Attribute::ParamError err;
    ASSERT_TRUE(hydroFile.Load(text.c_str(), "hydro.params.json", nullptr, err));
    ParamCommandHelper hydro(World::kHydro, hydroFile);
    SimCommandEntry entries[2] = { helper.BuildEntry(), hydro.BuildEntry() };
    EXPECT_STRNE(entries[0].name, entries[1].name);

    FakeSimCommandHost host;
    SimCommandSet set;
    host.failOnNth = 2;
    EXPECT_FALSE(set.RegisterAll(host, entries, 2, "TwoStageTest"));
    EXPECT_EQ(1u, host.unregCount);
    EXPECT_EQ(Dia::Core::StringCRC("cow.weather.params"), host.unregNames[0]);
    EXPECT_EQ(0u, set.Count());

    host.failOnNth = 0;
    EXPECT_TRUE(set.RegisterAll(host, entries, 2, "TwoStageTest"));
    EXPECT_EQ(2u, set.Count());
    EXPECT_EQ(Dia::Core::StringCRC("cow.hydro.params"), host.regNames[host.regCount - 1]);
    // Each handler talks to its own file.
    Json::Value args(Json::objectValue);
    args["action"] = "get"; args["key"] = "flow";
    EXPECT_TRUE(entries[1].handler(args)["success"].asBool());
    EXPECT_FALSE(entries[0].handler(args)["success"].asBool());
}

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
