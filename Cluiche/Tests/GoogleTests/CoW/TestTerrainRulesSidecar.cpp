// Suite: CoW
// The terrain_rules parameter sidecar must reproduce the old TERRAIN_RULES_TABLE exactly (plan task 24).
// The (key, default) literals below were copied from the table in TerrainRules.h, not from the sidecar.

#include <gtest/gtest.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <Shared/WorldGeneration/TerrainRules.h>

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
        { "kT0_Grass", 0.60f },
        { "kM0_Grass", 0.35f },
        { "kWT_Grass", 0.25f },
        { "kWM_Grass", 0.20f },
        { "kT0_Forest", 0.50f },
        { "kM0_Forest", 0.60f },
        { "kWT_Forest", 0.25f },
        { "kWM_Forest", 0.20f },
        { "kT0_Desert", 0.80f },
        { "kM0_Desert", 0.08f },
        { "kWT_Desert", 0.25f },
        { "kWM_Desert", 0.12f },
        { "kT0_Tundra", 0.12f },
        { "kM0_Tundra", 0.30f },
        { "kWT_Tundra", 0.15f },
        { "kWM_Tundra", 0.30f },
        { "kT0_Rainforest", 0.85f },
        { "kM0_Rainforest", 0.90f },
        { "kWT_Rainforest", 0.20f },
        { "kWM_Rainforest", 0.15f },
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

TEST(TerrainRulesSidecar, TableLiteralCountIs99)
{
    EXPECT_EQ(kOldTableCount, 99u);
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
