// Suite: CoW
// The terrain_rules parameter sidecar must reproduce the old TERRAIN_RULES_TABLE exactly (plan task 24).
// The (key, default) literals below were copied from the table in TerrainRules.h, not from the sidecar.

#include <gtest/gtest.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <DiaMaths/Core/RandomStream.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

using Dia::Attribute::ParameterFile;
using Dia::Attribute::ParamError;
using Dia::Core::StringCRC;

namespace
{
    struct TableRow { const char* key; float def; };

    // Copied from TERRAIN_RULES_TABLE (key, default).
    const TableRow kOldTable[] =
    {
        { "kCoastElev", 0.02f },
        { "kBorderWidth", 2.0f },
        { "kEdgeFade", 0.5f },
        { "kShelfScale", 6.0f },
        { "kMinLandmass", 6.0f },
        { "kMinIslandShare", 0.02f },
        { "kMaxIslandShare", 0.95f },
        { "kIslandGapMin", 2.0f },
        { "kIslandGapMax", 7.0f },
        { "kIslandSpacing", 6.0f },
        { "kWarpAmp", 0.15f },
        { "kCoastAmp", 0.12f },
        { "kRidgeCellMin", 0.35f },
        { "kRidgeWidthMin", 1.2f },
        { "kRidgeWidthMax", 2.8f },
        { "kSpineReach", 0.8f },
        { "kRingReach", 0.6f },
        { "kMinPiece", 2.0f },
        { "kPassFloor", 0.2f },
        { "kBarrierQuantile", 0.75f },
        { "kBarrierWobble", 1.5f },
        { "kBarrierHalfWidth", 1.5f },
        { "kPlateauInner", 0.5f },
        { "kVentSpacing", 4.0f },
        { "kLowlandRise", 0.25f },
        { "kInlandScale", 6.0f },
        { "kMountainHeight", 0.6f },
        { "kBarrierHeight", 0.35f },
        { "kVentHeight", 0.5f },
        { "kVentRadius", 2.0f },
        { "kRuggedAmp", 0.12f },
        { "kMinBasinDepth", 0.01f },
        { "kHighlandRain", 2.0f },
        { "kFlowMax", 400.0f },
        { "kFlowMin", 12.0f },
        { "kSourceMinElev", 0.3f },
        { "kMinRiverLength", 4.0f },
        { "kRidgeCost", 4.0f },
        { "kBarrierCost", 4.0f },
        { "kFirstShoresShare", 0.45f },
        { "kAncientShare", 0.15f },
        { "kRiverCorridor", 0.25f },
        { "kAncientMinDepth", 3.0f },
        { "kCarveMax", 0.12f },
        { "kCarveFullAcc", 8.0f },
        { "kShelfWidth", 3.0f },
        { "kCliffSlope", 0.18f },
        { "kSteepSlope", 0.10f },
        { "kPeakElev", 0.80f },
        { "kMountainElev", 0.55f },
        { "kHighlandElev", 0.35f },
        { "kHillElev", 0.15f },
        { "kHillSlope", 0.05f },
        { "kLatCool", 0.85f },
        { "kLapse", 0.6f },
        { "kRiverPenalty", 2.0f },
        { "kMoistScale", 8.0f },
        { "kBandBlend", 5.0f },
        { "kAirDecay", 0.04f },
        { "kOrographic", 4.0f },
        { "kProximityMix", 0.4f },
        { "kRainBonus", 1.0f },
        { "kWetlandMaxElev", 0.12f },
        { "kWetlandMoist", 0.7f },
        { "kPlateauWet", 0.5f },
        { "kMaxFixupPasses", 8.0f },
        { "kMinRegionCells", 36.0f },
        { "kSeedBiomeBonus", 3.0f },
        { "kBiomeMismatch", 0.8f },
        { "kRegionRidgeCost", 1.0f },
        { "kMaxRegionShare", 1.7f },
        { "kMaxRegionSlack", 8.0f },
        { "kMaxAbsorbedShare", 0.03f },
        { "kMaxFragmentShare", 0.06f },
        { "kNodeSpacing", 3.0f },
        { "kCorridorSpacing", 6.0f },
        { "kCoastalGapReach", 4.0f },
        { "kRiverNodeSpacing", 7.0f },
        { "kCellsPerNode", 10.0f },
    };
    constexpr size_t kOldTableCount = sizeof(kOldTable) / sizeof(kOldTable[0]);

    std::string ReadSidecar()
    {
        std::error_code       ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";
            if (std::filesystem::exists(candidate, ec))
            {
                std::ifstream     in(candidate, std::ios::binary);
                std::stringstream ss;
                ss << in.rdbuf();
                return ss.str();
            }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }
}

TEST(TerrainRulesSidecar, TableLiteralCountIs79)
{
    EXPECT_EQ(kOldTableCount, 79u);
}

// Task 24: climate lives in the biome registry; S-1's parameters carry no climate value.
namespace
{
    bool LoadSidecarForClimateCheck(ParameterFile& file)
    {
        const std::string text = ReadSidecar();
        ParamError err;
        return !text.empty() && file.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err);
    }
}

TEST(TerrainRulesSidecar, HasNoClimateParameters)
{
    const std::string text = ReadSidecar();
    ASSERT_FALSE(text.empty()) << "sidecar not found";
    for (const char* prefix : { "kT0_", "kM0_", "kWT_", "kWM_" })
    {
        EXPECT_EQ(text.find(prefix), std::string::npos) << prefix;
    }
    ParameterFile file;
    ASSERT_TRUE(LoadSidecarForClimateCheck(file));
    EXPECT_EQ(file.Schema().GetAttributeCount(), 79u);
}

TEST(TerrainRulesSidecar, DefaultsEqualOldTableAndKeySetMatches)
{
    const std::string text = ReadSidecar();
    ASSERT_FALSE(text.empty()) << "sidecar not found";

    ParameterFile file;
    ParamError    err;
    ASSERT_TRUE(file.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();

    EXPECT_EQ(file.Schema().GetAttributeCount(), kOldTableCount);

    std::set<std::string> tableKeys;
    for (const TableRow& row : kOldTable)
    {
        EXPECT_TRUE(tableKeys.insert(row.key).second) << "duplicate table key " << row.key;
        const Dia::Attribute::AttributeDefinition* def = file.Schema().FindAttribute(StringCRC(row.key));
        ASSERT_NE(def, nullptr) << "sidecar lacks " << row.key;
        EXPECT_EQ(def->fullKey.AsCStr(), std::string(row.key));
        EXPECT_EQ(def->default_value, row.def) << row.key << " schema default";
        EXPECT_EQ(file.Values().GetBaseValue(StringCRC(row.key)), row.def) << row.key << " loaded value";
    }

    // Key set equality: every sidecar attribute is a table key (counts already equal).
    for (unsigned int i = 0; i < file.Schema().GetAttributeCount(); ++i)
    {
        const std::string k = file.Schema().GetAttributeByIndex(i).fullKey.AsCStr();
        EXPECT_EQ(tableKeys.count(k), 1u) << "sidecar has extra key " << k;
    }
}

namespace
{
    // The conversion every integer-valued rule has always used (copied from the old TerrainRules::GetInt).
    int OldGetInt(float v) { return static_cast<int>(v + (v >= 0.0f ? 0.5f : -0.5f)); }

    bool LoadSidecar(ParameterFile& file)
    {
        const std::string text = ReadSidecar();
        ParamError err;
        return !text.empty() && file.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err);
    }
}

// AC 15: the binding covers the sidecar exactly, both ways.
TEST(TerrainRulesSidecar, BindingCoversSidecarExactly)
{
    ParameterFile file;
    ASSERT_TRUE(LoadSidecar(file));

    auto report = std::make_unique<Dia::Attribute::BindingReport>();
    EXPECT_TRUE(CoW::WorldGeneration::TerrainRules::ResolveBindings(file.Schema(), *report));
    EXPECT_EQ(report->unboundParameters.Size(), 0u);
    EXPECT_EQ(report->bindingsWithoutEntry.Size(), 0u);
    EXPECT_EQ(report->countMismatches.Size(), 0u);
    EXPECT_TRUE(report->IsClean());
}

// Snapshot of the freshly loaded sidecar reproduces every old table default.
TEST(TerrainRulesSidecar, FromParameterFileEqualsOldTableDefaults)
{
    ParameterFile file;
    ASSERT_TRUE(LoadSidecar(file));

    CoW::WorldGeneration::TerrainRules rules;
    ASSERT_TRUE(CoW::WorldGeneration::TerrainRules::FromParameterFile(file, rules));
    EXPECT_EQ(rules.rulesVersion, 4u);
    EXPECT_FALSE(rules.degraded);
    ASSERT_EQ(CoW::WorldGeneration::kRuleCount, kOldTableCount);
    // Int members hold the rounded default (half away from zero).
    EXPECT_EQ(rules.kMinRegionCells, 36);
    EXPECT_EQ(rules.kNodeSpacing, 3);
    EXPECT_EQ(rules.kMaxFixupPasses, 8);
    for (uint32_t i = 0; i < CoW::WorldGeneration::kRuleCount; ++i)
    {
        EXPECT_EQ(rules.ValueAt(i), kOldTable[i].def) << kOldTable[i].key;
    }
}

// Integer rounding: half away from zero, identical to the old GetInt for every default and for probe values.
TEST(TerrainRulesSidecar, IntConversionMatchesOldGetInt)
{
    using CoW::WorldGeneration::TerrainRules;
    for (const TableRow& row : kOldTable)
    {
        EXPECT_EQ(TerrainRules::RoundToInt(row.def), OldGetInt(row.def)) << row.key;
    }
    const float probes[] = { 0.0f, 0.4f, 0.5f, 0.6f, 1.5f, 2.5f, 2.49f, 7.99f, -0.4f, -0.5f, -0.6f, -1.5f, -2.5f, -0.0f };
    for (float p : probes)
    {
        EXPECT_EQ(TerrainRules::RoundToInt(p), OldGetInt(p)) << p;
    }
    EXPECT_EQ(TerrainRules::RoundToInt(2.5f), 3);
    EXPECT_EQ(TerrainRules::RoundToInt(-2.5f), -3);
    EXPECT_EQ(TerrainRules::RoundToInt(0.5f), 1);
    EXPECT_EQ(TerrainRules::RoundToInt(-0.5f), -1);
    EXPECT_EQ(TerrainRules::RoundToInt(2.49f), 2);
}

// ---- Task 26 (AC 18, AC 19): the session owns its rules file; no rules, no generation. ----

namespace
{
    // The shipped sidecar with kCoastElev's default pushed out of its range: Load must fail naming kCoastElev.
    std::string BrokenSidecarText()
    {
        std::string text = CoW::WorldGeneration::TestSidecarText();
        const std::string needle = "\"default\": 0.02";
        const size_t pos = text.find(needle);
        if (pos != std::string::npos)
        {
            text.replace(pos, needle.size(), "\"default\": 99.0");
        }
        return text;
    }

    bool IsRulesNotLoaded(const Json::Value& reply)
    {
        return reply.isObject() && reply.isMember("success") && !reply["success"].asBool()
            && reply.isMember("error") && reply["error"].asString() == "rules_not_loaded";
    }
}

TEST(TerrainRulesSession, BrokenSidecarTextTestFixtureReallyBreaksTheLoad)
{
    const std::string text = BrokenSidecarText();
    ASSERT_NE(text, CoW::WorldGeneration::TestSidecarText());
    ParameterFile file;
    ParamError    err;
    EXPECT_FALSE(file.Load(text.c_str(), "terrain_rules.schema.json", nullptr, err));
    EXPECT_STREQ(err.key.AsCStr(), "kCoastElev");
}

TEST(TerrainRulesSession, DefaultConstructedSessionRefusesEveryGenerationEntryPoint)
{
    CoW::WorldGeneration::WorldGenSession s;
    EXPECT_FALSE(s.RulesLoaded());

    Json::Value seed(Json::objectValue);
    seed["seed"] = 7;
    EXPECT_TRUE(IsRulesNotLoaded(s.Set(seed)));
    Json::Value size(Json::objectValue);
    size["size"] = "Small";
    EXPECT_TRUE(IsRulesNotLoaded(s.SetSize(size)));
    EXPECT_TRUE(IsRulesNotLoaded(s.Regenerate(Json::Value(Json::objectValue))));
    Json::Value preset(Json::objectValue);
    preset["id"] = "continental";
    EXPECT_TRUE(IsRulesNotLoaded(s.PresetLoad(preset)));
    Json::Value import(Json::objectValue);
    import["name"] = "anything";
    EXPECT_TRUE(IsRulesNotLoaded(s.ImportWorld(import)));
    EXPECT_TRUE(IsRulesNotLoaded(s.Validate(Json::Value(Json::objectValue))));

    Dia::Maths::RandomStream rng(1);
    EXPECT_FALSE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 0u);
    EXPECT_FALSE(s.IsGenerating());
}

TEST(TerrainRulesSession, LoadRulesFromGoodSidecarBindsRulesAndGenerates)
{
    CoW::WorldGeneration::WorldGenSession s;
    s.SetBiomeRegistry(&CoW::World::Testing::ShippedBiomeRegistry());   // generation needs the registry
    ParamError err;
    ASSERT_TRUE(s.LoadRules(CoW::WorldGeneration::TestSidecarText().c_str(), err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(s.RulesLoaded());
    EXPECT_EQ(s.Rules().kCoastElev, CoW::WorldGeneration::TestRules().kCoastElev);

    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), 1u);
}

TEST(TerrainRulesSession, FailedLoadRulesOnAFreshSessionNamesTheKeyAndLeavesItUnloaded)
{
    CoW::WorldGeneration::WorldGenSession s;
    const std::string broken = BrokenSidecarText();
    ParamError err;
    EXPECT_FALSE(s.LoadRules(broken.c_str(), err));
    EXPECT_STREQ(err.key.AsCStr(), "kCoastElev");
    EXPECT_FALSE(s.RulesLoaded());
    EXPECT_TRUE(IsRulesNotLoaded(s.Regenerate(Json::Value(Json::objectValue))));
    EXPECT_FALSE(s.LoadRules(nullptr, err));
    EXPECT_FALSE(s.RulesLoaded());
}

TEST(TerrainRulesSession, FailedLoadRulesLeavesAPreviouslyLoadedSessionIntact)
{
    CoW::WorldGeneration::TestSession s;
    const float before = s.Rules().kCoastElev;
    const std::string broken = BrokenSidecarText();
    ParamError err;
    EXPECT_FALSE(s.LoadRules(broken.c_str(), err));
    EXPECT_STREQ(err.key.AsCStr(), "kCoastElev");
    EXPECT_TRUE(s.RulesLoaded());
    EXPECT_EQ(s.Rules().kCoastElev, before);

    Json::Value seed(Json::objectValue);
    seed["seed"] = 9;
    const Json::Value reply = s.Set(seed);
    EXPECT_TRUE(reply["success"].asBool());
    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(s.Tick(rng));
}

TEST(TerrainRulesSession, LoadRulesWithAChangedDefaultReplacesTheRulesBeforeAnyWorldExists)
{
    std::string text = CoW::WorldGeneration::TestSidecarText();
    const size_t pos = text.find("\"default\": 0.02");
    ASSERT_NE(pos, std::string::npos);
    text.replace(pos, std::string("\"default\": 0.02").size(), "\"default\": 0.03");

    CoW::WorldGeneration::TestSession s;
    ParamError err;
    ASSERT_TRUE(s.LoadRules(text.c_str(), err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_FLOAT_EQ(s.Rules().kCoastElev, 0.03f);
}

TEST(TerrainRulesSession, LoadRulesIsRefusedOnceAWorldExists)
{
    CoW::WorldGeneration::TestSession s;
    Dia::Maths::RandomStream rng(1);
    ASSERT_TRUE(s.Tick(rng));
    ParamError err;
    EXPECT_FALSE(s.LoadRules(CoW::WorldGeneration::TestSidecarText().c_str(), err));
    EXPECT_STREQ(err.key.AsCStr(), "state");
    EXPECT_TRUE(s.RulesLoaded());
}

// AC 18 (part 2): the old file and its alias are gone; the sidecar and its alias are the only rules asset.
TEST(TerrainRulesSidecar, OldTerrainRulesJsonAndAliasAreGone)
{
    std::error_code       ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    std::filesystem::path root;
    while (!dir.empty())
    {
        if (std::filesystem::exists(dir / "Cluiche/Assets/CoW/assets.catalogue.json", ec)) { root = dir; break; }
        if (dir == dir.parent_path()) { break; }
        dir = dir.parent_path();
    }
    ASSERT_FALSE(root.empty());
    EXPECT_FALSE(std::filesystem::exists(root / "Cluiche/Assets/CoW/Global/WorldGen/terrain_rules.json", ec));
    EXPECT_TRUE(std::filesystem::exists(root / "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json", ec));

    std::ifstream in(root / "Cluiche/Assets/CoW/assets.catalogue.json", std::ios::binary);
    Json::Value   catalogue;
    Json::CharReaderBuilder builder;
    std::string errs;
    ASSERT_TRUE(Json::parseFromStream(builder, in, &catalogue, &errs)) << errs;
    bool oldAlias = false, newAlias = false;
    for (const Json::Value& a : catalogue["assets"])
    {
        const std::string id = a["id"].asString();
        if (id == "json.terrain_rules") { oldAlias = true; }
        if (id == "json.terrain_rules_schema")
        {
            newAlias = true;
            EXPECT_EQ(a["source_path"].asString(), "Global/WorldGen/Rules/terrain_rules.schema.json");
        }
    }
    EXPECT_FALSE(oldAlias);
    EXPECT_TRUE(newAlias);
}
