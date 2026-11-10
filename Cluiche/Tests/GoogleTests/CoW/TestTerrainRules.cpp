// S-1 Terrain Generator task 2: table-driven TerrainRules.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainRules.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace CoW::WorldGeneration;

namespace {

bool HasWarning(const RuleWarnings& w, const char* text)
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

std::filesystem::path FindShippedRules()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/terrain_rules.json";
        if (std::filesystem::exists(candidate, ec))
        {
            return candidate;
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

} // namespace

TEST(TerrainRules, DefaultsMatchTable)
{
    const TerrainRules r = TerrainRules::Defaults();
    EXPECT_EQ(r.rulesVersion, 2u);
    EXPECT_FALSE(r.degraded);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(r.values[i], GetRuleDef(i).defaultValue) << GetRuleDef(i).key;
    }
    EXPECT_FLOAT_EQ(r.kCoastElev(), 0.02f);
    EXPECT_FLOAT_EQ(r.kFlowMax(), 400.0f);
    EXPECT_EQ(r.GetInt(Rule::kBorderWidth), 2);
}

TEST(TerrainRules, DefaultsAreWithinOwnRange)
{
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        const RuleDef& d = GetRuleDef(i);
        EXPECT_LE(d.minValue, d.defaultValue) << d.key;
        EXPECT_GE(d.maxValue, d.defaultValue) << d.key;
        EXPECT_LT(d.minValue, d.maxValue) << d.key;
    }
}

TEST(TerrainRules, KeysAreUniqueAndNonEmpty)
{
    std::set<std::string> keys;
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        const std::string k = GetRuleDef(i).key;
        EXPECT_FALSE(k.empty());
        EXPECT_TRUE(keys.insert(k).second) << "duplicate key " << k;
    }
    EXPECT_EQ(keys.size(), static_cast<size_t>(kRuleCount));
    EXPECT_EQ(keys.count("rulesVersion"), 0u);
}

TEST(TerrainRules, DefaultsRoundTripThroughJson)
{
    const TerrainRules a = TerrainRules::Defaults();
    const Json::Value json = a.ToJson();
    EXPECT_EQ(json.getMemberNames().size(), static_cast<size_t>(kRuleCount) + 1);

    TerrainRules b;
    RuleWarnings warnings;
    EXPECT_TRUE(b.LoadFromJson(json, warnings));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(b.degraded);
    EXPECT_EQ(b.rulesVersion, a.rulesVersion);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(b.values[i], a.values[i]) << GetRuleDef(i).key;
    }
}

TEST(TerrainRules, MissingKeyUsesDefaultWithWarning)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json["kEdgeFade"] = 0.9;
    json.removeMember("kCoastElev");

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_FLOAT_EQ(r.kCoastElev(), 0.02f);
    EXPECT_FLOAT_EQ(r.kEdgeFade(), 0.9f);
    EXPECT_TRUE(HasWarning(warnings, "missing:kCoastElev"));
    EXPECT_EQ(warnings.Size(), 1u);
    EXPECT_FALSE(r.degraded);
}

TEST(TerrainRules, OutOfRangeIsClampedWithWarningAndDegraded)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json["kEdgeFade"] = 5.0;     // max 1
    json["kMinPiece"] = -3.0;    // min 1

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_FLOAT_EQ(r.kEdgeFade(), GetRuleDef(Rule::kEdgeFade).maxValue);
    EXPECT_FLOAT_EQ(r.kMinPiece(), GetRuleDef(Rule::kMinPiece).minValue);
    EXPECT_TRUE(HasWarning(warnings, "range:kEdgeFade"));
    EXPECT_TRUE(HasWarning(warnings, "range:kMinPiece"));
    EXPECT_TRUE(r.degraded);
}

TEST(TerrainRules, NonNumericValueFallsBackToDefaultAndDegrades)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json["kEdgeFade"] = "high";

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_FLOAT_EQ(r.kEdgeFade(), 0.5f);
    EXPECT_TRUE(HasWarning(warnings, "invalid:kEdgeFade"));
    EXPECT_TRUE(r.degraded);
}

TEST(TerrainRules, UnknownKeyIgnoredWithWarning)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json["kNotARule"] = 1.0;

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_TRUE(HasWarning(warnings, "unknown:kNotARule"));
    EXPECT_EQ(warnings.Size(), 1u);
    EXPECT_FALSE(r.degraded);
}

TEST(TerrainRules, RulesVersionReadFromFile)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json["rulesVersion"] = 7;

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_EQ(r.rulesVersion, 7u);
    EXPECT_EQ(warnings.Size(), 0u);
}

TEST(TerrainRules, MissingRulesVersionWarnsAndDefaults)
{
    Json::Value json = TerrainRules::Defaults().ToJson();
    json.removeMember("rulesVersion");

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_EQ(r.rulesVersion, kDefaultRulesVersion);
    EXPECT_TRUE(HasWarning(warnings, "missing:rulesVersion"));
}

TEST(TerrainRules, NonObjectRootKeepsDefaultsAndDegrades)
{
    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_FALSE(r.LoadFromJson(Json::Value(3), warnings));
    EXPECT_TRUE(r.degraded);
    EXPECT_FLOAT_EQ(r.kCoastElev(), 0.02f);
}

TEST(TerrainRules, EmptyObjectDoesNotOverflowWarnings)
{
    // Every key missing plus many unknown keys: must not exceed warning capacity.
    Json::Value json(Json::objectValue);
    for (int i = 0; i < 300; ++i)
    {
        json["unknown" + std::to_string(i)] = 1;
    }
    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings));
    EXPECT_EQ(warnings.Size(), warnings.Capacity());
    EXPECT_TRUE(HasWarning(warnings, "warnings:truncated"));
    EXPECT_FLOAT_EQ(r.kCoastElev(), 0.02f);
}

TEST(TerrainRules, ShippedFileMatchesTableAndLoadsClean)
{
    const std::filesystem::path path = FindShippedRules();
    ASSERT_FALSE(path.empty()) << "terrain_rules.json not found above " << __FILE__;

    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good());
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    ASSERT_TRUE(Json::parseFromStream(builder, in, &root, &errs)) << errs;

    std::set<std::string> fileKeys;
    for (const std::string& k : root.getMemberNames())
    {
        fileKeys.insert(k);
    }
    std::set<std::string> tableKeys{ "rulesVersion" };
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        tableKeys.insert(GetRuleDef(i).key);
    }
    EXPECT_EQ(fileKeys, tableKeys);

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(root, warnings));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(r.degraded);
    EXPECT_EQ(r.rulesVersion, 2u);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(r.values[i], GetRuleDef(i).defaultValue) << GetRuleDef(i).key;
    }
}

TEST(TerrainRules, NewBiomeRulesDefaultsMatch)
{
    const TerrainRules r = TerrainRules::Defaults();

    // Global biome tuning
    EXPECT_FLOAT_EQ(r.kLatCool(), 0.85f);
    EXPECT_FLOAT_EQ(r.kLapse(), 0.6f);
    EXPECT_FLOAT_EQ(r.kRiverPenalty(), 2.0f);
    EXPECT_FLOAT_EQ(r.kMoistScale(), 8.0f);
    EXPECT_FLOAT_EQ(r.kBandBlend(), 5.0f);
    EXPECT_FLOAT_EQ(r.kAirDecay(), 0.04f);
    EXPECT_FLOAT_EQ(r.kOrographic(), 4.0f);
    EXPECT_FLOAT_EQ(r.kProximityMix(), 0.4f);
    EXPECT_FLOAT_EQ(r.kRainBonus(), 1.0f);
    EXPECT_FLOAT_EQ(r.kWetlandMaxElev(), 0.12f);
    EXPECT_FLOAT_EQ(r.kWetlandMoist(), 0.7f);
    EXPECT_FLOAT_EQ(r.kPlateauWet(), 0.5f);
    EXPECT_EQ(r.GetInt(Rule::kMaxFixupPasses), 8);

    // Grass
    EXPECT_FLOAT_EQ(r.kT0_Grass(), 0.60f);
    EXPECT_FLOAT_EQ(r.kM0_Grass(), 0.35f);
    EXPECT_FLOAT_EQ(r.kWT_Grass(), 0.25f);
    EXPECT_FLOAT_EQ(r.kWM_Grass(), 0.20f);

    // Forest
    EXPECT_FLOAT_EQ(r.kT0_Forest(), 0.50f);
    EXPECT_FLOAT_EQ(r.kM0_Forest(), 0.60f);
    EXPECT_FLOAT_EQ(r.kWT_Forest(), 0.25f);
    EXPECT_FLOAT_EQ(r.kWM_Forest(), 0.20f);

    // Desert
    EXPECT_FLOAT_EQ(r.kT0_Desert(), 0.80f);
    EXPECT_FLOAT_EQ(r.kM0_Desert(), 0.08f);
    EXPECT_FLOAT_EQ(r.kWT_Desert(), 0.25f);
    EXPECT_FLOAT_EQ(r.kWM_Desert(), 0.12f);

    // Tundra
    EXPECT_FLOAT_EQ(r.kT0_Tundra(), 0.12f);
    EXPECT_FLOAT_EQ(r.kM0_Tundra(), 0.30f);
    EXPECT_FLOAT_EQ(r.kWT_Tundra(), 0.15f);
    EXPECT_FLOAT_EQ(r.kWM_Tundra(), 0.30f);

    // Rainforest
    EXPECT_FLOAT_EQ(r.kT0_Rainforest(), 0.85f);
    EXPECT_FLOAT_EQ(r.kM0_Rainforest(), 0.90f);
    EXPECT_FLOAT_EQ(r.kWT_Rainforest(), 0.20f);
    EXPECT_FLOAT_EQ(r.kWM_Rainforest(), 0.15f);
}

TEST(TerrainRules, NewBiomeRulesRoundTripThroughJson)
{
    const TerrainRules a = TerrainRules::Defaults();
    const Json::Value json = a.ToJson();

    TerrainRules b;
    RuleWarnings warnings;
    EXPECT_TRUE(b.LoadFromJson(json, warnings));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(b.degraded);

    // Check new biome rules round-trip
    EXPECT_FLOAT_EQ(b.kLatCool(), a.kLatCool());
    EXPECT_FLOAT_EQ(b.kLapse(), a.kLapse());
    EXPECT_FLOAT_EQ(b.kT0_Grass(), a.kT0_Grass());
    EXPECT_FLOAT_EQ(b.kM0_Rainforest(), a.kM0_Rainforest());
}
