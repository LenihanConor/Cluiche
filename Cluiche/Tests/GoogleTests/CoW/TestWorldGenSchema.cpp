// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
// The shipped worldgen.schema.json must describe every WorldLayerSource layer.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldLayerSource.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaValueSchema/ValueSchemaLoader.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace CoW::WorldGeneration;

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
    EXPECT_EQ(r.mEntries.Size(), WorldLayerSource::Count());
    EXPECT_EQ(r.mWarnings.Size(), 0u);
}

TEST(WorldGenSchema, EveryDescriptorHasAMatchingEntry)
{
    const Loaded& l = Get();
    ASSERT_TRUE(l.ok);
    EXPECT_EQ(l.root["entries"].size(), WorldLayerSource::Count());
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(i);
        const Json::Value* e = FindEntry(l, d.id);
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
    EXPECT_EQ(count("terrainType"), static_cast<unsigned>(TerrainType::Count));
    EXPECT_EQ(count("waterSource"), static_cast<unsigned>(WaterSource::River) + 1u);
    EXPECT_EQ(count("depthZone"), static_cast<unsigned>(DepthZone::AncientInteriors) + 1u);
    EXPECT_EQ(count("flowDirection"), 7u);   // 6 hex directions + sink (kFlowSink = 6)
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
    EXPECT_EQ((*relief)["ui"]["elevation_layer"].asString(), "elevation");
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
