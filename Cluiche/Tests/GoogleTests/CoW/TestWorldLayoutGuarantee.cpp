// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (WorldLayoutGuarantee amendment),
// regions-and-sites.md (Layout guarantee). WorldLayoutGuarantee wraps CheckLayout as an IWorldGuarantee; SiteRegistry
// stores IWorldGuarantee.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/RegionsAndSites.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>

#include <DiaMaths/Core/RandomStream.h>

#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{

struct Placed
{
    WorldStructure world;
    RegionsAndSites::PlaceReport report;
};

Placed MakePlaced(uint64_t seed)
{
    Placed p;
    TerrainRunner runner;
    runner.Run(seed, WorldShapeParams{}, GridDimensions{ 66, 42 });
    p.world = runner.World();
    SiteRegistry registry;
    Dia::Maths::RandomStream rng(1);
    RegionsAndSites::Place(p.world, registry, TerrainRules::Defaults(), DeriveSubSeed(seed, uint64_t(0)), 0, rng, &p.report);
    return p;
}

GuaranteeResult RunLayout(const Placed& p)
{
    WorldLayoutGuarantee g(TerrainRules::Defaults(), p.report.limits);
    class NoNav : public INavigationQuery
    {
    public:
        bool IsReachable(uint32_t, uint32_t) const override { return false; }
        bool FindProofPath(uint32_t, uint32_t, ProofPath&) const override { return false; }
        bool FindProofPathToAny(uint32_t, std::span<const uint32_t>, ProofPath&) const override { return false; }
    } nav;
    return g.Check({ p.world, nav });
}

bool AnyDiagnosticContains(const GuaranteeResult& r, const char* needle)
{
    for (const Diagnostic& d : r.diagnostics) { if (std::string(d.text).find(needle) != std::string::npos) { return true; } }
    return false;
}

uint32_t FirstLandCell(const WorldStructure& w)
{
    for (int i = 0; i < w.CellCount(); ++i)
    {
        if (w.Cells()[i].landmass != 0 && w.Cells()[i].waterSource == WaterSource::None) { return static_cast<uint32_t>(i); }
    }
    return kNoCellIndex;
}

} // namespace

TEST(WorldLayoutGuarantee, IdIsStable)
{
    WorldLayoutGuarantee g(TerrainRules::Defaults(), LayoutLimits{});
    EXPECT_EQ(g.GetId(), StringCRC("CoW.Guarantee.WorldLayout"));
}

TEST(WorldLayoutGuarantee, GeneratedWorldsSatisfyItWithNoEvidenceOfViolations)
{
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        const Placed p = MakePlaced(seed);
        ASSERT_TRUE(CheckLayout(p.world, TerrainRules::Defaults(), p.report.limits).ok) << "seed " << seed;   // precondition
        const GuaranteeResult r = RunLayout(p);
        EXPECT_TRUE(r.satisfied) << "seed " << seed;
        EXPECT_TRUE(r.diagnostics.IsEmpty());
        EXPECT_TRUE(r.violatingCells.IsEmpty());
        EXPECT_TRUE(r.measurements.IsEmpty());
        EXPECT_EQ(r.idCrc, StringCRC("CoW.Guarantee.WorldLayout").Value());
    }
}

TEST(WorldLayoutGuarantee, UncoveredLandFailsWithCellEvidenceAndACount)
{
    Placed p = MakePlaced(1);
    const uint32_t cell = FirstLandCell(p.world);
    ASSERT_NE(cell, kNoCellIndex);
    p.world.Cells()[cell].region = kNoRegion;
    const GuaranteeResult r = RunLayout(p);
    EXPECT_FALSE(r.satisfied);
    bool found = false;
    for (uint32_t c : r.violatingCells) { found = found || c == cell; }
    EXPECT_TRUE(found);
    EXPECT_TRUE(AnyDiagnosticContains(r, LayoutCodeName(LayoutCode::UncoveredLand)));
    bool counted = false;
    for (const Measurement& m : r.measurements)
    {
        if (m.keyCrc == MeasurementKeys::ViolationCount().Value() && m.tag == static_cast<int>(LayoutCode::UncoveredLand))
        {
            counted = true;
            EXPECT_GE(m.value, 1.0f);
        }
    }
    EXPECT_TRUE(counted);
}

TEST(WorldLayoutGuarantee, EveryCheckViolationBecomesOneDiagnosticAndMatchesTheReport)
{
    Placed p = MakePlaced(2);
    const uint32_t cell = FirstLandCell(p.world);
    p.world.Cells()[cell].region = kNoRegion;
    for (int i = 0; i < p.world.CellCount(); ++i)   // and an ocean cell with a region
    {
        if (p.world.Cells()[i].landmass == 0) { p.world.Cells()[i].region = 1; break; }
    }
    const LayoutReport report = CheckLayout(p.world, TerrainRules::Defaults(), p.report.limits);
    ASSERT_FALSE(report.ok);
    const GuaranteeResult r = RunLayout(p);
    EXPECT_FALSE(r.satisfied);
    EXPECT_EQ(r.diagnostics.Size() + r.droppedDiagnostics, report.violations.size());
    EXPECT_TRUE(AnyDiagnosticContains(r, LayoutCodeName(LayoutCode::UncoveredLand)));
    EXPECT_TRUE(AnyDiagnosticContains(r, LayoutCodeName(LayoutCode::OceanHasRegion)));
}

TEST(WorldLayoutGuarantee, TerrainLimitedWorldsAnnotateRegionCountViolationsAsTerrainCause)
{
    Placed p = MakePlaced(1);
    p.world.regions.absorbedCells = 60000;   // forces AbsorbedOverCap
    LayoutLimits limited = p.report.limits;
    limited.regionLimitedBy = DepthZone::WildReaches;
    WorldLayoutGuarantee g(TerrainRules::Defaults(), limited);
    class NoNav : public INavigationQuery
    {
    public:
        bool IsReachable(uint32_t, uint32_t) const override { return false; }
        bool FindProofPath(uint32_t, uint32_t, ProofPath&) const override { return false; }
        bool FindProofPathToAny(uint32_t, std::span<const uint32_t>, ProofPath&) const override { return false; }
    } nav;
    const GuaranteeResult limitedResult = g.Check({ p.world, nav });
    EXPECT_FALSE(limitedResult.satisfied);
    EXPECT_TRUE(AnyDiagnosticContains(limitedResult, LayoutCodeName(LayoutCode::AbsorbedOverCap)));
    EXPECT_TRUE(AnyDiagnosticContains(limitedResult, "terrain cause"));

    const GuaranteeResult plain = RunLayout(p);   // same world, limits without regionLimitedBy
    EXPECT_FALSE(plain.satisfied);
    EXPECT_FALSE(AnyDiagnosticContains(plain, "terrain cause"));
}

TEST(WorldLayoutGuarantee, DeterministicAcrossCalls)
{
    Placed p = MakePlaced(3);
    p.world.Cells()[FirstLandCell(p.world)].region = kNoRegion;
    EXPECT_EQ(RunLayout(p), RunLayout(p));
}

TEST(WorldLayoutGuarantee, RunsAsAGuaranteeOfTheValidator)
{
    Placed p = MakePlaced(1);
    WorldLayoutGuarantee layout(TerrainRules::Defaults(), p.report.limits);
    FixedGuarantee other("Other", true);
    ValidationParams params;
    params.AddGuarantee(&layout);
    params.AddGuarantee(&other);
    ValidationResult r = ReachabilityValidator::Validate(p.world, params);   // no profile: a layout check needs none
    EXPECT_EQ(r.guarantees.Size(), 2u);
    EXPECT_TRUE(r.Find(layout.GetId())->satisfied);

    p.world.Cells()[FirstLandCell(p.world)].region = kNoRegion;
    r = ReachabilityValidator::Validate(p.world, params);
    EXPECT_FALSE(r.Find(layout.GetId())->satisfied);
    EXPECT_TRUE(r.Find(other.GetId())->satisfied);
}

TEST(SiteRegistryGuarantees, StoresWorldGuaranteesAndRejectsDuplicateIds)
{
    SiteRegistry registry;
    WorldLayoutGuarantee layout(TerrainRules::Defaults(), LayoutLimits{});
    WorldLayoutGuarantee layoutAgain(TerrainRules::Defaults(), LayoutLimits{});
    FixedGuarantee other("Other", true);
    EXPECT_TRUE(registry.RegisterGuarantee(&layout));
    EXPECT_FALSE(registry.RegisterGuarantee(&layout));        // same pointer
    EXPECT_FALSE(registry.RegisterGuarantee(&layoutAgain));   // same id
    EXPECT_TRUE(registry.RegisterGuarantee(&other));
    EXPECT_FALSE(registry.RegisterGuarantee(nullptr));
    ASSERT_EQ(registry.Guarantees().size(), 2u);
    EXPECT_EQ(registry.Guarantees()[0], &layout);
    EXPECT_EQ(registry.Guarantees()[1], &other);

    // The registered guarantees feed a ValidationParams directly.
    ValidationParams params;
    for (const IWorldGuarantee* g : registry.Guarantees()) { EXPECT_TRUE(params.AddGuarantee(g)); }
    EXPECT_EQ(params.guarantees.Size(), 2u);
}
