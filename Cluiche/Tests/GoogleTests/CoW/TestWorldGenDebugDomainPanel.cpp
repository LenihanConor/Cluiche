// CoW::WorldGenDebugDomain panel sections: validation, file, progress, per-cell absorbed flag and the
// validate / export / import / cancel / selectWorld / discard commands (S-1 worldgen-panel-rest plan task 2).
// Every command must change the session exactly as the matching cow.worldgen.* command does (SD-S1-002).
#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

#ifdef DIA_DEBUG

#include <Modules/WorldGen/WorldGenDebugDomain.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <DraoiHexWorldTerrain/WorldFiles.h>
#include <DiaCore/Json/external/json/json.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

using namespace CoW;
using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using Dia::Core::StringCRC;

namespace
{
    namespace fs = std::filesystem;

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

    struct PanelFixture
    {
        WorldGenerationModule module{ StringCRC("WorldGenerationModule") };
        bool                  rulesReady = [this] { module.SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText()); module.LoadRulesSidecar(); module.GetSession().SetBiomeRegistry(&::Draoi::HexWorld::Testing::ShippedBiomeRegistry()); return true; }();
        WorldGenDebugDomain   domain{ module };
        Dia::Maths::RandomStream rng{ 1 };
        fs::path dir;

        explicit PanelFixture(const char* tag)
        {
            dir = fs::temp_directory_path() / (std::string("cow_domain_panel_") + tag);
            std::error_code ec;
            fs::remove_all(dir, ec);
            module.GetSession().SetWorldDirectories("", dir.string());
        }
        ~PanelFixture()
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }

        WorldGenSession& S() { return module.GetSession(); }
        void Gen() { S().Tick(rng); }
        void Settle() { for (int i = 0; i < 80 && (S().IsDirty() || S().IsGenerating()); ++i) Gen(); }
        Json::Value State() { domain.ForceFullFrame(); return Frame(); }
        Json::Value Frame() { Json::Value s; domain.GetJSONState(s); return s; }
        void Cmd(const char* name, const Json::Value& args = Json::Value(Json::objectValue)) { domain.OnCommand(StringCRC(name), args); }
        // A successful Regenerate (regions and sites present) on the given seed.
        void Regen(int seed)
        {
            Cmd("regenerate", Obj("seed", seed));
            Settle();
        }
        void WriteWorldFile(const char* name, const std::string& text)
        {
            fs::create_directories(dir);
            std::ofstream out(dir / (std::string(name) + ".wld.json"), std::ios::binary);
            out << text;
        }
    };
}

TEST(WorldGenDebugDomainPanel, SectionsAreNullOrIdleBeforeAnyWorld)
{
    PanelFixture f("empty");
    const Json::Value st = f.State();
    EXPECT_TRUE(st["validation"].isNull());
    ASSERT_TRUE(st["file"].isObject());
    EXPECT_TRUE(st["file"]["worlds"].isArray());
    EXPECT_FALSE(st["file"]["locked"].asBool());
    EXPECT_FALSE(st["file"]["imported"].asBool());
    EXPECT_TRUE(st["file"]["lastExport"].isNull());
    EXPECT_TRUE(st["file"]["lastImport"].isNull());
    ASSERT_TRUE(st["progress"].isObject());
    EXPECT_TRUE(st["progress"]["lastOutcome"].isNull());
    EXPECT_NE(st["progress"]["state"].asString(), "Generating");
}

TEST(WorldGenDebugDomainPanel, ValidateCommandMatchesSessionValidateAndKeepsTheRevision)
{
    PanelFixture f("validate");
    f.Gen();
    ASSERT_TRUE(f.State()["validation"].isNull());   // a live-preview world has no report and nothing validated yet
    const uint32_t revision = f.S().Revision();

    f.Cmd("validate");

    PanelFixture ref("validate_ref");
    ref.Gen();
    const Json::Value direct = ref.S().Validate(Json::Value(Json::objectValue));
    ASSERT_TRUE(direct["success"].asBool());

    EXPECT_EQ(f.S().Revision(), revision);
    const Json::Value v = f.State()["validation"];
    ASSERT_TRUE(v.isObject());
    EXPECT_TRUE(v["fresh"].asBool());
    EXPECT_EQ(v["worldRevision"].asUInt(), revision);
    EXPECT_EQ(v["passed"].asBool(), direct["ok"].asBool());
    ASSERT_EQ(v["guarantees"].size(), direct["guarantees"].size());
    for (Json::ArrayIndex i = 0; i < direct["guarantees"].size(); ++i)
    {
        EXPECT_EQ(v["guarantees"][i]["id"], direct["guarantees"][i]["id"]);
        EXPECT_EQ(v["guarantees"][i]["ok"], direct["guarantees"][i]["ok"]);
        EXPECT_EQ(v["guarantees"][i]["counts"], direct["guarantees"][i]["counts"]);
        EXPECT_EQ(v["guarantees"][i]["diagnostics"], direct["guarantees"][i]["diagnostics"]);
    }
    EXPECT_FALSE(v.isMember("proofPaths"));
    EXPECT_FALSE(Compact(v).find("\"cells\"") != std::string::npos);   // no bulk evidence in the snapshot
    EXPECT_TRUE(f.Frame()["validateError"].isNull());
}

TEST(WorldGenDebugDomainPanel, ValidateBeforeAnyWorldReportsTheError)
{
    PanelFixture f("validate_none");
    f.Cmd("validate");
    const Json::Value st = f.State();
    EXPECT_TRUE(st["validation"].isNull());
    EXPECT_TRUE(st["validateError"].isString());
}

TEST(WorldGenDebugDomainPanel, ReportSuccessFillsValidationFromTheSessionReport)
{
    PanelFixture f("report_ok");
    f.Gen();
    f.Regen(7);
    ASSERT_NE(f.S().LastReport(), nullptr);
    const Json::Value report = f.S().Report(Json::Value(Json::objectValue));
    const Json::Value v = f.State()["validation"];
    ASSERT_TRUE(v.isObject());
    EXPECT_EQ(v["outcome"].asString(), "Complete");
    EXPECT_EQ(v["seed"], report["outerSeed"]);
    EXPECT_EQ(v["worldRevision"], report["finalValidation"]["worldRevision"]);
    EXPECT_TRUE(v["fresh"].asBool());
    ASSERT_EQ(v["guarantees"].size(), report["finalValidation"]["guarantees"].size());
    for (Json::ArrayIndex i = 0; i < v["guarantees"].size(); ++i)
    {
        EXPECT_EQ(v["guarantees"][i]["id"], report["finalValidation"]["guarantees"][i]["id"]);
        EXPECT_EQ(v["guarantees"][i]["ok"], report["finalValidation"]["guarantees"][i]["ok"]);
        EXPECT_EQ(v["guarantees"][i]["counts"], report["finalValidation"]["guarantees"][i]["counts"]);
    }
    ASSERT_EQ(v["attempts"].size(), report["attempts"].size());
    for (Json::ArrayIndex i = 0; i < v["attempts"].size(); ++i)
    {
        EXPECT_EQ(v["attempts"][i]["index"], report["attempts"][i]["attempt"]);
        EXPECT_EQ(v["attempts"][i]["subSeed"], report["attempts"][i]["subSeed"]);
        EXPECT_EQ(v["attempts"][i]["failedGuarantees"].size(), report["attempts"][i]["failedGuarantees"].size());
        EXPECT_TRUE(v["attempts"][i]["ms"].isNumeric());
    }
    const Json::Value card = f.State()["progress"]["lastOutcome"];
    EXPECT_EQ(card["kind"].asString(), "generation");
    EXPECT_EQ(card["outcome"].asString(), "Complete");
    EXPECT_FALSE(card["previousWorldKept"].asBool());
}

namespace
{
    // A failed 2-attempt Regenerate on top of an existing world (the guarantee fails both attempts).
    void FailTwoAttempts(PanelFixture& f, CoW::WorldGeneration::Testing::FailFirstNGuarantee& flaky)
    {
        ASSERT_TRUE(f.module.GetSiteRegistry().RegisterGuarantee(&flaky));
        Json::Value a(Json::objectValue);
        a["maxRetryAttempts"] = 2;
        f.Cmd("regenerate", a);
        f.Settle();
    }
}

TEST(WorldGenDebugDomainPanel, FailedReportKeepsThePreviousWorldAndNamesTheOutcome)
{
    PanelFixture f("report_fail");
    f.Gen();
    const uint32_t revision = f.S().Revision();
    CoW::WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    FailTwoAttempts(f, flaky);
    ASSERT_NE(f.S().LastReport(), nullptr);
    ASSERT_EQ(f.S().LastReport()->outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(f.S().Revision(), revision);

    const Json::Value st = f.State();
    const Json::Value v = st["validation"];
    ASSERT_TRUE(v.isObject());
    EXPECT_EQ(v["outcome"].asString(), "Unsatisfiable");
    EXPECT_FALSE(v["passed"].asBool());
    EXPECT_FALSE(v["fresh"].asBool());   // the failed run produced no world: its evidence is not the current world's
    ASSERT_EQ(v["attempts"].size(), 2u);
    EXPECT_EQ(v["attempts"][0u]["index"].asUInt(), 0u);
    EXPECT_EQ(v["attempts"][1u]["index"].asUInt(), 1u);
    EXPECT_EQ(v["attempts"][0u]["failedGuarantees"].size(), 1u);

    const Json::Value card = st["progress"]["lastOutcome"];
    EXPECT_EQ(card["kind"].asString(), "generation");
    EXPECT_EQ(card["outcome"].asString(), "Unsatisfiable");
    EXPECT_TRUE(card["previousWorldKept"].asBool());
    EXPECT_EQ(st["progress"]["state"].asString(), "Complete");   // the pill reads Complete, the card is red
}

TEST(WorldGenDebugDomainPanel, ProgressShowsAttemptsWhileGeneratingAndCancelKeepsTheWorld)
{
    PanelFixture f("cancel");
    f.Gen();
    const uint32_t revision = f.S().Revision();
    CoW::WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    ASSERT_TRUE(f.module.GetSiteRegistry().RegisterGuarantee(&flaky));
    Json::Value a(Json::objectValue);
    a["maxRetryAttempts"] = 5;
    f.Cmd("regenerate", a);
    f.Gen();   // first attempt only
    ASSERT_TRUE(f.S().IsGenerating());

    const Json::Value st = f.State();
    EXPECT_EQ(st["progress"]["state"].asString(), "Generating");
    EXPECT_EQ(st["progress"]["attempt"].asUInt(), 1u);
    EXPECT_EQ(st["progress"]["maxAttempts"].asUInt(), 5u);
    EXPECT_TRUE(st["progress"]["elapsedMs"].isNumeric());

    f.Cmd("cancel");
    EXPECT_FALSE(f.S().IsGenerating());
    EXPECT_EQ(f.S().Revision(), revision);
    ASSERT_NE(f.S().LastReport(), nullptr);
    EXPECT_EQ(f.S().LastReport()->outcome, GenerationOutcome::Cancelled);
    const Json::Value after = f.State();
    EXPECT_NE(after["progress"]["state"].asString(), "Generating");
    EXPECT_EQ(after["progress"]["lastOutcome"]["outcome"].asString(), "CANCEL");
    EXPECT_TRUE(after["progress"]["lastOutcome"]["previousWorldKept"].asBool());

    f.Cmd("cancel");   // nothing to cancel: not an error, no change
    EXPECT_EQ(f.S().Revision(), revision);
}

TEST(WorldGenDebugDomainPanel, ExportCommandWritesTheSameFileAsTheSessionAndListsIt)
{
    PanelFixture f("export");
    f.Gen();
    f.Regen(5);

    Json::Value exportArgs(Json::objectValue);
    exportArgs["name"] = "from_panel";
    f.Cmd("export", exportArgs);
    exportArgs["name"] = "direct";
    const Json::Value direct = f.module.ExportWorldFile(exportArgs);
    ASSERT_TRUE(direct["success"].asBool());

    const Json::Value st = f.State();
    const Json::Value file = st["file"];
    ASSERT_TRUE(file["lastExport"].isObject());
    EXPECT_TRUE(file["lastExport"]["success"].asBool());
    EXPECT_EQ(file["lastExport"]["name"].asString(), "from_panel");
    EXPECT_EQ(file["lastExport"]["bytes"], direct["bytes"]);
    EXPECT_EQ(file["defaultName"].asString(), DefaultWorldName(f.S().World().provenance.seed));

    const Json::Value worlds = f.S().Worlds();
    EXPECT_EQ(file["worlds"], worlds["worlds"]);
    EXPECT_EQ(file["worlds"].size(), 2u);

    // Export again without overwrite: refused, the error is shown.
    f.Cmd("export", Obj("name", "from_panel"));
    EXPECT_FALSE(f.State()["file"]["lastExport"]["success"].asBool());
    EXPECT_TRUE(f.State()["file"]["lastExport"]["error"].isString());

    // Default name (seed) when none is given.
    f.Cmd("export");
    EXPECT_EQ(f.State()["file"]["lastExport"]["name"].asString(), DefaultWorldName(f.S().World().provenance.seed));
}

TEST(WorldGenDebugDomainPanel, ImportCommandMatchesDirectImportAndOpensLocked)
{
    PanelFixture f("import");
    f.Gen();
    f.Regen(5);
    f.Cmd("export", Obj("name", "w"));
    ASSERT_TRUE(f.State()["file"]["lastExport"]["success"].asBool());

    PanelFixture ref("import_ref");
    ref.Gen();
    ref.Regen(5);
    ASSERT_TRUE(ref.S().ExportWorld(Obj("name", "w"))["success"].asBool());
    ASSERT_TRUE(ref.S().ImportWorld(Obj("name", "w"))["success"].asBool());

    f.Cmd("import", Obj("name", "w"));
    EXPECT_EQ(f.S().Revision(), ref.S().Revision());
    EXPECT_TRUE(f.S().IsLocked());
    EXPECT_TRUE(f.S().IsImported());
    EXPECT_EQ(f.S().Seed(), ref.S().Seed());

    const Json::Value file = f.State()["file"];
    EXPECT_TRUE(file["locked"].asBool());
    EXPECT_TRUE(file["imported"].asBool());
    ASSERT_TRUE(file["lastImport"].isObject());
    EXPECT_TRUE(file["lastImport"]["success"].asBool());
    EXPECT_EQ(file["lastImport"]["name"].asString(), "w");
    ASSERT_TRUE(file["origin"].isObject());
    EXPECT_EQ(file["origin"]["seed"].asUInt64(), f.S().Seed());
}

TEST(WorldGenDebugDomainPanel, ImportedWorldCardIsAnImportNotAGenerationOutcome)
{
    PanelFixture f("import_card");
    f.Gen();
    f.Regen(5);
    ASSERT_EQ(f.State()["progress"]["lastOutcome"]["kind"].asString(), "generation");
    f.Cmd("export", Obj("name", "w"));
    f.Cmd("import", Obj("name", "w"));

    Json::Value card = f.State()["progress"]["lastOutcome"];
    EXPECT_EQ(card["kind"].asString(), "import");
    EXPECT_EQ(card["outcome"].asString(), "Imported");
    EXPECT_EQ(card["name"].asString(), "w");
    EXPECT_FALSE(card["previousWorldKept"].asBool());

    // A failed Regenerate on the imported world is a generation outcome again, and the import stays in force.
    CoW::WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    ASSERT_TRUE(f.module.GetSiteRegistry().RegisterGuarantee(&flaky));
    Json::Value a(Json::objectValue);
    a["maxRetryAttempts"] = 2;
    a["unlock"] = true;
    f.Cmd("regenerate", a);
    f.Settle();
    card = f.State()["progress"]["lastOutcome"];
    EXPECT_EQ(card["kind"].asString(), "generation");
    EXPECT_EQ(card["outcome"].asString(), "Unsatisfiable");
    EXPECT_TRUE(card["previousWorldKept"].asBool());
    EXPECT_TRUE(f.State()["file"]["imported"].asBool());
}

TEST(WorldGenDebugDomainPanel, ImportOfABadFileShowsTheErrorWithLineAndColumn)
{
    PanelFixture f("import_bad");
    f.Gen();
    const uint32_t revision = f.S().Revision();
    f.WriteWorldFile("broken", "{\n  \"schema\": ,\n}\n");

    f.Cmd("import", Obj("name", "broken"));
    EXPECT_EQ(f.S().Revision(), revision);   // all or nothing
    EXPECT_FALSE(f.S().IsImported());

    PanelFixture ref("import_bad_ref");
    ref.Gen();
    ref.WriteWorldFile("broken", "{\n  \"schema\": ,\n}\n");
    const Json::Value direct = ref.S().ImportWorld(Obj("name", "broken"));
    ASSERT_FALSE(direct["success"].asBool());
    ASSERT_TRUE(direct.isMember("line"));

    const Json::Value imp = f.State()["file"]["lastImport"];
    ASSERT_TRUE(imp.isObject());
    EXPECT_FALSE(imp["success"].asBool());
    EXPECT_EQ(imp["error"], direct["error"]);
    EXPECT_EQ(imp["line"], direct["line"]);
    EXPECT_EQ(imp["column"], direct["column"]);
    EXPECT_GT(imp["line"].asUInt(), 0u);
    EXPECT_FALSE(f.State()["file"]["imported"].asBool());
    EXPECT_TRUE(f.State()["progress"]["lastOutcome"].isNull());   // a failed import is not an outcome card
}

TEST(WorldGenDebugDomainPanel, ImportOfAMissingFileOrInvalidNameChangesNothingAndRecordsTheError)
{
    PanelFixture f("import_missing");
    f.Regen(9);
    const uint32_t revision = f.S().Revision();
    const std::string hashes = f.S().Layers(Json::Value(Json::objectValue)).toStyledString();

    f.Cmd("import", Obj("name", "no_such_world"));
    Json::Value imp = f.State()["file"]["lastImport"];
    ASSERT_TRUE(imp.isObject());
    EXPECT_FALSE(imp["success"].asBool());
    EXPECT_TRUE(imp["error"].isString());
    EXPECT_EQ(f.S().Revision(), revision);
    EXPECT_FALSE(f.S().IsLocked());

    f.Cmd("import", Obj("name", "../escape"));   // path separators are not a valid world name
    imp = f.State()["file"]["lastImport"];
    EXPECT_FALSE(imp["success"].asBool());
    EXPECT_EQ(f.S().Revision(), revision);
    EXPECT_EQ(f.S().Layers(Json::Value(Json::objectValue)).toStyledString(), hashes);
    EXPECT_TRUE(f.State()["progress"]["lastOutcome"].isNull() ||
                f.State()["progress"]["lastOutcome"]["kind"].asString() != "import");
}

TEST(WorldGenDebugDomainPanel, ExportWithoutOverwriteRefusesAnExistingFileAndKeepsIt)
{
    PanelFixture f("export_clash");
    f.Regen(4);
    f.Cmd("export", Obj("name", "same"));
    ASSERT_TRUE(f.State()["file"]["lastExport"]["success"].asBool());
    const auto path = f.dir / "same.wld.json";
    const auto size = fs::file_size(path);

    f.Regen(5);   // a different world
    f.Cmd("export", Obj("name", "same"));
    EXPECT_FALSE(f.State()["file"]["lastExport"]["success"].asBool());
    EXPECT_EQ(fs::file_size(path), size);

    Json::Value overwrite(Json::objectValue);
    overwrite["name"] = "same";
    overwrite["overwrite"] = true;
    f.Cmd("export", overwrite);
    EXPECT_TRUE(f.State()["file"]["lastExport"]["success"].asBool());
}

TEST(WorldGenDebugDomainPanel, ExportBeforeAnyWorldReportsTheErrorAndWritesNothing)
{
    PanelFixture f("export_empty");
    f.Cmd("export", Obj("name", "early"));
    EXPECT_FALSE(f.State()["file"]["lastExport"]["success"].asBool());
    EXPECT_FALSE(fs::exists(f.dir / "early.wld.json"));
    f.Cmd("cancel");   // nothing running: harmless
    f.Cmd("discard");  // nothing locked: harmless
    EXPECT_FALSE(f.S().IsLocked());
}

TEST(WorldGenDebugDomainPanel, SelectWorldKeepsOnlyKnownNames)
{
    PanelFixture f("select");
    f.Gen();
    f.Cmd("export", Obj("name", "alpha"));
    f.Cmd("export", Obj("name", "beta"));
    EXPECT_EQ(f.State()["file"]["selected"].asString(), "");

    f.Cmd("selectWorld", Obj("name", "beta"));
    EXPECT_EQ(f.State()["file"]["selected"].asString(), "beta");
    f.Cmd("selectWorld", Obj("name", "nope"));
    EXPECT_EQ(f.State()["file"]["selected"].asString(), "beta");
    f.Cmd("selectWorld", Obj("name", 5));
    EXPECT_EQ(f.State()["file"]["selected"].asString(), "beta");
}

TEST(WorldGenDebugDomainPanel, DiscardUnlocksAndAppliesThePendingChange)
{
    PanelFixture f("discard");
    f.Gen();
    f.Regen(5);
    f.Cmd("export", Obj("name", "w"));
    f.Cmd("import", Obj("name", "w"));
    ASSERT_TRUE(f.S().IsLocked());
    const uint32_t revision = f.S().Revision();
    const Json::Value before = f.S().Params();

    // Locked: a plain slider command is refused (the panel asks first).
    Json::Value slide(Json::objectValue);
    slide["key"] = "landFraction";
    slide["value"] = 0.5;
    f.Cmd("setScale", slide);
    EXPECT_EQ(f.S().Params().toStyledString(), before.toStyledString());
    EXPECT_TRUE(f.S().IsLocked());
    EXPECT_FALSE(f.S().IsDirty());

    // Discard replays the pending change with the existing unlock arg.
    PanelFixture ref("discard_ref");
    ref.Gen();
    ref.Regen(5);
    ASSERT_TRUE(ref.S().ExportWorld(Obj("name", "w"))["success"].asBool());
    ASSERT_TRUE(ref.S().ImportWorld(Obj("name", "w"))["success"].asBool());
    Json::Value set(Json::objectValue);
    set["landFraction"] = 0.5;
    set["unlock"] = true;
    ASSERT_TRUE(ref.S().Set(set)["success"].asBool());

    Json::Value discard(Json::objectValue);
    discard["cmd"] = "setScale";
    discard["args"] = slide;
    f.Cmd("discard", discard);
    EXPECT_FALSE(f.S().IsLocked());
    EXPECT_TRUE(f.S().IsDirty());
    EXPECT_EQ(f.S().Params().toStyledString(), ref.S().Params().toStyledString());
    f.Settle();
    EXPECT_GT(f.S().Revision(), revision);
    EXPECT_FALSE(f.State()["file"]["imported"].asBool());
    EXPECT_TRUE(f.State()["progress"]["lastOutcome"].isNull() ||
                f.State()["progress"]["lastOutcome"]["kind"].asString() != "import");
}

TEST(WorldGenDebugDomainPanel, DiscardWithoutAPendingChangeJustUnlocks)
{
    PanelFixture f("discard_plain");
    f.Gen();
    f.Cmd("export", Obj("name", "w"));
    f.Cmd("import", Obj("name", "w"));
    ASSERT_TRUE(f.S().IsLocked());
    const Json::Value shape = f.S().Params()["params"];
    f.Cmd("discard");
    EXPECT_FALSE(f.S().IsLocked());
    EXPECT_EQ(f.S().Params()["params"], shape);
    f.Settle();
    EXPECT_FALSE(f.S().IsImported());   // regenerated from the recorded recipe

    // Not locked: discard changes nothing.
    f.Gen();
    const uint32_t revision = f.S().Revision();
    f.Cmd("discard");
    EXPECT_EQ(f.S().Revision(), revision);
    EXPECT_FALSE(f.S().IsDirty());
}

TEST(WorldGenDebugDomainPanel, BadArgsAndUnknownKeysAreIgnored)
{
    PanelFixture f("bad");
    f.Gen();
    f.Regen(3);
    const uint32_t revision = f.S().Revision();
    const Json::Value before = f.State();

    f.Cmd("selectWorld");
    f.Cmd("import");
    f.Cmd("import", Obj("name", 5));
    f.Cmd("export", Json::Value("x"));
    f.Cmd("discard", Json::Value(7));
    f.Cmd("discard", Obj("cmd", "bogus"));
    f.Cmd("discard", Obj("cmd", "selectCell"));
    f.Cmd("validateAll");

    EXPECT_EQ(f.S().Revision(), revision);
    EXPECT_FALSE(f.S().IsDirty());
    EXPECT_FALSE(f.S().IsLocked());
    const Json::Value after = f.State();
    EXPECT_EQ(Compact(before["stats"]), Compact(after["stats"]));
    EXPECT_EQ(Compact(before["validation"]), Compact(after["validation"]));
    EXPECT_EQ(Compact(before["progress"]), Compact(after["progress"]));
}

TEST(WorldGenDebugDomainPanel, CellAbsorbedFlagFollowsRegionZoneMismatch)
{
    PanelFixture f("absorbed");
    f.Gen();
    bool sawAbsorbed = false;
    bool sawPlain = false;
    for (int seed = 1; seed <= 12 && !sawAbsorbed; ++seed)
    {
        f.Regen(seed);
        const WorldStructure& w = f.S().World();
        if (w.regions.regions.Size() == 0) continue;
        for (int c = 0; c < w.CellCount(); ++c)
        {
            const Cell& cell = w.Cells()[c];
            if (cell.region == 0) continue;
            const RegionRecord& rec = w.regions.regions[cell.region - 1];
            const bool expected = !rec.isIsland && cell.stratum != rec.stratum;
            if (expected && sawAbsorbed) continue;
            if (!expected && sawPlain && !sawAbsorbed) continue;
            Json::Value xy(Json::objectValue);
            xy["x"] = c % w.dimensions.width;
            xy["y"] = c / w.dimensions.width;
            f.Cmd("selectCell", xy);
            const Json::Value cellJson = f.State()["cell"];
            ASSERT_TRUE(cellJson.isObject());
            EXPECT_EQ(cellJson["absorbed"].asBool(), expected) << "seed " << seed << " cell " << c;
            (expected ? sawAbsorbed : sawPlain) = true;
            if (sawAbsorbed && sawPlain) break;
        }
    }
    EXPECT_TRUE(sawPlain);
    EXPECT_TRUE(sawAbsorbed) << "no absorbed cell in seeds 1..12";
}

TEST(WorldGenDebugDomainPanel, CellAbsorbedIsFalseWithoutRegions)
{
    PanelFixture f("absorbed_none");
    f.Gen();   // live preview: no regions
    f.Cmd("selectCell", [] { Json::Value v(Json::objectValue); v["x"] = 3; v["y"] = 3; return v; }());
    const Json::Value cell = f.State()["cell"];
    ASSERT_TRUE(cell.isObject());
    EXPECT_TRUE(cell.isMember("absorbed"));
    EXPECT_FALSE(cell["absorbed"].asBool());
}

TEST(WorldGenDebugDomainPanel, SectionsLiveInEveryFrameWhileLeanFramesOmitStaticFields)
{
    PanelFixture f("lean");
    f.Gen();
    f.Regen(3);
    (void)f.State();
    const Json::Value lean = f.Frame();
    ASSERT_FALSE(lean["full"].asBool());
    EXPECT_TRUE(lean["validation"].isObject());
    EXPECT_TRUE(lean["file"].isObject());
    EXPECT_TRUE(lean["progress"].isObject());
    EXPECT_TRUE(lean["file"]["worlds"].isArray());
}

TEST(WorldGenDebugDomainPanel, CompactStateAtLargeWithFailedTwoAttemptReportStaysUnderSlot)
{
    PanelFixture f("large");
    f.Cmd("setSize", Obj("size", "Large"));
    f.Gen();
    f.Cmd("export", Obj("name", "big"));
    f.Cmd("validate");
    CoW::WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    FailTwoAttempts(f, flaky);
    ASSERT_NE(f.S().LastReport(), nullptr);
    ASSERT_EQ(f.S().LastReport()->attempts.Size(), 2u);
    Json::Value xy(Json::objectValue);
    xy["x"] = 52;
    xy["y"] = 32;
    f.Cmd("selectCell", xy);
    f.Cmd("import", Obj("name", "missing"));   // an error with text in file.lastImport

    const Json::Value st = f.State();
    ASSERT_TRUE(st["validation"].isObject());
    ASSERT_TRUE(st["cell"].isObject());
    const size_t bytes = Compact(st).size();
    std::cout << "[ measured ] compact WorldGen state at Large, failed 2-attempt report + cell + file: " << bytes << " bytes\n";
    EXPECT_LT(bytes, 12u * 1024u);
}


// Panel gap (cleanup bundle 2026-11-19): the picker needs to know which layers the world has no data for.
TEST(WorldGenDebugDomainPanel, ViewReportsUnavailableLayersBeforeAWorldAndAfterAnImport)
{
    PanelFixture f("unavailable");
    f.Gen();
    Json::Value before = f.State()["view"]["unavailableLayers"];
    ASSERT_TRUE(before.isArray());
    EXPECT_GT(before.size(), 0u);   // no world yet: nothing has data

    f.Regen(5);
    EXPECT_EQ(f.State()["view"]["unavailableLayers"].size(), 0u);   // a full generation fills every layer

    f.Cmd("export", Obj("name", "u"));
    f.Cmd("import", Obj("name", "u"));
    ASSERT_TRUE(f.S().IsImported());
    const Json::Value imported = f.State()["view"]["unavailableLayers"];
    bool hasTemperature = false, hasElevation = false;
    for (const Json::Value& v : imported) { hasTemperature |= v.asString() == "temperature"; hasElevation |= v.asString() == "elevation"; }
    EXPECT_TRUE(hasTemperature);    // intermediate climate field: not derivable from a file
    EXPECT_FALSE(hasElevation);     // rebuilt from the cells
}

#endif // DIA_DEBUG
