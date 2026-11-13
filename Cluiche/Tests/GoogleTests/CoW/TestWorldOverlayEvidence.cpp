// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7, AC3-AC6, AC10)
// Plan: worldgen-panel-rest.plan.md task 3 (coloured region edges, tagged site markers, dynasty starts, evidence).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/SiteGuarantees.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/WorldLayerSource.h>
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>

#include <cmath>

using namespace CoW::WorldGeneration;
using Dia::Maths::Vector2D;

namespace
{
float Dst(Vector2D a, Vector2D b) { return std::hypot(a.x - b.x, a.y - b.y); }

// 8x8 world, left half (col < 4) region 1, right half region 2.
void TwoRegions(WorldStructure& w)
{
    w.Resize({ 8, 8 });
    Cell* c = w.Cells();
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            c[row * 8 + col].region = col < 4 ? 1 : 2;
}

GuaranteeResult Failing(uint32_t id)
{
    GuaranteeResult g;
    g.idCrc = id;
    g.satisfied = false;
    return g;
}

ProofPath Path(std::initializer_list<uint32_t> cells)
{
    ProofPath p;
    for (uint32_t c : cells) p.cells.Add(c);
    p.from = *cells.begin();
    p.to = *(cells.end() - 1);
    return p;
}
}

TEST(WorldOverlayEvidence, RegionEdgesColouredOnTwoRegionWorldAndNoneInsideOneRegion)
{
    WorldStructure w;
    TwoRegions(w);
    WorldHexGrid grid(w.dimensions);
    const std::vector<RegionEdge> edges = BuildColouredRegionEdges(w, grid);
    ASSERT_FALSE(edges.empty());
    EXPECT_EQ(edges.size(), BuildRegionEdges(w, grid).size());
    for (const RegionEdge& e : edges)
    {
        EXPECT_NE(e.regionA, e.regionB);
        EXPECT_EQ(e.colourSlot, RegionPaletteSlot(e.regionA));
    }
    // One region only: nothing inside it, and the map border is not drawn.
    for (int i = 0; i < w.CellCount(); ++i) w.Cells()[i].region = 3;
    EXPECT_TRUE(BuildColouredRegionEdges(w, grid).empty());
}

TEST(WorldOverlayEvidence, CoastEdgeUsesTheLandRegionColour)
{
    WorldStructure w;
    w.Resize({ 8, 8 });
    for (int i = 0; i < w.CellCount(); ++i) w.Cells()[i].region = 0;
    w.Cells()[27].region = 5;   // lone region cell: every edge is a coast
    WorldHexGrid grid(w.dimensions);
    const std::vector<RegionEdge> edges = BuildColouredRegionEdges(w, grid);
    EXPECT_EQ(edges.size(), 6u);
    for (const RegionEdge& e : edges) EXPECT_EQ(e.colourSlot, RegionPaletteSlot(5));
}

TEST(WorldOverlayEvidence, SiteMarkersCarryKindShapeColourAndSkipUnregisteredKinds)
{
    WorldStructure w;
    w.Resize({ 8, 8 });
    const Dia::Core::StringCRC ka("Test.A"), kb("Test.B");
    Site s; s.kindCrc = SiteKindId(ka); s.cell = 9;  w.sites.Add(s);
    s.cell = 30;                                      w.sites.Add(s);
    s.kindCrc = SiteKindId(kb); s.cell = 12;          w.sites.Add(s);
    s.kindCrc = 0x1234u; s.cell = 40;                 w.sites.Add(s);   // unregistered
    WorldHexGrid grid(w.dimensions);

    const std::vector<SiteKindInfo> kinds = {
        MakeSiteKindInfo(ka, "A", 0xFF0000, SiteShape::Star),
        MakeSiteKindInfo(kb, "B", 0x00FF00, SiteShape::Triangle) };
    const std::vector<SiteMarker> m = BuildSiteMarkers(w, grid, kinds, 6.0f);
    ASSERT_EQ(m.size(), 3u);
    EXPECT_EQ(m[0].kindCrc, SiteKindId(ka));
    EXPECT_EQ(m[0].shape, SiteShape::Star);
    EXPECT_EQ(m[0].colour, 0xFF0000u);
    EXPECT_EQ(m[0].cell, 9u);
    EXPECT_LT(Dst(m[0].centre, grid.CellCenter(9)), 1e-3f);
    EXPECT_EQ(m[2].kindCrc, SiteKindId(kb));
    EXPECT_EQ(m[2].shape, SiteShape::Triangle);
    EXPECT_EQ(m[2].colour, 0x00FF00u);
    EXPECT_EQ(m[2].lines.size(), 3u);
    for (const SiteMarker& x : m)
        for (const LineSegment& l : x.lines) EXPECT_LE(Dst(l.a, x.centre), 6.0f + 1e-3f);

    // Per-kind filter: the drawer drops markers by kindCrc.
    size_t a = 0;
    for (const SiteMarker& x : m) if (x.kindCrc == SiteKindId(ka)) ++a;
    EXPECT_EQ(a, 2u);

    WorldHexGrid other({ 9, 8 });
    EXPECT_TRUE(BuildSiteMarkers(w, other, kinds, 6.0f).empty());
}

TEST(WorldOverlayEvidence, DynastyRingRedOnlyForTheViolatingPair)
{
    WorldStructure w;
    w.Resize({ 16, 16 });
    Site s; s.kindCrc = SiteKindId(SiteKinds::DynastyStart());
    s.cell = 17;  w.sites.Add(s);
    s.cell = 19;  w.sites.Add(s);
    s.cell = 200; w.sites.Add(s);
    WorldHexGrid grid(w.dimensions);

    GuaranteeResult g = Failing(SiteKinds::DynastyStart().Value());
    CellPair p; p.a = 17; p.b = 19; g.violatingPairs.Add(p);

    const std::vector<DynastyStartMarker> m = BuildDynastyStarts(w, grid, 10.0f, &g, 6.0f);
    ASSERT_EQ(m.size(), 3u);
    EXPECT_TRUE(m[0].violating);
    EXPECT_TRUE(m[1].violating);
    EXPECT_FALSE(m[2].violating);
    EXPECT_FALSE(m[0].marker.empty());
    ASSERT_FALSE(m[2].ring.empty());
    EXPECT_NEAR(Dst(m[2].ring[0].a, m[2].centre), 10.0f * std::sqrt(3.0f) * grid.CellSize(), 1e-2f);

    for (const DynastyStartMarker& x : BuildDynastyStarts(w, grid, 10.0f, nullptr, 6.0f))
        EXPECT_FALSE(x.violating);
}

TEST(WorldOverlayEvidence, OnePolylinePerProofPathOfTheExpandedGuaranteeOnly)
{
    WorldHexGrid grid({ 8, 8 });
    ValidationResult v;
    GuaranteeResult a; a.idCrc = 100; a.satisfied = true;
    a.proofPaths.Add(Path({ 0, 1, 2 }));
    a.proofPaths.Add(Path({ 8, 9 }));
    GuaranteeResult b; b.idCrc = 200; b.satisfied = true;
    b.proofPaths.Add(Path({ 16, 17, 18, 19 }));
    v.guarantees.Add(a);
    v.guarantees.Add(b);

    const GuaranteeEvidenceGeometry ea = BuildGuaranteeEvidence(&v, grid, 100);
    ASSERT_EQ(ea.polylines.size(), 2u);
    EXPECT_EQ(ea.polylines[0].points.size(), 3u);
    EXPECT_LT(Dst(ea.polylines[0].points[1], grid.CellCenter(1)), 1e-3f);
    EXPECT_EQ(ea.polylines[0].guaranteeCrc, 100u);
    EXPECT_EQ(ea.LineCount(), 3u);
    EXPECT_EQ(BuildGuaranteeEvidence(&v, grid, 200).polylines.size(), 1u);

    const GuaranteeEvidenceGeometry none = BuildGuaranteeEvidence(&v, grid, 0);
    EXPECT_TRUE(none.polylines.empty());
    EXPECT_TRUE(none.violations.empty());
    EXPECT_TRUE(BuildGuaranteeEvidence(nullptr, grid, 100).polylines.empty());
}

TEST(WorldOverlayEvidence, ViolatingCellsOfEveryFailingGuaranteeMarkedRegardlessOfExpansion)
{
    WorldHexGrid grid({ 8, 8 });
    ValidationResult v;
    GuaranteeResult a = Failing(100); a.violatingCells.Add(3); a.violatingCells.Add(4);
    GuaranteeResult b = Failing(200); b.violatingCells.Add(20);
    GuaranteeResult ok; ok.idCrc = 300; ok.satisfied = true; ok.violatingCells.Add(50);   // satisfied: not marked
    v.guarantees.Add(a); v.guarantees.Add(b); v.guarantees.Add(ok);

    for (const uint32_t expanded : { 0u, 100u, 999u })
    {
        const GuaranteeEvidenceGeometry g = BuildGuaranteeEvidence(&v, grid, expanded);
        ASSERT_EQ(g.violations.size(), 3u) << expanded;
        EXPECT_EQ(g.violations[2].cell, 20u);
        EXPECT_EQ(g.violations[2].guaranteeCrc, 200u);
        EXPECT_EQ(g.violations[0].outline.size(), 6u);
        EXPECT_EQ(g.violations[0].cross.size(), 2u);
        EXPECT_LT(Dst(g.violations[0].outline[0].a, grid.CellCenter(3)), grid.CellSize() + 1e-3f);
    }
}

TEST(WorldOverlayEvidence, StaleImportedOrFailedEvidenceDrawsNothing)
{
    EXPECT_TRUE(EvidenceIsFresh(5, 0, 5));
    EXPECT_TRUE(EvidenceIsFresh(0, 5, 5));
    EXPECT_TRUE(EvidenceIsFresh(4, 5, 5));
    EXPECT_FALSE(EvidenceIsFresh(4, 3, 5));    // stale
    EXPECT_FALSE(EvidenceIsFresh(0, 0, 5));    // imported / failed generation: no report revision
    EXPECT_FALSE(EvidenceIsFresh(0, 0, 0));    // revision 0 is "none"

    GenerationReport r;
    r.worldRevision = 4;
    GuaranteeResult g = Failing(100);
    g.violatingCells.Add(3);
    r.finalValidation.guarantees.Add(g);
    ValidationResult validated;

    EXPECT_EQ(FreshEvidence(&r, nullptr, 0, 4), &r.finalValidation);
    EXPECT_EQ(FreshEvidence(&r, nullptr, 0, 5), nullptr);                 // stale report
    EXPECT_EQ(FreshEvidence(&r, &validated, 5, 5), &validated);           // fresh Validate wins
    EXPECT_EQ(FreshEvidence(&r, &validated, 3, 5), nullptr);              // both stale
    EXPECT_EQ(FreshEvidence(nullptr, nullptr, 0, 5), nullptr);            // imported
    r.worldRevision = 0;                                                  // failed generation
    EXPECT_EQ(FreshEvidence(&r, nullptr, 0, 0), nullptr);

    WorldHexGrid grid({ 8, 8 });
    EXPECT_EQ(BuildGuaranteeEvidence(FreshEvidence(&r, nullptr, 0, 5), grid, 100).LineCount(), 0u);
}

TEST(WorldOverlayEvidence, LargeWorldCountsWithinBudgets)
{
    WorldGenSession s;
    Json::Value sz(Json::objectValue);
    sz["size"] = "Large";
    s.SetSize(sz);
    Json::Value seed(Json::objectValue);
    seed["seed"] = Json::Value(static_cast<Json::UInt64>(12345));
    Dia::Maths::RandomStream rng(12345);
    s.Regenerate(seed);
    ASSERT_TRUE(s.Tick(rng));

    WorldHexGrid grid(s.World().dimensions);
    const std::vector<RegionEdge> edges = BuildColouredRegionEdges(s.World(), grid);
    EXPECT_FALSE(edges.empty());
    EXPECT_LE(edges.size(), 2000u);
    EXPECT_EQ(edges.size(), BuildRegionEdges(s.World(), grid).size());

    const std::vector<SiteKindInfo> kinds = ListSiteKinds(s.Registry());
    const std::vector<SiteMarker> markers = BuildSiteMarkers(s.World(), grid, kinds, grid.CellSize() * 0.45f);
    EXPECT_LT(markers.size(), 100u + 200u) << "built-in nodes are ~117 at defaults; the <100 budget is for dragon-layer kinds";

    const Json::Value report = s.Report(Json::Value(Json::objectValue));
    (void)report;
    const ValidationResult* ev = FreshEvidence(s.LastReport(), s.HasEvidence() ? &s.Evidence() : nullptr,
                                               s.EvidenceRevision(), s.Revision());
    ASSERT_NE(ev, nullptr);
    uint32_t expand = 0;
    for (const GuaranteeResult& g : ev->guarantees) if (!g.proofPaths.IsEmpty()) { expand = g.idCrc; break; }
    const GuaranteeEvidenceGeometry g = BuildGuaranteeEvidence(ev, grid, expand);
    EXPECT_LT(g.LineCount(), 1500u);
    const std::vector<DynastyStartMarker> starts = BuildDynastyStarts(s.World(), grid, kDefaultMinimumDynastySpacing, nullptr, 6.0f);
    EXPECT_LT(starts.size(), 50u);
}
