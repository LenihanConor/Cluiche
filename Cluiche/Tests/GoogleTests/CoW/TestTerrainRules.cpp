// S-1 Terrain Generator task 2: table-driven TerrainRules.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <DiaAttribute/ParameterFile.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace CoW::WorldGeneration;

namespace {

// Rule metadata (name, range, default) comes from the sidecar schema.
const Dia::Attribute::AttributeDefinition& RuleMeta(uint32_t index)
{
    return TestSidecarFile().Schema().GetAttributeByIndex(index);
}
const Dia::Attribute::AttributeDefinition& RuleMeta(const char* key)
{
    return *TestSidecarFile().Schema().FindAttribute(Dia::Core::StringCRC(key));
}

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
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";
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
    const TerrainRules r = TestRules();
    EXPECT_EQ(r.rulesVersion, 4u);
    EXPECT_FALSE(r.degraded);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(r.ValueAt(i), RuleMeta(i).default_value) << RuleMeta(i).fullKey.AsCStr();
    }
    EXPECT_FLOAT_EQ(r.kCoastElev, 0.02f);
    EXPECT_FLOAT_EQ(r.kFlowMax, 400.0f);
    EXPECT_EQ(TerrainRules::RoundToInt(r.kBorderWidth), 2);
}

TEST(TerrainRules, DefaultsAreWithinOwnRange)
{
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        const auto& d = RuleMeta(i);
        EXPECT_LE(d.minimum_value, d.default_value) << d.fullKey.AsCStr();
        EXPECT_GE(d.maximum_value, d.default_value) << d.fullKey.AsCStr();
        EXPECT_LT(d.minimum_value, d.maximum_value) << d.fullKey.AsCStr();
    }
}

TEST(TerrainRules, KeysAreUniqueAndNonEmpty)
{
    std::set<std::string> keys;
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        const std::string k = RuleMeta(i).fullKey.AsCStr();
        EXPECT_FALSE(k.empty());
        EXPECT_TRUE(keys.insert(k).second) << "duplicate key " << k;
    }
    EXPECT_EQ(keys.size(), static_cast<size_t>(kRuleCount));
    EXPECT_EQ(keys.count("rulesVersion"), 0u);
}

TEST(TerrainRules, DefaultsRoundTripThroughJson)
{
    const TerrainRules a = TestRules();
    const Json::Value json = a.ToJson();
    EXPECT_EQ(json.getMemberNames().size(), static_cast<size_t>(kRuleCount) + 1);

    TerrainRules b;
    RuleWarnings warnings;
    EXPECT_TRUE(b.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(b.degraded);
    EXPECT_EQ(b.rulesVersion, a.rulesVersion);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(b.ValueAt(i), a.ValueAt(i)) << RuleMeta(i).fullKey.AsCStr();
    }
}

TEST(TerrainRules, MissingKeyUsesDefaultWithWarning)
{
    Json::Value json = TestRules().ToJson();
    json["kEdgeFade"] = 0.9;
    json.removeMember("kCoastElev");

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_FLOAT_EQ(r.kCoastElev, 0.02f);
    EXPECT_FLOAT_EQ(r.kEdgeFade, 0.9f);
    EXPECT_TRUE(HasWarning(warnings, "missing:kCoastElev"));
    EXPECT_EQ(warnings.Size(), 1u);
    EXPECT_FALSE(r.degraded);
}

TEST(TerrainRules, OutOfRangeIsClampedWithWarningAndDegraded)
{
    Json::Value json = TestRules().ToJson();
    json["kEdgeFade"] = 5.0;     // max 1
    json["kMinPiece"] = -3.0;    // min 1

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_FLOAT_EQ(r.kEdgeFade, RuleMeta("kEdgeFade").maximum_value);
    EXPECT_FLOAT_EQ(r.kMinPiece, RuleMeta("kMinPiece").minimum_value);
    EXPECT_TRUE(HasWarning(warnings, "range:kEdgeFade"));
    EXPECT_TRUE(HasWarning(warnings, "range:kMinPiece"));
    EXPECT_TRUE(r.degraded);
}

TEST(TerrainRules, NonNumericValueFallsBackToDefaultAndDegrades)
{
    Json::Value json = TestRules().ToJson();
    json["kEdgeFade"] = "high";

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_FLOAT_EQ(r.kEdgeFade, 0.5f);
    EXPECT_TRUE(HasWarning(warnings, "invalid:kEdgeFade"));
    EXPECT_TRUE(r.degraded);
}

TEST(TerrainRules, UnknownKeyIgnoredWithWarning)
{
    Json::Value json = TestRules().ToJson();
    json["kNotARule"] = 1.0;

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_TRUE(HasWarning(warnings, "unknown:kNotARule"));
    EXPECT_EQ(warnings.Size(), 1u);
    EXPECT_FALSE(r.degraded);
}

TEST(TerrainRules, RulesVersionReadFromFile)
{
    Json::Value json = TestRules().ToJson();
    json["rulesVersion"] = 7;

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(r.rulesVersion, 7u);
    EXPECT_EQ(warnings.Size(), 0u);
}

TEST(TerrainRules, MissingRulesVersionWarnsAndDefaults)
{
    Json::Value json = TestRules().ToJson();
    json.removeMember("rulesVersion");

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(r.rulesVersion, kDefaultRulesVersion);
    EXPECT_TRUE(HasWarning(warnings, "missing:rulesVersion"));
}

TEST(TerrainRules, NonObjectRootKeepsDefaultsAndDegrades)
{
    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_FALSE(r.LoadFromJson(Json::Value(3), warnings, TestSidecarText().c_str()));
    EXPECT_TRUE(r.degraded);
    EXPECT_FLOAT_EQ(r.kCoastElev, 0.02f);
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
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(warnings.Size(), warnings.Capacity());
    EXPECT_TRUE(HasWarning(warnings, "warnings:truncated"));
    EXPECT_FLOAT_EQ(r.kCoastElev, 0.02f);
}

TEST(TerrainRules, ShippedFileMatchesTableAndLoadsClean)
{
    const std::filesystem::path path = FindShippedRules();
    ASSERT_FALSE(path.empty()) << "terrain_rules.schema.json not found above " << __FILE__;

    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good());
    // The shipped file is now the parameter sidecar: read its entries as the flat key -> default object the
    // old terrain_rules.json held (plus rulesVersion), so every check below keeps its meaning.
    Json::Value sidecar;
    Json::CharReaderBuilder builder;
    std::string errs;
    ASSERT_TRUE(Json::parseFromStream(builder, in, &sidecar, &errs)) << errs;
    Json::Value root(Json::objectValue);
    root["rulesVersion"] = 4;
    for (const Json::Value& entry : sidecar["entries"])
    {
        if (entry["kind"].asString() != "parameter" || entry["type"].asString() == "map")
        {
            continue;   // terrain_type enum + terrain.* maps are pack-only, not flat rules JSON keys
        }
        root[entry["key"].asString()] = entry["default"];
    }

    std::set<std::string> fileKeys;
    for (const std::string& k : root.getMemberNames())
    {
        fileKeys.insert(k);
    }
    std::set<std::string> tableKeys{ "rulesVersion" };
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        tableKeys.insert(RuleMeta(i).fullKey.AsCStr());
    }
    EXPECT_EQ(fileKeys, tableKeys);

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(root, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(r.degraded);
    EXPECT_EQ(r.rulesVersion, 4u);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        EXPECT_FLOAT_EQ(r.ValueAt(i), RuleMeta(i).default_value) << RuleMeta(i).fullKey.AsCStr();
    }
}

TEST(TerrainRules, NewBiomeRulesDefaultsMatch)
{
    const TerrainRules r = TestRules();

    // Global biome tuning
    EXPECT_FLOAT_EQ(r.kLatCool, 0.85f);
    EXPECT_FLOAT_EQ(r.kLapse, 0.6f);
    EXPECT_FLOAT_EQ(r.kRiverPenalty, 2.0f);
    EXPECT_FLOAT_EQ(r.kMoistScale, 8.0f);
    EXPECT_FLOAT_EQ(r.kBandBlend, 5.0f);
    EXPECT_FLOAT_EQ(r.kAirDecay, 0.04f);
    EXPECT_FLOAT_EQ(r.kOrographic, 4.0f);
    EXPECT_FLOAT_EQ(r.kProximityMix, 0.4f);
    EXPECT_FLOAT_EQ(r.kRainBonus, 1.0f);
    EXPECT_FLOAT_EQ(r.kWetlandMaxElev, 0.12f);
    EXPECT_FLOAT_EQ(r.kWetlandMoist, 0.7f);
    EXPECT_FLOAT_EQ(r.kPlateauWet, 0.5f);
    EXPECT_EQ(r.kMaxFixupPasses, 8);
}

TEST(TerrainRules, NewBiomeRulesRoundTripThroughJson)
{
    const TerrainRules a = TestRules();
    const Json::Value json = a.ToJson();

    TerrainRules b;
    RuleWarnings warnings;
    EXPECT_TRUE(b.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(b.degraded);

    // Check new biome rules round-trip
    EXPECT_FLOAT_EQ(b.kLatCool, a.kLatCool);
    EXPECT_FLOAT_EQ(b.kLapse, a.kLapse);
}

TEST(TerrainRules, RegionRulesDefaults)
{
    const TerrainRules r = TestRules();

    // Region partition and node placement constants
    EXPECT_EQ(r.kMinRegionCells, 36);
    EXPECT_EQ(TerrainRules::RoundToInt(r.kSeedBiomeBonus), 3);
    EXPECT_FLOAT_EQ(r.kBiomeMismatch, 0.8f);
    EXPECT_FLOAT_EQ(r.kRegionRidgeCost, 1.0f);
    EXPECT_FLOAT_EQ(r.kMaxRegionShare, 1.7f);
    EXPECT_EQ(r.kMaxRegionSlack, 8);
    EXPECT_FLOAT_EQ(r.kMaxAbsorbedShare, 0.03f);
    EXPECT_FLOAT_EQ(r.kMaxFragmentShare, 0.06f);
    EXPECT_EQ(r.kNodeSpacing, 3);
    EXPECT_EQ(r.kCorridorSpacing, 6);
    EXPECT_EQ(r.kCoastalGapReach, 4);
    EXPECT_EQ(r.kRiverNodeSpacing, 7);
    EXPECT_EQ(r.kCellsPerNode, 10);
}

TEST(TerrainRules, RegionRulesOutOfRangeClamp)
{
    Json::Value json = TestRules().ToJson();
    json["kMinRegionCells"] = 0.5;     // min 1
    json["kSeedBiomeBonus"] = 100.0;   // max 32
    json["kBiomeMismatch"] = 2.0;      // max 1.0
    json["kMaxRegionSlack"] = -5.0;    // min 1
    json["kMaxFragmentShare"] = 0.9;   // max 0.3

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));

    EXPECT_EQ(r.kMinRegionCells, 1);
    EXPECT_EQ(TerrainRules::RoundToInt(r.kSeedBiomeBonus), 32);
    EXPECT_FLOAT_EQ(r.kBiomeMismatch, 1.0f);
    EXPECT_EQ(r.kMaxRegionSlack, 1);
    EXPECT_FLOAT_EQ(r.kMaxFragmentShare, 0.3f);

    EXPECT_TRUE(HasWarning(warnings, "range:kMinRegionCells"));
    EXPECT_TRUE(HasWarning(warnings, "range:kSeedBiomeBonus"));
    EXPECT_TRUE(HasWarning(warnings, "range:kBiomeMismatch"));
    EXPECT_TRUE(HasWarning(warnings, "range:kMaxRegionSlack"));
    EXPECT_TRUE(HasWarning(warnings, "range:kMaxFragmentShare"));
    EXPECT_TRUE(r.degraded);
}

TEST(TerrainRules, RegionRulesMissingWithWarning)
{
    Json::Value json = TestRules().ToJson();
    json.removeMember("kNodeSpacing");
    json.removeMember("kCoastalGapReach");

    TerrainRules r;
    RuleWarnings warnings;
    EXPECT_TRUE(r.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(r.kNodeSpacing, 3);      // falls back to default
    EXPECT_EQ(r.kCoastalGapReach, 4);   // falls back to default
    EXPECT_TRUE(HasWarning(warnings, "missing:kNodeSpacing"));
    EXPECT_TRUE(HasWarning(warnings, "missing:kCoastalGapReach"));
    EXPECT_FALSE(r.degraded);
}

TEST(TerrainRules, RegionRulesRoundTripThroughJson)
{
    const TerrainRules a = TestRules();
    const Json::Value json = a.ToJson();

    TerrainRules b;
    RuleWarnings warnings;
    EXPECT_TRUE(b.LoadFromJson(json, warnings, TestSidecarText().c_str()));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_FALSE(b.degraded);

    // Check region rules round-trip
    EXPECT_EQ(b.kMinRegionCells, a.kMinRegionCells);
    EXPECT_EQ(TerrainRules::RoundToInt(b.kSeedBiomeBonus), TerrainRules::RoundToInt(a.kSeedBiomeBonus));
    EXPECT_FLOAT_EQ(b.kBiomeMismatch, a.kBiomeMismatch);
    EXPECT_FLOAT_EQ(b.kRegionRidgeCost, a.kRegionRidgeCost);
    EXPECT_FLOAT_EQ(b.kMaxRegionShare, a.kMaxRegionShare);
    EXPECT_EQ(b.kMaxRegionSlack, a.kMaxRegionSlack);
    EXPECT_FLOAT_EQ(b.kMaxAbsorbedShare, a.kMaxAbsorbedShare);
    EXPECT_FLOAT_EQ(b.kMaxFragmentShare, a.kMaxFragmentShare);
    EXPECT_EQ(b.kNodeSpacing, a.kNodeSpacing);
    EXPECT_EQ(b.kCorridorSpacing, a.kCorridorSpacing);
    EXPECT_EQ(b.kCoastalGapReach, a.kCoastalGapReach);
    EXPECT_EQ(b.kRiverNodeSpacing, a.kRiverNodeSpacing);
    EXPECT_EQ(b.kCellsPerNode, a.kCellsPerNode);
}
