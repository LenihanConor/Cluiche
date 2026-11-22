// S-1 Terrain Profile (biome-registry plan Tasks 30, 30a): terrain_type enum + five terrain.* maps in the sidecar.
// The (key, default) literals below are copied from the former terrain navigation file and the spec table, NOT from the sidecar.
#include <gtest/gtest.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <memory>
#include <string>

using Dia::Attribute::ParameterFile;
using Dia::Attribute::ParamError;
using Dia::Core::StringCRC;
using namespace CoW::WorldGeneration;

namespace
{
    constexpr int kN = static_cast<int>(TerrainType::Count);   // 12

    const char* const kMapNames[5] = { "terrain.passable", "terrain.cost", "terrain.soil_depth_mult",
                                       "terrain.orographic_factor", "terrain.runoff" };

    // In TerrainType (enum) order. Copied from the former terrain navigation file (passable/cost) and the spec table.
    //                              Deep   Shal   Lake   River  Lowl   Hills  High   Mount  Peak   Cliff  Pass   Volc
    const bool  kPassable[kN] = { false, false, false, true,  true,  true,  true,  true,  false, false, true,  false };
    const float kCost[kN]     = { 1.0f,  1.0f,  1.0f,  1.5f,  1.0f,  1.5f,  2.0f,  4.0f,  1.0f,  1.0f,  1.0f,  1.0f };
    const float kSoil[kN]     = { 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  0.85f, 0.7f,  0.4f,  0.2f,  0.2f,  1.0f,  1.0f };
    const float kOro[kN]      = { 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f };
    const float kRunoff[kN]   = { 0.5f,  0.5f,  0.5f,  0.5f,  0.4f,  0.5f,  0.5f,  0.5f,  0.9f,  0.9f,  0.5f,  0.5f };

    std::string ColumnKey(const char* map, int i)
    {
        return std::string(map) + "." + TerrainTypeName(static_cast<TerrainType>(i));
    }

    float Default(const ParameterFile& f, const char* map, int i)
    {
        const auto* def = f.Schema().FindAttribute(StringCRC(ColumnKey(map, i).c_str()));
        EXPECT_NE(def, nullptr) << ColumnKey(map, i);
        return def ? def->default_value : -999.0f;
    }

    Json::Value ParseSidecar()
    {
        Json::Value root;
        Json::CharReaderBuilder b;
        std::string errs;
        const std::string& text = TestSidecarText();
        std::unique_ptr<Json::CharReader> r(b.newCharReader());
        EXPECT_TRUE(r->parse(text.data(), text.data() + text.size(), &root, &errs)) << errs;
        return root;
    }

    Json::Value* FindEntry(Json::Value& root, const char* key)
    {
        for (Json::Value& e : root["entries"])
            if (e["key"].asString() == key) return &e;
        return nullptr;
    }

    // The parity rule itself (AC 15), over a parsed sidecar: same ids, same order as the code enum.
    bool EnumEntryMatchesCode(const Json::Value& sidecar, std::string& why)
    {
        for (const Json::Value& e : sidecar["entries"])
        {
            if (e["key"].asString() != "terrain_type") continue;
            const Json::Value& vals = e["values"];
            if (vals.size() != static_cast<Json::ArrayIndex>(kN)) { why = "count " + std::to_string(vals.size()); return false; }
            for (int i = 0; i < kN; ++i)
            {
                if (vals[static_cast<Json::ArrayIndex>(i)]["id"].asString() != TerrainTypeName(static_cast<TerrainType>(i)))
                { why = "position " + std::to_string(i); return false; }
            }
            return true;
        }
        why = "no terrain_type entry";
        return false;
    }

    bool LoadText(const Json::Value& sidecar, ParameterFile& f, ParamError& err)
    {
        const std::string text = Json::writeString(Json::StreamWriterBuilder(), sidecar);
        return f.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err);
    }
}

// AC 12 bullet 1: all 5 maps load, 12 keys each, no key source needed (keyEnum).
TEST(TerrainProfile, FiveMapsLoadWithTwelveKeysEach)
{
    const ParameterFile& f = TestSidecarFile();
    for (const char* map : kMapNames)
    {
        int count = 0;
        const std::string prefix = std::string(map) + ".";
        for (unsigned int i = 0; i < f.Schema().GetAttributeCount(); ++i)
        {
            const std::string k = f.Schema().GetAttributeByIndex(i).fullKey.AsCStr();
            if (k.rfind(prefix, 0) == 0) ++count;
        }
        EXPECT_EQ(count, kN) << map;
        for (int i = 0; i < kN; ++i)
            EXPECT_NE(f.Schema().FindAttribute(StringCRC(ColumnKey(map, i).c_str())), nullptr) << ColumnKey(map, i);
    }
}

// Columns come out in enum value order (this is what BindMap's array index relies on).
TEST(TerrainProfile, MapColumnsFollowEnumOrderInTheSchema)
{
    const ParameterFile& f = TestSidecarFile();
    unsigned int base = 0;
    const unsigned int n = f.Schema().GetAttributeCount();
    while (base < n && std::string(f.Schema().GetAttributeByIndex(base).fullKey.AsCStr()) != ColumnKey("terrain.cost", 0)) ++base;
    ASSERT_LT(base + kN, n + 1);
    for (int i = 0; i < kN; ++i)
        EXPECT_STREQ(f.Schema().GetAttributeByIndex(base + i).fullKey.AsCStr(), ColumnKey("terrain.cost", i).c_str());
}

// AC 12 bullet 1: passable and cost defaults equal today's the former terrain navigation file (literals above).
TEST(TerrainProfile, PassableAndCostDefaultsEqualTheOldNavigationFile)
{
    const ParameterFile& f = TestSidecarFile();
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_EQ(Default(f, "terrain.passable", i) != 0.0f, kPassable[i]) << TerrainTypeName(static_cast<TerrainType>(i));
        EXPECT_EQ(Default(f, "terrain.cost", i), kCost[i]) << TerrainTypeName(static_cast<TerrainType>(i));
        EXPECT_EQ(f.Values().GetBaseValue(StringCRC(ColumnKey("terrain.cost", i).c_str())), kCost[i]);
    }
}

// AC 12 bullet 1: the three provisional maps carry the spec defaults.
TEST(TerrainProfile, ProvisionalMapsEqualTheSpecDefaults)
{
    const ParameterFile& f = TestSidecarFile();
    for (int i = 0; i < kN; ++i)
    {
        const char* n = TerrainTypeName(static_cast<TerrainType>(i));
        EXPECT_EQ(Default(f, "terrain.soil_depth_mult", i), kSoil[i]) << n;
        EXPECT_EQ(Default(f, "terrain.orographic_factor", i), kOro[i]) << n;
        EXPECT_EQ(Default(f, "terrain.runoff", i), kRunoff[i]) << n;
    }
}

// Spec ranges: cost [1,16], soil [0,4], orographic [0,4], runoff [0,1].
TEST(TerrainProfile, MapRangesMatchTheSpec)
{
    const ParameterFile& f = TestSidecarFile();
    struct R { const char* map; float lo, hi; };
    const R ranges[] = { { "terrain.cost", 1.0f, 16.0f }, { "terrain.soil_depth_mult", 0.0f, 4.0f },
                         { "terrain.orographic_factor", 0.0f, 4.0f }, { "terrain.runoff", 0.0f, 1.0f } };
    for (const R& r : ranges)
        for (int i = 0; i < kN; ++i)
        {
            const auto* def = f.Schema().FindAttribute(StringCRC(ColumnKey(r.map, i).c_str()));
            ASSERT_NE(def, nullptr) << ColumnKey(r.map, i);
            EXPECT_EQ(def->minimum_value, r.lo) << r.map;
            EXPECT_EQ(def->maximum_value, r.hi) << r.map;
        }
}

// AC 15: the sidecar's terrain_type enum entry equals the code enum (ids and order).
TEST(TerrainProfile, SidecarEnumMatchesTerrainTypeEnum)
{
    Json::Value sidecar = ParseSidecar();
    std::string why;
    EXPECT_TRUE(EnumEntryMatchesCode(sidecar, why)) << why;
    EXPECT_EQ(kN, 12);
}

// AC 15 negative: the parity check really fails on a reorder and on an added value.
TEST(TerrainProfile, ParityCheckFailsOnReorderedOrAddedEnumValue)
{
    Json::Value reordered = ParseSidecar();
    Json::Value* e = FindEntry(reordered, "terrain_type");
    ASSERT_NE(e, nullptr);
    std::swap((*e)["values"][0u], (*e)["values"][1u]);
    std::string why;
    EXPECT_FALSE(EnumEntryMatchesCode(reordered, why));

    Json::Value added = ParseSidecar();
    Json::Value extra(Json::objectValue);
    extra["id"] = "Swamp";
    (*FindEntry(added, "terrain_type"))["values"].append(extra);
    EXPECT_FALSE(EnumEntryMatchesCode(added, why));
}

// AC 11 / 14 / 12: a terrain map whose default omits a TerrainType key fails load, naming the column.
TEST(TerrainProfile, MapMissingAKeyFailsLoad)
{
    for (const char* map : kMapNames)
    {
        Json::Value sidecar = ParseSidecar();
        Json::Value* m = FindEntry(sidecar, map);
        ASSERT_NE(m, nullptr) << map;
        (*m)["default"].removeMember("Volcano");
        ParameterFile f; ParamError err;
        EXPECT_FALSE(LoadText(sidecar, f, err)) << map;
        EXPECT_STREQ(err.key.AsCStr(), ColumnKey(map, static_cast<int>(TerrainType::Volcano)).c_str());
    }
}

// A map default key that is no enum value fails load too (data.unresolved-map-key in the CLI).
TEST(TerrainProfile, MapWithAnUnknownKeyFailsLoad)
{
    Json::Value sidecar = ParseSidecar();
    (*FindEntry(sidecar, "terrain.runoff"))["default"]["Swamp"] = 0.5;
    ParameterFile f; ParamError err;
    EXPECT_FALSE(LoadText(sidecar, f, err));
}

// Binding (Task 30/31): the five maps bind into TerrainRules arrays via BindMap; coverage stays clean and the
// snapshot equals the defaults. New members (names proposed): terrainPassable, terrainCost, terrainSoilDepthMult,
// terrainOrographicFactor, terrainRunoff, each [kTerrainTypeCount].
TEST(TerrainProfile, FromParameterFileFillsTheProfileArrays)
{
    TerrainRules r;
    ASSERT_TRUE(TerrainRules::FromParameterFile(TestSidecarFile(), r));
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_EQ(r.terrainPassable[i], kPassable[i]) << i;
        EXPECT_EQ(r.terrainCost[i], kCost[i]) << i;
        EXPECT_EQ(r.terrainSoilDepthMult[i], kSoil[i]) << i;
        EXPECT_EQ(r.terrainOrographicFactor[i], kOro[i]) << i;
        EXPECT_EQ(r.terrainRunoff[i], kRunoff[i]) << i;
    }
}

TEST(TerrainProfile, BindingCoversTheSidecarIncludingTheMaps)
{
    auto report = std::make_unique<Dia::Attribute::BindingReport>();
    EXPECT_TRUE(TerrainRules::ResolveBindings(TestSidecarFile().Schema(), *report));
    EXPECT_EQ(report->unboundParameters.Size(), 0u);
    EXPECT_EQ(report->countMismatches.Size(), 0u);
    EXPECT_TRUE(report->IsClean());
}
