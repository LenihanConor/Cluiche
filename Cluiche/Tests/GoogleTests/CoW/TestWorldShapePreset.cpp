// S-1 Terrain Generator task 15: preset files and WorldShapePreset I/O (AC19).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace CoW::WorldGeneration;

namespace {

std::string FindShippedPresetDir()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
        if (std::filesystem::is_directory(candidate, ec))
        {
            return candidate.string();
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir)
        {
            break;
        }
        dir = parent;
    }
    return {};
}

// Fresh empty temp directory per test.
class TempDir
{
public:
    TempDir()
    {
        static int counter = 0;
        mPath = std::filesystem::temp_directory_path() / ("cow_preset_test_" + std::to_string(++counter) + "_" +
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

bool HasWarning(const RuleWarnings& w, const std::string& text)
{
    for (unsigned int i = 0; i < w.Size(); ++i)
    {
        if (std::string(w[i].AsChar()) == text)
        {
            return true;
        }
    }
    return false;
}

void WriteFile(const std::filesystem::path& p, const std::string& text)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

Json::Value ContinentalJson()
{
    WorldShapePreset p;
    p.id = "continental";
    p.displayName = "Continental";
    p.description = "d";
    return PresetToJson(p);
}

WorldShapePreset Expected(const char* id)
{
    WorldShapePreset e;
    e.id = id;
    WorldShapeParams& p = e.params;
    const std::string s = id;
    if (s == "continental")
    {
        p = WorldShapeParams{};
    }
    else if (s == "archipelago")
    {
        p.landFraction = 0.30f; p.continents = 2; p.islands = 5; p.continentIslands = 0.5f; p.peninsulas = 2; p.coastline = 0.8f;
        p.mountainousness = 0.4f; p.spineToRing = 0.0f; p.orientationDeg = 60.0f; p.passes = 3; p.barrier = 0.45f;
        p.barrierForm = BarrierForm::VolcanicChain; p.ruggedness = 0.5f; p.rivers = 0.35f; p.valleyDepth = 0.3f; p.lakes = 2;
        p.latitudeNorthDeg = 40.0f; p.latitudeSouthDeg = 10.0f;
    }
    else
    {
        p.landFraction = 0.55f; p.continents = 1; p.islands = 3; p.continentIslands = 0.85f; p.peninsulas = 1; p.coastline = 0.35f;
        p.mountainousness = 0.85f; p.spineToRing = 0.75f; p.orientationDeg = 120.0f; p.passes = 2; p.barrier = 0.8f;
        p.barrierForm = BarrierForm::Plateau; p.ruggedness = 0.7f; p.rivers = 0.65f; p.valleyDepth = 0.75f; p.lakes = 8;
        p.latitudeNorthDeg = 65.0f; p.latitudeSouthDeg = 35.0f;
    }
    return e;
}

} // namespace

// ---- Shipped files ------------------------------------------------------------------------------
TEST(WorldShapePreset, ShippedPresetsLoadWithNoWarningsAndMatchSpecTable)
{
    const std::string dir = FindShippedPresetDir();
    ASSERT_FALSE(dir.empty()) << "Presets dir not found above " << __FILE__;
    for (const char* id : { "continental", "archipelago", "highlands" })
    {
        SCOPED_TRACE(id);
        WorldShapePreset loaded;
        RuleWarnings warnings;
        const PresetResult r = LoadPreset(dir, id, loaded, warnings);
        ASSERT_TRUE(r.Ok()) << PresetStatusName(r.status) << ": " << r.message;
        EXPECT_EQ(warnings.Size(), 0u);
        EXPECT_EQ(loaded.id, id);
        EXPECT_FALSE(loaded.displayName.empty());
        EXPECT_FALSE(loaded.description.empty());
        EXPECT_EQ(loaded.version, kWorldShapePresetVersion);
        EXPECT_TRUE(loaded.params == Expected(id).params);
        // Every shipped value is already in range: Clamp() changes nothing.
        WorldShapeParams clamped = loaded.params;
        clamped.Clamp();
        EXPECT_TRUE(clamped == loaded.params);
    }
}

TEST(WorldShapePreset, ContinentalEqualsDefaultParams)
{
    const std::string dir = FindShippedPresetDir();
    ASSERT_FALSE(dir.empty());
    WorldShapePreset loaded;
    RuleWarnings warnings;
    ASSERT_TRUE(LoadPreset(dir, "continental", loaded, warnings).Ok());
    EXPECT_TRUE(loaded.params == WorldShapeParams{});
}

TEST(WorldShapePreset, ShippedPresetsAreDistinct)
{
    const std::string dir = FindShippedPresetDir();
    ASSERT_FALSE(dir.empty());
    WorldShapePreset a, b, c;
    RuleWarnings w;
    ASSERT_TRUE(LoadPreset(dir, "continental", a, w).Ok());
    ASSERT_TRUE(LoadPreset(dir, "archipelago", b, w).Ok());
    ASSERT_TRUE(LoadPreset(dir, "highlands", c, w).Ok());
    EXPECT_FALSE(a.params == b.params);
    EXPECT_FALSE(a.params == c.params);
    EXPECT_FALSE(b.params == c.params);
    EXPECT_EQ(b.params.continents, 2);   // Archipelago keeps 2 continents on purpose
}

TEST(WorldShapePreset, ListPresetsOnShippedDirIsSortedAndComplete)
{
    const std::string dir = FindShippedPresetDir();
    ASSERT_FALSE(dir.empty());
    const std::vector<PresetInfo> list = ListPresets(dir);
    ASSERT_GE(list.size(), 3u);
    for (size_t i = 1; i < list.size(); ++i)
    {
        EXPECT_LT(list[i - 1].id, list[i].id);
    }
    bool seen[3] = {};
    for (const PresetInfo& info : list)
    {
        if (info.id == "archipelago") { seen[0] = true; EXPECT_EQ(info.displayName, "Archipelago"); }
        if (info.id == "continental") { seen[1] = true; }
        if (info.id == "highlands")   { seen[2] = true; }
        EXPECT_EQ(info.version, kWorldShapePresetVersion);
        EXPECT_FALSE(info.sourcePath.empty());
    }
    EXPECT_TRUE(seen[0] && seen[1] && seen[2]);
}

// ---- Round trip --------------------------------------------------------------------------------
TEST(WorldShapePreset, SaveThenLoadRoundTripsEveryParamExactly)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "custom_1";
    p.displayName = "Custom \"One\"";
    p.description = "line1 \xC3\xA9 tab\t";
    // Values chosen to be awkward floats (not nice decimals).
    p.params.landFraction = 0.123456789f < 0.2f ? 0.2345679f : 0.123456789f;
    p.params.landFraction = 0.3141593f;
    p.params.continents = 3;
    p.params.islands = 6;
    p.params.continentIslands = 0.33333334f;
    p.params.peninsulas = 3;
    p.params.coastline = 0.1f;
    p.params.mountainousness = 0.999999f;
    p.params.spineToRing = 1e-5f;
    p.params.orientationDeg = 359.99f;
    p.params.passes = 4;
    p.params.barrier = 0.7071068f;
    p.params.barrierForm = BarrierForm::VolcanicChain;
    p.params.ruggedness = 0.2f;
    p.params.rivers = 0.9f;
    p.params.valleyDepth = 0.0f;
    p.params.lakes = 12;
    p.params.latitudeNorthDeg = -89.5f;
    p.params.latitudeSouthDeg = 89.123f;
    p.params.temperatureOffset = -0.29f;
    p.params.moistureOffset = 0.1234567f;

    ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());
    WorldShapePreset back;
    RuleWarnings w;
    const PresetResult r = LoadPreset(tmp.Str(), "custom_1", back, w);
    ASSERT_TRUE(r.Ok()) << r.message;
    EXPECT_EQ(w.Size(), 0u);
    EXPECT_TRUE(back.params == p.params);
    EXPECT_EQ(back.displayName, p.displayName);
    EXPECT_EQ(back.description, p.description);
    EXPECT_EQ(back.id, p.id);
}

TEST(WorldShapePreset, RoundTripSweepOverManyFloatValues)
{
    TempDir tmp;
    // Deterministic pseudo-random sweep across the ranges.
    uint32_t state = 12345u;
    auto next = [&state]() { state = state * 1664525u + 1013904223u; return (state >> 8) / static_cast<float>(1u << 24); };
    for (int i = 0; i < 200; ++i)
    {
        WorldShapePreset p;
        p.id = "sweep";
        p.displayName = "s";
        p.params.landFraction = 0.20f + 0.45f * next();
        p.params.continents = static_cast<uint8_t>(1 + static_cast<int>(next() * 2.99f));
        p.params.islands = static_cast<uint8_t>(next() * 6.99f);
        p.params.continentIslands = next();
        p.params.peninsulas = static_cast<uint8_t>(next() * 3.99f);
        p.params.coastline = next();
        p.params.mountainousness = next();
        p.params.spineToRing = next();
        p.params.orientationDeg = 359.99f * next();
        p.params.passes = static_cast<uint8_t>(next() * 4.99f);
        p.params.barrier = next();
        p.params.barrierForm = next() < 0.5f ? BarrierForm::Plateau : BarrierForm::VolcanicChain;
        p.params.ruggedness = next();
        p.params.rivers = next();
        p.params.valleyDepth = next();
        p.params.lakes = static_cast<uint8_t>(next() * 12.99f);
        p.params.latitudeNorthDeg = -90.0f + 180.0f * next();
        p.params.latitudeSouthDeg = -90.0f + 180.0f * next();
        p.params.temperatureOffset = -0.3f + 0.6f * next();
        p.params.moistureOffset = -0.3f + 0.6f * next();
        p.params.Clamp();

        ASSERT_TRUE(SavePreset(tmp.Str(), p, true).Ok());
        WorldShapePreset back;
        RuleWarnings w;
        ASSERT_TRUE(LoadPreset(tmp.Str(), "sweep", back, w).Ok());
        ASSERT_EQ(w.Size(), 0u) << i;
        ASSERT_TRUE(back.params == p.params) << "iteration " << i;
    }
}

TEST(WorldShapePreset, SavedFileUsesShortDecimalsNotFloatNoise)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "tidy";
    p.displayName = "Tidy";
    p.params.landFraction = 0.42f;
    ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());
    std::ifstream f(PresetFilePath(tmp.Str(), "tidy"));
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("0.42"), std::string::npos);
    EXPECT_EQ(text.find("0.4199"), std::string::npos) << text;
    EXPECT_NE(text.find("\"format\" : \"cow.worldshape\""), std::string::npos) << text;
}

TEST(WorldShapePreset, SaveWritesClampedParams)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "wild";
    p.displayName = "Wild";
    p.params.rivers = 5.0f;
    p.params.lakes = 200;
    p.params.orientationDeg = -30.0f;
    ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());
    WorldShapePreset back;
    RuleWarnings w;
    ASSERT_TRUE(LoadPreset(tmp.Str(), "wild", back, w).Ok());
    EXPECT_EQ(w.Size(), 0u);   // a saved file is always in range
    EXPECT_FLOAT_EQ(back.params.rivers, 1.0f);
    EXPECT_EQ(back.params.lakes, 12);
    EXPECT_FLOAT_EQ(back.params.orientationDeg, 330.0f);
}

// ---- Loading rules -----------------------------------------------------------------------------
TEST(WorldShapePreset, MissingParamKeyTakesDefaultWithOneWarning)
{
    Json::Value root = ContinentalJson();
    root["params"]["rivers"] = 0.9;
    root["params"].removeMember("lakes");
    WorldShapePreset out;
    RuleWarnings w;
    const PresetResult r = ParsePreset(root, "continental", out, w);
    ASSERT_TRUE(r.Ok());
    EXPECT_EQ(w.Size(), 1u);
    EXPECT_TRUE(HasWarning(w, "missing:lakes"));
    EXPECT_EQ(out.params.lakes, WorldShapeParams{}.lakes);
    EXPECT_FLOAT_EQ(out.params.rivers, 0.9f);
}

TEST(WorldShapePreset, MissingBarrierFormTakesDefaultWithWarning)
{
    Json::Value root = ContinentalJson();
    root["params"].removeMember("barrierForm");
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_TRUE(HasWarning(w, "missing:barrierForm"));
    EXPECT_EQ(w.Size(), 1u);
    EXPECT_EQ(out.params.barrierForm, BarrierForm::Plateau);
}

TEST(WorldShapePreset, UnknownKeyIsIgnoredWithWarning)
{
    Json::Value root = ContinentalJson();
    root["params"]["wobble"] = 3;
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_EQ(w.Size(), 1u);
    EXPECT_TRUE(HasWarning(w, "unknown:wobble"));
    EXPECT_TRUE(out.params == WorldShapeParams{});
}

TEST(WorldShapePreset, OutOfRangeValueIsClampedWithWarning)
{
    Json::Value root = ContinentalJson();
    root["params"]["rivers"] = 1.7;
    root["params"]["landFraction"] = 0.01;
    root["params"]["lakes"] = 99;
    root["params"]["continents"] = 0;
    root["params"]["latitudeNorthDeg"] = -500.0;
    root["params"]["temperatureOffset"] = 2.0;
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_EQ(w.Size(), 6u);
    EXPECT_TRUE(HasWarning(w, "range:rivers"));
    EXPECT_TRUE(HasWarning(w, "range:landFraction"));
    EXPECT_TRUE(HasWarning(w, "range:lakes"));
    EXPECT_TRUE(HasWarning(w, "range:continents"));
    EXPECT_TRUE(HasWarning(w, "range:latitudeNorthDeg"));
    EXPECT_TRUE(HasWarning(w, "range:temperatureOffset"));
    EXPECT_FLOAT_EQ(out.params.rivers, 1.0f);
    EXPECT_FLOAT_EQ(out.params.landFraction, 0.20f);
    EXPECT_EQ(out.params.lakes, 12);
    EXPECT_EQ(out.params.continents, 1);
    EXPECT_FLOAT_EQ(out.params.latitudeNorthDeg, -90.0f);
    EXPECT_FLOAT_EQ(out.params.temperatureOffset, 0.3f);
}

TEST(WorldShapePreset, ValuesExactlyAtBoundsDoNotWarn)
{
    Json::Value root = ContinentalJson();
    root["params"]["landFraction"] = 0.20;
    root["params"]["lakes"] = 12;
    root["params"]["continents"] = 3;
    root["params"]["latitudeNorthDeg"] = 90;
    root["params"]["latitudeSouthDeg"] = -90;
    root["params"]["orientationDeg"] = 0;
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_EQ(w.Size(), 0u);
}

TEST(WorldShapePreset, OrientationOutsideZeroTo360WrapsWithWarning)
{
    for (double deg : { -30.0, 360.0, 725.0 })
    {
        Json::Value root = ContinentalJson();
        root["params"]["orientationDeg"] = deg;
        WorldShapePreset out;
        RuleWarnings w;
        ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
        EXPECT_TRUE(HasWarning(w, "range:orientationDeg")) << deg;
        EXPECT_GE(out.params.orientationDeg, 0.0f);
        EXPECT_LT(out.params.orientationDeg, 360.0f);
    }
}

TEST(WorldShapePreset, NonIntegerValueForCountParamIsRoundedWithWarning)
{
    Json::Value root = ContinentalJson();
    root["params"]["islands"] = 3.4;
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_EQ(out.params.islands, 3);
    EXPECT_TRUE(HasWarning(w, "range:islands"));
}

TEST(WorldShapePreset, WrongTypeValueKeepsDefaultWithInvalidWarning)
{
    Json::Value root = ContinentalJson();
    root["params"]["rivers"] = "lots";
    WorldShapePreset out;
    RuleWarnings w;
    ASSERT_TRUE(ParsePreset(root, "continental", out, w).Ok());
    EXPECT_TRUE(HasWarning(w, "invalid:rivers"));
    EXPECT_FLOAT_EQ(out.params.rivers, WorldShapeParams{}.rivers);
}

TEST(WorldShapePreset, UnknownBarrierFormIsRejected)
{
    Json::Value root = ContinentalJson();
    root["params"]["barrierForm"] = "Wall";
    WorldShapePreset out;
    RuleWarnings w;
    const PresetResult r = ParsePreset(root, "continental", out, w);
    EXPECT_EQ(r.status, PresetStatus::UnknownBarrierForm);
    EXPECT_TRUE(out.id.empty());

    root["params"]["barrierForm"] = 3;
    EXPECT_EQ(ParsePreset(root, "continental", out, w).status, PresetStatus::UnknownBarrierForm);
}

TEST(WorldShapePreset, VersionGreaterThanSupportedIsRejected)
{
    Json::Value root = ContinentalJson();
    root["version"] = 2;
    WorldShapePreset out;
    RuleWarnings w;
    EXPECT_EQ(ParsePreset(root, "continental", out, w).status, PresetStatus::UnsupportedVersion);
    root["version"] = 0;
    EXPECT_EQ(ParsePreset(root, "continental", out, w).status, PresetStatus::UnsupportedVersion);
    root["version"] = "1";
    EXPECT_EQ(ParsePreset(root, "continental", out, w).status, PresetStatus::InvalidFormat);
    root.removeMember("version");
    EXPECT_EQ(ParsePreset(root, "continental", out, w).status, PresetStatus::InvalidFormat);
}

TEST(WorldShapePreset, IdMismatchAndInvalidIdAreRejected)
{
    WorldShapePreset out;
    RuleWarnings w;
    Json::Value root = ContinentalJson();
    EXPECT_EQ(ParsePreset(root, "highlands", out, w).status, PresetStatus::IdMismatch);
    EXPECT_TRUE(ParsePreset(root, "", out, w).Ok());   // empty expected id skips the match

    for (const char* bad : { "Upper", "has space", "dash-ed", "", "this_id_is_way_too_long_for_thirty_two_chars" })
    {
        root["id"] = bad;
        EXPECT_EQ(ParsePreset(root, "", out, w).status, PresetStatus::InvalidId) << bad;
    }
    root["id"] = 5;
    EXPECT_EQ(ParsePreset(root, "", out, w).status, PresetStatus::InvalidId);
}

TEST(WorldShapePreset, BadEnvelopeIsRejected)
{
    WorldShapePreset out;
    RuleWarnings w;
    EXPECT_EQ(ParsePreset(Json::Value(5), "", out, w).status, PresetStatus::InvalidFormat);
    Json::Value root = ContinentalJson();
    root["format"] = "cow.something_else";
    EXPECT_EQ(ParsePreset(root, "", out, w).status, PresetStatus::InvalidFormat);
    root = ContinentalJson();
    root.removeMember("params");
    EXPECT_EQ(ParsePreset(root, "", out, w).status, PresetStatus::InvalidFormat);
    root = ContinentalJson();
    root["params"] = Json::Value(Json::arrayValue);
    EXPECT_EQ(ParsePreset(root, "", out, w).status, PresetStatus::InvalidFormat);
}

// ---- Files -------------------------------------------------------------------------------------
TEST(WorldShapePreset, LoadFromFileRejectsVersion2AndIdMismatchAndGarbage)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "alpha";
    p.displayName = "Alpha";
    ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());

    // version 2 in a copy
    Json::Value v = PresetToJson(p);
    v["version"] = 2;
    WriteFile(tmp.Path() / "v2.worldshape.json", "");
    {
        Json::StreamWriterBuilder b;
        WriteFile(tmp.Path() / "v2.worldshape.json", Json::writeString(b, v));
    }
    WorldShapePreset out;
    RuleWarnings w;
    EXPECT_EQ(LoadPreset(tmp.Str(), "v2", out, w).status, PresetStatus::UnsupportedVersion);

    // id says alpha but the file is beta.worldshape.json
    std::filesystem::copy_file(tmp.Path() / "alpha.worldshape.json", tmp.Path() / "beta.worldshape.json");
    EXPECT_EQ(LoadPreset(tmp.Str(), "beta", out, w).status, PresetStatus::IdMismatch);

    WriteFile(tmp.Path() / "junk.worldshape.json", "{ not json");
    EXPECT_EQ(LoadPreset(tmp.Str(), "junk", out, w).status, PresetStatus::ParseError);

    EXPECT_EQ(LoadPreset(tmp.Str(), "missing", out, w).status, PresetStatus::FileNotFound);
    EXPECT_EQ(LoadPreset(tmp.Str(), "Bad Id", out, w).status, PresetStatus::InvalidId);
    EXPECT_EQ(LoadPreset(tmp.Str(), "../alpha", out, w).status, PresetStatus::InvalidId);
}

TEST(WorldShapePreset, SaveRefusesExistingUnlessOverwrite)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "dup";
    p.displayName = "First";
    ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());

    p.displayName = "Second";
    const PresetResult refused = SavePreset(tmp.Str(), p, false);
    EXPECT_EQ(refused.status, PresetStatus::AlreadyExists);
    WorldShapePreset back;
    RuleWarnings w;
    ASSERT_TRUE(LoadPreset(tmp.Str(), "dup", back, w).Ok());
    EXPECT_EQ(back.displayName, "First");

    ASSERT_TRUE(SavePreset(tmp.Str(), p, true).Ok());
    ASSERT_TRUE(LoadPreset(tmp.Str(), "dup", back, w).Ok());
    EXPECT_EQ(back.displayName, "Second");
}

TEST(WorldShapePreset, SaveRejectsInvalidIdAndCreatesMissingDirectory)
{
    TempDir tmp;
    WorldShapePreset p;
    p.id = "Bad/Id";
    EXPECT_EQ(SavePreset(tmp.Str(), p, true).status, PresetStatus::InvalidId);
    p.id = "";
    EXPECT_EQ(SavePreset(tmp.Str(), p, true).status, PresetStatus::InvalidId);

    p.id = "nested";
    p.displayName = "Nested";
    const std::string nested = (tmp.Path() / "a" / "b").string();
    const PresetResult r = SavePreset(nested, p, false);
    ASSERT_TRUE(r.Ok()) << r.message;
    EXPECT_TRUE(std::filesystem::exists(r.path));
}

TEST(WorldShapePreset, ListPresetsReturnsSortedValidFilesAndSkipsInvalid)
{
    TempDir tmp;
    EXPECT_TRUE(ListPresets((tmp.Path() / "nope").string()).empty());
    EXPECT_TRUE(ListPresets(tmp.Str()).empty());

    for (const char* id : { "zeta", "alpha", "mid" })
    {
        WorldShapePreset p;
        p.id = id;
        p.displayName = std::string("Name ") + id;
        p.description = "desc";
        ASSERT_TRUE(SavePreset(tmp.Str(), p, false).Ok());
    }
    WriteFile(tmp.Path() / "junk.worldshape.json", "garbage");
    WriteFile(tmp.Path() / "notes.txt", "not a preset");
    // id/file mismatch is skipped
    std::filesystem::copy_file(tmp.Path() / "alpha.worldshape.json", tmp.Path() / "copy.worldshape.json");

    const std::vector<PresetInfo> list = ListPresets(tmp.Str());
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0].id, "alpha");
    EXPECT_EQ(list[1].id, "mid");
    EXPECT_EQ(list[2].id, "zeta");
    EXPECT_EQ(list[0].displayName, "Name alpha");
    EXPECT_EQ(list[0].description, "desc");
    EXPECT_EQ(list[0].version, 1u);
    EXPECT_TRUE(std::filesystem::exists(list[0].sourcePath));
}

TEST(WorldShapePreset, IsValidPresetIdBoundaries)
{
    EXPECT_TRUE(IsValidPresetId("a"));
    EXPECT_TRUE(IsValidPresetId("a_1"));
    EXPECT_TRUE(IsValidPresetId(std::string(32, 'x')));
    EXPECT_FALSE(IsValidPresetId(std::string(33, 'x')));
    EXPECT_FALSE(IsValidPresetId(""));
    EXPECT_FALSE(IsValidPresetId("A"));
    EXPECT_FALSE(IsValidPresetId("a.b"));
    EXPECT_FALSE(IsValidPresetId("a b"));
}
