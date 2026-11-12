// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC2-AC5, AC8, Import Implementation)
//       regions-and-sites.md (AC11 JSON parts), worldgenerationpipeline.md (Test 3)
// WorldJson import: validation, all-or-nothing, byte-identical round trips, hand edits, unknown kinds.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldJson.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>

#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{

std::string Export(const WorldStructure& w, const WorldJsonContext& ctx = {})
{
    std::string text;
    JsonIssue error;
    EXPECT_TRUE(ExportWorldText(w, ctx, text, error)) << error.field << ": " << error.message;
    return text;
}

Json::Value Parse(const std::string& text)
{
    Json::Value root;
    Json::CharReaderBuilder b;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(b.newCharReader());
    EXPECT_TRUE(reader->parse(text.data(), text.data() + text.size(), &root, &errors)) << errors;
    return root;
}

std::string Write(const Json::Value& root)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "  ";
    return Json::writeString(b, root);
}

// Exports `w`, lets the test edit the parsed document, and returns the edited text.
std::string Edited(const WorldStructure& w, const std::function<void(Json::Value&)>& edit)
{
    Json::Value root = Parse(Export(w));
    edit(root);
    return Write(root);
}

WorldImportResult Import(const std::string& text, WorldStructure& out, const WorldJsonContext& ctx = {})
{
    return ImportWorldText(text, ctx, out);
}

bool HasWarning(const WorldImportResult& r, const std::string& needle)
{
    for (const std::string& w : r.warnings) { if (w.find(needle) != std::string::npos) { return true; } }
    return false;
}

// A failed import must name the field and leave the destination untouched.
void ExpectRejected(const std::string& text, const std::string& field, const std::string& messagePart = "")
{
    WorldStructure out = MakeTinyWorld();
    const WorldStructure before = MakeTinyWorld();
    const WorldImportResult r = Import(text, out);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error.field, field) << r.error.message;
    EXPECT_FALSE(r.error.message.empty());
    if (!messagePart.empty()) { EXPECT_NE(r.error.message.find(messagePart), std::string::npos) << r.error.message; }
    EXPECT_TRUE(SameWorld(out, before)) << "a failed import must not touch the destination world";
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// AC2 / Test 3: round trip
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, AC2_TinyWorldRoundTripsExactly)
{
    const WorldStructure original = MakeTinyWorld();
    WorldStructure imported;
    const WorldImportResult r = Import(Export(original), imported);
    ASSERT_TRUE(r.ok) << r.error.field << ": " << r.error.message;
    ASSERT_EQ(r.warnings.size(), 1u);   // only: Test.Kind is not registered here (it exports under its crc key)
    EXPECT_NE(r.warnings[0].find("kind is not registered"), std::string::npos) << r.warnings[0];
    EXPECT_TRUE(SameCells(original, imported));
    EXPECT_TRUE(original.regions == imported.regions);
    EXPECT_TRUE(original.sites == imported.sites);
    EXPECT_TRUE(SameProvenance(original.provenance, imported.provenance));
    EXPECT_EQ(original.mainLandmass, imported.mainLandmass);
    EXPECT_EQ(Export(original), Export(imported));   // and re-exports the identical text
}

TEST(WorldJsonImport, AC2_GeneratedWorldsRoundTripExactly_TwentyWorldsAcrossShapes)
{
    struct Shape { const char* name; const char* json; };
    const Shape shapes[] = {
        { "default",     "{}" },
        { "archipelago", "{\"islands\":6,\"landFraction\":0.3,\"continentIslands\":0.2}" },
        { "highlands",   "{\"mountainousness\":0.9,\"ruggedness\":0.8,\"barrier\":0.9}" },
        { "continental", "{\"continents\":2,\"landFraction\":0.5,\"rivers\":0.8,\"lakes\":8}" },
    };
    int worlds = 0;
    for (const Shape& shape : shapes)
    {
        Json::Value args;
        Json::Reader().parse(shape.json, args);
        for (uint64_t seed = 1; seed <= 5; ++seed)
        {
            const WorldStructure original = GenerateWorld(seed, args);
            ASSERT_GT(original.regions.regions.Size(), 0u) << shape.name << " " << seed;
            const std::string text = Export(original);
            WorldStructure imported;
            const WorldImportResult r = Import(text, imported);
            ASSERT_TRUE(r.ok) << shape.name << " " << seed << ": " << r.error.field << ": " << r.error.message;
            EXPECT_TRUE(r.warnings.empty()) << shape.name << " " << seed << ": " << r.warnings.front();
            EXPECT_TRUE(SameCells(original, imported)) << shape.name << " " << seed;
            EXPECT_TRUE(original.regions == imported.regions) << shape.name << " " << seed;
            EXPECT_TRUE(original.sites == imported.sites) << shape.name << " " << seed;
            EXPECT_TRUE(SameProvenance(original.provenance, imported.provenance)) << shape.name << " " << seed;
            EXPECT_EQ(text, Export(imported)) << shape.name << " " << seed;
            ++worlds;
        }
    }
    EXPECT_EQ(worlds, 20);
}

TEST(WorldJsonImport, WorldWithoutRegionsOrSitesRoundTrips)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng{ 1 };
    ASSERT_TRUE(s.Tick(rng));   // first generation: terrain only, regions stay empty
    const WorldStructure& original = s.World();
    ASSERT_EQ(original.regions.regions.Size(), 0u);
    WorldStructure imported;
    const WorldImportResult r = Import(Export(original), imported);
    ASSERT_TRUE(r.ok) << r.error.field << ": " << r.error.message;
    EXPECT_TRUE(SameWorld(original, imported));
}

TEST(WorldJsonImport, ImportedFloatsKeepEveryBit)
{
    WorldStructure original = MakeTinyWorld();
    const float values[] = { 0.0f, 1.0f, 0.1f, 1.0f / 3.0f, 0.99999994f, 1.17549435e-38f, 1.0e-45f, 0.7f,
                             std::nextafter(0.5f, 1.0f), std::nextafter(0.5f, 0.0f), 0.123456789f, 0.2f };
    for (int i = 0; i < 12; ++i) { original.Cells()[i].elevation = values[i]; original.Cells()[i].latitude = values[11 - i]; }
    WorldStructure imported;
    ASSERT_TRUE(Import(Export(original), imported).ok);
    EXPECT_TRUE(SameCells(original, imported));
}

// ---------------------------------------------------------------------------------------------------------------------
// AC3 / AC5 / AC8: validation, all-or-nothing
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, AC3a_MissingCellBiomeFieldIsRejectedWithItsPath)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][3].removeMember("biome"); }),
                   "world.cells[3].biome", "missing");
}

TEST(WorldJsonImport, AC3b_CellCountNotMatchingTheDimensionsIsRejected)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        Json::Value kept(Json::arrayValue);
        for (int i = 0; i < 11; ++i) { kept.append(r["world"]["cells"][i]); }
        r["world"]["cells"] = kept;
    }), "world.cells", "Cell count mismatch");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"].append(r["world"]["cells"][0]); }),
                   "world.cells", "expected 12");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["dimensions"]["width"] = 5; }),
                   "world.cells", "Cell count mismatch");
}

TEST(WorldJsonImport, AC3d_AC5_UnsupportedVersionIsRejectedWithTheSpecMessage)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["version"] = 2; }), "version", "Unsupported cow.world version: 2");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["version"] = 0; }), "version", "Unsupported cow.world version: 0");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["version"] = -1; }), "version", "Unsupported cow.world version: -1");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["version"] = "1"; }), "version");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["version"] = 1.5; }), "version");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r.removeMember("version"); }), "version");
}

TEST(WorldJsonImport, WrongFormatIsRejected)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["format"] = "cow.worldshape"; }), "format", "Unsupported format");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r.removeMember("format"); }), "format");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["format"] = 3; }), "format");
}

TEST(WorldJsonImport, AC8_BadTopologyElevationAndWaterSourceNameTheirField)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["topology"] = "square"; }), "world.topology", "square");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][4]["elevation"] = 1.5; }), "world.cells[4].elevation", "[0, 1]");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][4]["elevation"] = -0.1; }), "world.cells[4].elevation");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][5]["latitude"] = 2; }), "world.cells[5].latitude");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][6]["water_source"] = "Swamp"; }), "world.cells[6].water_source", "Swamp");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][6]["elevation"] = "high"; }), "world.cells[6].elevation");
}

TEST(WorldJsonImport, AC8_SiteOutsideTheGridIsRejected)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["cell"] = 12; }),
                   "world.sites.CoW.Node.Hunting[0].cell");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["cell"] = -1; }),
                   "world.sites.CoW.Node.Hunting[0].cell");
}

TEST(WorldJsonImport, AC8_AllFourWaterSourcesAndTheRangeEndsRoundTrip)
{
    WorldStructure original = MakeTinyWorld();
    const WaterSource all[] = { WaterSource::None, WaterSource::Ocean, WaterSource::Lake, WaterSource::River };
    for (int i = 0; i < 12; ++i) { original.Cells()[i].waterSource = all[i % 4]; }
    original.Cells()[0].elevation = 0.0f;
    original.Cells()[1].elevation = 1.0f;
    WorldStructure imported;
    ASSERT_TRUE(Import(Export(original), imported).ok);
    EXPECT_TRUE(SameCells(original, imported));
}

TEST(WorldJsonImport, EnumNamesMustBeKnown)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["biome"] = "Biome.Swamp"; }), "world.cells[0].biome", "Biome.Swamp");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["biome"] = 2; }), "world.cells[0].biome");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][1]["terrain"] = "Marsh"; }), "world.cells[1].terrain");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][2]["depth_zone"] = "Deep"; }), "world.cells[2].depth_zone");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["zone"] = "Nowhere"; }), "world.regions[0].zone");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][1]["primary_biome"] = "Biome.None"; }), "world.regions[1].primary_biome");
}

TEST(WorldJsonImport, IntegerFieldsAreRangeAndTypeChecked)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["region"] = -1; }), "world.cells[0].region");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["region"] = 1.5; }), "world.cells[0].region");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["region"] = "1"; }), "world.cells[0].region");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["region"] = true; }), "world.cells[0].region");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["region"] = 3; }), "world.cells[0].region", "does not exist");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["landmass"] = 256; }), "world.cells[0].landmass");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["terrain_flags"] = 64; }), "world.cells[0].terrain_flags");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["downstream"] = 6; }), "world.cells[0].downstream");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0]["downstream"] = 8; }), "world.cells[0].downstream");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["main_landmass"] = 256; }), "world.main_landmass");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["cells"][0] = 5; }), "world.cells[0]", "object");
}

TEST(WorldJsonImport, EveryRequiredCellFieldIsRequired)
{
    for (const char* field : { "biome", "terrain", "region", "elevation", "latitude", "water_source", "depth_zone", "landmass", "terrain_flags", "downstream" })
    {
        ExpectRejected(Edited(MakeTinyWorld(), [&](Json::Value& r) { r["world"]["cells"][7].removeMember(field); }),
                       std::string("world.cells[7].") + field, "missing");
    }
}

TEST(WorldJsonImport, DimensionLimitsAreEnforced)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["dimensions"]["width"] = 0; }), "world.dimensions.width");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["dimensions"]["height"] = -3; }), "world.dimensions.height");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["dimensions"]["width"] = 257; }), "world.dimensions.width");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["dimensions"]["width"] = "4"; }), "world.dimensions.width");
}

TEST(WorldJsonImport, GridAtTheCellCapIsAcceptedByTheLimitCheck)
{
    // 256 x 256 = 65,536 passes the dimension limits (the cell count then fails the count check, not the limits).
    WorldStructure out;
    Json::Value root = Parse(Export(MakeTinyWorld()));
    root["world"]["dimensions"]["width"] = 256;
    root["world"]["dimensions"]["height"] = 256;
    const WorldImportResult r = Import(Write(root), out);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error.field, "world.cells");
}

TEST(WorldJsonImport, RegionStructureIsValidated)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["id"] = 2; }), "world.regions[0].id", "in order");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][1]["id"] = 1; }), "world.regions[1].id");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["neighbours"].append(9); }), "world.regions[0].neighbours[1]", "does not exist");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["neighbours"][0] = 0; }), "world.regions[0].neighbours[0]");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["is_island"] = "no"; }), "world.regions[0].is_island");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["centre_cell"] = 12; }), "world.regions[0].centre_cell");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0]["purity"] = 1.2; }), "world.regions[0].purity");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["regions"][0].removeMember("zone"); }), "world.regions[0].zone");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["region_table"]["limited_by"] = "Nowhere"; }), "world.region_table.limited_by");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["region_table"]["absorbed_cells"] = 70000; }), "world.region_table.absorbed_cells");
}

TEST(WorldJsonImport, MoreThanSixtyFourRegionsIsRejected)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        for (int i = 0; i < 70; ++i) { r["world"]["regions"].append(r["world"]["regions"][0]); }
    }), "world.regions", "more than 64");
}

TEST(WorldJsonImport, SiteFieldsAreValidated)
{
    const char* base = "world.sites.CoW.Node.Hunting[0]";
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["region"] = 3; }), std::string(base) + ".region", "does not exist");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["variant"] = 65536; }), std::string(base) + ".variant");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["flags"] = -1; }), std::string(base) + ".flags");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0].removeMember("cell"); }), std::string(base) + ".cell", "missing");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"] = 5; }), "world.sites.CoW.Node.Hunting", "array");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"].append(r["world"]["sites"]["CoW.Node.Hunting"][0]); }),
                   "world.sites.CoW.Node.Hunting[1].cell", "already exists");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"] = Json::Value(Json::arrayValue); }), "world.sites");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"][""] = Json::Value(Json::arrayValue); }), "world.sites");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"][std::string(64, 'k')] = Json::Value(Json::arrayValue); }),
                   "world.sites." + std::string(28, 'k') + "...");
}

// ---------------------------------------------------------------------------------------------------------------------
// Parse level failures
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, UnparseableDocumentsAreRejectedNotCrashed)
{
    const std::string good = Export(MakeTinyWorld());
    const char* garbage[] = { "", "   ", "not json", "{", "[]", "null", "42", "\"cow.world\"", "{\"format\":", "{}\x01" };
    for (const char* g : garbage)
    {
        WorldStructure out = MakeTinyWorld();
        const WorldImportResult r = Import(g, out);
        EXPECT_FALSE(r.ok) << g;
        EXPECT_FALSE(r.error.message.empty()) << g;
        EXPECT_TRUE(SameWorld(out, MakeTinyWorld())) << g;
    }
    // Truncated at many points.
    for (size_t cut : { size_t(1), good.size() / 4, good.size() / 2, good.size() - 3, good.size() - 2 })
    {
        WorldStructure out = MakeTinyWorld();
        EXPECT_FALSE(Import(good.substr(0, cut), out).ok) << cut;
        EXPECT_TRUE(SameWorld(out, MakeTinyWorld())) << cut;
    }
}

TEST(WorldJsonImport, DuplicateKeysCommentsNaNAndTrailingTextAreRejected)
{
    std::string good = Export(MakeTinyWorld());
    WorldStructure out;
    // Duplicate "version" key.
    std::string dup = good;
    dup.replace(dup.find("\"version\": 1,"), 13, "\"version\": 1, \"version\": 1,");
    EXPECT_FALSE(Import(dup, out).ok);
    // NaN where a number belongs (not valid JSON).
    std::string nan = good;
    nan.replace(nan.find("\"elevation\": 0,"), 15, "\"elevation\": NaN,");
    EXPECT_FALSE(Import(nan, out).ok);
    // Comment.
    EXPECT_FALSE(Import("/* hi */\n" + good, out).ok);
    // Trailing text after the document.
    EXPECT_FALSE(Import(good + "{}", out).ok);
    EXPECT_FALSE(Import(good + "garbage", out).ok);
}

TEST(WorldJsonImport, OversizedTextIsRefusedBeforeParsing)
{
    WorldStructure out = MakeTinyWorld();
    const WorldImportResult r = Import(std::string(kMaxWorldJsonBytes + 1, ' '), out);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.message.find("larger"), std::string::npos);
    EXPECT_TRUE(SameWorld(out, MakeTinyWorld()));
}

TEST(WorldJsonImport, DeeplyNestedDocumentIsRejectedNotACrash)
{
    std::string deep(5000, '[');
    deep += std::string(5000, ']');
    WorldStructure out;
    EXPECT_FALSE(Import(deep, out).ok);
}

// ---------------------------------------------------------------------------------------------------------------------
// AC4: hand edits
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, AC4_MovedSiteImportsAtTheNewCellAndNothingElseChanges)
{
    const WorldStructure original = MakeTinyWorld();
    const std::string text = Edited(original, [](Json::Value& r)
    {
        r["world"]["sites"]["CoW.Node.Hunting"][0]["cell"] = 4;   // was 2
        r["world"]["sites"]["CoW.Node.Hunting"][0]["region"] = 1;
    });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    const Site* moved = imported.sites.Find(BuiltinNodeKinds::Hunting(), 4);
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(imported.sites.Find(BuiltinNodeKinds::Hunting(), 2), nullptr);
    EXPECT_EQ(imported.sites.Count(), original.sites.Count());
    EXPECT_TRUE(SameCells(original, imported));
    EXPECT_TRUE(original.regions == imported.regions);
    EXPECT_TRUE(SameProvenance(original.provenance, imported.provenance));
    for (const Site& site : original.sites.All())
    {
        if (site.kindCrc == SiteKindId(BuiltinNodeKinds::Hunting())) { continue; }
        bool found = false;
        for (const Site& other : imported.sites.All()) { found = found || other == site; }
        EXPECT_TRUE(found) << "site of kind crc " << site.kindCrc << " at cell " << site.cell << " changed";
    }
}

TEST(WorldJsonImport, HandEditedCellsAreKeptNotRecomputedOrReclassified)
{
    const WorldStructure original = MakeTinyWorld();
    const std::string text = Edited(original, [](Json::Value& r)
    {
        r["world"]["cells"][3]["biome"] = "Biome.Desert";          // would not be a Temperate Forest by any rule
        r["world"]["cells"][3]["elevation"] = 0.05;
    });
    WorldStructure imported;
    ASSERT_TRUE(Import(text, imported).ok);
    EXPECT_EQ(imported.Cells()[3].biome, Biome::DesertBasin);
    EXPECT_EQ(imported.Cells()[3].elevation, 0.05f);
}

TEST(WorldJsonImport, RegionSummariesAreRecomputedFromTheCellsAndMismatchesWarn)
{
    const WorldStructure original = MakeTinyWorld();
    const std::string text = Edited(original, [](Json::Value& r)
    {
        auto& reg = r["world"]["regions"][0];
        reg["cell_count"] = 99;
        reg["primary_biome"] = "Biome.Alpine";
        reg["purity"] = 0.99;
        reg["centre_cell"] = 11;
        reg["neighbours"] = Json::Value(Json::arrayValue);
    });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    EXPECT_TRUE(original.regions == imported.regions) << "the recomputed summaries must equal the generator's";
    for (const char* field : { "cell_count", "primary_biome", "purity", "centre_cell", "neighbours" })
    {
        EXPECT_TRUE(HasWarning(res, std::string("world.regions[0].") + field)) << field;
    }
    EXPECT_FALSE(HasWarning(res, "world.regions[1]"));
}

TEST(WorldJsonImport, EditingACellsRegionUpdatesTheSummariesOfBothRegions)
{
    const WorldStructure original = MakeTinyWorld();
    const std::string text = Edited(original, [](Json::Value& r) { r["world"]["cells"][5]["region"] = 2; });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_EQ(imported.regions.regions[0].cellCount, 3);
    EXPECT_EQ(imported.regions.regions[1].cellCount, 6);
    EXPECT_TRUE(HasWarning(res, "world.regions[0].cell_count: file 4, recomputed 3"));
    EXPECT_TRUE(HasWarning(res, "world.regions[1].cell_count: file 5, recomputed 6"));
}

TEST(WorldJsonImport, RegionWithNoCellsWarnsAndImports)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        for (int i = 0; i < 12; ++i) { if (r["world"]["cells"][i]["region"].asUInt() == 2) { r["world"]["cells"][i]["region"] = 1; } }
    });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "region has no cells"));
    EXPECT_EQ(imported.regions.regions[1].cellCount, 0);
}

TEST(WorldJsonImport, SiteWhoseRegionDisagreesWithItsCellWarns)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["CoW.Node.Hunting"][0]["region"] = 2; });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "world.sites.CoW.Node.Hunting[0].region"));
    EXPECT_EQ(imported.sites.Find(BuiltinNodeKinds::Hunting(), 2)->region, 2);   // authored value kept
}

TEST(WorldJsonImport, UnknownFieldsAreIgnoredWithAWarning)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        r["note"] = "hand edited";
        r["world"]["author"] = "me";
        r["world"]["cells"][0]["mood"] = "grumpy";
        r["world"]["sites"]["CoW.Node.Hunting"][0]["tag"] = "x";
    });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(SameWorld(imported, MakeTinyWorld()));
    EXPECT_TRUE(HasWarning(res, "note"));
    EXPECT_TRUE(HasWarning(res, "world.author"));
    EXPECT_TRUE(HasWarning(res, "world.cells[0].mood"));
    EXPECT_TRUE(HasWarning(res, "world.sites.CoW.Node.Hunting[0].tag"));
}

TEST(WorldJsonImport, WaterlessWorldImportsWithAWarning)
{
    WorldStructure dry = MakeTinyWorld();
    for (int i = 0; i < 12; ++i) { dry.Cells()[i].waterSource = WaterSource::None; }
    WorldStructure imported;
    const WorldImportResult res = Import(Export(dry), imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "no water cell"));
    EXPECT_TRUE(SameCells(dry, imported));
}

TEST(WorldJsonImport, WarningsAreCapped)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        for (int i = 0; i < 200; ++i) { r["junk" + std::to_string(i)] = i; }
    });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_EQ(res.warnings.size(), kMaxImportWarnings + 1);
    EXPECT_NE(res.warnings.back().find("more warnings"), std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------
// Provenance
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, ProvenanceRoundTripsIncludingTheFullSeedAndAllParams)
{
    WorldStructure original = MakeTinyWorld();
    original.provenance.seed = 0xFFFFFFFFFFFFFFFFull;
    original.provenance.shape.lakes = 11;
    original.provenance.shape.temperatureOffset = -0.25f;
    original.provenance.shape.latitudeNorthDeg = -33.3f;
    WorldStructure imported;
    ASSERT_TRUE(Import(Export(original), imported).ok);
    EXPECT_TRUE(SameProvenance(original.provenance, imported.provenance));
    EXPECT_EQ(imported.provenance.seed, 0xFFFFFFFFFFFFFFFFull);
}

TEST(WorldJsonImport, MissingProvenanceWarnsAndImports)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r) { r.removeMember("provenance"); });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "provenance: missing"));
    EXPECT_EQ(imported.provenance.seed, 0u);
}

TEST(WorldJsonImport, BadProvenanceFieldsAreRejected)
{
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["seed"] = -5; }), "provenance.seed");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["seed"] = "abc"; }), "provenance.seed");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"].removeMember("seed"); }), "provenance.seed");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["preset_id"] = std::string(33, 'x'); }), "provenance.preset_id");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["preset_id"] = 7; }), "provenance.preset_id");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["params"]["barrierForm"] = "Wall"; }), "provenance.params");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["params"] = 3; }), "provenance.params");
    ExpectRejected(Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"] = 3; }), "provenance");
}

TEST(WorldJsonImport, OutOfRangeProvenanceParamsAreClampedWithAWarning)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["params"]["landFraction"] = 5.0; });
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "landFraction"));
    EXPECT_LE(imported.provenance.shape.landFraction, 0.65f);
}

TEST(WorldJsonImport, VersionMismatchesWarn)
{
    WorldJsonContext ctx;
    ctx.rulesVersion = 9;
    WorldStructure imported;
    const WorldImportResult res = Import(Export(MakeTinyWorld()), imported, ctx);   // file says rules_version 4
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "provenance.rules_version"));
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r) { r["provenance"]["generator_version"] = 99; });
    const WorldImportResult res2 = Import(text, imported);
    ASSERT_TRUE(res2.ok);
    EXPECT_TRUE(HasWarning(res2, "provenance.generator_version"));
    ctx.rulesVersion = 4;
    EXPECT_FALSE(HasWarning(Import(Export(MakeTinyWorld()), imported, ctx), "rules_version"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Regions AC11 (JSON parts): generic kinds round trip, unknown kinds survive
// ---------------------------------------------------------------------------------------------------------------------

TEST(WorldJsonImport, AC11_RegisteredTestKindRoundTripsExactlyAndNeedsNoWarning)
{
    TestPlacer placer("Test.Kind", {}, 7);
    SiteRegistry registry;
    ASSERT_TRUE(registry.Register(&placer));
    WorldJsonContext ctx;
    ctx.registry = &registry;

    const WorldStructure original = MakeTinyWorld();   // carries a Test.Kind site at cell 7 (variant 3, flags 9)
    WorldStructure imported;
    const WorldImportResult res = Import(Export(original, ctx), imported, ctx);
    ASSERT_TRUE(res.ok);
    EXPECT_FALSE(HasWarning(res, "not registered"));
    const Site* site = imported.sites.Find(StringCRC("Test.Kind"), 7);
    ASSERT_NE(site, nullptr);
    EXPECT_EQ(site->region, 2);
    EXPECT_EQ(site->variant, 3);
    EXPECT_EQ(site->flags, 9);
    EXPECT_TRUE(original.sites == imported.sites);
}

TEST(WorldJsonImport, AC11_UnregisteredKindKeepsItsSitesAndReExportsUnchanged)
{
    const WorldStructure original = MakeTinyWorldWithUnknownKind("Dragon.Hearth");
    const std::string text = Export(original);

    WorldStructure imported;
    const WorldImportResult res = Import(text, imported);   // no registry: Dragon.Hearth and Test.Kind are unknown
    ASSERT_TRUE(res.ok);
    EXPECT_TRUE(HasWarning(res, "world.sites.Dragon.Hearth: kind is not registered"));
    const Site* hearth = imported.sites.Find(StringCRC("Dragon.Hearth"), 8);
    ASSERT_NE(hearth, nullptr);
    EXPECT_EQ(hearth->variant, 4);
    EXPECT_EQ(hearth->flags, 1);
    EXPECT_EQ(hearth->region, 2);
    EXPECT_STREQ(imported.sites.KindName(StringCRC("Dragon.Hearth").Value()), "Dragon.Hearth");
    EXPECT_EQ(Export(imported), text);   // byte for byte: the unknown kind comes back under its own name
}

TEST(WorldJsonImport, AC11_CrcKeyedKindRoundTripsToTheSameKindId)
{
    WorldStructure original = MakeTinyWorld();
    Site s; s.kindCrc = 0x0BADF00Du; s.cell = 1; s.region = 0; s.variant = 2; s.flags = 3;
    original.sites.Add(s);
    const std::string text = Export(original);
    ASSERT_NE(text.find("crc:0badf00d"), std::string::npos);
    WorldStructure imported;
    ASSERT_TRUE(Import(text, imported).ok);
    EXPECT_TRUE(original.sites == imported.sites);
    EXPECT_EQ(Export(imported), text);
}

TEST(WorldJsonImport, MalformedCrcKeysAreTreatedAsKindNames)
{
    // "crc:ZZZZZZZZ" is not a crc key, so it is an ordinary (odd) kind name that hashes to some id.
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r)
    {
        Json::Value list(Json::arrayValue);
        Json::Value site(Json::objectValue);
        site["cell"] = 1; site["region"] = 0; site["variant"] = 0; site["flags"] = 0;
        list.append(site);
        r["world"]["sites"]["crc:ZZZZZZZZ"] = list;
    });
    WorldStructure imported;
    ASSERT_TRUE(Import(text, imported).ok);
    EXPECT_NE(imported.sites.Find(StringCRC("crc:ZZZZZZZZ"), 1), nullptr);
}

TEST(WorldJsonImport, EmptySiteListsAreAccepted)
{
    const std::string text = Edited(MakeTinyWorld(), [](Json::Value& r) { r["world"]["sites"]["Empty.Kind"] = Json::Value(Json::arrayValue); });
    WorldStructure imported;
    ASSERT_TRUE(Import(text, imported).ok);
    EXPECT_EQ(imported.sites.Count(), MakeTinyWorld().sites.Count());
}

TEST(WorldJsonImport, ImportReplacesTheDestinationWorldCompletely)
{
    WorldStructure out = MakeTinyWorldWithUnknownKind("Old.Kind");
    WorldStructure small;
    small.Resize(GridDimensions{ 2, 2 });
    const std::string text = Export(small);
    ASSERT_TRUE(Import(text, out).ok);
    EXPECT_EQ(out.dimensions.width, 2);
    EXPECT_EQ(out.sites.Count(), 0);
    EXPECT_EQ(out.regions.regions.Size(), 0u);
    EXPECT_EQ(out.sites.KindName(StringCRC("Old.Kind").Value()), nullptr);
}
