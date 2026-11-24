// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
// The shipped worldgen.schema.json must describe every WorldLayerSource layer.
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/WorldLayerSource.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/SchemaTempDirectory.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>

using namespace Draoi::HexWorld;

namespace
{
std::filesystem::path FindFromRepoRoot(const char* relative)
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / relative;
        if (std::filesystem::exists(candidate, ec))
            return candidate;
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }
    return {};
}

std::string ReadFile(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Loaded
{
    std::string text;
    Json::Value root;
    std::set<std::string> rampIds;
    bool ok = false;
};

const Loaded& Get()
{
    static Loaded l;
    static bool done = false;
    if (done) return l;
    done = true;
    const auto schemaPath = FindFromRepoRoot("Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json");
    const auto rampPath = FindFromRepoRoot("Dia/DiaValueSchema/Data/ramps.json");
    if (schemaPath.empty() || rampPath.empty()) return l;
    l.text = ReadFile(schemaPath);
    Json::Value ramps;
    Json::Reader reader;
    if (!reader.parse(l.text, l.root) || !reader.parse(ReadFile(rampPath), ramps)) return l;
    for (const Json::Value& r : ramps["ramps"])
        l.rampIds.insert(r["id"].asString());
    l.ok = true;
    return l;
}

// "worldgen.terrain_type" -> "terrain_type" (the schema entry key)
std::string Field(const char* layerId) { return std::string(layerId).substr(std::string(kWorldLayerIdPrefix).size()); }

const Json::Value* FindEntry(const Loaded& l, const std::string& key)
{
    for (const Json::Value& e : l.root["entries"])
        if (e["key"].asString() == key) return &e;
    return nullptr;
}
}

TEST(WorldGenSchema, FileLoadsThroughValueSchemaLoader)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok) << "worldgen.schema.json or ramps.json not found/parsable";
    const Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(l.text.c_str(), "worldgen.schema.json");
    ASSERT_TRUE(r.mOk) << r.mFailure.mEntry.AsCStr() << ": " << r.mFailure.mReason.AsCStr();
    EXPECT_STREQ(r.mFor.AsCStr(), kWorldLayerSchemaFor);
    EXPECT_EQ(r.mEntries.Size(), WorldLayerSource::Count() + 1u /* + the shared strata_set entry of the dynamic extra-stratum layers */);
    EXPECT_EQ(r.mWarnings.Size(), 0u);
}

TEST(WorldGenSchema, EveryDescriptorHasAMatchingEntry)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    EXPECT_EQ(l.root["entries"].size(), WorldLayerSource::Count() + 1u /* + strata_set */);
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(i);
        const Json::Value* e = FindEntry(l, Field(d.id));
        ASSERT_NE(e, nullptr) << d.id;
        EXPECT_TRUE((*e)["label"].isString() && !(*e)["label"].asString().empty()) << d.id;
        EXPECT_TRUE((*e)["description"].isString() && !(*e)["description"].asString().empty()) << d.id;
        if (d.kind == WorldLayerKind::Float)
        {
            EXPECT_EQ((*e)["type"].asString(), "float") << d.id;
            EXPECT_TRUE((*e)["min"].isNumeric() && (*e)["max"].isNumeric()) << d.id;
            EXPECT_LT((*e)["min"].asDouble(), (*e)["max"].asDouble()) << d.id;
        }
        else
        {
            EXPECT_EQ((*e)["type"].asString(), "enum") << d.id;
            if (std::string(d.id) == "worldgen.biome_index")
            {
                // Registry-sized: filled from biomes.json by valuesFrom (Biome Registry task 26), no literal values.
                EXPECT_FALSE(e->isMember("values")) << d.id;
                EXPECT_TRUE(e->isMember("valuesFrom")) << d.id;
                continue;
            }
            ASSERT_TRUE((*e)["values"].isArray()) << d.id;
            EXPECT_EQ((*e)["values"].size(), d.categoryCount) << d.id;
            for (const Json::Value& v : (*e)["values"])
                EXPECT_TRUE(v["colour"].isString() && v["colour"].asString().size() == 7) << d.id;
        }
    }
}

TEST(WorldGenSchema, EnumCountsEqualCodeEnumSizes)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    auto count = [&](const char* key) { const Json::Value* e = FindEntry(l, key); return e ? (*e)["values"].size() : 0u; };
    EXPECT_EQ(count("terrain_type"), static_cast<unsigned>(TerrainType::Count));
    EXPECT_EQ(count("water_source"), static_cast<unsigned>(WaterSource::River) + 1u);
    EXPECT_EQ(count("stratum"), static_cast<unsigned>(kStratumAncient) + 1u);
    EXPECT_EQ(count("flow_direction"), 7u);   // 6 hex directions + sink (kFlowSink = 6)
    EXPECT_EQ(count("biome_index"), 0u);   // no literal values: valuesFrom the biome registry
    EXPECT_EQ(count("fixup_changes"), 3u);   // none, zone, neighbour
    EXPECT_EQ(count("regions"), kRegionPaletteSlots);   // none + 63 cycling region slots
}

TEST(WorldGenSchema, EveryRampExists)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    ASSERT_FALSE(l.rampIds.empty());
    for (const Json::Value& e : l.root["entries"])
    {
        const Json::Value& ramp = e["ui"]["ramp"];
        if (e["type"].asString() == "float")
        {
            ASSERT_TRUE(ramp.isString()) << e["key"].asString();
            EXPECT_EQ(l.rampIds.count(ramp.asString()), 1u) << e["key"].asString() << " ramp " << ramp.asString();
        }
    }
}

TEST(WorldGenSchema, ExactlyOneDefaultBaseAndItIsRelief)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    int n = 0;
    for (const Json::Value& e : l.root["entries"])
    {
        if (e["ui"]["default_base"].asBool())
        {
            ++n;
            EXPECT_EQ(e["key"].asString(), "relief");
        }
    }
    EXPECT_EQ(n, 1);
    const Json::Value* relief = FindEntry(l, "relief");
    ASSERT_NE(relief, nullptr);
    EXPECT_EQ((*relief)["ui"]["elevation_layer"].asString(), "worldgen.elevation");
    EXPECT_EQ((*relief)["ui"]["ramp"].asString(), "sequential.terrain");
    ASSERT_NE(FindEntry(l, "elevation"), nullptr);
}

TEST(WorldGenSchema, InsetAndBackdropOnEveryLayer)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    for (const Json::Value& e : l.root["entries"])
    {
        const Json::Value& inset = e["ui"]["inset"];
        ASSERT_TRUE(inset.isNumeric()) << e["key"].asString();
        EXPECT_GT(inset.asDouble(), 0.0) << e["key"].asString();
        EXPECT_LT(inset.asDouble(), 0.5) << e["key"].asString();
        EXPECT_TRUE(e["ui"]["backdrop"].isString() && e["ui"]["backdrop"].asString().size() == 7) << e["key"].asString();
    }
}

TEST(WorldGenSchema, BiomeLayersAreDescribed)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    for (const char* k : { "temperature", "moisture", "water_distance", "air_moisture", "rain_out", "biome_score", "biome_pin", "biome_index", "fixup_changes" })
    {
        EXPECT_NE(FindEntry(l, k), nullptr) << k;
        EXPECT_NE(WorldLayerSource::Find((std::string(kWorldLayerIdPrefix) + k).c_str()), nullptr) << k;
    }
    // The biomeIndex legend now comes from the registry (valuesFrom); distinct registry colours.
    const Draoi::HexWorld::BiomeRegistry& reg = Draoi::HexWorld::Testing::ShippedBiomeRegistry();
    std::set<uint32_t> colours;
    for (uint32_t i = 0; i < reg.Count(); ++i)
        colours.insert(reg.Colour(static_cast<Draoi::HexWorld::BiomeIndex>(i)));
    EXPECT_EQ(colours.size(), static_cast<size_t>(reg.Count())) << "biome colours must be distinct";
}

TEST(WorldGenSchema, RegionLayerHas64DistinctColoursAndNoneIsDark)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    const Json::Value* e = FindEntry(l, "regions");
    ASSERT_NE(e, nullptr);
    ASSERT_EQ((*e)["values"].size(), kRegionPaletteSlots);
    std::set<std::string> colours;
    for (const Json::Value& v : (*e)["values"])
        colours.insert(v["colour"].asString());
    EXPECT_EQ(colours.size(), static_cast<size_t>(kRegionPaletteSlots)) << "slot colours must be distinct";
    EXPECT_EQ((*e)["values"][0u]["id"].asString(), "none");
    const unsigned none = std::stoul((*e)["values"][0u]["colour"].asString().substr(1), nullptr, 16);
    EXPECT_LT(((none >> 16) & 0xFF) + ((none >> 8) & 0xFF) + (none & 0xFF), 3u * 0x50u) << "region 0 / ocean is dark, unlike any region";
}

namespace
{
// Loads the shipped worldgen.schema.json + biomes.schema.json beside a biomes.json copy through the directory loader (which resolves
// valuesFrom), and returns the resolved biomeIndex entry values.
std::vector<Dia::ValueSchema::EnumValue> ResolvedBiomeLegend(const char* dirName, const std::string& biomesJsonPath)
{
    Dia::ValueSchema::Testing::SchemaTempDirectory dir(dirName);
    dir.Write("worldgen.schema.json", ReadFile(FindFromRepoRoot("Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json")));
    dir.Write("World/biomes.schema.json", ReadFile(FindFromRepoRoot("Cluiche/Assets/CoW/Global/World/biomes.schema.json")));
    dir.Write("World/biomes.json", ReadFile(biomesJsonPath));
    Dia::ValueSchema::SchemaDirectoryLoader loader;
    loader.AddRoot(dir.Path().c_str());
    Dia::ValueSchema::ValueSchemaRegistry reg;
    const Dia::ValueSchema::SchemaLoadStats stats = loader.LoadInto(reg);
    std::vector<Dia::ValueSchema::EnumValue> out;
    EXPECT_EQ(stats.mValuesFromFailed, 0u);
    const Dia::ValueSchema::ValueInfo* info = reg.Find(Dia::Core::StringCRC((std::string(kWorldLayerSchemaFor) + "/biome_index").c_str()));
    if (info == nullptr) { ADD_FAILURE() << "biomeIndex not registered"; return out; }
    for (unsigned i = 0; i < info->mEnumValues.Size(); ++i)
        out.push_back(info->mEnumValues[i]);
    return out;
}

void ExpectLegendMatches(const std::vector<Dia::ValueSchema::EnumValue>& legend, const Draoi::HexWorld::BiomeRegistry& reg)
{
    ASSERT_EQ(legend.size(), reg.Count());
    for (uint32_t i = 0; i < reg.Count(); ++i)
    {
        const Draoi::HexWorld::BiomeIndex b = static_cast<Draoi::HexWorld::BiomeIndex>(i);
        EXPECT_STREQ(legend[i].mLabel.AsCStr(), reg.Name(b)) << i;
        EXPECT_EQ(static_cast<uint32_t>(std::strtoul(legend[i].mColour.AsCStr() + 1, nullptr, 16)), reg.Colour(b)) << i;
    }
}
}

TEST(WorldGenSchema, BiomeIndexEntryHasNoLiteralValues)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    const Json::Value* e = FindEntry(l, "biome_index");
    ASSERT_NE(e, nullptr);
    EXPECT_FALSE(e->isMember("values"));
    ASSERT_TRUE(e->isMember("valuesFrom"));
    EXPECT_EQ((*e)["valuesFrom"]["format"].asString(), "cow.biomes");
}

TEST(WorldGenSchema, BiomeLegendListsRegistryNamesAndColoursInIndexOrder)
{
    const Draoi::HexWorld::BiomeRegistry& reg = Draoi::HexWorld::Testing::ShippedBiomeRegistry();
    ASSERT_EQ(reg.Count(), 13u);
    ExpectLegendMatches(ResolvedBiomeLegend("WgLegendShipped", Draoi::HexWorld::Testing::ShippedBiomesPath()), reg);
}

TEST(WorldGenSchema, BiomeLegendFollowsAFourteenRowRegistry)
{
    const Draoi::HexWorld::BiomeRegistry& reg = Draoi::HexWorld::Testing::ExtraBiomeRegistry();
    ASSERT_EQ(reg.Count(), 14u);
    const std::string path = FindFromRepoRoot("Cluiche/Tests/GoogleTests/CoW/Fixtures/biomes_extra.json").string();
    ExpectLegendMatches(ResolvedBiomeLegend("WgLegendExtra", path), reg);
}

TEST(WorldGenSchema, BiomeIndexCategoryCountComesFromTheRegistry)
{
    const WorldLayerDesc* d = WorldLayerSource::Find("worldgen.biome_index");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(WorldLayerSource::CategoryCount(*d, Draoi::HexWorld::Testing::ShippedBiomeRegistry()), 13u);
    EXPECT_EQ(WorldLayerSource::CategoryCount(*d, Draoi::HexWorld::Testing::ExtraBiomeRegistry()), 14u);
    const WorldLayerDesc* terrain = WorldLayerSource::Find("worldgen.terrain_type");
    ASSERT_NE(terrain, nullptr);
    EXPECT_EQ(WorldLayerSource::CategoryCount(*terrain, Draoi::HexWorld::Testing::ExtraBiomeRegistry()), terrain->categoryCount);
}
