// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC2, AC4, SD-S1-009),
//       biome-classification.md (AC14), regions-and-sites.md (AC11 session side, AC14), worldgenerationpipeline.md (Test 3)
// WorldGenSession export/import: files, the locked-import contract, regeneration from provenance.
#include <gtest/gtest.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>
#include <DraoiHexWorldTerrain/WorldFiles.h>
#include <DraoiHexWorldTerrain/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace
{

Json::Value Obj() { return Json::Value(Json::objectValue); }

Json::Value Args(const char* json)
{
    Json::Value v;
    EXPECT_TRUE(Json::Reader().parse(json, v)) << json;
    return v;
}

class SessionImportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{ 0 };
        mRoot = fs::temp_directory_path() / ("cow_sessionimport_test_" + std::to_string(counter.fetch_add(1)) + "_" +
                                              std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFFFF));
        std::error_code ec;
        fs::remove_all(mRoot, ec);
        fs::create_directories(mRoot, ec);
        mOut = (mRoot / "worlds").string();
    }
    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(mRoot, ec);
    }

    // A session that has run one Regenerate (regions and sites present), with the worlds directory configured.
    void Generate(WorldGenSession& s, uint64_t seed, const char* shapeJson = "{}", const SiteRegistry* registry = nullptr)
    {
        s.SetWorldDirectories("", mOut);
        if (registry) { s.SetSiteRegistry(registry); }
        const Json::Value shape = Args(shapeJson);
        if (!shape.getMemberNames().empty()) { s.Set(shape); }
        Json::Value regen = Obj();
        regen["seed"] = static_cast<Json::UInt64>(seed);
        ASSERT_TRUE(s.Regenerate(regen)["success"].asBool());
        ASSERT_TRUE(s.Tick(mRng));
        ASSERT_TRUE(s.RegionsFresh());
    }

    Json::Value ExportOk(const WorldGenSession& s, const char* name)
    {
        Json::Value a = Obj(); a["name"] = name;
        const Json::Value r = s.ExportWorld(a);
        EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
        return r;
    }

    Json::Value ImportName(WorldGenSession& s, const char* name)
    {
        Json::Value a = Obj(); a["name"] = name;
        return s.ImportWorld(a);
    }

    std::string ReadFile(const std::string& name) const
    {
        std::ifstream f(WorldFilePath(mOut, name), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    void WriteFile(const std::string& name, const std::string& text) const
    {
        ASSERT_TRUE(WriteWorldFile(mOut, name, text, true).Ok());
    }

    fs::path mRoot;
    std::string mOut;
    Dia::Maths::RandomStream mRng{ 1 };
};

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(SessionImportTest, AC7_ExportDefaultsToTheSeedAsTheFileName)
{
    TestSession s;
    Generate(s, 424242);
    const Json::Value r = s.ExportWorld(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["name"].asString(), "424242");
    EXPECT_EQ(fs::path(r["path"].asString()).filename().string(), "424242.wld.json");
    EXPECT_EQ(fs::path(r["path"].asString()).parent_path().string(), fs::path(mOut).string());
    EXPECT_GT(r["bytes"].asUInt64(), 1000u);
    EXPECT_EQ(r["revision"].asUInt(), s.Revision());
    EXPECT_TRUE(fs::exists(WorldFilePath(mOut, "424242")));
    EXPECT_EQ(r["warnings"].size(), 0u);
}

TEST_F(SessionImportTest, ExportAcceptsNullArgsAndANamedFile)
{
    TestSession s;
    Generate(s, 5);
    const Json::Value nullArgs = s.ExportWorld(Json::Value());   // null args = all defaults
    EXPECT_TRUE(nullArgs["success"].asBool()) << nullArgs.toStyledString();
    EXPECT_EQ(nullArgs["name"].asString(), "5");
    ExportOk(s, "hand_made");
    EXPECT_TRUE(fs::exists(WorldFilePath(mOut, "hand_made")));
}

TEST_F(SessionImportTest, ExportRefusesBadArgumentsAndExistingFilesUnlessOverwrite)
{
    TestSession s;
    Generate(s, 5);
    EXPECT_FALSE(s.ExportWorld(Json::Value(7))["success"].asBool());
    Json::Value a = Obj(); a["name"] = 12;
    EXPECT_FALSE(s.ExportWorld(a)["success"].asBool());
    for (const char* bad : { "../x", "a/b", "", "NUL", "has space", "dot.dot" })
    {
        Json::Value b = Obj(); b["name"] = bad;
        const Json::Value r = s.ExportWorld(b);
        EXPECT_FALSE(r["success"].asBool()) << bad;
        EXPECT_EQ(r["status"].asString(), "InvalidName") << bad;
    }
    ExportOk(s, "twice");
    const std::string first = ReadFile("twice");
    Json::Value again = Obj(); again["name"] = "twice";
    const Json::Value refused = s.ExportWorld(again);
    EXPECT_FALSE(refused["success"].asBool());
    EXPECT_EQ(refused["status"].asString(), "AlreadyExists");
    EXPECT_EQ(ReadFile("twice"), first);
    again["overwrite"] = true;
    EXPECT_TRUE(s.ExportWorld(again)["success"].asBool());
}

TEST_F(SessionImportTest, ExportNeedsAGeneratedWorldAndAConfiguredDirectory)
{
    TestSession fresh;
    fresh.SetWorldDirectories("", mOut);
    EXPECT_FALSE(fresh.ExportWorld(Obj())["success"].asBool());   // nothing generated yet

    TestSession s;
    ASSERT_TRUE(s.Tick(mRng));   // generated, but no directory configured
    const Json::Value r = s.ExportWorld(Obj());
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["status"].asString(), "NoDirectory");
}

TEST_F(SessionImportTest, ExportOfATerrainOnlyWorldWarnsAboutMissingRegions)
{
    TestSession s;
    s.SetWorldDirectories("", mOut);
    ASSERT_TRUE(s.Tick(mRng));   // first generation: no regions
    const Json::Value r = s.ExportWorld(Obj());
    ASSERT_TRUE(r["success"].asBool());
    ASSERT_EQ(r["warnings"].size(), 1u);
    EXPECT_NE(r["warnings"][0].asString().find("no regions"), std::string::npos);
}

TEST_F(SessionImportTest, ExportRecordsTheSessionsPresetIdInProvenance)
{
    TestSession s;
    Generate(s, 9);
    // The session's preset id is session state; a session whose params were set from a preset exports that id.
    const fs::path presets = mRoot / "presets";
    s.SetPresetDirectories("", presets.string());
    Json::Value save = Obj(); save["id"] = "mypreset"; save["displayName"] = "My";
    ASSERT_TRUE(s.PresetSave(save)["success"].asBool());
    Json::Value load = Obj(); load["id"] = "mypreset";
    ASSERT_TRUE(s.PresetLoad(load)["success"].asBool());
    ExportOk(s, "withpreset");
    EXPECT_NE(ReadFile("withpreset").find("\"preset_id\": \"mypreset\""), std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------
// Import: round trip, adoption, lock
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(SessionImportTest, Test3_ExportThenImportIntoAnotherSessionIsExact)
{
    TestSession a;
    Generate(a, 77, "{\"islands\":5,\"rivers\":0.8}");
    ExportOk(a, "w");

    TestSession b;
    b.SetWorldDirectories("", mOut);
    const uint32_t before = b.Revision();
    const Json::Value r = ImportName(b, "w");
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["locked"].asBool());
    EXPECT_EQ(r["warnings"].size(), 0u);
    EXPECT_TRUE(SameWorld(a.World(), b.World()));
    EXPECT_EQ(b.Revision(), before + 1);
    EXPECT_EQ(b.State(), WorldGenState::Complete);
    EXPECT_TRUE(b.RegionsFresh());
    EXPECT_FALSE(b.IsDirty());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_TRUE(b.IsImported());
    // The session adopts the recipe.
    EXPECT_EQ(b.Seed(), 77u);
    EXPECT_TRUE(b.Shape() == a.Shape());
    EXPECT_EQ(b.Shape().islands, 5);
    EXPECT_EQ(b.Dimensions().width, a.Dimensions().width);
    // And re-exports byte-identical text.
    ExportOk(b, "w2");
    EXPECT_EQ(ReadFile("w2"), ReadFile("w"));
}

TEST_F(SessionImportTest, ImportReportsStatusParamsAndALayoutCheck)
{
    TestSession a;
    Generate(a, 3);
    ExportOk(a, "w");
    TestSession b;
    b.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(b, "w")["success"].asBool());

    const Json::Value status = b.Status();
    EXPECT_TRUE(status["locked"].asBool());
    EXPECT_EQ(status["source"].asString(), "imported");
    EXPECT_EQ(status["state"].asString(), "Complete");
    EXPECT_TRUE(status["regionsFresh"].asBool());
    EXPECT_TRUE(status["layout"].isObject());
    const Json::Value params = b.Params();
    EXPECT_TRUE(params["locked"].asBool());
    EXPECT_EQ(params["source"].asString(), "imported");
    EXPECT_TRUE(params["effective"]["imported"].asBool());
    EXPECT_EQ(params["seed"].asUInt64(), 3u);
    // A freshly generated session reports the opposite.
    EXPECT_FALSE(a.Status()["locked"].asBool());
    EXPECT_EQ(a.Status()["source"].asString(), "generated");
    EXPECT_FALSE(a.Params()["effective"]["imported"].asBool());
}

TEST_F(SessionImportTest, ImportAdoptsThePresetIdFromTheFile)
{
    TestSession a;
    Generate(a, 9);
    a.SetPresetDirectories("", (mRoot / "presets").string());
    Json::Value save = Obj(); save["id"] = "mypreset"; save["displayName"] = "My";
    a.PresetSave(save);
    Json::Value load = Obj(); load["id"] = "mypreset";
    a.PresetLoad(load);
    ExportOk(a, "p");
    TestSession b;
    b.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(b, "p")["success"].asBool());
    EXPECT_EQ(b.PresetId(), "mypreset");
    EXPECT_FALSE(b.PresetModified());
    EXPECT_EQ(b.Params()["preset"].asString(), "mypreset");
}

TEST_F(SessionImportTest, FailedImportsChangeNothing)
{
    TestSession s;
    Generate(s, 11);
    const WorldStructure before = s.World();
    const Json::Value statusBefore = s.Status();
    const Json::Value paramsBefore = s.Params();

    auto expectUntouched = [&](const Json::Value& reply, const char* what)
    {
        EXPECT_FALSE(reply["success"].asBool()) << what;
        EXPECT_FALSE(reply["error"].asString().empty()) << what;
        EXPECT_TRUE(SameWorld(s.World(), before)) << what;
        EXPECT_EQ(s.Status(), statusBefore) << what;
        EXPECT_EQ(s.Params(), paramsBefore) << what;
        EXPECT_FALSE(s.IsLocked()) << what;
        EXPECT_FALSE(s.IsImported()) << what;
    };

    expectUntouched(ImportName(s, "missing"), "missing file");
    expectUntouched(s.ImportWorld(Obj()), "no name");
    Json::Value numeric = Obj(); numeric["name"] = 5;
    expectUntouched(s.ImportWorld(numeric), "numeric name");
    expectUntouched(ImportName(s, "../escape"), "traversal");
    WriteFile("garbage", "this is not json");
    expectUntouched(ImportName(s, "garbage"), "garbage");
    WriteFile("empty", "");
    expectUntouched(ImportName(s, "empty"), "empty file");

    ExportOk(s, "good");
    std::string text = ReadFile("good");
    std::string v2 = text;
    v2.replace(v2.find("\"version\": 2"), 12, "\"version\": 3");
    WriteFile("v2", v2);
    const Json::Value rv2 = ImportName(s, "v2");
    expectUntouched(rv2, "version 3");
    EXPECT_EQ(rv2["field"].asString(), "version");
    EXPECT_NE(rv2["error"].asString().find("Unsupported cow.world version: 3"), std::string::npos);

    std::string cut = text.substr(0, text.size() / 2);
    WriteFile("cut", cut);
    expectUntouched(ImportName(s, "cut"), "truncated");
}

TEST_F(SessionImportTest, ImportReplacesAPendingChangeAndTickDoesNotRegenerate)
{
    TestSession a;
    Generate(a, 21);
    ExportOk(a, "w");
    TestSession b;
    Generate(b, 22);
    Json::Value pending = Obj(); pending["rivers"] = 0.9;
    b.Set(pending);   // dirty: a live-preview regeneration is waiting
    ASSERT_TRUE(b.IsDirty());
    ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
    EXPECT_FALSE(b.IsDirty());
    EXPECT_FALSE(b.Tick(mRng)) << "an imported world must not be regenerated away by a pending Tick";
    EXPECT_TRUE(SameWorld(a.World(), b.World()));
    EXPECT_EQ(b.Seed(), 21u);
    EXPECT_FLOAT_EQ(b.Shape().rivers, a.Shape().rivers);
}

TEST_F(SessionImportTest, SD_S1_009_EveryActionIsRefusedWhileLockedUnlessItCarriesUnlock)
{
    TestSession a;
    Generate(a, 31);
    ExportOk(a, "w");
    TestSession b;
    b.SetWorldDirectories("", mOut);
    b.SetPresetDirectories("", (mRoot / "presets").string());
    Json::Value save = Obj(); save["id"] = "other"; save["displayName"] = "Other";
    ASSERT_TRUE(b.PresetSave(save)["success"].asBool());
    ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
    const WorldStructure imported = b.World();

    auto expectLocked = [&](const Json::Value& reply, const char* what)
    {
        EXPECT_FALSE(reply["success"].asBool()) << what;
        EXPECT_EQ(reply["error"].asString(), "locked") << what;
        EXPECT_TRUE(reply["locked"].asBool()) << what;
        EXPECT_TRUE(reply["needsConfirm"].asBool()) << what;
        EXPECT_FALSE(reply["message"].asString().empty()) << what;
        EXPECT_TRUE(b.IsLocked()) << what;
        EXPECT_FALSE(b.IsDirty()) << what;
        EXPECT_FALSE(b.Tick(mRng)) << what;
        EXPECT_TRUE(SameWorld(b.World(), imported)) << what;
    };

    expectLocked(b.Set(Args("{\"rivers\":0.1}")), "Set slider");
    expectLocked(b.Set(Args("{\"seed\":99}")), "Set seed");
    expectLocked(b.Set(Obj()), "Set nothing");
    expectLocked(b.SetSize(Args("{\"size\":\"Large\"}")), "SetSize");
    expectLocked(b.PresetLoad(Args("{\"id\":\"other\"}")), "PresetLoad");
    expectLocked(b.Regenerate(Obj()), "Regenerate");
    expectLocked(b.Regenerate(Args("{\"seed\":5}")), "Regenerate seed");
    expectLocked(b.Set(Args("{\"unlock\":false,\"rivers\":0.1}")), "unlock false");
    expectLocked(b.Set(Args("{\"unlock\":\"yes\",\"rivers\":0.1}")), "unlock not a bool");
    expectLocked(b.Set(Args("{\"unlock\":1}")), "unlock numeric");
    EXPECT_EQ(b.Seed(), 31u);   // nothing leaked through

    // Queries and PresetSave are not actions on the world: allowed while locked.
    EXPECT_TRUE(b.Params()["success"].isNull() || b.Params()["params"].isObject());
    EXPECT_TRUE(b.Regions()["fresh"].asBool());
    EXPECT_TRUE(b.Sites(Obj())["success"].asBool());
    Json::Value save2 = Obj(); save2["id"] = "another"; save2["displayName"] = "Another";
    EXPECT_TRUE(b.PresetSave(save2)["success"].asBool());
    ExportOk(b, "while_locked");
    EXPECT_TRUE(b.IsLocked());
}

TEST_F(SessionImportTest, UnlockingAppliesTheActionDropsTheEditsAndRegenerates)
{
    TestSession a;
    Generate(a, 41);
    ExportOk(a, "w");

    // Set with unlock.
    {
        TestSession b; b.SetWorldDirectories("", mOut);
        ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
        const Json::Value r = b.Set(Args("{\"unlock\":true,\"rivers\":0.2}"));
        ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
        EXPECT_FALSE(b.IsLocked());
        EXPECT_TRUE(b.IsDirty());
        EXPECT_FLOAT_EQ(b.Shape().rivers, 0.2f);
        ASSERT_TRUE(b.Tick(mRng));
        EXPECT_FALSE(b.IsImported());
        EXPECT_EQ(b.Status()["source"].asString(), "generated");
        EXPECT_FALSE(b.Status()["locked"].asBool());
        EXPECT_FALSE(b.Params()["effective"]["imported"].asBool());
    }
    // Set with only unlock: the world is regenerated from the recorded recipe even though no slider changed.
    {
        TestSession b; b.SetWorldDirectories("", mOut);
        ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
        ASSERT_TRUE(b.Set(Args("{\"unlock\":true}"))["success"].asBool());
        EXPECT_TRUE(b.IsDirty());
        ASSERT_TRUE(b.Tick(mRng));
        EXPECT_FALSE(b.IsImported());
        // Live preview from the recorded recipe reproduces the terrain and biomes; regions (placed on Regenerate only) are gone.
        ASSERT_EQ(a.World().CellCount(), b.World().CellCount());
        for (int i = 0; i < a.World().CellCount(); ++i)
        {
            Cell expected = a.World().Cells()[i];
            expected.region = kNoRegion;
            ASSERT_EQ(0, std::memcmp(&expected, &b.World().Cells()[i], sizeof(Cell))) << "cell " << i;
        }
        EXPECT_EQ(b.World().regions.regions.Size(), 0u) << "live preview leaves regions empty (SD-S1-004)";
    }
    // SetSize with unlock.
    {
        TestSession b; b.SetWorldDirectories("", mOut);
        ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
        ASSERT_TRUE(b.SetSize(Args("{\"size\":\"Standard\",\"unlock\":true}"))["success"].asBool());
        EXPECT_FALSE(b.IsLocked());
        ASSERT_TRUE(b.Tick(mRng));
        EXPECT_EQ(b.World().dimensions.width, 80);
    }
    // PresetLoad with unlock.
    {
        TestSession b; b.SetWorldDirectories("", mOut);
        b.SetPresetDirectories("", (mRoot / "presets").string());
        Json::Value save = Obj(); save["id"] = "other"; save["displayName"] = "Other";
        ASSERT_TRUE(b.PresetSave(save)["success"].asBool());
        ASSERT_TRUE(ImportName(b, "w")["success"].asBool());
        ASSERT_TRUE(b.PresetLoad(Args("{\"id\":\"other\",\"unlock\":true}"))["success"].asBool());
        EXPECT_FALSE(b.IsLocked());
        EXPECT_TRUE(b.IsDirty());
    }
}

TEST_F(SessionImportTest, AnActionThatFailsValidationDoesNotUnlock)
{
    TestSession a;
    Generate(a, 51);
    ExportOk(a, "w");
    TestSession b;
    b.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(b, "w")["success"].asBool());

    EXPECT_FALSE(b.Regenerate(Args("{\"seed\":\"abc\",\"unlock\":true}"))["success"].asBool());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_FALSE(b.SetSize(Args("{\"size\":\"Huge\",\"unlock\":true}"))["success"].asBool());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_FALSE(b.PresetLoad(Args("{\"id\":\"nope\",\"unlock\":true}"))["success"].asBool());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_FALSE(b.Set(Json::Value(5))["success"].asBool());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_FALSE(b.IsDirty());
}

TEST_F(SessionImportTest, Biome_AC14_ImportedBiomesAreNeverReclassifiedAndProvenanceReproducesThem)
{
    TestSession a;
    Generate(a, 61, "{\"temperatureOffset\":0.2,\"moistureOffset\":-0.15,\"mountainousness\":0.8}");
    ExportOk(a, "w");

    // Hand-edit one biome to something no rule would pick, then import.
    std::string text = ReadFile("w");
    const size_t at = text.find("\"biome\": \"Biome.Grassland\"");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::string("\"biome\": \"Biome.Grassland\"").size(), "\"biome\": \"Biome.Desert\"");
    WriteFile("edited", text);

    TestSession b;
    b.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(b, "edited")["success"].asBool());
    int changed = 0;
    for (int i = 0; i < a.World().CellCount(); ++i)
    {
        changed += a.World().Cells()[i].biome != b.World().Cells()[i].biome;
    }
    EXPECT_EQ(changed, 1) << "exactly the edited cell differs; nothing reclassified";
    for (int t = 0; t < 3; ++t) { EXPECT_FALSE(b.Tick(mRng)); }
    EXPECT_EQ(changed, 1);

    // set without unlock fails with "locked".
    const Json::Value refused = b.Set(Args("{\"temperatureOffset\":0.1}"));
    EXPECT_FALSE(refused["success"].asBool());
    EXPECT_EQ(refused["error"].asString(), "locked");

    // Regenerating from the exported provenance reproduces the original biome array byte for byte, climate offsets included.
    TestSession c;
    c.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(c, "w")["success"].asBool());
    EXPECT_FLOAT_EQ(c.Shape().temperatureOffset, 0.2f);
    EXPECT_FLOAT_EQ(c.Shape().moistureOffset, -0.15f);
    ASSERT_TRUE(c.Regenerate(Args("{\"unlock\":true}"))["success"].asBool());
    ASSERT_TRUE(c.Tick(mRng));
    ASSERT_EQ(c.World().CellCount(), a.World().CellCount());
    for (int i = 0; i < a.World().CellCount(); ++i)
    {
        ASSERT_EQ(c.World().Cells()[i].biome, a.World().Cells()[i].biome) << i;
    }
    EXPECT_EQ(0, std::memcmp(c.World().Cells(), a.World().Cells(), sizeof(Cell) * static_cast<size_t>(a.World().CellCount())));
}

TEST_F(SessionImportTest, Regions_AC14_ImportedRegionsAndSitesAreKeptAndProvenanceReproducesThem)
{
    TestSession a;
    Generate(a, 71);
    ExportOk(a, "w");

    // Hand-edit: drop one Hunting site; nothing recomputes the rest.
    TestSession probe;
    probe.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(probe, "w")["success"].asBool());
    const auto huntingBefore = probe.World().sites.Of(BuiltinNodeKinds::Hunting());
    ASSERT_GT(huntingBefore.size(), 2u);

    WorldStructure edited = a.World();
    SiteSet rebuilt;
    int skipped = 0;
    for (const Site& s : edited.sites.All())
    {
        if (s.kindCrc == SiteKindId(BuiltinNodeKinds::Hunting()) && skipped == 0) { ++skipped; continue; }   // dropped
        rebuilt.Add(s);
    }
    edited.sites = rebuilt;
    std::string editedText; JsonIssue issue;
    WorldJsonContext ctx = TestImportContext(); ctx.registry = &a.Registry();
    ASSERT_TRUE(ExportWorldText(edited, ctx, editedText, issue));
    WriteFile("edited", editedText);

    TestSession b;
    b.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(b, "edited")["success"].asBool());
    EXPECT_TRUE(b.World().regions == a.World().regions) << "stored regions kept";
    EXPECT_EQ(b.World().sites.Count(), a.World().sites.Count() - 1) << "stored sites kept exactly, nothing re-placed";
    EXPECT_TRUE(b.World().sites == edited.sites);
    EXPECT_EQ(b.Sites(Obj())["total"].asInt(), edited.sites.Count());

    // Regenerating from the exported provenance reproduces the original regions and sites byte for byte.
    TestSession c;
    c.SetWorldDirectories("", mOut);
    ASSERT_TRUE(ImportName(c, "w")["success"].asBool());
    ASSERT_TRUE(c.Regenerate(Args("{\"unlock\":true}"))["success"].asBool());
    ASSERT_TRUE(c.Tick(mRng));
    EXPECT_TRUE(SameWorld(a.World(), c.World()));
    EXPECT_TRUE(c.World().regions == a.World().regions);
    EXPECT_TRUE(c.World().sites == a.World().sites);
    EXPECT_TRUE(c.RegionsFresh());
}

TEST_F(SessionImportTest, AC4_HandEditedSiteImportsAtItsNewCellThroughTheSession)
{
    TestSession a;
    Generate(a, 81);
    ExportOk(a, "w");
    const auto hunting = a.World().sites.Of(BuiltinNodeKinds::Hunting());
    ASSERT_FALSE(hunting.empty());
    const Site moved = hunting[0];
    // Find a free cell of the same region.
    uint32_t target = 0;
    for (int i = 0; i < a.World().CellCount(); ++i)
    {
        if (a.World().Cells()[i].region == moved.region && !a.World().sites.Has(static_cast<uint32_t>(i))) { target = static_cast<uint32_t>(i); break; }
    }
    ASSERT_NE(target, 0u);
    std::string text = ReadFile("w");
    const std::string from = "{ \"cell\": " + std::to_string(moved.cell) + ", \"region\": " + std::to_string(moved.region);
    ASSERT_NE(text.find(from), std::string::npos);
    text.replace(text.find(from), from.size(), "{ \"cell\": " + std::to_string(target) + ", \"region\": " + std::to_string(moved.region));
    WriteFile("moved", text);

    TestSession b;
    b.SetWorldDirectories("", mOut);
    const Json::Value r = ImportName(b, "moved");
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_NE(b.World().sites.Find(BuiltinNodeKinds::Hunting(), target), nullptr);
    EXPECT_EQ(b.World().sites.Find(BuiltinNodeKinds::Hunting(), moved.cell), nullptr);
    EXPECT_EQ(b.World().sites.Count(), a.World().sites.Count());
    EXPECT_TRUE(SameCells(a.World(), b.World()));
    EXPECT_TRUE(b.IsLocked());
}

// ---------------------------------------------------------------------------------------------------------------------
// Imported worlds stay inspectable
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(SessionImportTest, ImportedWorldOfAnotherSizeKeepsTheQueriesWorking)
{
    WriteFile("tiny", [&] { std::string t; JsonIssue i; WorldJsonContext c = TestImportContext(); ExportWorldText(MakeTinyWorld(), c, t, i); return t; }());
    TestSession s;
    Generate(s, 91);   // a Small world first, so stale buffers of the wrong size could be read
    const Json::Value r = ImportName(s, "tiny");
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(s.Dimensions().width, 4);
    EXPECT_EQ(s.Dimensions().height, 3);
    EXPECT_EQ(s.Status()["size"].asString(), "Custom");
    EXPECT_EQ(s.Status()["dimensions"]["width"].asInt(), 4);

    Json::Value cell = Obj(); cell["index"] = 6;
    const Json::Value c = s.Cell(cell);
    EXPECT_TRUE(c["success"].asBool()) << c.toStyledString();
    EXPECT_EQ(c["region"].asUInt(), 2u);

    Json::Value layer = Obj(); layer["layer"] = "elevation";
    const Json::Value e = s.Layers(layer);
    ASSERT_TRUE(e["success"].asBool()) << e.toStyledString();
    EXPECT_EQ(e["total"].asInt(), 12);
    ASSERT_EQ(e["values"].size(), 12u);
    for (int i = 0; i < 12; ++i)
    {
        EXPECT_EQ(static_cast<float>(e["values"][i].asDouble()), s.World().Cells()[i].elevation) << i;
    }
    layer["layer"] = "biomeIndex";
    const Json::Value b = s.Layers(layer);
    ASSERT_TRUE(b["success"].asBool());
    for (int i = 0; i < 12; ++i) { EXPECT_EQ(b["values"][i].asUInt(), static_cast<unsigned>(s.World().Cells()[i].biome)) << i; }
    layer["layer"] = "flowAccumulation";   // cannot be rebuilt from cells: present, zero
    const Json::Value f = s.Layers(layer);
    ASSERT_TRUE(f["success"].asBool());
    for (const Json::Value& v : f["values"]) { EXPECT_EQ(v.asDouble(), 0.0); }
    const Json::Value summary = s.Layers(Obj());   // the stage summary with hashes also works (the module adds "success")
    EXPECT_TRUE(summary.isObject());
    EXPECT_TRUE(summary["stages"].isArray());
}

TEST_F(SessionImportTest, UnlockingAfterImportingADifferentSizeRegeneratesAtTheFilesSize)
{
    WriteFile("tiny", [&] { std::string t; JsonIssue i; WorldJsonContext c = TestImportContext(); ExportWorldText(MakeTinyWorld(), c, t, i); return t; }());
    TestSession s;
    Generate(s, 92);
    ASSERT_TRUE(ImportName(s, "tiny")["success"].asBool());
    ASSERT_TRUE(s.Regenerate(Args("{\"unlock\":true}"))["success"].asBool());
    ASSERT_TRUE(s.Tick(mRng));
    EXPECT_EQ(s.World().dimensions.width, 4) << "the recipe's size comes with the file";
    EXPECT_FALSE(s.IsLocked());
}

TEST_F(SessionImportTest, WorldWithoutRegionsImportsAsNotFresh)
{
    TestSession a;
    a.SetWorldDirectories("", mOut);
    ASSERT_TRUE(a.Tick(mRng));   // terrain only
    ExportOk(a, "terrain");
    TestSession b;
    b.SetWorldDirectories("", mOut);
    const Json::Value r = ImportName(b, "terrain");
    ASSERT_TRUE(r["success"].asBool());
    EXPECT_FALSE(r["regionsFresh"].asBool());
    EXPECT_FALSE(b.RegionsFresh());
    EXPECT_TRUE(b.Status()["layout"].isNull());
    EXPECT_TRUE(b.IsLocked());
    EXPECT_TRUE(SameWorld(a.World(), b.World()));
}

// ---------------------------------------------------------------------------------------------------------------------
// Site kinds through the session (Regions AC11)
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(SessionImportTest, AC11_RegisteredKindRoundTripsThroughTheSessionAndAnUnknownKindIsKept)
{
    TestPlacer placer("Test.Session", {}, 5);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&placer));

    TestSession a;
    Generate(a, 101, "{}", &registry);
    ASSERT_NE(a.World().sites.Find(StringCRC("Test.Session"), 5), nullptr);
    ExportOk(a, "withkind");
    EXPECT_NE(ReadFile("withkind").find("\"Test.Session\""), std::string::npos);

    // Same registry: imported exactly, no warning.
    {
        TestSession b;
        b.SetWorldDirectories("", mOut);
        b.SetSiteRegistry(&registry);
        const Json::Value r = ImportName(b, "withkind");
        ASSERT_TRUE(r["success"].asBool());
        EXPECT_EQ(r["warnings"].size(), 0u);
        EXPECT_TRUE(b.World().sites == a.World().sites);
        EXPECT_NE(b.World().sites.Find(StringCRC("Test.Session"), 5), nullptr);
    }
    // No registry: the kind is kept, warned about, and re-exports unchanged.
    {
        TestSession c;
        c.SetWorldDirectories("", mOut);
        const Json::Value r = ImportName(c, "withkind");
        ASSERT_TRUE(r["success"].asBool());
        bool warned = false;
        for (const Json::Value& w : r["warnings"]) { warned = warned || w.asString().find("Test.Session") != std::string::npos; }
        EXPECT_TRUE(warned);
        EXPECT_TRUE(c.World().sites == a.World().sites);
        ExportOk(c, "again");
        EXPECT_EQ(ReadFile("again"), ReadFile("withkind"));
        // The Sites query lists it by hex id when nobody registers it; it still lists.
        EXPECT_EQ(c.Sites(Obj())["total"].asInt(), a.World().sites.Count());
    }
}

TEST_F(SessionImportTest, DeployedDirectoryIsSearchedWhenTheOutDirectoryHasNoSuchWorld)
{
    const std::string deployed = (mRoot / "deployed").string();
    TestSession a;
    Generate(a, 111);
    ExportOk(a, "shipped");
    fs::create_directories(deployed);
    fs::copy_file(WorldFilePath(mOut, "shipped"), WorldFilePath(deployed, "shipped"));
    fs::remove(WorldFilePath(mOut, "shipped"));

    TestSession b;
    b.SetWorldDirectories(deployed, mOut);
    ASSERT_TRUE(ImportName(b, "shipped")["success"].asBool());
    EXPECT_TRUE(SameWorld(a.World(), b.World()));
}
