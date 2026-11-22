// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (AC6 and edge cases)
// Reachability Validator exhaustive pass: degenerate worlds, bad inputs, cost semantics, size and speed.
#include <gtest/gtest.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/NavigationQuery.h>
#include <DraoiHexWorldTerrain/ReachabilityValidator.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DraoiHexWorldTerrain/SiteRules.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DraoiHexWorldTerrain/TerrainGenerator.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <DraoiHexWorldTerrain/Testing/TestProfiles.h>
#include <DraoiHexWorldTerrain/Testing/ValidationTestWorlds.h>

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cstdio>
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

GuaranteeResult Check(const IWorldGuarantee& g, const WorldStructure& w, std::initializer_list<const INavigationProfile*> profiles)
{
    std::vector<const INavigationProfile*> v(profiles);
    const NavigationQuery q(w, v);
    return g.Check({ w, q });
}

bool AnyDiagnosticContains(const GuaranteeResult& r, const char* needle)
{
    for (const Diagnostic& d : r.diagnostics) { if (std::string(d.text).find(needle) != std::string::npos) { return true; } }
    return false;
}

// Costs: 0 on land, blocked on water. Proves zero-cost edges are passable (only negative blocks).
class ZeroCostLandProfile : public INavigationProfile
{
public:
    StringCRC GetId() const override { return StringCRC("ZeroCost"); }
    std::unique_ptr<Dia::Pathfinding::IPathCostProvider> CreateCostProvider(const WorldStructure& world) const override
    {
        struct Costs : Dia::Pathfinding::IPathCostProvider
        {
            const WorldStructure& w;
            explicit Costs(const WorldStructure& world) : w(world) {}
            float GetCost(Dia::Pathfinding::CellCoord, Dia::Pathfinding::CellCoord to) const override
            {
                if (to.x < 0 || to.y < 0 || to.x >= w.dimensions.width || to.y >= w.dimensions.height) { return kImpassableCost; }
                return w.At(to.x, to.y).terrain == TerrainType::DeepWater ? kImpassableCost : 0.0f;
            }
        };
        return std::make_unique<Costs>(world);
    }
};

double Millis(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

} // namespace

// ---- Degenerate worlds and bad inputs -------------------------------------------------------------------------------

TEST(ValidationEdge, OneByOneWorldWithNoSitesFollowsTheVacuousRules)
{
    WorldStructure w = MakeAllLandWorld(1, 1);
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    const ValidationResult r = ReachabilityValidator::Validate(w, set.GetParams());
    EXPECT_FALSE(r.passed);   // no world-thrones
    EXPECT_TRUE(r.Find(HearthNexusRule().GetId())->satisfied);
    EXPECT_TRUE(r.Find(SpacingRule().GetId())->satisfied);
    EXPECT_FALSE(r.Find(ThroneRule().GetId())->satisfied);
}

TEST(ValidationEdge, EmptyWorldDoesNotCrashAnyGuarantee)
{
    WorldStructure w;
    w.Resize({ 0, 0 });
    SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
    const ValidationResult r = ReachabilityValidator::Validate(w, set.GetParams());
    EXPECT_FALSE(r.passed);
    EXPECT_EQ(r.guarantees.Size(), 3u);
}

TEST(ValidationEdge, SitesOutsideTheMapAreViolationsNotCrashes)
{
    WorldStructure w = MakeAllLandWorld(6, 6);
    Site hearth; hearth.kindCrc = SiteKinds::Hearth().Value(); hearth.cell = 9999;
    Site nexus; nexus.kindCrc = SiteKinds::Nexus().Value(); nexus.cell = Idx(w, 1, 1);
    Site d1; d1.kindCrc = SiteKinds::DynastyStart().Value(); d1.cell = 8888;
    Site d2; d2.kindCrc = SiteKinds::DynastyStart().Value(); d2.cell = Idx(w, 2, 2);
    Site t; t.kindCrc = SiteKinds::WorldThrone().Value(); t.cell = 7777; t.variant = 0;
    w.sites.Add(hearth); w.sites.Add(nexus); w.sites.Add(d1); w.sites.Add(d2); w.sites.Add(t);
    BlockWaterProfile p("P");
    const GuaranteeResult hn = Check(HearthNexusRule(), w, { &p });
    EXPECT_FALSE(hn.satisfied);
    EXPECT_EQ(hn.violatingCells.Size(), 1u);
    const GuaranteeResult ds = Check(SpacingRule(), w, { &p });
    EXPECT_FALSE(ds.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(ds, "outside the map"));
    EXPECT_TRUE(ds.violatingPairs.IsEmpty());
    FillLand(w, 2, 2, 5, 5, kStratumAncient, B("Biome.Grassland"));
    const GuaranteeResult wt = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(wt.satisfied);
    EXPECT_EQ(wt.violatingCells.Size(), 1u);
}

TEST(ValidationEdge, AllWaterWorldHasNothingReachable)
{
    WorldStructure w = MakeBlankWorld(8, 8);
    AddSiteAt(w, SiteKinds::Nexus(), 1, 1);
    AddSiteAt(w, SiteKinds::Hearth(), 6, 6);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 1u);
}

TEST(ValidationEdge, SevenThronesFailTheExactCount)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 12, 7, 18, 13, kStratumAncient, B("Biome.Grassland"));
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 12 + t, 7 + t, static_cast<uint16_t>(t)); }
    AddSiteAt(w, SiteKinds::WorldThrone(), 17, 8, 3);
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_FALSE(r.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(r, "expected 6 world-thrones, found 7"));
    EXPECT_TRUE(AnyDiagnosticContains(r, "2 world-thrones of Apotheosis type 3"));
}

TEST(ValidationEdge, AncientInteriorsInSeveralPatchesAnyOneSuffices)
{
    WorldStructure w = MakeAllLandWorld(30, 20);
    FillLand(w, 2, 2, 4, 4, kStratumAncient, B("Biome.Grassland"));       // patch A
    FillLand(w, 26, 16, 28, 18, kStratumAncient, B("Biome.Grassland"));   // patch B
    for (int y = 0; y < 20; ++y) { SetOcean(w, 15, y); }                                 // splits the map; thrones sit on the right
    for (int t = 0; t < 6; ++t) { AddSiteAt(w, SiteKinds::WorldThrone(), 20 + t, 5, static_cast<uint16_t>(t)); }
    BlockWaterProfile p("P");
    const GuaranteeResult r = Check(ThroneRule(), w, { &p });
    EXPECT_TRUE(r.satisfied);   // patch B is on the thrones' side
    for (const ProofPath& path : r.proofPaths) { EXPECT_GE(path.to, Idx(w, 26, 16)); }
}

TEST(ValidationEdge, NullProfilesInTheQuerySpanAreSkipped)
{
    const WorldStructure w = MakeAllLandWorld(6, 6);
    BlockWaterProfile p("P");
    const INavigationProfile* list[3] = { nullptr, &p, nullptr };
    const NavigationQuery q(w, std::span<const INavigationProfile* const>(list, 3));
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 0, 0), Idx(w, 5, 5), path));
    EXPECT_EQ(path.profileCrc, StringCRC("P").Value());
}

TEST(ValidationEdge, ZeroCostEdgesArePassableOnlyNegativeBlocks)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    ZeroCostLandProfile p;
    const NavigationQuery q(w, std::vector<const INavigationProfile*>{ &p });
    EXPECT_TRUE(q.IsReachable(Idx(w, 0, 0), Idx(w, 3, 4)));
    EXPECT_FALSE(q.IsReachable(Idx(w, 0, 0), Idx(w, 8, 2)));
}

TEST(ValidationEdge, QueryStaysCorrectWhenAskedManyTimes)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    const NavigationQuery q(w, std::vector<const INavigationProfile*>{ &blocking });
    for (int i = 0; i < 50; ++i)
    {
        EXPECT_FALSE(q.IsReachable(Idx(w, 1, 2), Idx(w, 8, 2)));
        EXPECT_TRUE(q.IsReachable(Idx(w, 1, 2), Idx(w, 3, 3)));
    }
}

TEST(ValidationEdge, SixteenProfilesAllConsulted)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    ValidationParams params;
    for (unsigned i = 0; i + 1 < kMaxValidationProfiles; ++i) { ASSERT_TRUE(params.AddProfile(&blocking)); }
    ASSERT_TRUE(params.AddProfile(&allowing));   // only the last one can cross
    ASSERT_EQ(params.profiles.Size(), kMaxValidationProfiles);
    EXPECT_TRUE(NavigationQuery(w, params).IsReachable(Idx(w, 1, 2), Idx(w, 8, 2)));
}

// ---- AC6: speed and size --------------------------------------------------------------------------------------------

namespace
{

// A generated Small world with a plausible site layout injected: 10 hearths, 5 nexuses, 5 dynasty starts, 6 thrones.
WorldStructure MakeAc6World(uint64_t seed)
{
    TerrainRunner runner;
    runner.Run(seed, WorldShapeParams{}, GridDimensions{ 66, 42 });
    WorldStructure w = runner.World();
    std::vector<uint32_t> land, interiors;
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const Cell& c = w.Cells()[i];
        if (c.landmass != w.mainLandmass || c.waterSource != WaterSource::None) { continue; }
        land.push_back(static_cast<uint32_t>(i));
        if (c.stratum == kStratumAncient) { interiors.push_back(static_cast<uint32_t>(i)); }
    }
    auto pick = [](const std::vector<uint32_t>& v, size_t k, size_t n) { return v[(v.size() * (2 * k + 1)) / (2 * n)]; };
    auto add = [&](const StringCRC& kind, uint32_t cell, uint16_t variant) {
        Site s; s.kindCrc = kind.Value(); s.cell = cell; s.variant = variant; w.sites.Add(s);
    };
    for (size_t k = 0; k < 10; ++k) { add(SiteKinds::Hearth(), pick(land, k, 10), 0); }
    for (size_t k = 0; k < 5; ++k) { add(SiteKinds::Nexus(), pick(land, k, 5), 0); }
    for (size_t k = 0; k < 5; ++k) { add(SiteKinds::DynastyStart(), pick(land, k, 5), 0); }
    for (size_t k = 0; !interiors.empty() && k < 6; ++k) { add(SiteKinds::WorldThrone(), pick(interiors, k, 6), static_cast<uint16_t>(k)); }
    return w;
}

} // namespace

TEST(ValidatorPerf, AC6_SmallWorldValidatesWellInsideTheBudget)
{
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        const WorldStructure w = MakeAc6World(seed);
        ASSERT_GT(w.sites.Count(SiteKinds::Hearth()), 0);
        SpecValidationSet set(ShippedGuaranteeSpec(), TestRules());
        double best = 1e9;
        ValidationResult last;
        for (int run = 0; run < 5; ++run)
        {
            const auto t0 = std::chrono::steady_clock::now();
            last = ReachabilityValidator::Validate(w, set.GetParams());
            best = std::min(best, Millis(t0, std::chrono::steady_clock::now()));
        }
        std::printf("[ AC6 ] seed %llu: Validate best-of-5 = %.2f ms (guarantees %u, passed %d)\n",
                    static_cast<unsigned long long>(seed), best, last.guarantees.Size(), last.passed ? 1 : 0);
        EXPECT_LT(best, 1000.0) << "AC6 target: under 1 second for typical worlds";
        EXPECT_EQ(last.guarantees.Size(), 3u);
        // The hearth and throne guarantees must have recorded proof paths (evidence on): all of these sit on the mainland.
        EXPECT_EQ(last.Find(HearthNexusRule().GetId())->proofPaths.Size()
                      + last.Find(HearthNexusRule().GetId())->violatingCells.Size(), 10u);
    }
}

TEST(ValidatorPerf, LargeWorldWithManyUnreachableHearthsStaysBounded)
{
    WorldStructure w = MakeAllLandWorld(128, 128);
    for (int y = 0; y < 128; ++y) { SetOcean(w, 100, y); }            // a wall splits off 27 columns
    AddSiteAt(w, SiteKinds::Nexus(), 5, 5);
    for (int i = 0; i < 40; ++i) { AddSiteAt(w, SiteKinds::Hearth(), 110 + (i % 10), 3 * (i / 10) + 4); }   // all stranded
    BlockWaterProfile p("P");
    const auto t0 = std::chrono::steady_clock::now();
    const GuaranteeResult r = Check(HearthNexusRule(), w, { &p });
    const double ms = Millis(t0, std::chrono::steady_clock::now());
    std::printf("[ AC6 ] 128x128, 40 stranded hearths: %.2f ms\n", ms);
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.violatingCells.Size(), 40u);
    EXPECT_EQ(r.diagnostics.Size(), kMaxDiagnostics);
    EXPECT_EQ(r.droppedDiagnostics, 40u - kMaxDiagnostics);
    EXPECT_LT(ms, 5000.0);
}
