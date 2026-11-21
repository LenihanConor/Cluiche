// Spec: docs/specs/features/draoi/guarantee-set-injection.md (AC 1: generic rules, no CoW vocabulary)
// The three generic rules (reach_any, min_spacing, variant_cover) over non-CoW kinds and nouns, on hand-built worlds.
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/World/Testing/StratumTestIndices.h>
#include <Shared/WorldGeneration/NavigationQuery.h>
#include <Shared/WorldGeneration/SiteRules.h>
#include <Shared/WorldGeneration/Testing/RegionTestWorlds.h>
#include <Shared/WorldGeneration/Testing/TestProfiles.h>
#include <Shared/WorldGeneration/Testing/ValidationTestWorlds.h>

#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Core::StringCRC;

namespace
{

bool AnyDiagnosticIs(const GuaranteeResult& r, const char* text)
{
    for (const Diagnostic& d : r.diagnostics) { if (std::string(d.text) == text) { return true; } }
    return false;
}

GuaranteeResult Check(const IWorldGuarantee& g, const WorldStructure& w, std::initializer_list<const INavigationProfile*> profiles)
{
    std::vector<const INavigationProfile*> v(profiles);
    const NavigationQuery q(w, v);
    return g.Check({ w, q });
}

GuaranteeEntry CampWellEntry()
{
    GuaranteeEntry e;
    e.id = "Test.Guarantee.CampWell"; e.type = GuaranteeRuleType::ReachAny;
    e.from = "Test.Site.Camp"; e.to = "Test.Site.Well";
    e.noun = "Camp"; e.toNoun = "Well"; e.toNounPlural = "Wells";
    return e;
}

GuaranteeEntry FlagEntry(float minimum)
{
    GuaranteeEntry e;
    e.id = "Test.Guarantee.FlagSpacing"; e.type = GuaranteeRuleType::MinSpacing;
    e.kind = "Test.Site.Flag"; e.minimum = minimum;
    e.noun = "Flag"; e.nounPlural = "Flags";
    e.minimumKey = "flagMinimum"; e.distanceKey = "flagDistance";
    return e;
}

GuaranteeEntry TotemEntry()
{
    GuaranteeEntry e;
    e.id = "Test.Guarantee.Totem"; e.type = GuaranteeRuleType::VariantCover;
    e.kind = "Test.Site.Totem"; e.variantCount = 3; e.toStratumIndex = kStratumAncient;
    e.noun = "totem"; e.nounPlural = "totems"; e.variantNoun = "colour"; e.targetNoun = "Sacred Grove";
    e.countKey = "totemsOfColour"; e.targetCountKey = "groveCells";
    return e;
}

// 30x10 all land, a Sacred Grove (zone 3) block at x 12..17, y 3..6.
WorldStructure MakeGroveWorld()
{
    WorldStructure w = MakeAllLandWorld(30, 10, kStratumWild);
    FillLand(w, 12, 3, 18, 7, kStratumAncient, B("Biome.Grassland"));
    return w;
}

} // namespace

// ---- reach_any ------------------------------------------------------------------------------------------------------

TEST(SiteRules, ReachAny_IdComesFromTheEntry)
{
    EXPECT_EQ(ReachAnyGuarantee(CampWellEntry()).GetId(), StringCRC("Test.Guarantee.CampWell"));
}

TEST(SiteRules, ReachAny_SatisfiedWhenEverySourceReachesATarget)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    AddSiteAt(w, "Test.Site.Well", 1, 3);
    AddSiteAt(w, "Test.Site.Camp", 2, 5);
    AddSiteAt(w, "Test.Site.Camp", 0, 0);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ReachAnyGuarantee(CampWellEntry()), w, { &p });
    EXPECT_TRUE(r.satisfied);
    EXPECT_EQ(r.proofPaths.Size(), 2u);
    EXPECT_TRUE(r.violatingCells.IsEmpty());
    EXPECT_EQ(r.idCrc, StringCRC("Test.Guarantee.CampWell").Value());
}

TEST(SiteRules, ReachAny_UnreachableSourceFailsWithDataNouns)
{
    WorldStructure w = MakeChannelWorld(12, 7, 4, 2);
    AddSiteAt(w, "Test.Site.Well", 1, 3);
    AddSiteAt(w, "Test.Site.Camp", 9, 2);   // right bank
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ReachAnyGuarantee(CampWellEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 1u);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Camp at (9,2) unreachable from any Well"));
}

TEST(SiteRules, ReachAny_NoTargetsAtAllUsesThePluralNoun)
{
    WorldStructure w = MakeAllLandWorld(8, 8, kStratumWild);
    AddSiteAt(w, "Test.Site.Camp", 3, 4);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ReachAnyGuarantee(CampWellEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Camp at (3,4) unreachable: no Wells exist"));
}

TEST(SiteRules, ReachAny_NoSourcesIsVacuouslySatisfied)
{
    WorldStructure w = MakeAllLandWorld(8, 8, kStratumWild);
    AddSiteAt(w, "Test.Site.Well", 3, 4);
    BlockWaterProfile p("P");
    EXPECT_TRUE(Check(ReachAnyGuarantee(CampWellEntry()), w, { &p }).satisfied);
}

TEST(SiteRules, ReachAny_ZoneTargetReachesAnyCellOfTheZone)
{
    GuaranteeEntry e = CampWellEntry();
    e.to.clear(); e.toStratumIndex = kStratumAncient; e.toNoun = "Grove"; e.toNounPlural = "Groves";
    WorldStructure w = MakeGroveWorld();
    AddSiteAt(w, "Test.Site.Camp", 1, 1);
    BlockWaterProfile p("P");
    EXPECT_TRUE(Check(ReachAnyGuarantee(e), w, { &p }).satisfied);

    WorldStructure none = MakeAllLandWorld(8, 8, kStratumWild);
    AddSiteAt(none, "Test.Site.Camp", 1, 1);
    const GuaranteeResult r = Check(ReachAnyGuarantee(e), none, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Camp at (1,1) unreachable: no Groves exist"));
}

// ---- min_spacing ----------------------------------------------------------------------------------------------------

TEST(SiteRules, MinSpacing_PairsAtOrAboveTheMinimumSatisfy)
{
    WorldStructure w = MakeAllLandWorld(30, 20, kStratumWild);
    AddSiteAt(w, "Test.Site.Flag", 2, 2);
    AddSiteAt(w, "Test.Site.Flag", 12, 2);
    const uint32_t d = HexSteps(w, Idx(w, 2, 2), Idx(w, 12, 2));
    EXPECT_TRUE(Check(MinSpacingGuarantee(FlagEntry(static_cast<float>(d))), w, {}).satisfied);
    EXPECT_FALSE(Check(MinSpacingGuarantee(FlagEntry(static_cast<float>(d) + 1.0f)), w, {}).satisfied);
}

TEST(SiteRules, MinSpacing_TooCloseNamesBothPositionsAndUsesDataKeys)
{
    WorldStructure w = MakeAllLandWorld(30, 20, kStratumWild);
    AddSiteAt(w, "Test.Site.Flag", 5, 5);
    AddSiteAt(w, "Test.Site.Flag", 7, 5);
    const GuaranteeResult r = Check(MinSpacingGuarantee(FlagEntry(10.0f)), w, {});
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingPairs.Size(), 1u);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Flags at (5,5) and (7,5) too close: distance 2 < minimum 10"));
    ASSERT_EQ(r.measurements.Size(), 2u);
    EXPECT_EQ(r.measurements[0].keyCrc, StringCRC("flagMinimum").Value());
    EXPECT_FLOAT_EQ(r.measurements[0].value, 10.0f);
    EXPECT_EQ(r.measurements[1].keyCrc, StringCRC("flagDistance").Value());
    EXPECT_FLOAT_EQ(r.measurements[1].value, 2.0f);
}

TEST(SiteRules, MinSpacing_FewerThanTwoSitesIsVacuousWithNoEvidence)
{
    WorldStructure w = MakeAllLandWorld(30, 20, kStratumWild);
    AddSiteAt(w, "Test.Site.Flag", 5, 5);
    const GuaranteeResult r = Check(MinSpacingGuarantee(FlagEntry(10.0f)), w, {});
    EXPECT_TRUE(r.satisfied);
    EXPECT_TRUE(r.measurements.IsEmpty());
}

TEST(SiteRules, MinSpacing_SiteOutsideTheMapFailsAndIsReported)
{
    WorldStructure w = MakeAllLandWorld(10, 10, kStratumWild);
    AddSiteAt(w, "Test.Site.Flag", 1, 1);
    Site off;
    off.kindCrc = StringCRC("Test.Site.Flag").Value();
    off.cell = 5000;
    w.sites.Add(off);
    const GuaranteeResult r = Check(MinSpacingGuarantee(FlagEntry(2.0f)), w, {});
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 1u);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Flag at cell 5000 is outside the map"));
}

TEST(SiteRules, MinSpacing_ExposesTheMinimum)
{
    EXPECT_FLOAT_EQ(MinSpacingGuarantee(FlagEntry(7.5f)).MinimumSpacing(), 7.5f);
}

// ---- variant_cover --------------------------------------------------------------------------------------------------

TEST(SiteRules, VariantCover_ThreeReachableVariantsSatisfy)
{
    WorldStructure w = MakeGroveWorld();
    for (int v = 0; v < 3; ++v) { AddSiteAt(w, "Test.Site.Totem", 12 + v, 3 + v, static_cast<uint16_t>(v)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w, { &p });
    EXPECT_TRUE(r.satisfied);
    EXPECT_TRUE(r.diagnostics.IsEmpty());
    EXPECT_EQ(r.proofPaths.Size(), 3u);
    int perColour = 0;
    for (const Measurement& m : r.measurements)
    {
        if (m.keyCrc == StringCRC("totemsOfColour").Value()) { EXPECT_FLOAT_EQ(m.value, 1.0f); ++perColour; }
        if (m.keyCrc == StringCRC("groveCells").Value()) { EXPECT_FLOAT_EQ(m.value, 24.0f); }
    }
    EXPECT_EQ(perColour, 3);
}

TEST(SiteRules, VariantCover_MissingVariantAndWrongCount)
{
    WorldStructure w = MakeGroveWorld();
    AddSiteAt(w, "Test.Site.Totem", 12, 3, 0);
    AddSiteAt(w, "Test.Site.Totem", 13, 4, 1);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "expected 3 totems, found 2"));
    EXPECT_TRUE(AnyDiagnosticIs(r, "no totem of colour 2"));
}

TEST(SiteRules, VariantCover_DuplicateVariantFails)
{
    WorldStructure w = MakeGroveWorld();
    AddSiteAt(w, "Test.Site.Totem", 12, 3, 0);
    AddSiteAt(w, "Test.Site.Totem", 13, 4, 1);
    AddSiteAt(w, "Test.Site.Totem", 14, 5, 1);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "2 totems of colour 1"));
    EXPECT_TRUE(AnyDiagnosticIs(r, "no totem of colour 2"));
}

TEST(SiteRules, VariantCover_InvalidVariantIsReported)
{
    WorldStructure w = MakeGroveWorld();
    AddSiteAt(w, "Test.Site.Totem", 12, 3, 0);
    AddSiteAt(w, "Test.Site.Totem", 13, 4, 1);
    AddSiteAt(w, "Test.Site.Totem", 14, 5, 3);   // first invalid variant
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Totem at (14,5) has invalid colour 3"));
}

TEST(SiteRules, VariantCover_SiteCutOffFromTheZoneFails)
{
    // A water wall at column 9 cuts the totem at (1,1) off from the grove.
    WorldStructure w2 = MakeGroveWorld();
    for (int y = 0; y < 10; ++y) { SetTerrain(w2, 9, y, TerrainType::DeepWater); }
    AddSiteAt(w2, "Test.Site.Totem", 1, 1, 0);
    AddSiteAt(w2, "Test.Site.Totem", 13, 4, 1);
    AddSiteAt(w2, "Test.Site.Totem", 14, 5, 2);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w2, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 1u);
    EXPECT_TRUE(AnyDiagnosticIs(r, "Totem at (1,1) unreachable from the Sacred Grove"));
}

TEST(SiteRules, VariantCover_EmptyTargetZoneIsATerrainCause)
{
    WorldStructure w = MakeAllLandWorld(10, 10, kStratumWild);
    for (int v = 0; v < 3; ++v) { AddSiteAt(w, "Test.Site.Totem", v, v, static_cast<uint16_t>(v)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(VariantCoverGuarantee(TotemEntry()), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticIs(r, "no Sacred Grove cells (terrain cause)"));
}

TEST(SiteRules, VariantCover_AppliesOnlyWhenSitesOfTheKindExist)
{
    const VariantCoverGuarantee rule(TotemEntry());
    WorldStructure w = MakeGroveWorld();
    EXPECT_FALSE(rule.AppliesTo(w));
    AddSiteAt(w, "Test.Site.Totem", 12, 3, 0);
    EXPECT_TRUE(rule.AppliesTo(w));
    EXPECT_EQ(rule.Kind(), StringCRC("Test.Site.Totem"));
}

// ---- factory --------------------------------------------------------------------------------------------------------

TEST(SiteRules, MakeSiteRuleBuildsTheRuleOfTheEntryType)
{
    EXPECT_EQ(MakeSiteRule(CampWellEntry())->GetId(), StringCRC("Test.Guarantee.CampWell"));
    EXPECT_EQ(MakeSiteRule(FlagEntry(1.0f))->GetId(), StringCRC("Test.Guarantee.FlagSpacing"));
    EXPECT_EQ(MakeSiteRule(TotemEntry())->GetId(), StringCRC("Test.Guarantee.Totem"));
}
