// Exhaustive-test pass for the S-1 world generation module (Task 6): edge cases and hostile inputs for
// WorldGenSession (Set, SetSize, Regenerate, Presets, Layers, Cell) and a few module-level scenarios.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/RandomStreamModule.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;

namespace
{

Json::Value Obj(const char* key, const Json::Value& v)
{
    Json::Value o(Json::objectValue);
    o[key] = v;
    return o;
}

bool Ok(const Json::Value& r) { return r.isObject() && r.isMember("success") && r["success"].asBool(); }

bool Contains(const Json::Value& arr, const char* s)
{
    for (const Json::Value& v : arr)
        if (v.isString() && v.asString() == s) return true;
    return false;
}

void Generate(WorldGenSession& s)
{
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
}

bool SameCells(const WorldStructure& a, const WorldStructure& b)
{
    return a.CellCount() == b.CellCount() &&
           std::memcmp(a.Cells(), b.Cells(), static_cast<size_t>(a.CellCount()) * sizeof(CoW::WorldGeneration::Cell)) == 0;
}

// Session generated once and left clean, for tests that only inspect Set() replies.
struct Clean
{
    WorldGenSession s;
    Clean() { Generate(s); }
};

class TempDir
{
public:
    TempDir()
    {
        static int counter = 0;
        mPath = std::filesystem::temp_directory_path() / ("cow_wgedge_test_" + std::to_string(++counter) + "_" +
                std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
        std::filesystem::create_directories(mPath, ec);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
    }
    std::string Str() const { return mPath.string(); }
    std::filesystem::path Path() const { return mPath; }
private:
    std::filesystem::path mPath;
};

size_t CountEntries(const std::filesystem::path& root)
{
    size_t n = 0;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        ++n;
    return n;
}

uint64_t RefFnv(const void* data, size_t bytes)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < bytes; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

std::string HexOf(uint64_t h)
{
    char b[17];
    std::snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(h));
    return b;
}

Json::Value Page(const char* layer, const Json::Value& offset, const Json::Value& count)
{
    Json::Value o(Json::objectValue);
    o["layer"] = layer;
    if (!offset.isNull()) o["offset"] = offset;
    if (!count.isNull()) o["count"] = count;
    return o;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Set: hostile numeric / typed values
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeSet, NonFiniteNumbersAreRejectedAndStateUntouched)
{
    Clean c;
    const double bad[] = { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity() };
    for (double v : bad)
    {
        for (const char* key : { "rivers", "orientationDeg", "lakes", "latitudeNorthDeg" })
        {
            const Json::Value r = c.s.Set(Obj(key, v));
            EXPECT_TRUE(Contains(r["rejected"], key)) << key << " " << v;
            EXPECT_EQ(r["applied"].size(), 0u);
        }
    }
    EXPECT_TRUE(c.s.Shape() == WorldShapeParams{});
    EXPECT_FALSE(c.s.IsDirty());
}

TEST(WorldGenEdgeSet, HugeFiniteFloatsClampAndAreReported)
{
    Clean c;
    Json::Value a(Json::objectValue);
    a["rivers"] = 1e300;
    a["coastline"] = -1e300;
    a["temperatureOffset"] = 1e300;
    a["latitudeSouthDeg"] = -1e300;
    const Json::Value r = c.s.Set(a);
    EXPECT_EQ(r["rejected"].size(), 0u) << r.toStyledString();
    EXPECT_EQ(r["clamped"].size(), 4u) << r.toStyledString();
    EXPECT_FLOAT_EQ(c.s.Shape().rivers, 1.0f);
    EXPECT_FLOAT_EQ(c.s.Shape().coastline, 0.0f);
    EXPECT_FLOAT_EQ(c.s.Shape().temperatureOffset, 0.3f);
    EXPECT_FLOAT_EQ(c.s.Shape().latitudeSouthDeg, -90.0f);
}

TEST(WorldGenEdgeSet, HugeOrientationStaysInRange)
{
    Clean c;
    for (double v : { 1e300, -1e300, 1e30, -1e30, 3.4e38, 1e10 })
    {
        const Json::Value r = c.s.Set(Obj("orientationDeg", v));
        EXPECT_TRUE(Contains(r["applied"], "orientationDeg")) << v;
        const float o = c.s.Shape().orientationDeg;
        EXPECT_TRUE(std::isfinite(o)) << v;
        EXPECT_GE(o, 0.0f) << v;
        EXPECT_LT(o, 360.0f) << v;
    }
}

TEST(WorldGenEdgeSet, NonNumericTypesAreRejected)
{
    Clean c;
    Json::Value arr(Json::arrayValue);
    arr.append(1);
    Json::Value nested(Json::objectValue);
    nested["v"] = 1;
    const Json::Value badValues[] = { Json::Value("0.5"), Json::Value("1"), Json::Value(true), Json::Value(false),
                                      Json::Value(), arr, nested, Json::Value("") };
    for (const char* key : { "rivers", "lakes", "orientationDeg", "barrier", "passes" })
    {
        for (const Json::Value& v : badValues)
        {
            const Json::Value r = c.s.Set(Obj(key, v));
            EXPECT_TRUE(Contains(r["rejected"], key)) << key << " " << v.toStyledString();
            EXPECT_EQ(r["applied"].size(), 0u) << key;
        }
    }
    EXPECT_TRUE(c.s.Shape() == WorldShapeParams{});
    EXPECT_FALSE(c.s.IsDirty());
}

TEST(WorldGenEdgeSet, IntegerParamExtremes)
{
    Clean c;
    struct Case { double in; int expect; bool clamped; };
    const Case cases[] = {
        { -1, 0, true }, { -1e10, 0, true }, { 0, 0, false }, { 255, 12, true }, { 256, 12, true }, { 1e10, 12, true },
        { 1e300, 12, true }, { -1e300, 0, true }, { 12, 12, false }, { 12.4, 12, false }, { 12.5, 12, true },
        { 0.4, 0, false }, { -0.4, 0, false }, { -0.6, 0, true }, { 5.5, 6, false } };
    for (const Case& k : cases)
    {
        const Json::Value r = c.s.Set(Obj("lakes", k.in));
        EXPECT_EQ(c.s.Shape().lakes, k.expect) << k.in;
        EXPECT_EQ(Contains(r["clamped"], "lakes"), k.clamped) << k.in;
        EXPECT_TRUE(Contains(r["applied"], "lakes")) << k.in;
    }
    // Lower-bounded integer param: continents has min 1.
    EXPECT_TRUE(Contains(c.s.Set(Obj("continents", 0))["clamped"], "continents"));
    EXPECT_EQ(c.s.Shape().continents, 1);
    EXPECT_TRUE(Contains(c.s.Set(Obj("continents", 4))["clamped"], "continents"));
    EXPECT_EQ(c.s.Shape().continents, 3);
    EXPECT_EQ(c.s.Set(Obj("continents", 1))["clamped"].size(), 0u);
    EXPECT_EQ(c.s.Set(Obj("continents", 3))["clamped"].size(), 0u);
    // Unsigned JSON integers beyond int64 still clamp.
    EXPECT_TRUE(Contains(c.s.Set(Obj("islands", Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))["clamped"], "islands"));
    EXPECT_EQ(c.s.Shape().islands, 6);
}

TEST(WorldGenEdgeSet, FloatBoundsAreNotClampedPerParam)
{
    // Every float row: exact min and exact max are not "clamped"; just outside is.
    struct Row { const char* key; double lo; double hi; };
    const Row rows[] = {
        { "landFraction", 0.20, 0.65 }, { "continentIslands", 0, 1 }, { "coastline", 0, 1 }, { "mountainousness", 0, 1 },
        { "spineToRing", 0, 1 }, { "barrier", 0, 1 }, { "ruggedness", 0, 1 }, { "rivers", 0, 1 }, { "valleyDepth", 0, 1 },
        { "latitudeNorthDeg", -90, 90 }, { "latitudeSouthDeg", -90, 90 }, { "temperatureOffset", -0.3, 0.3 },
        { "moistureOffset", -0.3, 0.3 } };
    for (const Row& r : rows)
    {
        Clean c;
        EXPECT_EQ(c.s.Set(Obj(r.key, r.lo))["clamped"].size(), 0u) << r.key << " lo";
        EXPECT_EQ(c.s.Set(Obj(r.key, r.hi))["clamped"].size(), 0u) << r.key << " hi";
        EXPECT_TRUE(Contains(c.s.Set(Obj(r.key, r.lo - 0.01))["clamped"], r.key)) << r.key << " below";
        EXPECT_TRUE(Contains(c.s.Set(Obj(r.key, r.hi + 0.01))["clamped"], r.key)) << r.key << " above";
    }
    const Row irows[] = { { "continents", 1, 3 }, { "islands", 0, 6 }, { "peninsulas", 0, 3 }, { "passes", 0, 4 }, { "lakes", 0, 12 } };
    for (const Row& r : irows)
    {
        Clean c;
        EXPECT_EQ(c.s.Set(Obj(r.key, r.lo))["clamped"].size(), 0u) << r.key << " lo";
        EXPECT_EQ(c.s.Set(Obj(r.key, r.hi))["clamped"].size(), 0u) << r.key << " hi";
        EXPECT_TRUE(Contains(c.s.Set(Obj(r.key, r.lo - 1))["clamped"], r.key)) << r.key << " below";
        EXPECT_TRUE(Contains(c.s.Set(Obj(r.key, r.hi + 1))["clamped"], r.key)) << r.key << " above";
    }
}

TEST(WorldGenEdgeSet, OrientationWrapTable)
{
    struct Case { double in; float expect; bool clamped; };
    const Case cases[] = {
        { 0.0, 0.0f, false }, { 359.0, 359.0f, false }, { 360.0, 0.0f, true }, { 720.0, 0.0f, true }, { 725.0, 5.0f, true },
        { -30.0, 330.0f, true }, { -360.0, 0.0f, true }, { -725.0, 355.0f, true }, { 180.0, 180.0f, false } };
    for (const Case& k : cases)
    {
        Clean c;
        const Json::Value r = c.s.Set(Obj("orientationDeg", k.in));
        EXPECT_NEAR(c.s.Shape().orientationDeg, k.expect, 1e-3f) << k.in;
        EXPECT_EQ(Contains(r["clamped"], "orientationDeg"), k.clamped) << k.in;
    }
    {
        Clean c;
        const Json::Value r = c.s.Set(Obj("orientationDeg", 359.999));
        EXPECT_EQ(r["clamped"].size(), 0u);
        EXPECT_NEAR(c.s.Shape().orientationDeg, 359.999f, 1e-3f);
        EXPECT_LT(c.s.Shape().orientationDeg, 360.0f);
    }
    {
        // A tiny negative input must not round up to exactly 360.
        Clean c;
        c.s.Set(Obj("orientationDeg", -1e-9));
        EXPECT_GE(c.s.Shape().orientationDeg, 0.0f);
        EXPECT_LT(c.s.Shape().orientationDeg, 360.0f);
    }
}

TEST(WorldGenEdgeSet, BarrierFormIsCaseSensitiveAndStrict)
{
    Clean c;
    for (const char* bad : { "plateau", "PLATEAU", "volcanicchain", "VolcanicChain ", " Plateau", "", "Volcanic" })
    {
        const Json::Value r = c.s.Set(Obj("barrierForm", bad));
        EXPECT_TRUE(Contains(r["rejected"], "barrierForm")) << "'" << bad << "'";
    }
    EXPECT_TRUE(Contains(c.s.Set(Obj("barrierForm", 0))["rejected"], "barrierForm"));
    EXPECT_TRUE(Contains(c.s.Set(Obj("barrierForm", Json::Value()))["rejected"], "barrierForm"));
    EXPECT_TRUE(Contains(c.s.Set(Obj("barrierForm", true))["rejected"], "barrierForm"));
    EXPECT_EQ(c.s.Shape().barrierForm, BarrierForm::Plateau);
    EXPECT_FALSE(c.s.IsDirty());
    EXPECT_TRUE(Contains(c.s.Set(Obj("barrierForm", "VolcanicChain"))["applied"], "barrierForm"));
    EXPECT_TRUE(c.s.IsDirty());
    EXPECT_TRUE(Contains(c.s.Set(Obj("barrierForm", "Plateau"))["applied"], "barrierForm"));
    EXPECT_EQ(c.s.Shape().barrierForm, BarrierForm::Plateau);
}

TEST(WorldGenEdgeSet, KeyNamesAreCaseSensitive)
{
    Clean c;
    Json::Value a(Json::objectValue);
    a["Rivers"] = 0.9;
    a["RIVERS"] = 0.9;
    a["rivers "] = 0.9;
    a[""] = 0.9;
    a["Seed"] = 5;
    a["Unlock"] = true;
    const Json::Value r = c.s.Set(a);
    EXPECT_EQ(r["applied"].size(), 0u);
    EXPECT_EQ(r["rejected"].size(), 6u);
    EXPECT_FALSE(c.s.IsDirty());
}

TEST(WorldGenEdgeSet, MixedValidInvalidUnknownAndUnlockInOneCall)
{
    Clean c;
    Json::Value unlockArr(Json::arrayValue);
    unlockArr.append("x");
    Json::Value a(Json::objectValue);
    a["rivers"] = 0.5;       // valid
    a["lakes"] = 99;         // valid but clamped
    a["bogus"] = 1;          // unknown
    a["seed"] = "x";         // invalid seed
    a["barrier"] = "hi";     // invalid type
    a["unlock"] = unlockArr; // ignored whatever the shape
    const Json::Value r = c.s.Set(a);
    ASSERT_TRUE(Ok(r));
    EXPECT_EQ(r["applied"].size(), 2u);
    EXPECT_TRUE(Contains(r["applied"], "rivers"));
    EXPECT_TRUE(Contains(r["applied"], "lakes"));
    EXPECT_EQ(r["clamped"].size(), 1u);
    EXPECT_TRUE(Contains(r["clamped"], "lakes"));
    EXPECT_EQ(r["rejected"].size(), 3u);
    EXPECT_TRUE(Contains(r["rejected"], "bogus"));
    EXPECT_TRUE(Contains(r["rejected"], "seed"));
    EXPECT_TRUE(Contains(r["rejected"], "barrier"));
    EXPECT_FALSE(Contains(r["rejected"], "unlock"));
    EXPECT_FLOAT_EQ(c.s.Shape().rivers, 0.5f);
    EXPECT_EQ(c.s.Shape().lakes, 12);
    EXPECT_EQ(c.s.Seed(), 1u);
}

TEST(WorldGenEdgeSet, UnlockShapesAreAcceptedAndIgnored)
{
    Clean c;
    Json::Value arr(Json::arrayValue);
    arr.append(1);
    for (const Json::Value& v : { Json::Value(true), Json::Value(false), Json::Value(), Json::Value("yes"), Json::Value(5), arr,
                                   Json::Value(Json::objectValue) })
    {
        const Json::Value r = c.s.Set(Obj("unlock", v));
        EXPECT_TRUE(Ok(r));
        EXPECT_EQ(r["applied"].size(), 0u);
        EXPECT_EQ(r["rejected"].size(), 0u);
    }
    EXPECT_FALSE(c.s.IsDirty());
    EXPECT_FALSE(c.s.Params()["locked"].asBool());
}

TEST(WorldGenEdgeSet, EmptyObjectIsASuccessfulNoOp)
{
    Clean c;
    const Json::Value r = c.s.Set(Json::Value(Json::objectValue));
    EXPECT_TRUE(Ok(r));
    EXPECT_EQ(r["applied"].size() + r["clamped"].size() + r["rejected"].size(), 0u);
    EXPECT_FALSE(c.s.IsDirty());
}

TEST(WorldGenEdgeSet, NullAndArrayArgsAreErrors)
{
    Clean c;
    EXPECT_FALSE(Ok(c.s.Set(Json::Value())));
    EXPECT_FALSE(Ok(c.s.Set(Json::Value(Json::arrayValue))));
    EXPECT_FALSE(Ok(c.s.Set(Json::Value(true))));
    EXPECT_FALSE(c.s.IsDirty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Set: seed
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeSeed, ExtremesAndInvalidShapes)
{
    Clean c;
    EXPECT_TRUE(Contains(c.s.Set(Obj("seed", 0))["applied"], "seed"));
    EXPECT_EQ(c.s.Seed(), 0u);
    EXPECT_TRUE(c.s.IsDirty());
    Generate(c.s);   // seed 0 must generate fine
    EXPECT_EQ(c.s.State(), WorldGenState::Complete);

    EXPECT_TRUE(Contains(c.s.Set(Obj("seed", Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))["applied"], "seed"));
    EXPECT_EQ(c.s.Seed(), UINT64_MAX);
    EXPECT_EQ(c.s.Params()["seed"].asUInt64(), UINT64_MAX);
    EXPECT_EQ(c.s.Status()["seed"].asUInt64(), UINT64_MAX);
    Generate(c.s);
    EXPECT_EQ(c.s.State(), WorldGenState::Complete);

    const Json::Value badSeeds[] = { Json::Value(-1), Json::Value(static_cast<Json::Int64>(INT64_MIN)), Json::Value(1.5),
                                     Json::Value(-0.5), Json::Value("5"), Json::Value(true), Json::Value(),
                                     Json::Value(Json::arrayValue), Json::Value(Json::objectValue),
                                     Json::Value(std::numeric_limits<double>::quiet_NaN()),
                                     Json::Value(std::numeric_limits<double>::infinity()),
                                     Json::Value(1e30) };
    for (const Json::Value& v : badSeeds)
    {
        const Json::Value r = c.s.Set(Obj("seed", v));
        EXPECT_TRUE(Contains(r["rejected"], "seed")) << v.toStyledString();
        EXPECT_EQ(c.s.Seed(), UINT64_MAX);
    }
    EXPECT_FALSE(c.s.IsDirty());
}

TEST(WorldGenEdgeSeed, Int64MaxIsAccepted)
{
    Clean c;
    c.s.Set(Obj("seed", Json::Value(static_cast<Json::Int64>(INT64_MAX))));
    EXPECT_EQ(c.s.Seed(), static_cast<uint64_t>(INT64_MAX));
}

TEST(WorldGenEdgeSeed, RegenerateSeedExtremesAndOddArgs)
{
    Clean c;
    EXPECT_TRUE(Ok(c.s.Regenerate(Obj("seed", 0))));
    EXPECT_EQ(c.s.Seed(), 0u);
    EXPECT_TRUE(Ok(c.s.Regenerate(Obj("seed", Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))));
    EXPECT_EQ(c.s.Seed(), UINT64_MAX);
    Generate(c.s);
    EXPECT_FALSE(c.s.IsDirty());

    // Invalid seeds fail and leave seed and dirtiness unchanged.
    for (const Json::Value& v : { Json::Value(-1), Json::Value(1.5), Json::Value("7"), Json::Value(true), Json::Value(),
                                   Json::Value(Json::arrayValue), Json::Value(std::numeric_limits<double>::quiet_NaN()) })
    {
        const Json::Value r = c.s.Regenerate(Obj("seed", v));
        EXPECT_FALSE(Ok(r)) << v.toStyledString();
        EXPECT_TRUE(r["error"].isString());
        EXPECT_EQ(c.s.Seed(), UINT64_MAX);
        EXPECT_FALSE(c.s.IsDirty());
    }

    // Non-object args fail; null and unknown keys are fine.
    EXPECT_FALSE(Ok(c.s.Regenerate(Json::Value(5))));
    EXPECT_FALSE(Ok(c.s.Regenerate(Json::Value("x"))));
    EXPECT_FALSE(Ok(c.s.Regenerate(Json::Value(Json::arrayValue))));
    EXPECT_FALSE(c.s.IsDirty());
    EXPECT_TRUE(Ok(c.s.Regenerate(Json::Value())));
    EXPECT_TRUE(c.s.IsDirty());
    Generate(c.s);
    EXPECT_TRUE(Ok(c.s.Regenerate(Obj("whatever", 1))));
    EXPECT_TRUE(c.s.IsDirty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Dirty / revision / tick semantics
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeTick, SetToIdenticalValuesIsNoOpForEveryParam)
{
    Clean c;
    const Json::Value current = c.s.Params()["params"];
    const uint32_t rev = c.s.Revision();
    const Json::Value r = c.s.Set(current);
    EXPECT_EQ(r["applied"].size(), 20u);
    EXPECT_EQ(r["clamped"].size(), 0u);
    EXPECT_FALSE(c.s.IsDirty());
    Dia::Maths::RandomStream rng(1);
    EXPECT_FALSE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), rev);
}

TEST(WorldGenEdgeSet, ClampToCurrentValueIsReportedButDoesNotDirty)
{
    Clean c;
    c.s.Set(Obj("rivers", 1.0));
    c.s.Set(Obj("lakes", 12));
    Generate(c.s);
    const uint32_t rev = c.s.Revision();

    Json::Value a(Json::objectValue);
    a["rivers"] = 5.0;
    a["lakes"] = 99;
    const Json::Value r = c.s.Set(a);
    EXPECT_TRUE(Contains(r["clamped"], "rivers"));
    EXPECT_TRUE(Contains(r["clamped"], "lakes"));
    EXPECT_TRUE(Contains(r["applied"], "rivers"));
    EXPECT_FALSE(c.s.IsDirty());
    Dia::Maths::RandomStream rng(1);
    EXPECT_FALSE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), rev);
}

TEST(WorldGenEdgeTick, RegenerateThenSetInSameFrameIsOneFullGeneration)
{
    Clean c;
    const uint32_t rev = c.s.Revision();
    c.s.Regenerate(Json::Value(Json::objectValue));
    c.s.Set(Obj("rivers", 0.9));   // would normally be a partial run
    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), rev + 1);
    EXPECT_EQ(c.s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
    EXPECT_FALSE(c.s.Tick(rng));
}

TEST(WorldGenEdgeTick, SetThenRegenerateInSameFrameIsOneFullGeneration)
{
    Clean c;
    const uint32_t rev = c.s.Revision();
    c.s.Set(Obj("rivers", 0.9));
    c.s.Regenerate(Json::Value());
    c.s.Set(Obj("mountainousness", 0.1));
    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), rev + 1);
    EXPECT_EQ(c.s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
    // The force-full flag is consumed: the next partial change is a partial run again.
    c.s.Set(Obj("rivers", 0.2));
    ASSERT_TRUE(c.s.Tick(rng));
    EXPECT_LT(c.s.Status()["stagesRan"].size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));
}

TEST(WorldGenEdgeTick, RepeatedRegenerateIsDeterministicAndBumpsRevisionEachTime)
{
    Clean c;
    const WorldStructure first = c.s.World();
    Dia::Maths::RandomStream rng(1);
    for (uint32_t i = 0; i < 5; ++i)
    {
        const uint32_t before = c.s.Revision();
        ASSERT_TRUE(Ok(c.s.Regenerate(Json::Value(Json::objectValue))));
        ASSERT_TRUE(c.s.Tick(rng));
        EXPECT_EQ(c.s.Revision(), before + 1);
        EXPECT_TRUE(SameCells(first, c.s.World())) << "iteration " << i;
    }
    // Same seed passed explicitly still re-runs and still produces identical bytes.
    const uint32_t before = c.s.Revision();
    c.s.Regenerate(Obj("seed", 1));
    ASSERT_TRUE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), before + 1);
    EXPECT_TRUE(SameCells(first, c.s.World()));
}

TEST(WorldGenEdgeTick, TwoRegeneratesBeforeTickAreOneGeneration)
{
    Clean c;
    const uint32_t rev = c.s.Revision();
    c.s.Regenerate(Obj("seed", 5));
    c.s.Regenerate(Obj("seed", 6));
    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(c.s.Tick(rng));
    EXPECT_EQ(c.s.Revision(), rev + 1);
    EXPECT_EQ(c.s.Seed(), 6u);
    EXPECT_FALSE(c.s.Tick(rng));
}

TEST(WorldGenEdgeTick, SeedRoundTripRestoresBytes)
{
    Clean c;
    const WorldStructure base = c.s.World();
    c.s.Set(Obj("seed", 12345));
    Generate(c.s);
    EXPECT_FALSE(SameCells(base, c.s.World()));
    c.s.Set(Obj("seed", 1));
    Generate(c.s);
    EXPECT_TRUE(SameCells(base, c.s.World()));
}

// ---------------------------------------------------------------------------------------------------------------------
// SetSize
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeSize, StrictNamesAndShapes)
{
    Clean c;
    for (const char* bad : { "small", "SMALL", " Small", "Small ", "Small\n", "", "Medium", "Standard1", "Lar ge" })
    {
        EXPECT_FALSE(Ok(c.s.SetSize(Obj("size", bad)))) << "'" << bad << "'";
    }
    EXPECT_FALSE(Ok(c.s.SetSize(Obj("size", Json::Value()))));
    EXPECT_FALSE(Ok(c.s.SetSize(Obj("size", true))));
    EXPECT_FALSE(Ok(c.s.SetSize(Obj("size", Json::Value(Json::arrayValue)))));
    EXPECT_FALSE(Ok(c.s.SetSize(Json::Value())));
    EXPECT_FALSE(Ok(c.s.SetSize(Json::Value("Small"))));
    EXPECT_FALSE(Ok(c.s.SetSize(Json::Value(Json::arrayValue))));
    EXPECT_FALSE(Ok(c.s.SetSize(Obj("Size", "Small"))));
    EXPECT_EQ(c.s.Dimensions().width, 66);
    EXPECT_FALSE(c.s.IsDirty());
    const Json::Value e = c.s.SetSize(Obj("size", "Huge"));
    EXPECT_TRUE(e["error"].isString());
}

TEST(WorldGenEdgeSize, ExtraKeysAreIgnoredAndResultCarriesDimensions)
{
    Clean c;
    Json::Value a(Json::objectValue);
    a["size"] = "Large";
    a["junk"] = 5;
    const Json::Value r = c.s.SetSize(a);
    ASSERT_TRUE(Ok(r));
    EXPECT_EQ(r["dimensions"]["width"].asInt(), 104);
    EXPECT_EQ(r["dimensions"]["height"].asInt(), 64);
    EXPECT_TRUE(c.s.IsDirty());
}

TEST(WorldGenEdgeSize, SizeRoundTripRestoresSmallBytes)
{
    Clean c;
    const WorldStructure smallWorld = c.s.World();
    c.s.SetSize(Obj("size", "Standard"));
    Generate(c.s);
    EXPECT_EQ(c.s.World().CellCount(), 80 * 52);
    c.s.SetSize(Obj("size", "Small"));
    Generate(c.s);
    EXPECT_TRUE(SameCells(smallWorld, c.s.World()));
}

TEST(WorldGenEdgeSize, ToggleBeforeTickBackToCurrentSizeStillRegeneratesOnce)
{
    // Small -> Large -> Small in one frame: dims equal the generated ones, but the first change left the session dirty.
    // One generation is harmless; assert it is exactly one and the world is Small.
    Clean c;
    c.s.SetSize(Obj("size", "Large"));
    c.s.SetSize(Obj("size", "Small"));
    Dia::Maths::RandomStream rng(1);
    c.s.Tick(rng);
    EXPECT_EQ(c.s.World().CellCount(), 66 * 42);
    EXPECT_FALSE(c.s.Tick(rng));
}

TEST(WorldGenEdgeSize, PresetLoadThenSetSizeKeepsPresetIdAndNotModified)
{
    TempDir out;
    WorldShapePreset p;
    p.id = "keepme";
    p.displayName = "Keep";
    p.params.landFraction = 0.5f;
    ASSERT_TRUE(SavePreset(out.Str(), p, true).Ok());

    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    ASSERT_TRUE(Ok(s.PresetLoad(Obj("id", "keepme"))));
    s.SetSize(Obj("size", "Large"));
    EXPECT_EQ(s.Params()["preset"].asString(), "keepme");
    EXPECT_FALSE(s.Params()["modified"].asBool());
    // A same-value Set after load does not mark the preset modified either.
    s.Set(Obj("landFraction", 0.5));
    EXPECT_EQ(s.Params()["preset"].asString(), "keepme");
    EXPECT_FALSE(s.Params()["modified"].asBool());
    // A seed change is not a shape change.
    s.Set(Obj("seed", 99));
    EXPECT_EQ(s.Params()["preset"].asString(), "keepme");
    EXPECT_FALSE(s.Params()["modified"].asBool());
    // A real shape change is.
    s.Set(Obj("landFraction", 0.3));
    EXPECT_EQ(s.Params()["preset"].asString(), "");
    EXPECT_TRUE(s.Params()["modified"].asBool());
}

TEST(WorldGenEdgeSize, ModifiedFlagSurvivesFurtherSetsUntilNextLoadOrSave)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    ASSERT_TRUE(Ok(s.PresetSave(Json::Value([] { Json::Value a(Json::objectValue); a["id"] = "base"; a["displayName"] = "B"; return a; }()))));
    s.Set(Obj("rivers", 0.1));
    EXPECT_TRUE(s.Params()["modified"].asBool());
    s.Set(Obj("rivers", 0.2));
    EXPECT_TRUE(s.Params()["modified"].asBool());
    ASSERT_TRUE(Ok(s.PresetLoad(Obj("id", "base"))));
    EXPECT_FALSE(s.Params()["modified"].asBool());
    EXPECT_EQ(s.Params()["preset"].asString(), "base");
}

// ---------------------------------------------------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgePreset, SaveWithOptionalFieldsOmitted)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    const Json::Value r = s.PresetSave(Obj("id", "minimal"));   // no displayName, no description, no overwrite
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    const Json::Value list = s.Presets();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0]["id"].asString(), "minimal");
    EXPECT_EQ(list[0]["displayName"].asString(), "minimal");   // falls back to the id
    EXPECT_EQ(list[0]["description"].asString(), "");
}

TEST(WorldGenEdgeSave, OverwriteMustBeARealBool)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    ASSERT_TRUE(Ok(s.PresetSave(Obj("id", "ow"))));
    for (const Json::Value& v : { Json::Value("true"), Json::Value(1), Json::Value(), Json::Value("yes"), Json::Value(false) })
    {
        Json::Value a(Json::objectValue);
        a["id"] = "ow";
        a["overwrite"] = v;
        const Json::Value r = s.PresetSave(a);
        EXPECT_FALSE(Ok(r)) << v.toStyledString();
        EXPECT_EQ(r["status"].asString(), "AlreadyExists");
    }
    Json::Value a(Json::objectValue);
    a["id"] = "ow";
    a["overwrite"] = true;
    EXPECT_TRUE(Ok(s.PresetSave(a)));
}

TEST(WorldGenEdgeSave, IdLengthBoundary)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    EXPECT_TRUE(Ok(s.PresetSave(Obj("id", std::string(32, 'a')))));
    const Json::Value r = s.PresetSave(Obj("id", std::string(33, 'a')));
    EXPECT_FALSE(Ok(r));
    EXPECT_EQ(r["status"].asString(), "InvalidId");
    EXPECT_FALSE(Ok(s.PresetSave(Obj("id", ""))));
    EXPECT_EQ(s.Presets().size(), 1u);
}

TEST(WorldGenEdgeSave, TraversalAndSeparatorIdsNeverWriteOutsideOutDir)
{
    TempDir root;
    const std::filesystem::path outPath = root.Path() / "out";
    std::filesystem::create_directories(outPath);
    const size_t before = CountEntries(root.Path());

    WorldGenSession s;
    s.SetPresetDirectories("", outPath.string());
    const char* ids[] = { "../evil", "..\\evil", "a/b", "a\\b", "a.b", ".", "..", "evil/../../x", "C:evil", "/abs", "UPPER", "sp ace",
                          "tab\t", "evil.worldshape.json", "~", "a b", "é" };
    for (const char* id : ids)
    {
        Json::Value a(Json::objectValue);
        a["id"] = id;
        a["displayName"] = "x";
        a["overwrite"] = true;
        const Json::Value r = s.PresetSave(a);
        EXPECT_FALSE(Ok(r)) << id;
        EXPECT_EQ(r["status"].asString(), "InvalidId") << id;
    }
    EXPECT_EQ(CountEntries(root.Path()), before);
    EXPECT_TRUE(std::filesystem::is_empty(outPath));
    // An id with an embedded NUL passed as a std::string (the JSON path) is also rejected.
    Json::Value nulId(Json::objectValue);
    nulId["id"] = std::string("a\0b", 3);
    EXPECT_FALSE(Ok(s.PresetSave(nulId)));
    EXPECT_TRUE(std::filesystem::is_empty(outPath));
}

TEST(WorldGenEdgePreset, LoadRejectsTraversalIds)
{
    TempDir root;
    const std::filesystem::path deployed = root.Path() / "deployed";
    std::filesystem::create_directories(deployed);
    // A valid-looking preset file one level above the deployed dir must not be reachable.
    WorldShapePreset p;
    p.id = "secret";
    p.displayName = "S";
    ASSERT_TRUE(SavePreset(root.Str(), p, true).Ok());

    WorldGenSession s;
    s.SetPresetDirectories(deployed.string(), "");
    for (const char* id : { "../secret", "..\\secret", "a/../../secret", "secret.worldshape", "SECRET", "", "x y" })
    {
        EXPECT_FALSE(Ok(s.PresetLoad(Obj("id", id)))) << id;
    }
    EXPECT_FALSE(Ok(s.PresetLoad(Obj("id", std::string(33, 'a')))));
    EXPECT_FALSE(Ok(s.PresetLoad(Json::Value())));
    EXPECT_FALSE(Ok(s.PresetLoad(Json::Value("id"))));
    EXPECT_TRUE(s.IsDirty());   // untouched: still the initial pending generation
    EXPECT_TRUE(s.Params()["preset"].asString().empty());
}

TEST(WorldGenEdgePreset, SaveDisplayNameVariantsDoNotCrashAndReturnWellFormedResult)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    int n = 0;
    for (const Json::Value& name : { Json::Value(""), Json::Value(std::string(5000, 'x')), Json::Value("quote\"back\\slash\nnl"),
                                      Json::Value(5), Json::Value() })
    {
        Json::Value a(Json::objectValue);
        a["id"] = "name" + std::to_string(n++);
        a["displayName"] = name;
        a["description"] = std::string(2000, 'd');
        const Json::Value r = s.PresetSave(a);
        ASSERT_TRUE(r.isObject());
        EXPECT_TRUE(r.isMember("success"));
    }
    // What was written must reload.
    for (int i = 0; i < n; ++i)
    {
        EXPECT_TRUE(Ok(s.PresetLoad(Obj("id", ("name" + std::to_string(i)).c_str())))) << i;
    }
    // Round-trips awkward strings exactly.
    Json::Value a(Json::objectValue);
    a["id"] = "esc";
    a["displayName"] = "quote\"back\\slash";
    ASSERT_TRUE(Ok(s.PresetSave(a)));
    bool found = false;
    for (const Json::Value& e : s.Presets())
        if (e["id"].asString() == "esc") { found = true; EXPECT_EQ(e["displayName"].asString(), "quote\"back\\slash"); }
    EXPECT_TRUE(found);
}

TEST(WorldGenEdgePreset, BothDirsUnsetCommandsFailCleanly)
{
    WorldGenSession s;
    EXPECT_EQ(s.Presets().size(), 0u);
    const Json::Value l = s.PresetLoad(Obj("id", "highlands"));
    EXPECT_FALSE(Ok(l));
    EXPECT_FALSE(l["error"].asString().empty());
    EXPECT_FALSE(Ok(s.PresetSave(Obj("id", "x"))));
}

TEST(WorldGenEdgePreset, NonexistentConfiguredDirsAreEmptyNotCrash)
{
    TempDir root;
    WorldGenSession s;
    s.SetPresetDirectories((root.Path() / "nope1").string(), (root.Path() / "nope2").string());
    EXPECT_EQ(s.Presets().size(), 0u);
    EXPECT_FALSE(Ok(s.PresetLoad(Obj("id", "x"))));
    // Save creates the out dir on demand but never the deployed one.
    EXPECT_TRUE(Ok(s.PresetSave(Obj("id", "x"))));
    EXPECT_TRUE(std::filesystem::exists(root.Path() / "nope2"));
    EXPECT_FALSE(std::filesystem::exists(root.Path() / "nope1"));
}

TEST(WorldGenEdgePreset, SaveFailureLeavesPresetStateUnchanged)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    ASSERT_TRUE(Ok(s.PresetSave(Obj("id", "first"))));
    s.Set(Obj("rivers", 0.4));
    ASSERT_TRUE(s.Params()["modified"].asBool());
    EXPECT_FALSE(Ok(s.PresetSave(Obj("id", "first"))));   // exists, no overwrite
    EXPECT_FALSE(Ok(s.PresetSave(Obj("id", "BAD"))));
    EXPECT_TRUE(s.Params()["modified"].asBool());
    EXPECT_EQ(s.Params()["preset"].asString(), "");
}

// ---------------------------------------------------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeLayers, EveryLayerReassemblesFromDefaultPagesWithMatchingHash)
{
    WorldGenSession s;
    Generate(s);
    const size_t total = static_cast<size_t>(s.Dimensions().width) * s.Dimensions().height;
    ASSERT_EQ(total, 2772u);
    const TerrainLayers& tl = s.Generator().GetLayers();
    const Json::Value summary = s.Layers(Json::Value(Json::objectValue));
    std::set<std::string> seen;

    for (int li = 0; li < TerrainLayers::GetLayerCount(); ++li)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(li);
        const bool isFloat = info.type == LayerType::Float;
        const float* fd = isFloat ? tl.GetFloat(li) : nullptr;
        const uint32_t* ud = isFloat ? nullptr : tl.GetUInt(li);
        ASSERT_TRUE(isFloat ? fd != nullptr : ud != nullptr) << info.name;
        seen.insert(info.name);

        std::vector<uint32_t> raw;
        size_t offset = 0;
        for (int pages = 0; pages < 50; ++pages)
        {
            Json::Value args(Json::objectValue);
            args["layer"] = info.name;
            args["offset"] = static_cast<Json::UInt64>(offset);
            const Json::Value r = s.Layers(args);
            ASSERT_TRUE(Ok(r)) << info.name << " " << r.toStyledString();
            EXPECT_EQ(r["total"].asUInt(), total);
            EXPECT_EQ(r["offset"].asUInt64(), offset);
            // Every default page serialises well under the 16 KB sim reply.
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "";
            EXPECT_LT(Json::writeString(builder, r).size(), 15000u) << info.name << " offset " << offset;
            ASSERT_LE(r["count"].asUInt(), 500u);
            for (const Json::Value& v : r["values"])
            {
                uint32_t bits;
                if (isFloat) { const float f = static_cast<float>(v.asDouble()); std::memcpy(&bits, &f, 4); }
                else bits = v.asUInt();
                raw.push_back(bits);
            }
            if (r["nextOffset"].isNull()) break;
            ASSERT_EQ(r["nextOffset"].asUInt64(), offset + r["count"].asUInt64());
            offset = r["nextOffset"].asUInt64();
        }
        ASSERT_EQ(raw.size(), total) << info.name;
        EXPECT_EQ(std::memcmp(raw.data(), isFloat ? static_cast<const void*>(fd) : static_cast<const void*>(ud), total * 4), 0) << info.name;

        // Hash of the reassembled values equals the summary hash.
        std::string summaryHash = "<none>";
        for (const Json::Value& st : summary["stages"])
            for (const Json::Value& l : st["layers"])
                if (l["name"].asString() == info.name) summaryHash = l["hash"].asString();
        EXPECT_EQ(summaryHash, HexOf(RefFnv(raw.data(), raw.size() * 4))) << info.name;
    }
    EXPECT_EQ(static_cast<int>(seen.size()), TerrainLayers::GetLayerCount());   // layer names are unique
}

TEST(WorldGenEdgeLayers, PageBoundariesAndCrossingTheEnd)
{
    WorldGenSession s;
    Generate(s);
    // Float layer: pages of the 500 cap land on exact multiples.
    Json::Value r = s.Layers(Page("elevation", 500, Json::Value()));
    ASSERT_TRUE(Ok(r));
    EXPECT_EQ(r["count"].asUInt(), 500u);
    EXPECT_EQ(r["nextOffset"].asUInt(), 1000u);
    r = s.Layers(Page("elevation", 2500, Json::Value()));
    EXPECT_EQ(r["count"].asUInt(), 272u);
    EXPECT_TRUE(r["nextOffset"].isNull());

    // UInt layer: 1000 cap.
    r = s.Layers(Page("landmass", 1000, 1000));
    EXPECT_EQ(r["count"].asUInt(), 1000u);
    EXPECT_EQ(r["nextOffset"].asUInt(), 2000u);
    r = s.Layers(Page("landmass", 2000, 1000));
    EXPECT_EQ(r["count"].asUInt(), 772u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    r = s.Layers(Page("landmass", 2700, 500));   // crosses the end
    EXPECT_EQ(r["count"].asUInt(), 72u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    r = s.Layers(Page("landmass", 2771, 1));
    EXPECT_EQ(r["count"].asUInt(), 1u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    r = s.Layers(Page("landmass", 2770, 1));
    EXPECT_EQ(r["nextOffset"].asUInt(), 2771u);
    // Exact fit: offset + count == total gives a null nextOffset, one short does not.
    r = s.Layers(Page("landmass", 2762, 10));
    EXPECT_EQ(r["count"].asUInt(), 10u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    r = s.Layers(Page("landmass", 2762, 9));
    EXPECT_EQ(r["nextOffset"].asUInt(), 2771u);
}

TEST(WorldGenEdgeLayers, HugeCountOffsetAndArgumentTypes)
{
    WorldGenSession s;
    Generate(s);
    Json::Value r = s.Layers(Page("landmass", Json::Value(), Json::Value(static_cast<Json::UInt64>(1000000000))));
    ASSERT_TRUE(Ok(r));
    EXPECT_EQ(r["count"].asUInt(), 1000u);
    r = s.Layers(Page("elevation", Json::Value(), Json::Value(static_cast<Json::UInt64>(1000000000))));
    EXPECT_EQ(r["count"].asUInt(), 500u);
    r = s.Layers(Page("landmass", Json::Value(), Json::Value(static_cast<Json::UInt64>(9000000000000000ull))));
    EXPECT_TRUE(Ok(r));
    EXPECT_EQ(r["count"].asUInt(), 1000u);

    // Offsets that are huge, beyond the 9e15 cap, or not plain integers are errors.
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(static_cast<Json::UInt64>(1000000000)), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(static_cast<Json::UInt64>(UINT64_MAX)), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(static_cast<Json::UInt64>(UINT64_MAX)), 5))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(1e16), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(static_cast<Json::Int64>(INT64_MIN)), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(std::numeric_limits<double>::quiet_NaN()), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(std::numeric_limits<double>::infinity()), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(true), Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(), Json::Value(true)))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(), Json::Value(std::numeric_limits<double>::infinity())))));
    EXPECT_FALSE(Ok(s.Layers(Page("landmass", Json::Value(), Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))));
    // Integral-valued doubles are accepted.
    r = s.Layers(Page("landmass", Json::Value(10.0), Json::Value(3.0)));
    ASSERT_TRUE(Ok(r));
    EXPECT_EQ(r["offset"].asUInt(), 10u);
    EXPECT_EQ(r["count"].asUInt(), 3u);
}

TEST(WorldGenEdgeLayers, LayerArgumentVariants)
{
    WorldGenSession s;
    Generate(s);
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", ""))));
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", "Elevation"))));   // case sensitive
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", "elevation "))));
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", Json::Value(Json::arrayValue)))));
    // Non-object args fall back to the stage summary rather than failing.
    EXPECT_TRUE(s.Layers(Json::Value()).isMember("stages"));
    EXPECT_TRUE(s.Layers(Json::Value(5)).isMember("stages"));
    // Extra keys beside "layer" are ignored.
    Json::Value a = Page("landmass", 0, 3);
    a["junk"] = true;
    EXPECT_TRUE(Ok(s.Layers(a)));
}

TEST(WorldGenEdgeLayers, PagesTrackTheGeneratedWorldAfterResizeAndRegeneration)
{
    WorldGenSession s;
    Generate(s);
    const uint32_t rev1 = s.Layers(Page("landmass", 0, 1))["revision"].asUInt();
    s.SetSize(Obj("size", "Large"));
    Generate(s);
    const Json::Value r = s.Layers(Page("landmass", 6600, 1000));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["total"].asUInt(), 104u * 64u);
    EXPECT_EQ(r["count"].asUInt(), 104u * 64u - 6600u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    EXPECT_GT(r["revision"].asUInt(), rev1);
}

TEST(WorldGenEdgeLayers, QueriesBetweenSetSizeAndTickDescribeTheGeneratedWorld)
{
    // SetSize only changes pending state; queries before the next Tick must stay inside the generated buffers.
    WorldGenSession s;
    Generate(s);
    s.SetSize(Obj("size", "Large"));
    ASSERT_TRUE(s.IsDirty());

    const Json::Value r = s.Layers(Page("elevation", 2500, 500));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["total"].asUInt(), 66u * 42u);
    EXPECT_EQ(r["count"].asUInt(), 272u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    EXPECT_FALSE(Ok(s.Layers(Page("elevation", 2773, 1))));

    const Json::Value summary = s.Layers(Json::Value(Json::objectValue));
    const TerrainLayers& tl = s.Generator().GetLayers();
    const int li = tl.FindLayer("elevation");
    const float* d = tl.GetFloat(li);
    const std::string expected = HexOf(RefFnv(d, 66u * 42u * 4u));
    std::string got;
    for (const Json::Value& st : summary["stages"])
        for (const Json::Value& l : st["layers"])
            if (l["name"].asString() == "elevation") got = l["hash"].asString();
    EXPECT_EQ(got, expected);

    // Cell stays consistent with the generated grid too.
    EXPECT_TRUE(Ok(s.Cell(Obj("index", 2771))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", 2772))));
}

TEST(WorldGenEdgeLayers, RevisionAndHashStableAcrossIdenticalRegeneration)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value a = s.Layers(Json::Value(Json::objectValue));
    s.Regenerate(Json::Value());
    Generate(s);
    const Json::Value b = s.Layers(Json::Value(Json::objectValue));
    EXPECT_EQ(b["revision"].asUInt(), a["revision"].asUInt() + 1);
    for (size_t i = 0; i < a["stages"].size(); ++i)
        EXPECT_EQ(a["stages"][static_cast<Json::ArrayIndex>(i)]["layers"], b["stages"][static_cast<Json::ArrayIndex>(i)]["layers"]) << i;
}

// ---------------------------------------------------------------------------------------------------------------------
// Cell
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldGenEdgeCell, CornersAndCentre)
{
    WorldGenSession s;
    Generate(s);
    const int w = s.Dimensions().width;
    const int h = s.Dimensions().height;
    struct P { int x; int y; };
    const P pts[] = { { 0, 0 }, { w - 1, 0 }, { 0, h - 1 }, { w - 1, h - 1 }, { w / 2, h / 2 } };
    for (const P& p : pts)
    {
        Json::Value a(Json::objectValue);
        a["x"] = p.x;
        a["y"] = p.y;
        const Json::Value r = s.Cell(a);
        ASSERT_TRUE(Ok(r)) << p.x << "," << p.y << " " << r.toStyledString();
        EXPECT_EQ(r["x"].asInt(), p.x);
        EXPECT_EQ(r["y"].asInt(), p.y);
        EXPECT_EQ(r["index"].asInt(), p.y * w + p.x);
        EXPECT_EQ(s.Cell(Obj("index", p.y * w + p.x)), r);
        int edges = 0;
        for (const Json::Value& n : r["neighbours"]) edges += n.isString() ? 1 : 0;
        const bool isCorner = (p.x == 0 || p.x == w - 1) && (p.y == 0 || p.y == h - 1);
        if (isCorner) EXPECT_GE(edges, 2);
        else EXPECT_EQ(edges, 0);
    }
}

TEST(WorldGenEdgeCell, EveryCellHasSymmetricNeighbours)
{
    WorldGenSession s;
    Generate(s);
    const int w = s.Dimensions().width;
    const int count = s.World().CellCount();
    ASSERT_EQ(count, 2772);

    std::vector<Json::Value> cells;
    cells.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        cells.push_back(s.Cell(Obj("index", i)));
        ASSERT_TRUE(Ok(cells.back())) << i;
        ASSERT_EQ(cells.back()["index"].asInt(), i);
        ASSERT_EQ(cells.back()["y"].asInt() * w + cells.back()["x"].asInt(), i);
        ASSERT_EQ(cells.back()["neighbours"].size(), 6u);
    }
    int edgeSlots = 0;
    int checked = 0;
    for (int i = 0; i < count; ++i)
    {
        const Json::Value& c = cells[i];
        for (uint32_t d = 0; d < 6; ++d)
        {
            const Json::Value& n = c["neighbours"][d];
            if (n.isString())
            {
                EXPECT_EQ(n.asString(), "edge");
                ++edgeSlots;
                continue;
            }
            ASSERT_TRUE(n.isObject()) << i << " dir " << d;
            EXPECT_EQ(n["dir"].asUInt(), d);
            const int j = n["y"].asInt() * w + n["x"].asInt();
            ASSERT_GE(j, 0);
            ASSERT_LT(j, count);
            ASSERT_NE(j, i);
            const Json::Value& back = cells[j]["neighbours"][(d + 3) % 6];
            ASSERT_TRUE(back.isObject()) << i << " -> " << j << " dir " << d;
            EXPECT_EQ(back["y"].asInt() * w + back["x"].asInt(), i) << i << " dir " << d;
            ++checked;
        }
    }
    EXPECT_GT(edgeSlots, 0);
    EXPECT_EQ(checked + edgeSlots, count * 6);
}

TEST(WorldGenEdgeCell, NumericArgumentVariants)
{
    WorldGenSession s;
    Generate(s);
    const int w = s.Dimensions().width;
    const int h = s.Dimensions().height;
    auto xy = [](const Json::Value& x, const Json::Value& y) { Json::Value a(Json::objectValue); a["x"] = x; a["y"] = y; return a; };

    EXPECT_FALSE(Ok(s.Cell(xy(w, 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(0, h))));
    EXPECT_FALSE(Ok(s.Cell(xy(w, h))));
    EXPECT_FALSE(Ok(s.Cell(xy(-1, -1))));
    EXPECT_FALSE(Ok(s.Cell(xy(1.5, 2))));
    EXPECT_FALSE(Ok(s.Cell(xy(2, 1.5))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(static_cast<Json::UInt64>(UINT64_MAX)), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(0, Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(static_cast<Json::Int64>(INT64_MIN)), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(1e300), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(std::numeric_limits<double>::quiet_NaN()), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(true), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value(), 0))));
    EXPECT_FALSE(Ok(s.Cell(xy(Json::Value("3"), 0))));
    EXPECT_FALSE(Ok(s.Cell(Obj("x", 3))));
    EXPECT_FALSE(Ok(s.Cell(Obj("y", 3))));
    EXPECT_FALSE(Ok(s.Cell(Json::Value())));
    EXPECT_FALSE(Ok(s.Cell(Json::Value(5))));
    EXPECT_FALSE(Ok(s.Cell(Json::Value(Json::arrayValue))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", Json::Value(static_cast<Json::UInt64>(UINT64_MAX))))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", Json::Value(static_cast<Json::Int64>(INT64_MIN))))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", 1e300))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", Json::Value()))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", true))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", w * h))));
    EXPECT_TRUE(Ok(s.Cell(Obj("index", w * h - 1))));
    EXPECT_TRUE(Ok(s.Cell(Obj("index", 0))));

    // Integral-valued doubles are accepted and agree with the integer form.
    const Json::Value viaDouble = s.Cell(xy(3.0, 4.0));
    ASSERT_TRUE(Ok(viaDouble));
    EXPECT_EQ(viaDouble, s.Cell(xy(3, 4)));
    EXPECT_EQ(s.Cell(Obj("index", 7.0)), s.Cell(Obj("index", 7)));

    // "index" wins over a bad x/y pair; a bad index is not rescued by good x/y.
    Json::Value both = xy(-5, -5);
    both["index"] = 3;
    EXPECT_TRUE(Ok(s.Cell(both)));
    Json::Value badIdx = xy(1, 1);
    badIdx["index"] = -1;
    EXPECT_FALSE(Ok(s.Cell(badIdx)));
}

// ---------------------------------------------------------------------------------------------------------------------
// Module-level scenarios (real Application with MessageBusHost + RandomStream + WorldGenerationModule on a 30 Hz SimPU)
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

using namespace Dia::ApplicationFlow;
using Dia::Core::StringCRC;

Cluiche::AppFlow::MessageBusHostModule* g_edgeBus = nullptr;
CoW::WorldGenerationModule*              g_edgeWg  = nullptr;

Module* CreateEdgeBus(const StringCRC& id) { g_edgeBus = new Cluiche::AppFlow::MessageBusHostModule(id); return g_edgeBus; }
Module* CreateEdgeRng(const StringCRC& id) { return new CoW::RandomStreamModule(id); }
Module* CreateEdgeWg(const StringCRC& id)  { g_edgeWg = new CoW::WorldGenerationModule(id); return g_edgeWg; }

constexpr float kEdgeDt = 1.0f / 30.0f;

struct EdgeFixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    CoW::WorldGenerationModule* wg = nullptr;
    Dia::MessageBus::BusSubscriptionHandle sub;
    std::vector<CoW::WorldGenerationComplete> msgs;
    std::string config;

    explicit EdgeFixture(const char* configJson = nullptr)
    {
        if (configJson) config = configJson;
        reg.Register(StringCRC("MessageBusHostModule"), &CreateEdgeBus);
        reg.Register(StringCRC("RandomStreamModule"), &CreateEdgeRng);
        reg.Register(StringCRC("WorldGenerationModule"), &CreateEdgeWg);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;
        for (const char* name : { "MessageBusHostModule", "RandomStreamModule" })
        {
            ModuleDeclaration d;
            d.instanceId = StringCRC(name);
            d.typeId = StringCRC(name);
            d.stages.Add(StringCRC("all"));
            pu.modules.Add(d);
        }
        ModuleDeclaration wgDecl;
        wgDecl.instanceId = StringCRC("WorldGenerationModule");
        wgDecl.typeId = StringCRC("WorldGenerationModule");
        wgDecl.stages.Add(StringCRC("all"));
        wgDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        wgDecl.dependencies.Add(StringCRC("RandomStreamModule"));
        if (!config.empty()) wgDecl.configJson = config.c_str();
        pu.modules.Add(wgDecl);
        manifest.processingUnits.Add(pu);
    }

    bool Boot()
    {
        g_edgeBus = nullptr; g_edgeWg = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start()) return false;
        bus = g_edgeBus;
        wg = g_edgeWg;
        if (!bus || !wg) return false;
        bus->GetBus().RegisterType<CoW::WorldGenerationComplete, 4>();
        sub = bus->GetBus().Subscribe<CoW::WorldGenerationComplete>(StringCRC("EdgeSink"),
            [this](const CoW::WorldGenerationComplete& m) { msgs.push_back(m); });
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kEdgeDt);
            if (wg->GetState() == ModuleState::kActive) return true;
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kEdgeDt); }
};

} // namespace

TEST(WorldGenEdgeModule, UnknownAndMistypedConfigKeysAreIgnoredWithDefaults)
{
    EdgeFixture f("{\"presets_dir\":5,\"out_presets_dir\":[1],\"auto_generate\":null,\"size\":5,\"seed\":1.5,"
                  "\"unknown\":{\"a\":1},\"another\":\"x\"}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
    ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 66);
    EXPECT_EQ(f.wg->GetSession().Seed(), 1u);
}

TEST(WorldGenEdgeModule, ConfigNotAnObjectUsesDefaults)
{
    for (const char* cfg : { "[1,2,3]", "42", "\"text\"", "null" })
    {
        EdgeFixture f(cfg);
        ASSERT_TRUE(f.Boot()) << cfg;
        f.Pump(3);
        EXPECT_EQ(f.wg->GetWorldRevision(), 1u) << cfg;
        EXPECT_EQ(f.wg->GetSession().Seed(), 1u) << cfg;
    }
}

TEST(WorldGenEdgeModule, ConfigSeedExtremesAndInvalidSizeCase)
{
    {
        EdgeFixture f("{\"seed\":18446744073709551615,\"size\":\"Large\"}");
        ASSERT_TRUE(f.Boot());
        f.Pump(3);
        EXPECT_EQ(f.wg->GetSession().Seed(), UINT64_MAX);
        EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 104);
    }
    {
        EdgeFixture f("{\"seed\":0,\"size\":\"large\"}");   // wrong case: ignored, seed 0 honoured
        ASSERT_TRUE(f.Boot());
        f.Pump(3);
        EXPECT_EQ(f.wg->GetSession().Seed(), 0u);
        EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 66);
    }
    {
        EdgeFixture f("{\"seed\":-1}");
        ASSERT_TRUE(f.Boot());
        f.Pump(3);
        EXPECT_EQ(f.wg->GetSession().Seed(), 1u);
    }
}

TEST(WorldGenEdgeModule, BusMessageCountEqualsRevisionBumpsOverMixedCommands)
{
    EdgeFixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    CoW::WorldGeneration::WorldGenSession& s = f.wg->GetSession();
    ASSERT_EQ(f.wg->GetWorldRevision(), 1u);

    auto step = [&]() { f.wg->NotifyAction(); f.Pump(3); };
    uint32_t expectedBumps = 0;
    auto expectBump = [&](bool bump) { if (bump) ++expectedBumps; };

    s.Set(Obj("rivers", 0.9));                        step(); expectBump(true);   //  1 shape change
    s.Set(Obj("rivers", 0.9));                        step(); expectBump(false);  //  2 same value
    s.Set(Obj("rivers", 50));                         step(); expectBump(true);   //  3 clamps to 1.0 (changed from 0.9)
    s.Set(Obj("rivers", 50));                         step(); expectBump(false);  //  4 clamps to current
    s.Regenerate(Json::Value());                      step(); expectBump(true);   //  5
    s.Regenerate(Obj("seed", 7));                     step(); expectBump(true);   //  6
    s.SetSize(Obj("size", "Standard"));               step(); expectBump(true);   //  7
    s.SetSize(Obj("size", "Standard"));               step(); expectBump(false);  //  8
    s.Set(Obj("bogus", 1));                           step(); expectBump(false);  //  9 rejected only
    s.Set(Obj("seed", 8));                            step(); expectBump(true);   // 10

    EXPECT_EQ(f.wg->GetWorldRevision(), 1u + expectedBumps);
    EXPECT_EQ(f.msgs.size(), 1u + expectedBumps);
    // Messages carry strictly increasing revisions with the seed active at publish time.
    for (size_t i = 0; i < f.msgs.size(); ++i)
        EXPECT_EQ(f.msgs[i].worldRevision, static_cast<uint32_t>(i + 1)) << i;
    EXPECT_EQ(f.msgs.back().seed, 8u);
}

TEST(WorldGenEdgeModule, AutoGenerateFalseWithSeveralActionsBeforeFirstTickIsOneGeneration)
{
    EdgeFixture f("{\"auto_generate\":false}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    CoW::WorldGeneration::WorldGenSession& s = f.wg->GetSession();
    s.Set(Obj("rivers", 0.2));
    s.Set(Obj("seed", 3));
    s.SetSize(Obj("size", "Standard"));
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);   // session mutation alone does not generate
    f.wg->NotifyAction();
    f.Pump(3);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
    EXPECT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].seed, 3u);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 80);
}

TEST(WorldGenEdgeModule, StopThenFreshAppRegistersHealthCleanly)
{
    for (int round = 0; round < 3; ++round)
    {
        EdgeFixture f;
        ASSERT_TRUE(f.Boot()) << round;
        f.Pump(3);
        EXPECT_TRUE(f.wg->IsHealthRegistered()) << round;
        EXPECT_EQ(f.wg->GetWorldRevision(), 1u) << round;
        f.app->RequestShutdown();
        f.Pump(20);
        EXPECT_FALSE(f.wg->IsHealthRegistered()) << round;
    }
}
