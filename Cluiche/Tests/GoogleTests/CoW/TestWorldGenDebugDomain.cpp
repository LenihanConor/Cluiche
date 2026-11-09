// CoW::WorldGenDebugDomain: panel state contract + command routing through WorldGenSession.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (UI Layout, Goals 7-10)
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#ifdef DIA_DEBUG

#include <Modules/WorldGenDebugDomain.h>
#include <DiaCore/Json/external/json/json.h>

#include <climits>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
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
        // Always a full frame (static block included), the shape the contract documents for the first frame.
        Json::Value State() { domain.ForceFullFrame(); return Frame(); }
        // The next frame exactly as the Sim pack would receive it (lean unless a full frame is due).
        Json::Value Frame() { Json::Value s; domain.GetJSONState(s); return s; }
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

// ---- Gap audit (plan task 6) ----

namespace
{
    Json::Value Xy(const Json::Value& x, const Json::Value& y)
    {
        Json::Value a(Json::objectValue);
        a["x"] = x;
        a["y"] = y;
        return a;
    }

    std::string ControlKeys(const Json::Value& st)
    {
        std::string keys;
        for (const Json::Value& c : st["controls"]) keys += c["key"].asString() + ",";
        return keys;
    }
}

TEST(WorldGenDebugDomain, StateBeforeFirstWorldIsCompleteAndSafe)
{
    Fixture f;
    const Json::Value st = f.State();
    EXPECT_EQ(st["stats"]["status"].asString(), "Idle");
    EXPECT_EQ(st["stats"]["revision"].asUInt(), 0u);
    EXPECT_FALSE(st["controls"].empty());
    EXPECT_EQ(st["sizes"].size(), 3u);
    EXPECT_EQ(st["timings"].size(), static_cast<unsigned>(kImplementedStageCount));
    EXPECT_TRUE(st["cell"].isNull());
    EXPECT_FALSE(st.isMember("cellError"));
    EXPECT_TRUE(st["lastSave"].isNull());
}

TEST(WorldGenDebugDomain, SetSizeWithSelectionOutsideNewBoundsClearsToCellError)
{
    Fixture f;
    f.Cmd("setSize", Obj("size", "Large"));
    f.Gen();
    f.Cmd("selectCell", Xy(100, 60));
    EXPECT_TRUE(f.State()["cell"].isObject());

    f.Cmd("setSize", Obj("size", "Small"));
    // Pending size only: the generated grid is still Large, so the cell is still answerable.
    EXPECT_TRUE(f.State()["cell"].isObject());
    f.Gen();
    const Json::Value st = f.State();
    EXPECT_EQ(st["stats"]["width"].asInt(), 66);
    EXPECT_TRUE(st["cell"].isNull());
    EXPECT_TRUE(st["cellError"].isString());

    // Selecting inside the new bounds recovers.
    f.Cmd("selectCell", Xy(5, 5));
    const Json::Value ok = f.State();
    EXPECT_TRUE(ok["cell"].isObject());
    EXPECT_FALSE(ok.isMember("cellError"));
}

TEST(WorldGenDebugDomain, SelectCellAtGridEdgesAndOutOfRange)
{
    Fixture f;
    f.Gen();   // Small 66x42
    f.Cmd("selectCell", Xy(65, 41));
    Json::Value st = f.State();
    ASSERT_TRUE(st["cell"].isObject());
    EXPECT_EQ(st["cell"]["x"].asInt(), 65);
    EXPECT_EQ(st["cell"]["y"].asInt(), 41);
    EXPECT_EQ(st["cell"]["index"].asInt(), 41 * 66 + 65);

    f.Cmd("selectCell", Xy(0, 0));
    EXPECT_TRUE(f.State()["cell"].isObject());

    const Json::Value bad[] = { Xy(66, 0), Xy(0, 42), Xy(-1, 0), Xy(0, -1), Xy(INT_MAX, INT_MAX), Xy(INT_MIN, INT_MIN) };
    for (const Json::Value& b : bad)
    {
        f.Cmd("selectCell", b);
        st = f.State();
        EXPECT_TRUE(st["cell"].isNull()) << Compact(b);
        EXPECT_TRUE(st["cellError"].isString()) << Compact(b);
    }

    // A value that does not fit an int is rejected before the selection changes (the last bad selection stays).
    f.Cmd("selectCell", Xy(Json::Value(static_cast<Json::Int64>(1) << 40), 0));
    EXPECT_TRUE(f.State()["cell"].isNull());
    f.Cmd("selectCell", Xy(1.5, 2));   // non-integral: ignored
    EXPECT_TRUE(f.State()["cell"].isNull());
}

TEST(WorldGenDebugDomain, BurstOfScaleCommandsCoalescesToOneGenerationLastValueWins)
{
    Fixture f;
    f.Gen();
    const unsigned rev0 = f.State()["stats"]["revision"].asUInt();
    for (int i = 0; i < 100; ++i)
    {
        Json::Value a;
        a["key"] = "landFraction";
        a["value"] = 0.2 + 0.003 * i;
        f.Cmd("setScale", a);
    }
    EXPECT_TRUE(f.module.GetSession().IsDirty());
    f.Gen();
    f.Gen();   // second tick must be a no-op
    const Json::Value st = f.State();
    EXPECT_EQ(st["stats"]["revision"].asUInt(), rev0 + 1);
    for (const Json::Value& c : st["controls"])
    {
        if (c["key"].asString() == "landFraction") EXPECT_NEAR(c["value"].asDouble(), 0.2 + 0.003 * 99, 1e-4);
    }
}

TEST(WorldGenDebugDomain, SetScaleWithNaNInfAndStringsNeverCorruptsState)
{
    Fixture f;
    f.Gen();
    const double before = [&] {
        for (const Json::Value& c : f.State()["controls"]) if (c["key"].asString() == "landFraction") return c["value"].asDouble();
        return -1.0;
    }();
    const Json::Value bad[] = {
        Json::Value(std::numeric_limits<double>::quiet_NaN()),
        Json::Value(std::numeric_limits<double>::infinity()),
        Json::Value(-std::numeric_limits<double>::infinity()),
        Json::Value("0.5"),
        Json::Value("abc"),
        Json::Value(Json::nullValue),
        Json::Value(true),
        Json::Value(Json::objectValue),
    };
    for (const Json::Value& v : bad)
    {
        Json::Value a;
        a["key"] = "landFraction";
        a["value"] = v;
        f.Cmd("setScale", a);
        f.Gen();
        const Json::Value st = f.State();
        const std::string text = Compact(st);
        EXPECT_EQ(text.find("nan"), std::string::npos);
        EXPECT_EQ(text.find("inf"), std::string::npos);
        for (const Json::Value& c : st["controls"])
        {
            if (c["key"].asString() != "landFraction") continue;
            ASSERT_TRUE(c["value"].isNumeric()) << Compact(v);
            const double now = c["value"].asDouble();
            EXPECT_TRUE(std::isfinite(now)) << Compact(v);
            EXPECT_GE(now, c["min"].asDouble()) << Compact(v);
            EXPECT_LE(now, c["max"].asDouble()) << Compact(v);
        }
    }
    (void)before;
}

TEST(WorldGenDebugDomain, ControlListIsStableAcrossSizeChanges)
{
    Fixture f;
    f.Gen();
    const std::string keys = ControlKeys(f.State());
    for (const char* size : { "Large", "Standard", "Small", "Large" })
    {
        f.Cmd("setSize", Obj("size", size));
        f.Gen();
        const Json::Value st = f.State();
        EXPECT_EQ(ControlKeys(st), keys) << size;
        EXPECT_EQ(st["stats"]["size"].asString(), size);
    }
}

TEST(WorldGenDebugDomain, CompactStateAtLargeWithCellAndLastSaveStaysUnderSlot)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "WorldGenDebugDomainSizeTest";
    std::error_code ec;
    fs::remove_all(dir, ec);

    Fixture f;
    f.module.GetSession().SetPresetDirectories("", dir.string());
    f.Cmd("setSize", Obj("size", "Large"));
    f.Gen();
    f.Cmd("selectCell", Xy(52, 32));
    Json::Value save;
    save["id"] = "size_test";
    save["displayName"] = "Size Test";
    save["description"] = std::string(200, 'd');
    f.Cmd("presetSave", save);
    const Json::Value st = f.State();
    ASSERT_TRUE(st["cell"].isObject());
    ASSERT_TRUE(st["lastSave"].isObject());
    const size_t bytes = Compact(st).size();
    std::cout << "[ measured ] compact WorldGen state at Large with cell + lastSave: " << bytes << " bytes\n";
    EXPECT_LT(bytes, 12u * 1024u);   // contract; the snapshot slot is 32 KB
    fs::remove_all(dir, ec);
}

// ---- IWorldGenView forwarding (worldgen-view plan task 8) ----

namespace
{
    // Records every forwarded call; selection / mode / overlays live here like they do in the real viewer.
    class FakeView : public CoW::IWorldGenView
    {
    public:
        bool SelectCell(int x, int y) override { ++selectCalls; if (x < 0 || y < 0 || x >= 66 || y >= 42) return false; hasSel = true; selX = x; selY = y; return true; }
        void ClearSelection() override { ++clearCalls; hasSel = false; }
        bool HasSelection() override { return hasSel; }
        bool GetSelection(int& x, int& y) override { x = selX; y = selY; return hasSel; }
        bool SetColourMode(const char* id) override
        {
            ++modeCalls;
            lastMode = id;
            std::string s = id;
            if (s == "terrain") s = "relief";
            for (const char* m : { "relief", "elevation", "water" }) { if (s == m) { mode = s; return true; } }
            return false;
        }
        const char* GetColourMode() override { return mode.c_str(); }
        void ListColourModes(const std::function<void(const char*)>& fn) override { for (const char* m : { "relief", "elevation", "water" }) fn(m); }
        void ListColourGroups(const std::function<void(const char*, const char*)>& fn) override
        {
            fn("relief", "Terrain"); fn("elevation", "Terrain"); fn("water", "Water");
        }
        bool SetOverlayEnabled(const char* id, bool enabled) override
        {
            ++overlayCalls;
            lastOverlay = id;
            lastOverlayEnabled = enabled;
            if (std::string(id) == "rivers") { rivers = enabled; return true; }
            if (std::string(id) == "selection.neighbours") { neighbours = enabled; return true; }
            return false;
        }
        void ListOverlays(const std::function<void(const char*, bool, bool)>& fn) override
        {
            fn("rivers", rivers, true);
            fn("selection.neighbours", neighbours, hasSel);
        }
        void FillLegend(Json::Value& out) override { out = legend; }

        int selectCalls = 0, clearCalls = 0, modeCalls = 0, overlayCalls = 0;
        bool hasSel = false;
        int selX = 0, selY = 0;
        std::string mode = "relief", lastMode, lastOverlay;
        bool rivers = true, neighbours = true, lastOverlayEnabled = false;
        Json::Value legend{ Json::nullValue };
    };

    struct ViewFixture : Fixture
    {
        FakeView view;
        ViewFixture() { domain.SetView(&view); }
        ~ViewFixture() { domain.SetView(nullptr); }
    };
}

TEST(WorldGenDebugDomainView, CommandsForwardToTheView)
{
    ViewFixture f;
    f.Gen();
    f.Cmd("selectCell", Xy(3, 4));
    EXPECT_EQ(f.view.selectCalls, 1);
    EXPECT_TRUE(f.view.hasSel);
    EXPECT_EQ(f.view.selX, 3);
    EXPECT_EQ(f.view.selY, 4);

    f.Cmd("setColourMode", Obj("mode", "elevation"));
    EXPECT_EQ(f.view.modeCalls, 1);
    EXPECT_EQ(f.view.mode, "elevation");

    f.Cmd("toggle", Obj("drawer", "rivers"));
    EXPECT_EQ(f.view.lastOverlay, "rivers");
    EXPECT_FALSE(f.view.lastOverlayEnabled);   // was on, flipped off
    EXPECT_FALSE(f.view.rivers);
    f.Cmd("toggle", Obj("drawer", "rivers"));
    EXPECT_TRUE(f.view.rivers);

    f.Cmd("clearCell", Json::Value(Json::objectValue));
    EXPECT_EQ(f.view.clearCalls, 1);
    EXPECT_FALSE(f.view.hasSel);
}

TEST(WorldGenDebugDomainView, ColourModeReportsReliefAsTerrainAndAcceptsTerrain)
{
    ViewFixture f;
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "terrain");
    f.Cmd("setColourMode", Obj("mode", "elevation"));
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "elevation");
    f.Cmd("setColourMode", Obj("mode", "terrain"));
    EXPECT_EQ(f.view.lastMode, "terrain");
    EXPECT_EQ(f.view.mode, "relief");
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "terrain");
}

TEST(WorldGenDebugDomainView, UnknownModeIsIgnored)
{
    ViewFixture f;
    f.Cmd("setColourMode", Obj("mode", "notALayer"));
    EXPECT_EQ(f.view.mode, "relief");
    f.Cmd("setColourMode", Json::Value());
    f.Cmd("setColourMode", Obj("mode", 5));
    EXPECT_EQ(f.State()["view"]["colourMode"].asString(), "terrain");
}

TEST(WorldGenDebugDomainView, ViewModesDrawersAndLegendShape)
{
    ViewFixture f;
    Json::Value legend(Json::objectValue);
    legend["min"] = 0.0;
    legend["max"] = 1.0;
    f.view.legend = legend;
    f.Gen();
    const Json::Value st = f.State();

    ASSERT_EQ(st["view"]["modes"].size(), 3u);
    EXPECT_EQ(st["view"]["modes"][0u].asString(), "terrain");
    EXPECT_EQ(st["view"]["modes"][1u].asString(), "elevation");
    EXPECT_EQ(st["view"]["modes"][2u].asString(), "water");

    const Json::Value& d = st["view"]["drawers"];
    ASSERT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0u]["name"].asString(), "Tiles");
    EXPECT_TRUE(d[0u]["enabled"].asBool());
    EXPECT_TRUE(d[0u]["available"].asBool());
    EXPECT_EQ(d[1u]["name"].asString(), "rivers");
    EXPECT_TRUE(d[1u]["enabled"].asBool());
    EXPECT_EQ(d[2u]["name"].asString(), "selection.neighbours");
    EXPECT_FALSE(d[2u]["available"].asBool());   // nothing selected
    EXPECT_EQ(d[3u]["name"].asString(), "Selection");
    EXPECT_TRUE(d[3u]["available"].asBool());

    EXPECT_EQ(st["view"]["legend"], legend);
    f.view.legend = Json::Value(Json::nullValue);
    EXPECT_TRUE(f.Frame()["view"]["legend"].isNull());
}

TEST(WorldGenDebugDomainView, ViewerSideSelectionAppearsInCell)
{
    ViewFixture f;
    f.Gen();
    EXPECT_TRUE(f.State()["cell"].isNull());
    f.view.hasSel = true;   // as a click in the viewer would
    f.view.selX = 7;
    f.view.selY = 9;
    const Json::Value st = f.State();
    ASSERT_TRUE(st["cell"].isObject());
    EXPECT_EQ(st["cell"]["x"].asInt(), 7);
    EXPECT_EQ(st["cell"]["selected"]["x"].asInt(), 7);
    EXPECT_EQ(st["cell"]["selected"]["y"].asInt(), 9);
    EXPECT_TRUE(st["view"]["drawers"][2u]["available"].asBool());

    // selection outside the generated world: cellError behaviour unchanged
    f.view.selX = 500;
    const Json::Value bad = f.State();
    EXPECT_TRUE(bad["cell"].isNull());
    EXPECT_TRUE(bad["cellError"].isString());
}

TEST(WorldGenDebugDomainView, ViewerIsTheOnlyStateOwner)
{
    ViewFixture f;
    f.Gen();
    f.Cmd("selectCell", Xy(1, 1));
    f.domain.SetView(nullptr);   // local fallback state was never touched
    EXPECT_TRUE(f.State()["cell"].isNull());
    f.domain.SetView(&f.view);
    EXPECT_TRUE(f.State()["cell"].isObject());
}

TEST(WorldGenDebugDomainView, OutOfRangeSelectIsRejectedByTheView)
{
    ViewFixture f;
    f.Gen();
    f.Cmd("selectCell", Xy(9999, 1));
    EXPECT_EQ(f.view.selectCalls, 1);
    EXPECT_TRUE(f.State()["cell"].isNull());
}

TEST(WorldGenDebugDomainView, UnavailableOverlayToggleIsIgnored)
{
    ViewFixture f;
    f.Cmd("toggle", Obj("drawer", "selection.neighbours"));   // unavailable: nothing selected
    EXPECT_EQ(f.view.overlayCalls, 0);
    f.Cmd("toggle", Obj("drawer", "Tiles"));                  // fixed entry, not an overlay
    f.Cmd("toggle", Obj("drawer", "nope"));
    EXPECT_EQ(f.view.overlayCalls, 0);
}

TEST(WorldGenDebugDomainView, SetViewBumpsStaticVersionAndLeanFramesKeepLegend)
{
    Fixture f;
    f.Gen();
    const int v0 = f.State()["staticVersion"].asInt();
    FakeView view;
    f.domain.SetView(&view);
    const Json::Value first = f.Frame();
    EXPECT_TRUE(first["full"].asBool());
    EXPECT_GT(first["staticVersion"].asInt(), v0);
    const Json::Value lean = f.Frame();
    EXPECT_FALSE(lean["full"].asBool());
    EXPECT_FALSE(lean["view"].isMember("modes"));
    EXPECT_FALSE(lean["view"].isMember("layers"));
    EXPECT_TRUE(lean["view"].isMember("legend"));
    f.domain.SetView(nullptr);
}

TEST(WorldGenDebugDomainView, CompactStateAtLargeWithLegendStaysUnderSlot)
{
    ViewFixture f;
    Json::Value legend(Json::arrayValue);   // worst realistic legend: 13-entry category layer
    for (int i = 0; i < 13; ++i)
    {
        Json::Value e(Json::objectValue);
        e["label"] = "Category label " + std::to_string(i);
        e["colour"] = "#a1b2c3";
        legend.append(e);
    }
    f.view.legend = legend;
    f.Cmd("setSize", Obj("size", "Large"));
    f.Gen();
    f.Cmd("selectCell", Xy(52, 32));
    const size_t bytes = Compact(f.State()).size();
    std::cout << "[ measured ] compact WorldGen state at Large with view, legend (13 categories) + cell: " << bytes << " bytes\n";
    EXPECT_LT(bytes, 12u * 1024u);
}

#endif // DIA_DEBUG

// ---- Static / dynamic split (perf fix 2026-11-09): lean frames carry only what changes per frame ----

TEST(WorldGenDebugDomain, FirstFrameIsFullAndLaterFramesAreLean)
{
    Fixture f;
    f.Gen();
    const Json::Value first = f.Frame();
    EXPECT_TRUE(first["full"].asBool());
    EXPECT_TRUE(first.isMember("presets"));
    EXPECT_TRUE(first["view"].isMember("modes"));
    EXPECT_TRUE(first["controls"][0].isMember("min"));
    EXPECT_TRUE(first["timings"][0].isMember("name"));
    EXPECT_TRUE(first["sizes"][0].isMember("width"));

    const Json::Value lean = f.Frame();
    EXPECT_FALSE(lean["full"].asBool());
    EXPECT_FALSE(lean.isMember("presets"));
    EXPECT_FALSE(lean["view"].isMember("modes"));
    EXPECT_EQ(lean["staticVersion"], first["staticVersion"]);
    ASSERT_EQ(lean["controls"].size(), first["controls"].size());
    for (Json::ArrayIndex i = 0; i < lean["controls"].size(); ++i)
    {
        EXPECT_EQ(lean["controls"][i].getMemberNames().size(), 2u);   // key + value only
        EXPECT_EQ(lean["controls"][i]["key"], first["controls"][i]["key"]);
        EXPECT_EQ(lean["controls"][i]["value"], first["controls"][i]["value"]);
    }
    ASSERT_EQ(lean["sizes"].size(), 3u);
    EXPECT_FALSE(lean["sizes"][0].isMember("width"));
    EXPECT_EQ(lean["sizes"][0]["selected"], first["sizes"][0]["selected"]);
    ASSERT_EQ(lean["timings"].size(), first["timings"].size());
    EXPECT_FALSE(lean["timings"][0].isMember("name"));
    EXPECT_EQ(lean["timings"][0]["ms"], first["timings"][0]["ms"]);
    EXPECT_EQ(lean["stats"], first["stats"]);
}

TEST(WorldGenDebugDomain, LeanFrameIsMuchSmallerThanFull)
{
    Fixture f;
    f.Gen();
    const size_t full = Compact(f.Frame()).size();
    const size_t lean = Compact(f.Frame()).size();
    EXPECT_LT(lean * 2, full) << "lean " << lean << " full " << full;
}

TEST(WorldGenDebugDomain, LeanFramesFollowLiveValues)
{
    Fixture f;
    f.Gen();
    f.Frame();   // full
    f.Cmd("setScale", [] { Json::Value a(Json::objectValue); a["key"] = "landFraction"; a["value"] = 0.5; return a; }());
    const Json::Value lean = f.Frame();
    EXPECT_FALSE(lean["full"].asBool());
    bool found = false;
    for (const Json::Value& c : lean["controls"])
    {
        if (c["key"].asString() == "landFraction") { found = true; EXPECT_NEAR(c["value"].asDouble(), 0.5, 1e-6); }
    }
    EXPECT_TRUE(found);
}

TEST(WorldGenDebugDomain, FullFrameRepeatsEveryIntervalSoAReloadedPageRecovers)
{
    Fixture f;
    f.Gen();
    EXPECT_TRUE(f.Frame()["full"].asBool());
    int fulls = 0;
    for (int i = 0; i < static_cast<int>(WorldGenDebugDomain::kFullFrameInterval); ++i)
    {
        if (f.Frame()["full"].asBool()) ++fulls;
    }
    EXPECT_EQ(fulls, 1);
}

TEST(WorldGenDebugDomain, StaticVersionBumpsWhenPresetListChanges)
{
    Fixture f;
    f.Gen();
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "wgdd_static_version";
    std::filesystem::remove_all(dir);
    f.module.GetSession().SetPresetDirectories("", dir.string());
    const Json::Value before = f.State();
    const int v0 = before["staticVersion"].asInt();

    Json::Value args(Json::objectValue);
    args["id"] = "freshpreset";
    args["displayName"] = "Fresh";
    f.Cmd("presetSave", args);

    const Json::Value after = f.Frame();
    EXPECT_TRUE(after["full"].asBool()) << "a changed static block is sent immediately";
    EXPECT_GT(after["staticVersion"].asInt(), v0);
    bool listed = false;
    for (const Json::Value& p : after["presets"]) listed = listed || p["id"].asString() == "freshpreset";
    EXPECT_TRUE(listed);
    EXPECT_FALSE(f.Frame()["full"].asBool());
    std::filesystem::remove_all(dir);
}

TEST(WorldGenDebugDomain, StaticVersionIsStableWhenNothingStaticChanged)
{
    Fixture f;
    f.Gen();
    const int v0 = f.State()["staticVersion"].asInt();
    EXPECT_EQ(f.State()["staticVersion"].asInt(), v0);
}

TEST(WorldGenDebugDomainView, FullFrameCarriesGroupedLayers)
{
    ViewFixture f;
    const Json::Value st = f.State();
    const Json::Value& layers = st["view"]["layers"];
    ASSERT_EQ(layers.size(), 3u);
    EXPECT_EQ(layers[0u]["id"].asString(), "terrain");
    EXPECT_EQ(layers[0u]["label"].asString(), "Relief");
    EXPECT_EQ(layers[0u]["group"].asString(), "Terrain");
    EXPECT_EQ(layers[1u]["id"].asString(), "elevation");
    EXPECT_EQ(layers[1u]["label"].asString(), "Elevation");
    EXPECT_EQ(layers[2u]["id"].asString(), "water");
    EXPECT_EQ(layers[2u]["label"].asString(), "Water");
    EXPECT_EQ(layers[2u]["group"].asString(), "Water");
}

TEST(WorldGenDebugDomainView, ViewLayersAndModesAreStableAcrossSizeChanges)
{
    ViewFixture f;
    f.Gen();
    const Json::Value first = f.State();
    const std::string layers = first["view"]["layers"].toStyledString();
    const std::string modes = first["view"]["modes"].toStyledString();
    ASSERT_FALSE(first["view"]["layers"].empty());
    for (const char* size : { "Large", "Standard", "Small", "Large" })
    {
        f.Cmd("setSize", Obj("size", size));
        f.Gen();
        const Json::Value st = f.State();
        EXPECT_EQ(st["view"]["layers"].toStyledString(), layers) << size;
        EXPECT_EQ(st["view"]["modes"].toStyledString(), modes) << size;
    }
}

TEST(WorldGenDebugDomainView, NoViewLayersListTerrainLayers)
{
    Fixture f;
    const Json::Value st = f.State();
    const Json::Value& layers = st["view"]["layers"];
    ASSERT_EQ(layers.size(), st["view"]["modes"].size());
    EXPECT_EQ(layers[0u]["id"].asString(), "terrain");
    EXPECT_EQ(layers[0u]["label"].asString(), "Relief");
}
