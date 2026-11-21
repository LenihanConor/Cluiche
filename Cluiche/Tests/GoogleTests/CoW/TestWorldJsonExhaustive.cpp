// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC6 speed, AC7 default path, limits)
// Exhaustive pass for Feature 4: speed, sweeps over the shipped presets, size guards and boundary worlds.
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <Shared/WorldGeneration/WorldFiles.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <Shared/World/Testing/StratumTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace
{

using Clock = std::chrono::steady_clock;
double MsSince(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

std::string FindShippedPresetDir()
{
    std::error_code ec;
    fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const fs::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
        if (fs::is_directory(candidate, ec)) { return candidate.generic_string(); }
        const fs::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    return std::string();
}

Json::Value Obj() { return Json::Value(Json::objectValue); }

// A session with a world produced by Regenerate for `preset` (shipped preset id, "" = defaults) at `seed`.
void Generate(WorldGenSession& s, const std::string& preset, uint64_t seed, const char* size = nullptr)
{
    Dia::Maths::RandomStream rng{ 1 };
    if (!preset.empty())
    {
        s.SetPresetDirectories(FindShippedPresetDir(), "");
        Json::Value load = Obj(); load["id"] = preset;
        ASSERT_TRUE(s.PresetLoad(load)["success"].asBool()) << preset;
    }
    if (size != nullptr)
    {
        Json::Value sz = Obj(); sz["size"] = size;
        ASSERT_TRUE(s.SetSize(sz)["success"].asBool());
    }
    Json::Value regen = Obj(); regen["seed"] = static_cast<Json::UInt64>(seed);
    ASSERT_TRUE(s.Regenerate(regen)["success"].asBool());
    ASSERT_TRUE(s.Tick(rng));
}

WorldImportResult RoundTrip(const WorldStructure& original, WorldStructure& imported, std::string* textOut = nullptr)
{
    std::string text;
    JsonIssue issue;
    EXPECT_TRUE(ExportWorldText(original, TestImportContext(), text, issue)) << issue.field << ": " << issue.message;
    if (textOut) { *textOut = text; }
    return ImportWorldText(text, TestImportContext(), imported);
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// AC6: speed
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonPerf, AC6_SmallWorldExportAndImportTakeWellUnderThreeSeconds)
{
    TestSession s;
    Generate(s, "", 1);
    const WorldStructure& w = s.World();
    ASSERT_EQ(w.CellCount(), 66 * 42);
    ASSERT_GT(w.sites.Count(), 50);

    std::string text;
    JsonIssue issue;
    const auto t0 = Clock::now();
    ASSERT_TRUE(ExportWorldText(w, TestImportContext(), text, issue));
    const double exportMs = MsSince(t0);
    WorldStructure imported;
    const auto t1 = Clock::now();
    const WorldImportResult r = ImportWorldText(text, TestImportContext(), imported);
    const double importMs = MsSince(t1);
    ASSERT_TRUE(r.ok);
    std::cout << "[perf] Small " << w.CellCount() << " cells, " << w.sites.Count() << " sites, " << text.size()
              << " bytes: export " << exportMs << " ms, import " << importMs << " ms (Debug build)\n";
    RecordProperty("export_ms", static_cast<int>(exportMs));
    RecordProperty("import_ms", static_cast<int>(importMs));
    EXPECT_LT(exportMs, 3000.0);
    EXPECT_LT(importMs, 3000.0);
}

TEST(WorldJsonPerf, FileRoundTripOfASmallWorldIsFast)
{
    TestSession s;
    Generate(s, "", 2);
    const fs::path dir = fs::temp_directory_path() / "cow_json_perf_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    s.SetWorldDirectories("", dir.string());
    const auto t0 = Clock::now();
    ASSERT_TRUE(s.ExportWorld(Obj())["success"].asBool());
    TestSession b;
    b.SetWorldDirectories("", dir.string());
    Json::Value im = Obj(); im["name"] = DefaultWorldName(2);
    ASSERT_TRUE(b.ImportWorld(im)["success"].asBool());
    const double ms = MsSince(t0);
    std::cout << "[perf] session export + import through a file: " << ms << " ms (Debug build)\n";
    EXPECT_LT(ms, 3000.0);
    fs::remove_all(dir, ec);
}

// ---------------------------------------------------------------------------------------------------------------------
// AC7 / limits
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonLimits, AC7_TheDefaultOutDirectoryIsUnderCluicheOutCoW)
{
    EXPECT_STREQ(CoW::WorldGenerationModule::kDefaultOutWorldsDir, "Cluiche/out/CoW/worlds");
    CoW::WorldGenerationModule module{ StringCRC("WorldGenerationModule") };
    module.SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText()); module.LoadRulesSidecar(); module.GetSession().SetBiomeRegistry(&::CoW::World::Testing::ShippedBiomeRegistry());
    EXPECT_EQ(module.GetOutWorldsDirectory(), "Cluiche/out/CoW/worlds");
    EXPECT_EQ(fs::path(WorldFilePath(module.GetOutWorldsDirectory(), DefaultWorldName(12345))).generic_string(),
              "Cluiche/out/CoW/worlds/12345.wld.json");
}

TEST(WorldJsonLimits, ExportRefusesTextLargerThanImportWouldAccept)
{
    // 256 x 256 cells with two kinds on every cell: far over 16 MiB.
    WorldStructure w;
    w.Resize(GridDimensions{ 256, 256 });
    // Insert in (kindCrc, cell) order so the sorted vector appends.
    const uint32_t a = StringCRC("Zz.Kind.A").Value(), b = StringCRC("Zz.Kind.B").Value();
    const uint32_t first = a < b ? a : b, second = a < b ? b : a;
    for (uint32_t kind : { first, second })
    {
        for (uint32_t cell = 0; cell < 65536; ++cell)
        {
            Site s; s.kindCrc = kind; s.cell = cell;
            ASSERT_TRUE(w.sites.Add(s));
        }
    }
    std::string text = "stale";
    JsonIssue issue;
    EXPECT_FALSE(ExportWorldText(w, TestImportContext(), text, issue));
    EXPECT_TRUE(text.empty());
    EXPECT_EQ(issue.field, "world");
    EXPECT_NE(issue.message.find("larger than"), std::string::npos);
}

TEST(SLOW_WorldJsonLimits, BiggestAllowedGridRoundTrips_TwoHundredFiftySixSquareOfOcean)
{
    WorldStructure w;
    w.Resize(GridDimensions{ 256, 256 });
    for (int i = 0; i < w.CellCount(); ++i)
    {
        w.Cells()[i] = MakeCell(B("Biome.Ocean"), TerrainType::DeepWater, kNoStratum, WaterSource::Ocean, 0.0f, 0.5f, 0, 0);
    }
    std::string text;
    WorldStructure imported;
    const auto t0 = Clock::now();
    const WorldImportResult r = RoundTrip(w, imported, &text);
    std::cout << "[perf] 256x256 ocean world: " << text.size() << " bytes, export + import " << MsSince(t0) << " ms (Debug build)\n";
    ASSERT_TRUE(r.ok) << r.error.field << ": " << r.error.message;
    EXPECT_LE(text.size(), kMaxWorldJsonBytes);
    EXPECT_TRUE(SameCells(w, imported));
}

TEST(WorldJsonLimits, KindNamesUpToSixtyThreeCharactersWork_SixtyFourDoNot)
{
    const std::string ok(63, 'k');
    WorldStructure w = MakeTinyWorld();
    Site s; s.kindCrc = StringCRC(ok.c_str()).Value(); s.cell = 3; s.region = 1;
    ASSERT_TRUE(w.sites.Add(s));
    ASSERT_TRUE(w.sites.RememberKindName(s.kindCrc, ok));
    WorldStructure imported;
    const WorldImportResult r = RoundTrip(w, imported);
    ASSERT_TRUE(r.ok) << r.error.message;
    EXPECT_TRUE(w.sites == imported.sites);
    EXPECT_STREQ(imported.sites.KindName(s.kindCrc), ok.c_str());

    std::string text;
    JsonIssue issue;
    ASSERT_TRUE(ExportWorldText(w, TestImportContext(), text, issue));
    const std::string longName(64, 'k');
    text.replace(text.find(ok), ok.size(), longName);
    EXPECT_FALSE(ImportWorldText(text, TestImportContext(), imported).ok);
}

// ---------------------------------------------------------------------------------------------------------------------
// Boundary worlds
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonEdge, OneByOneWorldRoundTrips)
{
    WorldStructure w;
    w.Resize(GridDimensions{ 1, 1 });
    w.Cells()[0] = MakeCell(B("Biome.Grassland"), TerrainType::Lowland, kStratumFirst, WaterSource::None, 0.4f, 0.3f, 1, 1);
    RegionRecord r; r.id = 1; r.zone = kStratumFirst; r.landmass = 1;
    w.regions.AddRegion(r);
    RecomputeRegionRecords(w, ::CoW::World::Testing::ShippedBiomeRegistry());
    Site s; s.kindCrc = SiteKindId(BuiltinNodeKinds::Hunting()); s.cell = 0; s.region = 1;
    w.sites.Add(s);
    WorldStructure imported;
    const WorldImportResult res = RoundTrip(w, imported);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    EXPECT_TRUE(SameWorld(w, imported));
}

TEST(WorldJsonEdge, ExactlyThePermittedRegionCountRoundTripsAndOneMoreIsRefused)
{
    WorldStructure w;
    w.Resize(GridDimensions{ 64, 1 });
    for (int i = 0; i < 64; ++i)
    {
        w.Cells()[i] = MakeCell(B("Biome.Grassland"), TerrainType::Lowland, kStratumWild, WaterSource::None, 0.4f, 0.3f,
                                static_cast<RegionId>(i + 1), 1);
        RegionRecord r; r.id = static_cast<RegionId>(i + 1); r.zone = kStratumWild; r.landmass = 1;
        ASSERT_TRUE(w.regions.AddRegion(r));
    }
    RecomputeRegionRecords(w, ::CoW::World::Testing::ShippedBiomeRegistry());
    EXPECT_FALSE(w.regions.AddRegion(RegionRecord{})) << "the table itself is full at 64";
    WorldStructure imported;
    const WorldImportResult res = RoundTrip(w, imported);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    EXPECT_TRUE(w.regions == imported.regions);
    EXPECT_EQ(imported.regions.regions.Size(), 64u);
}

TEST(WorldJsonEdge, RegionWithMoreThanSixteenNeighboursRoundTripsStably)
{
    // Row 1 is one long region; row 0 and row 2 hold 2-cell regions, so region 1 touches 40 others (the list keeps 16).
    WorldStructure w;
    w.Resize(GridDimensions{ 40, 3 });
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 40; ++x)
        {
            RegionId region = 1;
            if (y != 1) { region = static_cast<RegionId>(2 + (x / 2) + (y == 2 ? 20 : 0)); }
            w.Cells()[y * 40 + x] = MakeCell(B("Biome.Grassland"), TerrainType::Lowland, kStratumWild, WaterSource::None, 0.4f, 0.3f, region, 1);
        }
    }
    for (unsigned id = 1; id <= 41; ++id)
    {
        RegionRecord r; r.id = static_cast<RegionId>(id); r.zone = kStratumWild; r.landmass = 1;
        ASSERT_TRUE(w.regions.AddRegion(r));
    }
    const bool overflow = RecomputeRegionRecords(w, ::CoW::World::Testing::ShippedBiomeRegistry());
    EXPECT_TRUE(overflow);
    EXPECT_EQ(w.regions.regions[0].neighbours.Size(), static_cast<unsigned>(RegionRecord::kMaxNeighbours));
    WorldStructure imported;
    const WorldImportResult res = RoundTrip(w, imported);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    EXPECT_TRUE(SameWorld(w, imported));
    for (const std::string& warning : res.warnings) { EXPECT_EQ(warning.find("neighbours"), std::string::npos) << warning; }
}

TEST(WorldJsonEdge, SitesAtTheFirstAndLastCellAndManyOfThem)
{
    WorldStructure w = MakeTinyWorld();
    for (uint32_t cell : { 0u, 11u })
    {
        Site s; s.kindCrc = SiteKindId(BuiltinNodeKinds::Resource()); s.cell = cell; s.region = w.Cells()[cell].region;
        w.sites.Add(s);   // cell 3 already holds one; 0 and 11 are new
    }
    WorldStructure imported;
    ASSERT_TRUE(RoundTrip(w, imported).ok);
    EXPECT_TRUE(w.sites == imported.sites);
    EXPECT_NE(imported.sites.Find(BuiltinNodeKinds::Resource(), 0), nullptr);
    EXPECT_NE(imported.sites.Find(BuiltinNodeKinds::Resource(), 11), nullptr);

    // 5,000 sites of one kind on a 100 x 100 world.
    WorldStructure big;
    big.Resize(GridDimensions{ 100, 100 });
    for (int i = 0; i < big.CellCount(); ++i) { big.Cells()[i] = MakeCell(B("Biome.Ocean"), TerrainType::DeepWater, kNoStratum, WaterSource::Ocean, 0.0f, 0.5f, 0, 0); }
    for (uint32_t cell = 0; cell < 5000; ++cell)
    {
        Site s; s.kindCrc = SiteKindId(BuiltinNodeKinds::Hunting()); s.cell = cell * 2;
        ASSERT_TRUE(big.sites.Add(s));
    }
    WorldStructure bigImported;
    ASSERT_TRUE(RoundTrip(big, bigImported).ok);
    EXPECT_TRUE(big.sites == bigImported.sites);
    EXPECT_EQ(bigImported.sites.Count(), 5000);
}

TEST(WorldJsonEdge, ZeroVariantAndMaximumVariantAndFlagsSurvive)
{
    WorldStructure w = MakeTinyWorld();
    Site lo; lo.kindCrc = StringCRC("Edge.Kind").Value(); lo.cell = 1; lo.variant = 0; lo.flags = 0;
    Site hi; hi.kindCrc = StringCRC("Edge.Kind").Value(); hi.cell = 2; hi.region = 1; hi.variant = 65535; hi.flags = 65535;
    w.sites.Add(lo); w.sites.Add(hi);
    w.sites.RememberKindName(lo.kindCrc, "Edge.Kind");
    WorldStructure imported;
    ASSERT_TRUE(RoundTrip(w, imported).ok);
    EXPECT_TRUE(w.sites == imported.sites);
    EXPECT_EQ(imported.sites.Find(StringCRC("Edge.Kind"), 2)->flags, 65535);
}

TEST(WorldJsonEdge, ImportedWorldCanBeImportedAgainAndAgain)
{
    TestSession s;
    Generate(s, "", 4);
    WorldStructure current = s.World();
    std::string firstText;
    for (int round = 0; round < 3; ++round)
    {
        WorldStructure next;
        std::string text;
        ASSERT_TRUE(RoundTrip(current, next, &text).ok) << round;
        if (round == 0) { firstText = text; }
        EXPECT_EQ(text, firstText) << "round " << round;
        EXPECT_TRUE(SameWorld(current, next)) << round;
        current = std::move(next);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Sweeps (SLOW_): every shipped preset, 20 seeds; Large worlds
// ---------------------------------------------------------------------------------------------------------------------

TEST(SLOW_WorldJsonSweep, EveryShippedPresetTwentySeedsRoundTripsByteForByte)
{
    ASSERT_FALSE(FindShippedPresetDir().empty());
    int worlds = 0;
    for (const char* preset : { "continental", "archipelago", "highlands" })
    {
        for (uint64_t seed = 1; seed <= 20; ++seed)
        {
            TestSession s;
            Generate(s, preset, seed);
            const WorldStructure& original = s.World();
            std::string text;
            WorldStructure imported;
            const WorldImportResult r = RoundTrip(original, imported, &text);
            ASSERT_TRUE(r.ok) << preset << " " << seed << ": " << r.error.field << ": " << r.error.message;
            EXPECT_TRUE(r.warnings.empty()) << preset << " " << seed << ": " << r.warnings.front();
            ASSERT_TRUE(SameCells(original, imported)) << preset << " " << seed;
            EXPECT_TRUE(original.regions == imported.regions) << preset << " " << seed;
            EXPECT_TRUE(original.sites == imported.sites) << preset << " " << seed;
            std::string again; JsonIssue issue;
            ASSERT_TRUE(ExportWorldText(imported, TestImportContext(), again, issue));
            EXPECT_EQ(text, again) << preset << " " << seed;
            ++worlds;
        }
    }
    EXPECT_EQ(worlds, 60);
}

TEST(SLOW_WorldJsonSweep, ProvenanceRegeneratesTheExactWorldForEveryPreset)
{
    ASSERT_FALSE(FindShippedPresetDir().empty());
    const fs::path dir = fs::temp_directory_path() / "cow_json_sweep_provenance";
    std::error_code ec;
    fs::remove_all(dir, ec);
    for (const char* preset : { "continental", "archipelago", "highlands" })
    {
        for (uint64_t seed : { 3ull, 11ull, 17ull })
        {
            TestSession a;
            a.SetWorldDirectories("", dir.string());
            Generate(a, preset, seed);
            Json::Value ex = Obj(); ex["name"] = "w"; ex["overwrite"] = true;
            ASSERT_TRUE(a.ExportWorld(ex)["success"].asBool());

            TestSession b;
            b.SetWorldDirectories("", dir.string());
            Json::Value im = Obj(); im["name"] = "w";
            ASSERT_TRUE(b.ImportWorld(im)["success"].asBool());
            EXPECT_EQ(b.PresetId(), preset);
            Json::Value regen = Obj(); regen["unlock"] = true;
            ASSERT_TRUE(b.Regenerate(regen)["success"].asBool());
            Dia::Maths::RandomStream rng{ 1 };
            ASSERT_TRUE(b.Tick(rng));
            EXPECT_TRUE(SameWorld(a.World(), b.World())) << preset << " " << seed;
        }
    }
    fs::remove_all(dir, ec);
}

TEST(SLOW_WorldJsonSweep, LargeWorldsRoundTripWithinTheBudget)
{
    for (uint64_t seed : { 1ull, 2ull })
    {
        TestSession s;
        Generate(s, "", seed, "Large");
        const WorldStructure& original = s.World();
        ASSERT_EQ(original.dimensions.width, 104);
        ASSERT_EQ(original.dimensions.height, 64);
        std::string text; JsonIssue issue;
        const auto t0 = Clock::now();
        ASSERT_TRUE(ExportWorldText(original, TestImportContext(), text, issue));
        const double exportMs = MsSince(t0);
        WorldStructure imported;
        const auto t1 = Clock::now();
        const WorldImportResult r = ImportWorldText(text, TestImportContext(), imported);
        const double importMs = MsSince(t1);
        ASSERT_TRUE(r.ok) << r.error.field << ": " << r.error.message;
        std::cout << "[perf] Large " << original.CellCount() << " cells, " << text.size() << " bytes: export " << exportMs
                  << " ms, import " << importMs << " ms (Debug build)\n";
        EXPECT_TRUE(SameWorld(original, imported));
        EXPECT_LT(exportMs, 3000.0);
        EXPECT_LT(importMs, 3000.0);
    }
}
