// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <algorithm>
#include <cstring>
#include <string>

using namespace CoW::WorldGeneration;

namespace
{

Json::Value Obj(const char* key, const Json::Value& v)
{
    Json::Value o(Json::objectValue);
    o[key] = v;
    return o;
}

bool Contains(const Json::Value& arr, const char* s)
{
    for (const Json::Value& v : arr)
    {
        if (v.isString() && v.asString() == s)
        {
            return true;
        }
    }
    return false;
}

bool SameCells(const WorldStructure& a, const WorldStructure& b)
{
    return a.CellCount() == b.CellCount() &&
           std::memcmp(a.Cells(), b.Cells(), static_cast<size_t>(a.CellCount()) * sizeof(Cell)) == 0;
}

} // namespace

TEST(WorldGenSession, InitialState)
{
    WorldGenSession s;
    EXPECT_EQ(s.State(), WorldGenState::Idle);
    EXPECT_EQ(s.Revision(), 0u);
    EXPECT_EQ(s.Seed(), 1u);
    EXPECT_TRUE(s.IsDirty());
    EXPECT_EQ(s.World().CellCount(), 0);
    EXPECT_TRUE(s.Shape() == WorldShapeParams{});
    EXPECT_EQ(s.Dimensions().width, 66);
    EXPECT_EQ(s.Dimensions().height, 42);
    EXPECT_EQ(s.Status()["state"].asString(), "Idle");
}

TEST(WorldGenSession, FirstTickGenerates)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 1u);
    EXPECT_EQ(s.State(), WorldGenState::Complete);
    EXPECT_FALSE(s.IsDirty());
    EXPECT_EQ(s.World().CellCount(), 66 * 42);
    EXPECT_EQ(s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
}

TEST(WorldGenSession, TickWhenCleanDoesNothing)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    EXPECT_FALSE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 1u);
}

TEST(WorldGenSession, SetAppliesKnownKeys)
{
    WorldGenSession s;
    const Json::Value r = s.Set(Obj("rivers", 0.9));
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(Contains(r["applied"], "rivers"));
    EXPECT_EQ(r["clamped"].size(), 0u);
    EXPECT_EQ(r["rejected"].size(), 0u);
    EXPECT_FLOAT_EQ(s.Shape().rivers, 0.9f);
}

TEST(WorldGenSession, SetClampsOutOfRange)
{
    WorldGenSession s;
    Json::Value a(Json::objectValue);
    a["rivers"] = 1.5;
    a["lakes"] = 99;
    a["continents"] = 0;
    const Json::Value r = s.Set(a);
    EXPECT_TRUE(Contains(r["applied"], "rivers"));
    EXPECT_TRUE(Contains(r["clamped"], "rivers"));
    EXPECT_TRUE(Contains(r["clamped"], "lakes"));
    EXPECT_TRUE(Contains(r["clamped"], "continents"));
    EXPECT_FLOAT_EQ(s.Shape().rivers, 1.0f);
    EXPECT_EQ(s.Shape().lakes, 12);
    EXPECT_EQ(s.Shape().continents, 1);
}

TEST(WorldGenSession, SetBoundsAreNotClampedButJustOutsideIs)
{
    WorldGenSession s;
    Json::Value a(Json::objectValue);
    a["landFraction"] = 0.20;      // exact min, not clamped in float terms
    a["barrier"] = 0.0;
    a["barrier"] = 1.0;
    a["lakes"] = 12;
    a["islands"] = 0;
    a["latitudeNorthDeg"] = -90.0;
    a["orientationDeg"] = 0.0;
    Json::Value r = s.Set(a);
    EXPECT_EQ(r["clamped"].size(), 0u) << r.toStyledString();

    Json::Value b(Json::objectValue);
    b["lakes"] = 13;
    b["islands"] = -1;
    b["barrier"] = 1.0001;
    b["latitudeNorthDeg"] = -90.5;
    b["landFraction"] = 0.19;
    r = s.Set(b);
    EXPECT_EQ(r["clamped"].size(), 5u) << r.toStyledString();
}

TEST(WorldGenSession, OrientationWrapCountsAsClampedOnlyOutsideRange)
{
    WorldGenSession s;
    Json::Value r = s.Set(Obj("orientationDeg", 359.5));
    EXPECT_EQ(r["clamped"].size(), 0u);
    r = s.Set(Obj("orientationDeg", 360.0));
    EXPECT_TRUE(Contains(r["clamped"], "orientationDeg"));
    EXPECT_FLOAT_EQ(s.Shape().orientationDeg, 0.0f);
    r = s.Set(Obj("orientationDeg", -10.0));
    EXPECT_TRUE(Contains(r["clamped"], "orientationDeg"));
    EXPECT_FLOAT_EQ(s.Shape().orientationDeg, 350.0f);
}

TEST(WorldGenSession, SetRejectsUnknownAndBadTypes)
{
    WorldGenSession s;
    Json::Value a(Json::objectValue);
    a["bogus"] = 1;
    a["rivers"] = "high";
    a["barrierForm"] = "Cube";
    a["seed"] = -5;
    const Json::Value r = s.Set(a);
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["applied"].size(), 0u);
    EXPECT_TRUE(Contains(r["rejected"], "bogus"));
    EXPECT_TRUE(Contains(r["rejected"], "rivers"));
    EXPECT_TRUE(Contains(r["rejected"], "barrierForm"));
    EXPECT_TRUE(Contains(r["rejected"], "seed"));
    EXPECT_TRUE(s.Shape() == WorldShapeParams{});
    EXPECT_EQ(s.Seed(), 1u);
}

TEST(WorldGenSession, SetNonObjectFails)
{
    WorldGenSession s;
    const Json::Value r = s.Set(Json::Value(5));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_TRUE(r.isMember("error"));
    EXPECT_FALSE(s.Set(Json::Value("x"))["success"].asBool());
}

TEST(WorldGenSession, SetBarrierFormAndUnlockAndSeed)
{
    WorldGenSession s;
    Json::Value a(Json::objectValue);
    a["barrierForm"] = "VolcanicChain";
    a["unlock"] = true;
    a["seed"] = 42;
    const Json::Value r = s.Set(a);
    EXPECT_TRUE(Contains(r["applied"], "barrierForm"));
    EXPECT_TRUE(Contains(r["applied"], "seed"));
    EXPECT_EQ(r["rejected"].size(), 0u);
    EXPECT_EQ(s.Shape().barrierForm, BarrierForm::VolcanicChain);
    EXPECT_EQ(s.Seed(), 42u);
}

TEST(WorldGenSession, IntegerParamsRound)
{
    WorldGenSession s;
    Json::Value r = s.Set(Obj("lakes", 5.6));
    EXPECT_EQ(s.Shape().lakes, 6);
    EXPECT_EQ(r["clamped"].size(), 0u);
    s.Set(Obj("lakes", 5.4));
    EXPECT_EQ(s.Shape().lakes, 5);
}

TEST(WorldGenSession, SetOfSameValueDoesNotDirty)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    s.Set(Obj("rivers", 0.5));
    s.Set(Obj("seed", 1));
    EXPECT_FALSE(s.IsDirty());
    EXPECT_FALSE(s.Tick(rng));
}

TEST(WorldGenSession, LastValueWinsAndOneGeneration)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    s.Set(Obj("rivers", 0.2));
    s.Set(Obj("rivers", 0.8));
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 2u);
    EXPECT_FLOAT_EQ(s.Shape().rivers, 0.8f);
    EXPECT_FALSE(s.Tick(rng));
}

TEST(WorldGenSession, LivePreviewRunsOnlyDependentStages)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    s.Set(Obj("rivers", 0.9));
    ASSERT_TRUE(s.Tick(rng));
    const Json::Value ran = s.Status()["stagesRan"];
    EXPECT_TRUE(Contains(ran, "Rivers"));
    EXPECT_FALSE(Contains(ran, "SeedFields"));
    EXPECT_FALSE(Contains(ran, "Landmass"));
    EXPECT_FALSE(Contains(ran, "Elevation"));
    EXPECT_LT(ran.size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
}

TEST(WorldGenSession, LiveResultEqualsFreshSession)
{
    WorldGenSession live;
    Dia::Maths::RandomStream rngA(1);
    live.Tick(rngA);
    Json::Value a(Json::objectValue);
    a["rivers"] = 0.9;
    a["mountainousness"] = 0.2;
    a["lakes"] = 8;
    live.Set(a);
    live.Tick(rngA);
    live.Set(Obj("passes", 4));
    live.Tick(rngA);

    WorldGenSession fresh;
    Json::Value b = a;
    b["passes"] = 4;
    fresh.Set(b);
    Dia::Maths::RandomStream rngB(1);
    fresh.Tick(rngB);

    EXPECT_TRUE(live.Shape() == fresh.Shape());
    EXPECT_TRUE(SameCells(live.World(), fresh.World()));
}

TEST(WorldGenSession, RegenerateRunsAllStagesEvenWhenNothingChanged)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    const WorldStructure before = s.World();

    const Json::Value r = s.Regenerate(Json::Value(Json::objectValue));
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_TRUE(s.IsDirty());
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 2u);
    EXPECT_EQ(s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
    EXPECT_TRUE(SameCells(before, s.World()));
}

TEST(WorldGenSession, RegenerateWithNewSeed)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    const WorldStructure before = s.World();

    EXPECT_TRUE(s.Regenerate(Obj("seed", 777))["success"].asBool());
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Seed(), 777u);
    EXPECT_EQ(s.Revision(), 2u);
    EXPECT_EQ(s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
    EXPECT_FALSE(SameCells(before, s.World()));
    EXPECT_FALSE(s.Regenerate(Obj("seed", -1))["success"].asBool());
}

TEST(WorldGenSession, SeedChangeRerunsEverything)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    s.Set(Obj("seed", 9));
    ASSERT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
}

TEST(WorldGenSession, SetSizeChangesDimensionsAndRegenerates)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);

    Json::Value r = s.SetSize(Obj("size", "Standard"));
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["dimensions"]["width"].asInt(), 80);
    EXPECT_EQ(r["dimensions"]["height"].asInt(), 52);
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.World().CellCount(), 80 * 52);
    EXPECT_EQ(s.Status()["size"].asString(), "Standard");

    s.SetSize(Obj("size", "Large"));
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.World().CellCount(), 104 * 64);
    EXPECT_EQ(s.Status()["dimensions"]["width"].asInt(), 104);
}

TEST(WorldGenSession, SetSizeInvalidFails)
{
    WorldGenSession s;
    EXPECT_FALSE(s.SetSize(Obj("size", "Huge"))["success"].asBool());
    EXPECT_FALSE(s.SetSize(Obj("size", "small"))["success"].asBool());
    EXPECT_FALSE(s.SetSize(Obj("size", 3))["success"].asBool());
    EXPECT_FALSE(s.SetSize(Json::Value(Json::objectValue))["success"].asBool());
    EXPECT_EQ(s.Dimensions().width, 66);
}

TEST(WorldGenSession, SetSizeSameIsNoOp)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    EXPECT_TRUE(s.SetSize(Obj("size", "Small"))["success"].asBool());
    EXPECT_FALSE(s.IsDirty());
    EXPECT_FALSE(s.Tick(rng));
}

TEST(WorldGenSession, ParamsShape)
{
    WorldGenSession s;
    const Json::Value p = s.Params();
    EXPECT_EQ(p["params"].size(), 20u);
    EXPECT_EQ(p["ranges"].size(), 20u);
    EXPECT_EQ(p["seed"].asUInt64(), 1u);
    EXPECT_EQ(p["preset"].asString(), "");
    EXPECT_FALSE(p["modified"].asBool());
    EXPECT_FALSE(p["locked"].asBool());
    EXPECT_EQ(p["params"]["barrierForm"].asString(), "Plateau");
    for (const std::string& key : p["ranges"].getMemberNames())
    {
        const Json::Value& r = p["ranges"][key];
        EXPECT_LE(r["min"].asDouble(), r["default"].asDouble()) << key;
        EXPECT_LE(r["default"].asDouble(), r["max"].asDouble()) << key;
        EXPECT_TRUE(r["firstStage"].isString()) << key;
        EXPECT_NE(r["firstStage"].asString(), "Unknown") << key;
    }
    EXPECT_EQ(p["ranges"]["rivers"]["firstStage"].asString(), "Rivers");
    EXPECT_EQ(p["ranges"]["landFraction"]["firstStage"].asString(), "Landmass");
    EXPECT_DOUBLE_EQ(p["ranges"]["lakes"]["max"].asDouble(), 12.0);
    EXPECT_DOUBLE_EQ(p["ranges"]["landFraction"]["default"].asDouble(), 0.42);
    EXPECT_EQ(p["effective"]["lakes"].asInt(), 4);
}

TEST(WorldGenSession, ParamsEffectiveAfterGeneration)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
    const Json::Value e = s.Params()["effective"];
    EXPECT_EQ(e["continents"].asInt(), s.Generator().GetEffective().continents);
    EXPECT_EQ(e["lakes"].asInt(), s.Generator().GetEffective().lakes);
    EXPECT_TRUE(e.isMember("islandShare"));
}

TEST(WorldGenSession, StatusFields)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    s.Set(Obj("seed", 5));
    s.Tick(rng);
    const Json::Value st = s.Status();
    EXPECT_EQ(st["state"].asString(), "Complete");
    EXPECT_EQ(st["revision"].asUInt(), 1u);
    EXPECT_EQ(st["seed"].asUInt64(), 5u);
    EXPECT_EQ(st["size"].asString(), "Small");
    EXPECT_EQ(st["dimensions"]["height"].asInt(), 42);
    EXPECT_FALSE(st["locked"].asBool());
    EXPECT_GE(st["lastMs"].asDouble(), 0.0);
    EXPECT_TRUE(st["stagesRan"].isArray());
    EXPECT_TRUE(Contains(st["stagesRan"], "SeedFields"));
}

TEST(WorldGenSession, ParamsRoundTripThroughSet)
{
    WorldGenSession a;
    Json::Value change(Json::objectValue);
    change["landFraction"] = 0.37;
    change["continents"] = 2;
    change["orientationDeg"] = 123.4;
    change["barrierForm"] = "VolcanicChain";
    change["temperatureOffset"] = -0.17;
    change["latitudeSouthDeg"] = -12.5;
    a.Set(change);

    WorldGenSession b;
    const Json::Value r = b.Set(a.Params()["params"]);
    EXPECT_EQ(r["rejected"].size(), 0u);
    EXPECT_EQ(r["clamped"].size(), 0u);
    EXPECT_TRUE(a.Shape() == b.Shape());
}
