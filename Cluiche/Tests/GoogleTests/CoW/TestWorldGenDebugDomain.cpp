// CoW::WorldGenDebugDomain: panel state contract + command routing through WorldGenSession.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (UI Layout, Goals 7-10)
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#ifdef DIA_DEBUG

#include <Modules/WorldGenDebugDomain.h>
#include <DiaCore/Json/external/json/json.h>

#include <filesystem>
#include <string>

using namespace CoW;
using namespace CoW::WorldGeneration;
using Dia::Core::StringCRC;

namespace
{
    Json::Value Obj(const char* key, const Json::Value& v)
    {
        Json::Value o(Json::objectValue);
        o[key] = v;
        return o;
    }

    std::string Compact(const Json::Value& v)
    {
        Json::StreamWriterBuilder b;
        b["indentation"] = "";
        return Json::writeString(b, v);
    }

    struct Fixture
    {
        WorldGenerationModule module{ StringCRC("WorldGenerationModule") };
        WorldGenDebugDomain   domain{ module };
        Dia::Maths::RandomStream rng{ 1 };

        void Gen() { module.GetSession().Tick(rng); }
        Json::Value State() { Json::Value s; domain.GetJSONState(s); return s; }
        void Cmd(const char* name, const Json::Value& args) { domain.OnCommand(StringCRC(name), args); }
    };

    // Compares everything the commands can change.
    void ExpectSameSession(const WorldGenSession& a, const WorldGenSession& b)
    {
        EXPECT_EQ(a.Params().toStyledString(), b.Params().toStyledString());
        EXPECT_EQ(a.Dimensions().width, b.Dimensions().width);
        EXPECT_EQ(a.Dimensions().height, b.Dimensions().height);
        EXPECT_EQ(a.IsDirty(), b.IsDirty());
        EXPECT_EQ(a.Seed(), b.Seed());
    }
}

TEST(WorldGenDebugDomain, Identity)
{
    Fixture f;
    EXPECT_EQ(f.domain.GetDomainId(), StringCRC("WorldGen"));
    EXPECT_STREQ(f.domain.GetDisplayName(), "World Generation");
    EXPECT_EQ(f.domain.GetGroup(), StringCRC("Systems"));
    EXPECT_FALSE(f.domain.HasWorldDrawers());
}

TEST(WorldGenDebugDomain, StatsMatchSession)
{
    Fixture f;
    f.Gen();
    const Json::Value st = f.State();
    const Json::Value status = f.module.GetSession().Status();
    const Json::Value params = f.module.GetSession().Params();
    EXPECT_TRUE(st["drawers"].isArray());
    EXPECT_EQ(st["stats"]["status"], status["state"]);
    EXPECT_EQ(st["stats"]["revision"], status["revision"]);
    EXPECT_EQ(st["stats"]["seed"], status["seed"]);
    EXPECT_EQ(st["stats"]["size"].asString(), "Small");
    EXPECT_EQ(st["stats"]["width"].asInt(), 66);
    EXPECT_EQ(st["stats"]["height"].asInt(), 42);
    EXPECT_FALSE(st["stats"]["locked"].asBool());
    EXPECT_EQ(st["stats"]["preset"].asString(), "");
    EXPECT_FALSE(st["stats"]["presetModified"].asBool());
    EXPECT_EQ(st["stats"]["stagesRan"], status["stagesRan"]);
    EXPECT_TRUE(st["stats"].isMember("lastMs"));
    EXPECT_EQ(st["stats"]["status"].asString(), "Complete");
    (void)params;
}

TEST(WorldGenDebugDomain, ControlsCoverEveryParamKey)
{
    Fixture f;
    const Json::Value st = f.State();
    const Json::Value params = f.module.GetSession().Params();
    const Json::Value& controls = st["controls"];
    ASSERT_TRUE(controls.isArray());
    EXPECT_EQ(controls.size(), params["params"].getMemberNames().size());
    int prevStage = -1;
    for (const std::string& key : params["params"].getMemberNames())
    {
        const Json::Value* found = nullptr;
        for (const Json::Value& c : controls)
        {
            if (c["key"].asString() == key) found = &c;
        }
        ASSERT_NE(found, nullptr) << key;
        EXPECT_EQ((*found)["value"], params["params"][key]) << key;
        EXPECT_EQ((*found)["stageName"], params["ranges"][key]["firstStage"]) << key;
        EXPECT_TRUE(found->isMember("label"));
        EXPECT_TRUE(found->isMember("stage"));
        EXPECT_TRUE(found->isMember("default"));
        if (key != "barrierForm")
        {
            EXPECT_EQ((*found)["type"].asString(), "slider") << key;
            EXPECT_EQ((*found)["min"], params["ranges"][key]["min"]) << key;
            EXPECT_EQ((*found)["max"], params["ranges"][key]["max"]) << key;
        }
    }
    for (const Json::Value& c : controls)
    {
        EXPECT_GE(c["stage"].asInt(), prevStage);   // grouped by first stage
        prevStage = c["stage"].asInt();
    }
    // Integer vs float steps
    for (const Json::Value& c : controls)
    {
        if (c["key"].asString() == "continents") EXPECT_EQ(c["step"].asDouble(), 1.0);
        if (c["key"].asString() == "landFraction") EXPECT_DOUBLE_EQ(c["step"].asDouble(), 0.01);
    }
}

TEST(WorldGenDebugDomain, SizesPresetsTimingsView)
{
    Fixture f;
    f.Gen();
    const Json::Value st = f.State();

    ASSERT_EQ(st["sizes"].size(), 3u);
    EXPECT_TRUE(st["sizes"][0u]["selected"].asBool());
    EXPECT_FALSE(st["sizes"][2u]["selected"].asBool());
    EXPECT_EQ(st["sizes"][2u]["name"].asString(), "Large");
    EXPECT_EQ(st["sizes"][2u]["width"].asInt(), 104);

    EXPECT_TRUE(st["presets"].isArray());

    ASSERT_EQ(st["timings"].size(), static_cast<unsigned>(kImplementedStageCount));
    EXPECT_EQ(st["timings"][0u]["name"].asString(), "SeedFields");
    EXPECT_TRUE(st["timings"][0u]["ran"].asBool());
    EXPECT_TRUE(st["timings"][0u].isMember("ms"));

    EXPECT_EQ(st["view"]["colourMode"].asString(), "terrain");
    EXPECT_GE(st["view"]["modes"].size(), 2u);
    EXPECT_EQ(st["view"]["modes"][0u].asString(), "terrain");
    ASSERT_EQ(st["view"]["drawers"].size(), 6u);
    EXPECT_EQ(st["view"]["drawers"][0u]["name"].asString(), "Tiles");
    EXPECT_TRUE(st["view"]["drawers"][0u]["available"].asBool());
    EXPECT_EQ(st["view"]["drawers"][3u]["name"].asString(), "Region edges");
    EXPECT_FALSE(st["view"]["drawers"][3u]["available"].asBool());
    EXPECT_TRUE(st["cell"].isNull());
    EXPECT_TRUE(st["lastSave"].isNull());
}

TEST(WorldGenDebugDomain, StateStaysSmallAtLarge)
{
    Fixture f;
    f.Cmd("setSize", Obj("size", "Large"));
    f.Gen();
    f.Cmd("selectCell", [] { Json::Value a; a["x"] = 10; a["y"] = 10; return a; }());
    const Json::Value st = f.State();
    EXPECT_EQ(st["stats"]["size"].asString(), "Large");
    EXPECT_TRUE(st["cell"].isObject());
    EXPECT_LT(Compact(st).size(), 12u * 1024u);
}

TEST(WorldGenDebugDomain, SetScaleMatchesDirectSet)
{
    Fixture f;
    WorldGenSession ref;
    Json::Value a; a["key"] = "landFraction"; a["value"] = 0.5;
    f.Cmd("setScale", a);
    ref.Set(Obj("landFraction", 0.5));
    ExpectSameSession(f.module.GetSession(), ref);

    Json::Value b; b["key"] = "seed"; b["value"] = 77;
    f.Cmd("setScale", b);
    ref.Set(Obj("seed", 77));
    ExpectSameSession(f.module.GetSession(), ref);
    EXPECT_EQ(f.module.GetSession().Seed(), 77u);
}

TEST(WorldGenDebugDomain, RegenerateSeedAndSizeMatchDirect)
{
    Fixture f;
    WorldGenSession ref;
    f.Gen();
    ref.Tick(f.rng);   // both clean now (rng state irrelevant for flags)
    EXPECT_FALSE(f.module.GetSession().IsDirty());

    f.Cmd("setSeed", Obj("seed", 5));
    ref.Set(Obj("seed", 5));
    ExpectSameSession(f.module.GetSession(), ref);

    f.Cmd("setSize", Obj("size", "Standard"));
    ref.SetSize(Obj("size", "Standard"));
    ExpectSameSession(f.module.GetSession(), ref);
    EXPECT_EQ(f.module.GetSession().Dimensions().width, 80);

    f.Cmd("regenerate", Obj("seed", 9));
    ref.Regenerate(Obj("seed", 9));
    ExpectSameSession(f.module.GetSession(), ref);
    EXPECT_EQ(f.module.GetSession().Seed(), 9u);
}

TEST(WorldGenDebugDomain, CommandsNotifyModule)
{
    Fixture f;
    // NotifyAction has no getter; behavioural check: command leaves session dirty/updated.
    f.Cmd("regenerate", Json::Value(Json::objectValue));
    EXPECT_TRUE(f.module.GetSession().IsDirty());
}

TEST(WorldGenDebugDomain, PresetLoadAndSaveRoundTrip)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "WorldGenDebugDomainTest";
    std::error_code ec;
    fs::remove_all(dir, ec);

    Fixture f;
    f.module.GetSession().SetPresetDirectories("", dir.string());
    f.Cmd("setScale", [] { Json::Value a; a["key"] = "landFraction"; a["value"] = 0.4; return a; }());

    Json::Value save;
    save["id"] = "dd_test";
    save["displayName"] = "DD Test";
    f.Cmd("presetSave", save);
    Json::Value st = f.State();
    ASSERT_TRUE(st["lastSave"].isObject());
    EXPECT_TRUE(st["lastSave"]["success"].asBool());
    EXPECT_TRUE(st["lastSave"].isMember("writtenTo"));
    EXPECT_FALSE(st["lastSave"]["isSource"].asBool());
    EXPECT_EQ(st["stats"]["preset"].asString(), "dd_test");
    ASSERT_EQ(st["presets"].size(), 1u);
    EXPECT_EQ(st["presets"][0u]["id"].asString(), "dd_test");

    // Saving again without overwrite surfaces the error.
    f.Cmd("presetSave", save);
    st = f.State();
    EXPECT_FALSE(st["lastSave"]["success"].asBool());
    EXPECT_TRUE(st["lastSave"].isMember("error"));

    // Change then load restores
    f.Cmd("setScale", [] { Json::Value a; a["key"] = "landFraction"; a["value"] = 0.6; return a; }());
    EXPECT_TRUE(f.State()["stats"]["presetModified"].asBool());
    WorldGenSession ref;
    ref.SetPresetDirectories("", dir.string());
    ref.Set(Obj("landFraction", 0.6));
    ref.PresetLoad(Obj("id", "dd_test"));
    f.Cmd("presetLoad", Obj("id", "dd_test"));
    ExpectSameSession(f.module.GetSession(), ref);
    EXPECT_EQ(f.State()["stats"]["preset"].asString(), "dd_test");

    fs::remove_all(dir, ec);
}

TEST(WorldGenDebugDomain, SelectionRoundTrip)
{
    Fixture f;
    f.Gen();
    Json::Value sel; sel["x"] = 3; sel["y"] = 4;
    f.Cmd("selectCell", sel);
    Json::Value st = f.State();
    ASSERT_TRUE(st["cell"].isObject());
    EXPECT_EQ(st["cell"]["x"].asInt(), 3);
    EXPECT_EQ(st["cell"]["y"].asInt(), 4);
    EXPECT_EQ(st["cell"]["selected"]["x"].asInt(), 3);
    EXPECT_TRUE(st["cell"].isMember("layers"));
    EXPECT_TRUE(st["cell"].isMember("neighbours"));
    EXPECT_TRUE(st["cell"].isMember("terrain"));

    f.Cmd("clearCell", Json::Value(Json::objectValue));
    EXPECT_TRUE(f.State()["cell"].isNull());
}

TEST(WorldGenDebugDomain, OutOfRangeSelectionIsNoCellWithError)
{
    Fixture f;
    f.Gen();
    Json::Value sel; sel["x"] = 9999; sel["y"] = 2;
    f.Cmd("selectCell", sel);
    const Json::Value st = f.State();
    EXPECT_TRUE(st["cell"].isNull());
    EXPECT_TRUE(st["cellError"].isString());

    // Before first world: also no cell, no crash.
    Fixture g;
    Json::Value ok; ok["x"] = 1; ok["y"] = 1;
    g.Cmd("selectCell", ok);
    const Json::Value st2 = g.State();
    EXPECT_TRUE(st2["cell"].isNull());
    EXPECT_TRUE(st2["cellError"].isString());
}

TEST(WorldGenDebugDomain, ColourModeAndToggle)
{
    Fixture f;
    f.Cmd("setColourMode", Obj("mode", "elevation"));
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "elevation");
    f.Cmd("setColourMode", Obj("mode", "notALayer"));
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "elevation");

    const bool before = f.State()["view"]["drawers"][1u]["enabled"].asBool();
    f.Cmd("toggle", Obj("drawer", "Grid lines"));
    EXPECT_EQ(f.State()["view"]["drawers"][1u]["enabled"].asBool(), !before);
    f.Cmd("toggle", Obj("drawer", "Sites"));   // unavailable
    EXPECT_FALSE(f.State()["view"]["drawers"][4u]["enabled"].asBool());
}

TEST(WorldGenDebugDomain, UnknownCommandAndBadArgsAreNoOps)
{
    Fixture f;
    f.Gen();
    WorldGenSession ref;
    ref.Tick(f.rng);
    const Json::Value before = f.State();

    f.Cmd("bogus", Json::Value(Json::objectValue));
    f.Cmd("setScale", Json::Value());
    f.Cmd("setScale", Json::Value(Json::objectValue));
    f.Cmd("setScale", Obj("key", "landFraction"));
    f.Cmd("setSeed", Json::Value(Json::objectValue));
    f.Cmd("setSeed", Obj("seed", "abc"));
    f.Cmd("setSize", Obj("size", "Huge"));
    f.Cmd("setSize", Json::Value(5));
    f.Cmd("presetLoad", Json::Value());
    f.Cmd("presetLoad", Obj("id", "../etc"));
    f.Cmd("selectCell", Obj("x", "a"));
    f.Cmd("setColourMode", Json::Value());
    f.Cmd("toggle", Json::Value("x"));

    const Json::Value after = f.State();
    EXPECT_EQ(Compact(before), Compact(after));
    EXPECT_FALSE(f.module.GetSession().IsDirty());
}

#endif // DIA_DEBUG
