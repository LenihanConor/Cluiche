// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (AC1, AC2, AC3, AC4, AC10)
// The three built-in guarantees over SiteSet kinds, run on hand-authored worlds, and DefaultValidationSet.
#include <gtest/gtest.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/SiteRules.h>
#include <Shared/WorldGeneration/Testing/GuaranteeSpecTestHelpers.h>
#include <Shared/WorldGeneration/NavigationQuery.h>
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/Testing/TestProfiles.h>
#include <Shared/WorldGeneration/Testing/ValidationTestWorlds.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using Dia::Core::StringCRC;

namespace
{

// The three CoW rules built from the shipped guarantee spec (data-driven; the old CoW classes are gone).
const GuaranteeEntry& ShippedEntry(const char* id)
{
    const GuaranteeEntry* e = ShippedGuaranteeSpec().FindGuarantee(id);
    if (!e) { std::abort(); }
    return *e;
}
ReachAnyGuarantee HearthNexusRule() { return ReachAnyGuarantee(ShippedEntry("CoW.Guarantee.HearthNexus")); }
MinSpacingGuarantee SpacingRule() { return MinSpacingGuarantee(ShippedEntry("CoW.Guarantee.DynastySpacing")); }
MinSpacingGuarantee SpacingRule(float minimum)
{
    GuaranteeEntry e = ShippedEntry("CoW.Guarantee.DynastySpacing");
    e.minimum = minimum;
    return MinSpacingGuarantee(e);
}
VariantCoverGuarantee ThroneRule() { return VariantCoverGuarantee(ShippedEntry("CoW.Guarantee.WorldThrone")); }

bool AnyDiagnosticContains(const GuaranteeResult& r, const char* needle)
{
    for (const Diagnostic& d : r.diagnostics) { if (std::string(d.text).find(needle) != std::string::npos) { return true; } }
    return false;
}

bool HasViolatingCell(const GuaranteeResult& r, uint32_t cell)
{
    for (uint32_t c : r.violatingCells) { if (c == cell) { return true; } }
    return false;
}

const Measurement* FindMeasurement(const GuaranteeResult& r, const StringCRC& key, int tag = -1)
{
    for (const Measurement& m : r.measurements) { if (m.keyCrc == key.Value() && m.tag == tag) { return &m; } }
    return nullptr;
}

int CountMeasurements(const GuaranteeResult& r, const StringCRC& key)
{
    int n = 0;
    for (const Measurement& m : r.measurements) { if (m.keyCrc == key.Value()) { ++n; } }
    return n;
}

GuaranteeResult Check(const IWorldGuarantee& g, const WorldStructure& w, std::initializer_list<const INavigationProfile*> profiles)
{
    std::vector<const INavigationProfile*> v(profiles);
    const NavigationQuery q(w, v);
    return g.Check({ w, q });
}

// 30x20 all land, an Ancient Interiors block at x 12..17, y 7..12, and a valid site layout for the default set:
// 2 hearths, 2 nexuses, 3 dynasty starts far apart, 6 thrones (types 0..5) inside the block.
WorldStructure MakeValidWorld()
{
    WorldStructure w = MakeAllLandWorld(30, 20, kStratumWild);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    AddSiteAt(w, SiteKinds::Hearth(), 3, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 26, 16);
    AddSiteAt(w, SiteKinds::Nexus(), 5, 10);
    AddSiteAt(w, SiteKinds::Nexus(), 24, 5);
    AddSiteAt(w, SiteKinds::DynastyStart(), 2, 2);
    AddSiteAt(w, SiteKinds::DynastyStart(), 27, 2);
    AddSiteAt(w, SiteKinds::DynastyStart(), 14, 17);
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t)); }
    return w;
}

} // namespace

// ---- Kind ids -------------------------------------------------------------------------------------------------------

TEST(SiteKinds, IdsAreStableAndDistinct)
{
    EXPECT_EQ(SiteKinds::Hearth(), StringCRC("CoW.Site.Hearth"));
    EXPECT_EQ(SiteKinds::Nexus(), StringCRC("CoW.Site.Nexus"));
    EXPECT_EQ(SiteKinds::DynastyStart(), StringCRC("CoW.Site.DynastyStart"));
    EXPECT_EQ(SiteKinds::WorldThrone(), StringCRC("CoW.Site.WorldThrone"));
    EXPECT_NE(SiteKinds::Hearth().Value(), SiteKinds::Nexus().Value());
}

// ---- AC1: Hearth <-> Nexus ------------------------------------------------------------------------------------------

TEST(HearthNexusGuarantee, AC1_AllHearthsReachableSatisfies)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 2, 5);
    AddSiteAt(w, SiteKinds::Hearth(), 0, 0);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_TRUE(r.satisfied);
    EXPECT_TRUE(r.diagnostics.IsEmpty());
    EXPECT_TRUE(r.violatingCells.IsEmpty());
    EXPECT_EQ(r.idCrc, HearthNexusRule().GetId().Value());
}

TEST(HearthNexusGuarantee, AC1_HearthOnAnIsolatedIslandFailsAndNamesItsPosition)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 2, 5);
    AddSiteAt(w, SiteKinds::Hearth(), 9, 2);   // right bank, no Nexus there
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "(9,2)"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "Hearth"));
    EXPECT_FALSE(AnyDiagnosticContains(r, "(2,5)"));
}

TEST(HearthNexusGuarantee, AnyNexusWillDo)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 3);
    AddSiteAt(w, SiteKinds::Nexus(), 10, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 9, 2);
    BlockWaterProfile p("P");
    EXPECT_TRUE(Check(HearthNexusRule(), w, { &p }).satisfied);
}

TEST(HearthNexusGuarantee, NoNexusAtAllFailsEveryHearth)
{
    WorldStructure w = MakeAllLandWorld(8, 8);
    AddSiteAt(w, SiteKinds::Hearth(), 1, 1);
    AddSiteAt(w, SiteKinds::Hearth(), 6, 6);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 2u);
    EXPECT_TRUE(AnyDiagnosticContains(r, "no Nexus"));
}

TEST(HearthNexusGuarantee, NoHearthsIsVacuouslySatisfied)
{
    WorldStructure w = MakeAllLandWorld(8, 8);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 1);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_TRUE(r.satisfied);
    EXPECT_TRUE(r.proofPaths.IsEmpty());
}

TEST(HearthNexusGuarantee, HearthOnTheNexusCellIsReachableWithAOneCellPath)
{
    WorldStructure w = MakeAllLandWorld(8, 8);
    AddSiteAt(w, SiteKinds::Nexus(), 3, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 3, 3);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_TRUE(r.satisfied);
    ASSERT_EQ(r.proofPaths.Size(), 1u);
    EXPECT_EQ(r.proofPaths[0].cells.Size(), 1u);
}

// ---- AC10: hearth evidence ------------------------------------------------------------------------------------------

TEST(HearthNexusGuarantee, AC10_OneValidProofPathPerReachableHearthAndNoPathForTheUnreachableOne)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    const uint32_t nexusL = Idx(w, 1, 3), nexusR = Idx(w, 10, 3);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 3);
    AddSiteAt(w, SiteKinds::Hearth(), 2, 5);
    AddSiteAt(w, SiteKinds::Hearth(), 0, 0);
    BlockWaterProfile p("P");
    GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    ASSERT_EQ(r.proofPaths.Size(), 2u);
    for (const ProofPath& path : r.proofPaths)
    {
        EXPECT_EQ(path.to, nexusL);
        EXPECT_EQ(path.profileCrc, StringCRC("P").Value());
        EXPECT_TRUE(IsValidWalk(w, path));
        for (uint32_t c : path.cells) { EXPECT_NE(w.Cells()[c].terrain, TerrainType::DeepWater); }
    }
    // The paths start at the hearths (sorted by cell index: (0,0) then (2,5)).
    EXPECT_EQ(r.proofPaths[0].from, Idx(w, 0, 0));
    EXPECT_EQ(r.proofPaths[1].from, Idx(w, 2, 5));
    (void)nexusR;

    AddSiteAt(w, SiteKinds::Hearth(), 9, 2);
    r = Check(HearthNexusRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.proofPaths.Size(), 2u);   // the stranded hearth has none
    EXPECT_TRUE(HasViolatingCell(r, Idx(w, 9, 2)));
    EXPECT_EQ(r.violatingCells.Size(), 1u);
}

// ---- AC2: dynasty spacing -------------------------------------------------------------------------------------------

TEST(DynastySpacingGuarantee, AC2_PairsAtOrAboveTheMinimumSatisfy)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    AddSiteAt(w, SiteKinds::DynastyStart(), 0, 0);
    AddSiteAt(w, SiteKinds::DynastyStart(), 20, 0);
    AddSiteAt(w, SiteKinds::DynastyStart(), 10, 18);
    const float closest = static_cast<float>(std::min({ HexSteps(w, Idx(w, 0, 0), Idx(w, 20, 0)),
                                                        HexSteps(w, Idx(w, 0, 0), Idx(w, 10, 18)),
                                                        HexSteps(w, Idx(w, 20, 0), Idx(w, 10, 18)) }));
    BlockWaterProfile p("P");
    EXPECT_TRUE(Check(SpacingRule(closest), w, { &p }).satisfied);          // exactly at the minimum is fine
    EXPECT_FALSE(Check(SpacingRule(closest + 1.0f), w, { &p }).satisfied);  // one step short is not
}

TEST(DynastySpacingGuarantee, AC2_TooClosePairFailsAndNamesBothPositionsAndTheDistance)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    AddSiteAt(w, SiteKinds::DynastyStart(), 3, 3);
    AddSiteAt(w, SiteKinds::DynastyStart(), 6, 3);
    AddSiteAt(w, SiteKinds::DynastyStart(), 25, 15);
    const uint32_t d = HexSteps(w, Idx(w, 3, 3), Idx(w, 6, 3));
    const GuaranteeResult r = Check(SpacingRule(10.0f), w, {});   // spacing needs no navigation or profile
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "(3,3)"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "(6,3)"));
    EXPECT_TRUE(AnyDiagnosticContains(r, ("distance " + std::to_string(d)).c_str()));
    EXPECT_TRUE(AnyDiagnosticContains(r, "minimum 10"));
    EXPECT_FALSE(AnyDiagnosticContains(r, "(25,15)"));
}

TEST(DynastySpacingGuarantee, AC10_ViolatingPairsAndMeasurements)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    AddSiteAt(w, SiteKinds::DynastyStart(), 3, 3);
    AddSiteAt(w, SiteKinds::DynastyStart(), 6, 3);
    AddSiteAt(w, SiteKinds::DynastyStart(), 25, 15);
    const GuaranteeResult r = Check(SpacingRule(10.0f), w, {});
    ASSERT_EQ(r.violatingPairs.Size(), 1u);
    EXPECT_EQ(r.violatingPairs[0].a, Idx(w, 3, 3));
    EXPECT_EQ(r.violatingPairs[0].b, Idx(w, 6, 3));
    EXPECT_EQ(CountMeasurements(r, MeasurementKeys::PairDistance()), 3);   // every pair
    EXPECT_EQ(CountMeasurements(r, MeasurementKeys::MinimumSpacing()), 1);
    EXPECT_FLOAT_EQ(FindMeasurement(r, MeasurementKeys::MinimumSpacing())->value, 10.0f);
    for (const Measurement& m : r.measurements)
    {
        if (m.keyCrc == MeasurementKeys::PairDistance().Value())
        {
            EXPECT_FLOAT_EQ(m.value, static_cast<float>(HexSteps(w, m.a, m.b)));
        }
    }
}

TEST(DynastySpacingGuarantee, FewerThanTwoStartsIsVacuouslySatisfied)
{
    WorldStructure w = MakeAllLandWorld(10, 10);
    EXPECT_TRUE(Check(SpacingRule(10.0f), w, {}).satisfied);
    AddSiteAt(w, SiteKinds::DynastyStart(), 1, 1);
    const GuaranteeResult r = Check(SpacingRule(10.0f), w, {});
    EXPECT_TRUE(r.satisfied);
    EXPECT_EQ(CountMeasurements(r, MeasurementKeys::PairDistance()), 0);
}

TEST(DynastySpacingGuarantee, ManyViolationsCapDiagnosticsButKeepEveryPair)
{
    WorldStructure w = MakeAllLandWorld(12, 12);
    for (int x = 0; x < 9; ++x) { AddSiteAt(w, SiteKinds::DynastyStart(), x, 0); }   // 36 pairs, all within 8 steps
    const GuaranteeResult r = Check(SpacingRule(20.0f), w, {});
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingPairs.Size(), 36u);
    EXPECT_EQ(r.diagnostics.Size(), kMaxDiagnostics);
    EXPECT_EQ(r.droppedDiagnostics, 36u - kMaxDiagnostics);
}

// ---- AC3: world-thrones ---------------------------------------------------------------------------------------------

TEST(WorldThroneGuarantee, AC3_SixReachableThronesSatisfy)
{
    const WorldStructure w = MakeValidWorld();
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_TRUE(r.satisfied) << (r.diagnostics.IsEmpty() ? "" : r.diagnostics[0].text);
    EXPECT_TRUE(r.diagnostics.IsEmpty());
}

TEST(WorldThroneGuarantee, AC3_FewerThanSixThronesFailsWithACountMismatch)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    for (int t = 0; t < 5; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "expected 6 world-thrones, found 5"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "type 5"));   // the missing type is named
}

TEST(WorldThroneGuarantee, AC3_SixThronesButADuplicateTypeFails)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    const uint16_t types[6] = { 0, 0, 1, 2, 3, 4 };
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, types[t]); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "2 world-thrones of Apotheosis type 0"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "no world-throne of Apotheosis type 5"));
}

TEST(WorldThroneGuarantee, InvalidTypeIsReported)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t == 5 ? 9 : t)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "invalid Apotheosis type 9"));
}

TEST(WorldThroneGuarantee, TypeSixIsTheFirstInvalidType)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t == 5 ? 6 : t)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "invalid Apotheosis type 6"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "no world-throne of Apotheosis type 5"));
}

TEST(WorldThroneGuarantee, AC3_ThroneCutOffFromTheAncientInteriorsFailsAndNamesIt)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    for (int y = 0; y < 20; ++y) { SetOcean(w, 25, y); }   // a water column splits off columns 26..29
    for (int t = 0; t < 5; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t)); }
    AddSiteAt(w, SiteKinds::WorldThrone(), 28, 10, 5);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "(28,10)"));
    EXPECT_TRUE(HasViolatingCell(r, Idx(w, 28, 10)));
    EXPECT_EQ(r.violatingCells.Size(), 1u);
    EXPECT_EQ(r.proofPaths.Size(), 5u);
}

TEST(WorldThroneGuarantee, AC3_NoAncientInteriorsCellsIsATerrainCause)
{
    WorldStructure w = MakeAllLandWorld(30, 20, kStratumWild);
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 3 + t, 3, static_cast<uint16_t>(t)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    const Measurement* m = FindMeasurement(r, StringCRC("ancientInteriorsCells"));
    ASSERT_NE(m, nullptr);
    EXPECT_FLOAT_EQ(m->value, 0.0f);
    EXPECT_TRUE(AnyDiagnosticContains(r, "no Ancient Interiors cells (terrain cause)"));
    EXPECT_TRUE(r.violatingCells.IsEmpty());   // the thrones are not at fault
}

TEST(WorldThroneGuarantee, AC10_EvidenceProofPathsAndPerTypeCounts)
{
    const WorldStructure w = MakeValidWorld();
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    ASSERT_EQ(r.proofPaths.Size(), 6u);
    for (const ProofPath& path : r.proofPaths)
    {
        EXPECT_TRUE(IsValidWalk(w, path));
        EXPECT_EQ(w.Cells()[path.to].stratum, kStratumAncient);
        EXPECT_EQ(path.profileCrc, StringCRC("P").Value());
    }
    for (int t = 0; t < 6; ++t)
    {
        const Measurement* m = FindMeasurement(r, StringCRC("thronesOfType"), t);
        ASSERT_NE(m, nullptr) << "type " << t;
        EXPECT_FLOAT_EQ(m->value, 1.0f);
    }
    const Measurement* ai = FindMeasurement(r, StringCRC("ancientInteriorsCells"));
    ASSERT_NE(ai, nullptr);
    EXPECT_FLOAT_EQ(ai->value, 36.0f);   // 6 x 6 block
}

TEST(WorldThroneGuarantee, ThroneStandingOnAnAncientInteriorsCellNeedsNoMovement)
{
    const WorldStructure w = MakeValidWorld();
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    for (const ProofPath& path : r.proofPaths) { EXPECT_EQ(path.cells.Size(), 1u); }   // all six thrones sit in the block
}

// ---- DefaultValidationSet and AC4 on the real guarantees ------------------------------------------------------------

TEST(DefaultValidationSet, HoldsOneProfileAndTheThreeBuiltInsInOrder)
{
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    EXPECT_EQ(set.GetParams().profiles.Size(), 1u);
    ASSERT_EQ(set.GetParams().guarantees.Size(), 3u);
    EXPECT_EQ(set.GetParams().guarantees[0]->GetId(), HearthNexusRule().GetId());
    EXPECT_EQ(set.GetParams().guarantees[1]->GetId(), SpacingRule().GetId());
    EXPECT_EQ(set.GetParams().guarantees[2]->GetId(), ThroneRule().GetId());
    EXPECT_EQ(set.GetParams().profiles[0]->GetId(), StringCRC("CoW.Nav.TerrainTable"));
}

TEST(DefaultValidationSet, AC4_ValidWorldPassesWithThreeGuarantees)
{
    const WorldStructure w = MakeValidWorld();
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    const ValidationResult r = ReachabilityValidator::Validate(w, set.GetParams());
    EXPECT_TRUE(r.passed) << (r.guarantees.Size() > 0 ? "see per-guarantee results" : "");
    EXPECT_EQ(r.guarantees.Size(), 3u);
    for (const GuaranteeResult& g : r.guarantees) { EXPECT_TRUE(g.satisfied) << g.idCrc; }
}

TEST(DefaultValidationSet, AC4_AnyBrokenGuaranteeFailsTheWhole)
{
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());

    WorldStructure close = MakeValidWorld();
    AddSiteAt(close, SiteKinds::DynastyStart(), 4, 2);
    const ValidationResult r1 = ReachabilityValidator::Validate(close, set.GetParams());
    EXPECT_FALSE(r1.passed);
    EXPECT_FALSE(r1.Find(SpacingRule().GetId())->satisfied);
    EXPECT_TRUE(r1.Find(ThroneRule().GetId())->satisfied);   // independent

    WorldStructure fewer = MakeAllLandWorld(30, 20);
    FillLand(fewer, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    const ValidationResult r2 = ReachabilityValidator::Validate(fewer, set.GetParams());
    EXPECT_FALSE(r2.passed);
    EXPECT_FALSE(r2.Find(ThroneRule().GetId())->satisfied);
    EXPECT_TRUE(r2.Find(HearthNexusRule().GetId())->satisfied);   // no hearths: vacuous
}

TEST(DefaultValidationSet, CallersCanAppendToACopyOfTheParams)
{
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    ValidationParams copy = set.GetParams();
    FixedGuarantee extra("Extra", false);
    AllowWaterProfile more("More");
    EXPECT_TRUE(copy.AddGuarantee(&extra));
    EXPECT_TRUE(copy.AddProfile(&more));
    EXPECT_EQ(copy.guarantees.Size(), 4u);
    EXPECT_EQ(copy.profiles.Size(), 2u);
    EXPECT_EQ(set.GetParams().guarantees.Size(), 3u);   // the set is unchanged
    const WorldStructure w = MakeValidWorld();
    const ValidationResult r = ReachabilityValidator::Validate(w, copy);
    EXPECT_FALSE(r.passed);
    EXPECT_EQ(r.guarantees.Size(), 4u);
}

TEST(DefaultValidationSet, AC7_ValidatingTwiceGivesIdenticalEvidence)
{
    const WorldStructure w = MakeValidWorld();
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    const ValidationResult a = ReachabilityValidator::Validate(w, set.GetParams());
    const ValidationResult b = ReachabilityValidator::Validate(w, set.GetParams());
    EXPECT_EQ(a, b);
    SpecValidationSet other(ShippedGuaranteeSpec(), TestRules());
    EXPECT_EQ(a, ReachabilityValidator::Validate(w, other.GetParams()));
}
