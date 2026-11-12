// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/json-export-import.md (AC1, AC8 export side, Export Implementation)
// WorldJson export: deterministic readable text, kind-name resolution, refusals.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldJson.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>

#include <DiaCore/Json/external/json/json.h>

#include <cmath>
#include <limits>
#include <set>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{

Json::Value Parse(const std::string& text)
{
    Json::Value root;
    Json::CharReaderBuilder b;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(b.newCharReader());
    EXPECT_TRUE(reader->parse(text.data(), text.data() + text.size(), &root, &errors)) << errors;
    return root;
}

std::string Export(const WorldStructure& w, const WorldJsonContext& ctx = {})
{
    std::string text;
    JsonIssue error;
    EXPECT_TRUE(ExportWorldText(w, ctx, text, error)) << error.field << ": " << error.message;
    return text;
}

int LineCount(const std::string& s)
{
    int n = 0;
    for (char c : s) { n += c == '\n'; }
    return n;
}

} // namespace

TEST(WorldJsonExport, AC1_ProducesValidJsonWithTheSchemaEnvelope)
{
    const std::string text = Export(MakeTinyWorld());
    const Json::Value root = Parse(text);
    ASSERT_TRUE(root.isObject());
    EXPECT_EQ(root["format"].asString(), "cow.world");
    EXPECT_EQ(root["version"].asUInt(), 1u);
    EXPECT_EQ(root["world"]["topology"].asString(), "hex_flat_odd_q");
    EXPECT_EQ(root["world"]["dimensions"]["width"].asInt(), 4);
    EXPECT_EQ(root["world"]["dimensions"]["height"].asInt(), 3);
    EXPECT_EQ(root["world"]["main_landmass"].asUInt(), 1u);
    EXPECT_EQ(root["world"]["cells"].size(), 12u);
    EXPECT_EQ(root["world"]["regions"].size(), 2u);
    EXPECT_TRUE(root["world"]["sites"].isObject());
}

TEST(WorldJsonExport, AC1_IsPrettyPrintedAndUsesReadableNames)
{
    const std::string text = Export(MakeTinyWorld());
    EXPECT_GT(LineCount(text), 12 + 2 + 10);   // one line per cell and region plus structure: not a minified one-liner
    EXPECT_EQ(text.back(), '\n');
    EXPECT_EQ(text.find('\r'), std::string::npos);
    EXPECT_EQ(text.find("\t"), std::string::npos);
    for (const char* name : { "Biome.Grassland", "Biome.TemperateForest", "Biome.Lake", "Biome.Wetland", "Biome.Alpine",
                              "Biome.Desert", "Biome.Tundra", "Biome.Rainforest", "Biome.Volcanic", "Biome.Ocean",
                              "Lowland", "Hills", "Mountain", "Peak", "Pass", "Volcano", "Cliff", "DeepWater", "ShallowWater",
                              "FirstShores", "WildReaches", "AncientInteriors", "CoW.Node.Corridor", "CoW.Node.Resource" })
    {
        EXPECT_NE(text.find(name), std::string::npos) << name;
    }
    // Names, not integer ids: every cell's biome and terrain are strings.
    const Json::Value root = Parse(text);
    for (const Json::Value& cell : root["world"]["cells"])
    {
        EXPECT_TRUE(cell["biome"].isString());
        EXPECT_TRUE(cell["terrain"].isString());
        EXPECT_TRUE(cell["water_source"].isString());
        EXPECT_TRUE(cell["depth_zone"].isString());
    }
}

TEST(WorldJsonExport, SameWorldExportsByteIdenticalTextEveryTime)
{
    const WorldStructure w = MakeTinyWorld();
    const std::string a = Export(w);
    const std::string b = Export(w);
    const WorldStructure copy = w;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, Export(copy));
}

TEST(WorldJsonExport, GeneratedWorldExportsIdenticallyTwice)
{
    const WorldStructure w = GenerateWorld(7);
    ASSERT_GT(w.regions.regions.Size(), 0u);
    ASSERT_GT(w.sites.Count(), 0);
    EXPECT_EQ(Export(w), Export(GenerateWorld(7)));
}

TEST(WorldJsonExport, EveryCellFieldIsWritten)
{
    const WorldStructure w = MakeTinyWorld();
    const Json::Value root = Parse(Export(w));
    const Json::Value& cells = root["world"]["cells"];
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const Cell& c = w.Cells()[i];
        const Json::Value& j = cells[i];
        EXPECT_EQ(j["biome"].asString(), WorldJsonBiomeName(c.biome)) << i;
        EXPECT_EQ(j["terrain"].asString(), TerrainTypeName(c.terrain)) << i;
        EXPECT_EQ(j["region"].asUInt(), c.region) << i;
        EXPECT_EQ(static_cast<float>(j["elevation"].asDouble()), c.elevation) << i;
        EXPECT_EQ(static_cast<float>(j["latitude"].asDouble()), c.latitude) << i;
        EXPECT_EQ(j["water_source"].asString(), WaterSourceName(c.waterSource)) << i;
        EXPECT_EQ(j["depth_zone"].asString(), DepthZoneName(c.depthZone)) << i;
        EXPECT_EQ(j["landmass"].asUInt(), c.landmass) << i;
        EXPECT_EQ(j["terrain_flags"].asUInt(), c.terrainFlags) << i;
        EXPECT_EQ(j["downstream"].asUInt(), c.downstream) << i;
    }
}

TEST(WorldJsonExport, AC8_FloatsAreWrittenWithNineSignificantDigitsAndReadBackExactly)
{
    WorldStructure w = MakeTinyWorld();
    const float values[] = { 0.0f, 1.0f, 0.1f, 1.0f / 3.0f, 0.99999994f, 1.17549435e-38f, 1.0e-45f, 0.30000001192092896f,
                             std::nextafter(0.5f, 1.0f), std::nextafter(0.5f, 0.0f), 0.123456789f, 0.7f };
    for (int i = 0; i < 12; ++i)
    {
        w.Cells()[i].elevation = values[i];
        w.Cells()[i].latitude = values[11 - i];
    }
    const Json::Value root = Parse(Export(w));
    for (int i = 0; i < 12; ++i)
    {
        EXPECT_EQ(static_cast<float>(root["world"]["cells"][i]["elevation"].asDouble()), values[i]) << i;
        EXPECT_EQ(static_cast<float>(root["world"]["cells"][i]["latitude"].asDouble()), values[11 - i]) << i;
    }
}

TEST(WorldJsonExport, ProvenanceIsWrittenWithTheFullParamsBlock)
{
    const Json::Value root = Parse(Export(MakeTinyWorld()));
    const Json::Value& p = root["provenance"];
    EXPECT_EQ(p["seed"].asUInt64(), 0x1234567890ABCDEFull);   // exact 64-bit seed
    EXPECT_EQ(p["generator_version"].asUInt(), kTerrainGeneratorVersion);
    EXPECT_EQ(p["rules_version"].asUInt(), 4u);
    EXPECT_EQ(p["preset_id"].asString(), "tiny");
    const Json::Value& params = p["params"];
    EXPECT_EQ(params.getMemberNames().size(), 20u);
    EXPECT_EQ(params["barrierForm"].asString(), "VolcanicChain");
    EXPECT_EQ(static_cast<float>(params["landFraction"].asDouble()), 0.35f);
    EXPECT_EQ(static_cast<float>(params["orientationDeg"].asDouble()), 123.5f);
    for (const char* key : { "continents", "islands", "continentIslands", "peninsulas", "coastline", "mountainousness", "spineToRing",
                             "passes", "barrier", "ruggedness", "rivers", "valleyDepth", "lakes", "latitudeNorthDeg",
                             "latitudeSouthDeg", "temperatureOffset", "moistureOffset" })
    {
        EXPECT_TRUE(params.isMember(key)) << key;
    }
}

TEST(WorldJsonExport, ContextPresetIdOverridesTheWorldsOwn)
{
    WorldJsonContext ctx;
    ctx.presetId = "continental";
    EXPECT_EQ(Parse(Export(MakeTinyWorld(), ctx))["provenance"]["preset_id"].asString(), "continental");
    WorldStructure none = MakeTinyWorld();
    none.provenance.presetId[0] = '\0';
    EXPECT_EQ(Parse(Export(none))["provenance"]["preset_id"].asString(), "");
}

TEST(WorldJsonExport, RegionsAndRegionTableAreWritten)
{
    const WorldStructure w = MakeTinyWorld();
    const Json::Value root = Parse(Export(w));
    const Json::Value& regions = root["world"]["regions"];
    ASSERT_EQ(regions.size(), 2u);
    EXPECT_EQ(regions[0]["id"].asUInt(), 1u);
    EXPECT_EQ(regions[0]["zone"].asString(), "FirstShores");
    EXPECT_FALSE(regions[0]["is_island"].asBool());
    EXPECT_EQ(regions[0]["primary_biome"].asString(), "Biome.Grassland");
    EXPECT_EQ(static_cast<float>(regions[0]["purity"].asDouble()), 0.25f);
    EXPECT_EQ(regions[0]["cell_count"].asUInt(), 4u);
    EXPECT_EQ(regions[0]["centre_cell"].asUInt(), 3u);
    ASSERT_EQ(regions[0]["neighbours"].size(), 1u);
    EXPECT_EQ(regions[0]["neighbours"][0].asUInt(), 2u);
    const Json::Value& table = root["world"]["region_table"];
    EXPECT_EQ(table["absorbed_cells"].asUInt(), 2u);
    EXPECT_EQ(table["fragment_cells"].asUInt(), 1u);
    EXPECT_EQ(table["limited_by"].asString(), "WildReaches");
}

TEST(WorldJsonExport, SitesAreGroupedByKindWithTheirFields)
{
    const WorldStructure w = MakeTinyWorld();
    const Json::Value root = Parse(Export(w));
    const Json::Value& sites = root["world"]["sites"];
    ASSERT_TRUE(sites.isObject());
    EXPECT_EQ(sites.getMemberNames().size(), 5u);
    const Json::Value& corridor = sites["CoW.Node.Corridor"];
    ASSERT_EQ(corridor.size(), 2u);
    EXPECT_EQ(corridor[0]["cell"].asUInt(), 6u);
    EXPECT_EQ(corridor[0]["region"].asUInt(), 2u);
    EXPECT_EQ(corridor[0]["variant"].asUInt(), 0u);
    EXPECT_EQ(corridor[0]["flags"].asUInt(), 0x0105u);
    EXPECT_EQ(corridor[1]["cell"].asUInt(), 9u);
    EXPECT_EQ(sites["CoW.Node.Resource"][0]["flags"].asUInt(), 65535u);
    EXPECT_EQ(sites["CoW.Node.Resource"][0]["variant"].asUInt(), 7u);
}

TEST(WorldJsonExport, SiteKindKeysAreWrittenInTextOrderNotCrcOrder)
{
    const Json::Value root = Parse(Export(MakeTinyWorld()));
    const std::vector<std::string> names = root["world"]["sites"].getMemberNames();   // jsoncpp sorts: compare to the file text
    const std::string text = Export(MakeTinyWorld());
    size_t last = 0;
    for (const std::string& name : names)
    {
        const size_t at = text.find("\"" + name + "\"", text.find("\"sites\""));
        ASSERT_NE(at, std::string::npos) << name;
        EXPECT_GT(at, last) << name;   // file order == sorted order
        last = at;
    }
}

TEST(WorldJsonExport, KindKeyResolutionOrderIsBuiltinRegistryRememberedThenCrcFallback)
{
    SiteSet sites;
    TestPlacer placer("Test.Registered");
    SiteRegistry registry;
    ASSERT_TRUE(registry.Register(&placer));

    EXPECT_EQ(WorldJsonSiteKindKey(StringCRC("CoW.Node.River").Value(), nullptr, sites), "CoW.Node.River");
    EXPECT_EQ(WorldJsonSiteKindKey(StringCRC("Test.Registered").Value(), &registry, sites), "Test.Registered");
    // Not registered and not remembered: the crc fallback.
    char expected[16];
    std::snprintf(expected, sizeof(expected), "crc:%08x", StringCRC("Test.Registered").Value());
    EXPECT_EQ(WorldJsonSiteKindKey(StringCRC("Test.Registered").Value(), nullptr, sites), expected);
    // Remembered beats the fallback.
    ASSERT_TRUE(sites.RememberKindName(StringCRC("Test.Registered").Value(), "Test.Registered"));
    EXPECT_EQ(WorldJsonSiteKindKey(StringCRC("Test.Registered").Value(), nullptr, sites), "Test.Registered");
    // A registered kind wins over a remembered name (they hash equal, so the text is the same; the order is the contract).
    EXPECT_EQ(WorldJsonSiteKindKey(0xDEADBEEFu, nullptr, sites), "crc:deadbeef");
}

TEST(WorldJsonExport, RememberKindNameRejectsEmptyAndMismatchedNames)
{
    SiteSet sites;
    EXPECT_FALSE(sites.RememberKindName(StringCRC("A.B").Value(), ""));
    EXPECT_FALSE(sites.RememberKindName(StringCRC("A.B").Value(), "A.C"));   // does not hash to the id
    EXPECT_EQ(sites.KindName(StringCRC("A.B").Value()), nullptr);
    EXPECT_TRUE(sites.RememberKindName(StringCRC("A.B").Value(), "A.B"));
    ASSERT_NE(sites.KindName(StringCRC("A.B").Value()), nullptr);
    EXPECT_STREQ(sites.KindName(StringCRC("A.B").Value()), "A.B");
    sites.Clear();
    EXPECT_EQ(sites.KindName(StringCRC("A.B").Value()), nullptr);
}

TEST(WorldJsonExport, RememberedNamesDoNotChangeSiteSetEquality)
{
    SiteSet a, b;
    Site s; s.kindCrc = StringCRC("A.B").Value(); s.cell = 1;
    a.Add(s); b.Add(s);
    a.RememberKindName(s.kindCrc, "A.B");
    EXPECT_TRUE(a == b);
}

TEST(WorldJsonExport, UnknownKindExportsUnderItsRememberedName)
{
    const WorldStructure w = MakeTinyWorldWithUnknownKind("Dragon.Hearth");
    const Json::Value root = Parse(Export(w));
    ASSERT_TRUE(root["world"]["sites"].isMember("Dragon.Hearth"));
    EXPECT_EQ(root["world"]["sites"]["Dragon.Hearth"][0]["cell"].asUInt(), 8u);
    EXPECT_EQ(root["world"]["sites"]["Dragon.Hearth"][0]["variant"].asUInt(), 4u);
}

TEST(WorldJsonExport, KindNameWithQuotesAndBackslashesIsEscaped)
{
    const WorldStructure w = MakeTinyWorldWithUnknownKind("Odd\"Kind\\With\ttab");
    const Json::Value root = Parse(Export(w));   // parses => escaped correctly
    EXPECT_TRUE(root["world"]["sites"].isMember("Odd\"Kind\\With\ttab"));
}

TEST(WorldJsonExport, UnnamedUnknownKindFallsBackToTheCrcKey)
{
    WorldStructure w = MakeTinyWorld();
    Site s; s.kindCrc = 0x0BADF00Du; s.cell = 1; s.region = 0;
    w.sites.Add(s);
    const Json::Value root = Parse(Export(w));
    ASSERT_TRUE(root["world"]["sites"].isMember("crc:0badf00d"));
}

TEST(WorldJsonExport, WorldWithoutRegionsOrSitesExports)
{
    WorldStructure w;
    w.Resize(GridDimensions{ 2, 2 });
    const Json::Value root = Parse(Export(w));
    EXPECT_EQ(root["world"]["cells"].size(), 4u);
    EXPECT_EQ(root["world"]["regions"].size(), 0u);
    EXPECT_TRUE(root["world"]["sites"].isObject());
    EXPECT_EQ(root["world"]["sites"].size(), 0u);
}

TEST(WorldJsonExport, NonFiniteFloatsAreRefusedWithTheFieldNamed)
{
    for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
    {
        WorldStructure w = MakeTinyWorld();
        w.Cells()[5].elevation = bad;
        std::string text = "stale";
        JsonIssue error;
        EXPECT_FALSE(ExportWorldText(w, {}, text, error));
        EXPECT_TRUE(text.empty());
        EXPECT_EQ(error.field, "world.cells[5].elevation");
        EXPECT_FALSE(error.message.empty());

        WorldStructure l = MakeTinyWorld();
        l.Cells()[2].latitude = bad;
        EXPECT_FALSE(ExportWorldText(l, {}, text, error));
        EXPECT_EQ(error.field, "world.cells[2].latitude");
    }
    WorldStructure r = MakeTinyWorld();
    RegionRecord rec = r.regions.regions[0];
    r.regions = RegionTable{};
    rec.purity = std::numeric_limits<float>::quiet_NaN();
    r.regions.AddRegion(rec);
    std::string text; JsonIssue error;
    EXPECT_FALSE(ExportWorldText(r, {}, text, error));
    EXPECT_EQ(error.field, "world.regions[0].purity");
}

TEST(WorldJsonExport, EnumValuesOutOfRangeAreRefused)
{
    std::string text; JsonIssue error;
    {
        WorldStructure w = MakeTinyWorld();
        w.Cells()[3].biome = static_cast<Biome>(200);
        EXPECT_FALSE(ExportWorldText(w, {}, text, error));
        EXPECT_EQ(error.field, "world.cells[3].biome");
    }
    {
        WorldStructure w = MakeTinyWorld();
        w.Cells()[4].terrain = TerrainType::Count;
        EXPECT_FALSE(ExportWorldText(w, {}, text, error));
        EXPECT_EQ(error.field, "world.cells[4].terrain");
    }
    {
        WorldStructure w = MakeTinyWorld();
        w.Cells()[1].waterSource = static_cast<WaterSource>(9);
        EXPECT_FALSE(ExportWorldText(w, {}, text, error));
        EXPECT_EQ(error.field, "world.cells[1].water_source");
    }
    {
        WorldStructure w = MakeTinyWorld();
        w.Cells()[0].depthZone = static_cast<DepthZone>(9);
        EXPECT_FALSE(ExportWorldText(w, {}, text, error));
        EXPECT_EQ(error.field, "world.cells[0].depth_zone");
    }
}

TEST(WorldJsonExport, SiteOutsideTheGridIsRefused)
{
    WorldStructure w = MakeTinyWorld();
    Site s; s.kindCrc = StringCRC("Test.Kind").Value(); s.cell = 12;   // 12 cells: index 12 is out
    w.sites.Add(s);
    std::string text; JsonIssue error;
    EXPECT_FALSE(ExportWorldText(w, {}, text, error));
    EXPECT_NE(error.field.find("world.sites"), std::string::npos);
}

TEST(WorldJsonExport, EmptyWorldIsRefused)
{
    WorldStructure w;   // 0 x 0
    std::string text; JsonIssue error;
    EXPECT_FALSE(ExportWorldText(w, {}, text, error));
    EXPECT_EQ(error.field, "world.dimensions");
}

TEST(WorldJsonExport, NameParsersAreTheExactInverseOfTheWriter)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        Biome out = Biome::Count;
        ASSERT_TRUE(WorldJsonParseBiome(WorldJsonBiomeName(static_cast<Biome>(i)), out));
        EXPECT_EQ(static_cast<int>(out), i);
    }
    for (int i = 0; i < static_cast<int>(TerrainType::Count); ++i)
    {
        TerrainType out = TerrainType::Count;
        ASSERT_TRUE(WorldJsonParseTerrain(TerrainTypeName(static_cast<TerrainType>(i)), out));
        EXPECT_EQ(static_cast<int>(out), i);
    }
    for (int i = 0; i < 4; ++i)
    {
        WaterSource w = WaterSource::None; DepthZone z = DepthZone::None;
        ASSERT_TRUE(WorldJsonParseWaterSource(WaterSourceName(static_cast<WaterSource>(i)), w));
        ASSERT_TRUE(WorldJsonParseDepthZone(DepthZoneName(static_cast<DepthZone>(i)), z));
        EXPECT_EQ(static_cast<int>(w), i);
        EXPECT_EQ(static_cast<int>(z), i);
    }
    Biome b; TerrainType t; WaterSource w; DepthZone z;
    EXPECT_FALSE(WorldJsonParseBiome("", b));
    EXPECT_FALSE(WorldJsonParseBiome("Biome.Swamp", b));
    EXPECT_FALSE(WorldJsonParseBiome("biome.ocean", b));        // case sensitive
    EXPECT_FALSE(WorldJsonParseTerrain("Unknown", t));
    EXPECT_FALSE(WorldJsonParseWaterSource("Swamp", w));
    EXPECT_FALSE(WorldJsonParseDepthZone("None ", z));
    EXPECT_STREQ(WorldJsonBiomeName(Biome::Count), "Unknown");
}

TEST(WorldJsonExport, GoldenTextForTheTinyWorld)
{
    const std::string text = Export(MakeTinyWorld());
    static const char kExpected[] = R"JSON({
  "format": "cow.world",
  "version": 1,
  "provenance": {
    "seed": 1311768467294899695,
    "generator_version": 1,
    "rules_version": 4,
    "preset_id": "tiny",
    "params": {
      "barrier": 0.6,
      "barrierForm": "VolcanicChain",
      "coastline": 0.5,
      "continentIslands": 0.7,
      "continents": 1,
      "islands": 4,
      "lakes": 4,
      "landFraction": 0.35,
      "latitudeNorthDeg": 60,
      "latitudeSouthDeg": 20,
      "moistureOffset": 0,
      "mountainousness": 0.55,
      "orientationDeg": 123.5,
      "passes": 2,
      "peninsulas": 2,
      "rivers": 0.5,
      "ruggedness": 0.4,
      "spineToRing": 0.15,
      "temperatureOffset": 0,
      "valleyDepth": 0.4
    }
  },
  "world": {
    "topology": "hex_flat_odd_q",
    "dimensions": { "width": 4, "height": 3 },
    "main_landmass": 1,
    "cells": [
      { "biome": "Biome.Ocean", "terrain": "DeepWater", "region": 0, "elevation": 0, "latitude": 0.100000001, "water_source": "Ocean", "depth_zone": "None", "landmass": 0, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Ocean", "terrain": "ShallowWater", "region": 0, "elevation": 0, "latitude": 0.200000003, "water_source": "Ocean", "depth_zone": "None", "landmass": 0, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Grassland", "terrain": "Lowland", "region": 1, "elevation": 0.349999994, "latitude": 0.300000012, "water_source": "None", "depth_zone": "FirstShores", "landmass": 1, "terrain_flags": 1, "downstream": 7 },
      { "biome": "Biome.TemperateForest", "terrain": "Hills", "region": 1, "elevation": 0.5, "latitude": 0.400000006, "water_source": "None", "depth_zone": "FirstShores", "landmass": 1, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Lake", "terrain": "Lake", "region": 1, "elevation": 0.449999988, "latitude": 0.5, "water_source": "Lake", "depth_zone": "WildReaches", "landmass": 1, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Wetland", "terrain": "River", "region": 1, "elevation": 0.409999996, "latitude": 0.600000024, "water_source": "River", "depth_zone": "WildReaches", "landmass": 1, "terrain_flags": 0, "downstream": 3 },
      { "biome": "Biome.Alpine", "terrain": "Mountain", "region": 2, "elevation": 0.899999976, "latitude": 0.699999988, "water_source": "None", "depth_zone": "WildReaches", "landmass": 1, "terrain_flags": 6, "downstream": 7 },
      { "biome": "Biome.Desert", "terrain": "Highland", "region": 2, "elevation": 0.699999988, "latitude": 0.800000012, "water_source": "None", "depth_zone": "AncientInteriors", "landmass": 1, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Tundra", "terrain": "Peak", "region": 2, "elevation": 1, "latitude": 0.899999976, "water_source": "None", "depth_zone": "AncientInteriors", "landmass": 1, "terrain_flags": 0, "downstream": 7 },
      { "biome": "Biome.Rainforest", "terrain": "Pass", "region": 2, "elevation": 0.800000012, "latitude": 1, "water_source": "None", "depth_zone": "AncientInteriors", "landmass": 1, "terrain_flags": 8, "downstream": 7 },
      { "biome": "Biome.Volcanic", "terrain": "Volcano", "region": 2, "elevation": 0.850000024, "latitude": 0, "water_source": "None", "depth_zone": "AncientInteriors", "landmass": 1, "terrain_flags": 16, "downstream": 7 },
      { "biome": "Biome.Grassland", "terrain": "Cliff", "region": 0, "elevation": 0.100000001, "latitude": 0.333333343, "water_source": "None", "depth_zone": "WildReaches", "landmass": 2, "terrain_flags": 0, "downstream": 7 }
    ],
    "regions": [
      { "id": 1, "zone": "FirstShores", "landmass": 1, "is_island": false, "primary_biome": "Biome.Grassland", "purity": 0.25, "cell_count": 4, "centre_cell": 3, "neighbours": [2] },
      { "id": 2, "zone": "AncientInteriors", "landmass": 1, "is_island": false, "primary_biome": "Biome.Alpine", "purity": 0.200000003, "cell_count": 5, "centre_cell": 8, "neighbours": [1] }
    ],
    "region_table": { "absorbed_cells": 2, "fragment_cells": 1, "limited_by": "WildReaches" },
    "sites": {
      "CoW.Node.Corridor": [
        { "cell": 6, "region": 2, "variant": 0, "flags": 261 },
        { "cell": 9, "region": 2, "variant": 1, "flags": 2 }
      ],
      "CoW.Node.Hunting": [
        { "cell": 2, "region": 1, "variant": 0, "flags": 0 }
      ],
      "CoW.Node.Resource": [
        { "cell": 3, "region": 1, "variant": 7, "flags": 65535 }
      ],
      "CoW.Node.River": [
        { "cell": 5, "region": 1, "variant": 0, "flags": 0 }
      ],
      "crc:d8cc72f4": [
        { "cell": 7, "region": 2, "variant": 3, "flags": 9 }
      ]
    }
  }
}
)JSON";
    EXPECT_EQ(text, kExpected);
}
