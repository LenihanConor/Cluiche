// S-1 Terrain Generator task 14: determinism (AC1), slider isolation (AC14) and golden hashes.
// The expected dependency table and FirstStageAffected rows are hard-coded here from the spec, independent of
// kStageReads, so a wrong table entry in the generator is caught. Wide sweeps are SLOW_.
//
// Regenerating the goldens (only after an intentional generator change, with kTerrainGeneratorVersion bumped):
// set every constant in kGolden to 0, run `dia run googletest --filter="TerrainDeterminism.Golden*"` and copy the
// "actual" values from the failure messages.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;

namespace {

const GridDimensions kSmall{ 66, 42 };
const GridDimensions kIso{ 40, 30 };

// Spec table: stage -> stages it reads (direct). Stage 11 reads only the grid.
const std::set<int> kSpecReads[12] =
{
    {}, { 0 }, { 0, 1 }, { 0, 1 }, { 0, 1, 2, 3 }, { 4 }, { 4, 5 }, { 5 },
    { 1, 2, 3, 6 }, { 4, 5, 6, 7 }, { 1, 2, 3, 6, 7, 8, 9 }, {}
};

// Spec Sliders table: first stage affected per slider (ShapeParam order, 18 terrain sliders).
const int kSpecFirstStage[kTerrainSliderCount] =
{
    1, 1, 1, 1, 1, 1,   // landFraction continents islands continentIslands peninsulas coastline
    2, 2, 2, 2,         // mountainousness spineToRing orientationDeg passes
    3, 3,               // barrier barrierForm
    4,                  // ruggedness
    6,                  // rivers
    9,                  // valleyDepth
    7,                  // lakes
    11, 11              // latitudeNorth latitudeSouth
};

// Stages that must re-run (transitively) when a slider with the given first stage changes.
std::set<int> SpecDirty(int first)
{
    std::set<int> d{ first };
    for (int s = 1; s < 12; ++s)
    {
        for (int r : kSpecReads[s])
        {
            if (d.count(r)) { d.insert(s); break; }
        }
    }
    return d;
}

std::set<int> RanStages(const TerrainGenerator& g)
{
    std::set<int> r;
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        if (g.GetTimings()[s].ranLastUpdate) { r.insert(s); }
    }
    return r;
}

std::string Join(const std::vector<std::string>& v)
{
    std::string s;
    for (const std::string& x : v) { s += x + " "; }
    return s;
}

// First value index whose result differs from base for the slider (-1 if none).
int ValueDifferingFrom(const WorldShapeParams& base, int slider, int startAt = 0)
{
    const int n = SliderValueCount(slider);
    for (int k = 0; k < n; ++k)
    {
        WorldShapeParams p = base;
        SetSliderValue(p, slider, startAt + k);
        if (!(p == base)) { return startAt + k; }
    }
    return -1;
}

void CheckLiveMatchesFresh(uint64_t seed, int changes, GridDimensions dims, uint64_t scriptSeed)
{
    SCOPED_TRACE(seed);
    TerrainRunner live, fresh;
    WorldShapeParams params;
    const TerrainSnapshot a = live.Run(seed, params, dims);
    const TerrainSnapshot b = fresh.RunFresh(seed, params, dims);
    ASSERT_TRUE(DiffSnapshots(a, b).empty()) << Join(DiffSnapshots(a, b));

    const std::vector<SliderChange> script = BuildSliderScript(scriptSeed ^ seed, changes);
    int i = 0;
    for (const SliderChange& c : script)
    {
        SetSliderValue(params, c.slider, c.value);
        const TerrainSnapshot l = live.Run(seed, params, dims);
        const TerrainSnapshot f = fresh.RunFresh(seed, params, dims);
        const std::vector<std::string> diff = DiffSnapshots(l, f);
        ASSERT_TRUE(diff.empty()) << "change " << i << " slider " << ShapeParamName(SliderParam(c.slider))
                                  << " value " << c.value << " differs: " << Join(diff);
        ++i;
    }
}

// Changes one slider on a cached world, then reverts it. Checks AC14 isolation.
void CheckSliderIsolation(TerrainRunner& live, TerrainRunner& fresh, uint64_t seed, const WorldShapeParams& base,
                          int slider, int valueIndex, GridDimensions dims, const TerrainSnapshot& baseSnap)
{
    const char* name = ShapeParamName(SliderParam(slider));
    SCOPED_TRACE(name);
    WorldShapeParams changed = base;
    SetSliderValue(changed, slider, valueIndex);
    ASSERT_FALSE(changed == base);

    const std::set<int> expected = SpecDirty(kSpecFirstStage[slider]);
    const TerrainSnapshot after = live.Run(seed, changed, dims);

    // (i) only the dependent stages re-ran.
    EXPECT_EQ(RanStages(live.Gen()), expected);

    // (ii) layers of stages that did not re-run are untouched.
    const TerrainSnapshot freshAfter = fresh.RunFresh(seed, changed, dims);
    EXPECT_TRUE(DiffSnapshots(after, freshAfter).empty()) << "live vs fresh: " << Join(DiffSnapshots(after, freshAfter));
    for (int l = 0; l < TerrainLayers::GetLayerCount(); ++l)
    {
        const int stage = static_cast<int>(TerrainLayers::GetLayerInfo(l).stage);
        // The spec lets stage 3 merge chain vents into the stage-1 `volcanic` layer, so that layer has two writers.
        const bool sharedWithBarrier = std::strcmp(TerrainLayers::GetLayerInfo(l).name, "volcanic") == 0;
        if (expected.count(stage) == 0 && !(sharedWithBarrier && expected.count(3) != 0))
        {
            EXPECT_EQ(baseSnap.layerHash[l], after.layerHash[l]) << "cached layer changed: " << TerrainLayers::GetLayerInfo(l).name;
            // Undeclared-dependency check: a fresh regeneration of the stage must also be unaffected.
            EXPECT_EQ(baseSnap.layerHash[l], freshAfter.layerHash[l]) << "undeclared dependency: "
                << TerrainLayers::GetLayerInfo(l).name << " changed with " << name;
        }
    }

    // Reverting returns exactly to the base world.
    const TerrainSnapshot back = live.Run(seed, base, dims);
    EXPECT_EQ(RanStages(live.Gen()), expected);
    EXPECT_TRUE(DiffSnapshots(back, baseSnap).empty()) << "revert: " << Join(DiffSnapshots(back, baseSnap));
}

// ---- Goldens ----------------------------------------------------------------------------------
constexpr int kGoldenLayers = 27;
struct GoldenSet
{
    uint64_t seed;
    uint64_t cells;
    uint64_t layers[kGoldenLayers];
};

const GoldenSet kGolden[3] =
{
    { 1ull, 0x9806E9D075248467ull,
      {
          0x6C3C0775F70AC75Aull, 0x1CA0E078B1D52247ull, 0x9BEF190A3CA90285ull,
          0x54FB4C940896217Dull, 0x0F333F93568EC3E4ull, 0x0F120157F984B052ull,
          0x81A04DC9D8B093C4ull, 0x8DD1A666FDDC0661ull, 0xAACD4855136CDD48ull,
          0xFB3E98B48B858705ull, 0xB6F70F17F9E55D63ull, 0x64B18619AE162B4Eull,
          0x535EFB6B7393D2B5ull, 0x2470033C4D1B8643ull, 0xD7354DCB95C88A09ull,
          0x92DCA45D8533AAF4ull, 0x761347D96FB96E05ull, 0x20DB446B6D9584CBull,
          0x0394CD91C7DEEB58ull, 0xC098EDF9CA7B3BC6ull, 0x5DF1C2B01CA4275Cull,
          0xCE2DA522C444F6D4ull, 0x0672092CE311063Aull, 0x4253CD4D144F56F2ull,
          0xB214E33B643B7BB7ull, 0x23403465926278EAull, 0xBA892187C0868CAAull,
      } },
    { 12345ull, 0x3210F0B08F53D852ull,
      {
          0xB442BD9FCEF82277ull, 0x860B27D63D97D08Dull, 0x55CA2BEAC38282A5ull,
          0x4A0FFEF3F08EFD6Bull, 0x19AE0C668FD61864ull, 0xF3E9F16755F60E00ull,
          0x43F28BA6D0579DB5ull, 0x6A947C39728988D1ull, 0x82315C0F34C50C28ull,
          0x2894F170149EBC26ull, 0x861D48E5E338759Cull, 0x9D09675310054580ull,
          0x430E6B757A514770ull, 0x6C15A9F613395B56ull, 0x01B03954DD76EF0Dull,
          0xD6D4BEECE8127A35ull, 0x430E6B757A514770ull, 0xAE40B624C280A758ull,
          0x0BD4E5F148614C19ull, 0x78E29063104EFFE6ull, 0x2D82A9A246C08012ull,
          0xA45CFA9F1C092415ull, 0x3035D3CA0E040FCEull, 0x28B4C57F372FE92Full,
          0x38433A56F4843656ull, 0xC0FD5C15535C3D15ull, 0xBA892187C0868CAAull,
      } },
    { 0xDEADBEEFCAFEBABEull, 0x421D3BA5B494551Cull,
      {
          0xA09A3A9850077403ull, 0x97F22A2F7F2FEFD2ull, 0x7D604400DA8280F6ull,
          0x1C457649FAFD0B94ull, 0xA1859F07EB0DBA74ull, 0xD0FC37E7325E8985ull,
          0xD659F85A984658A5ull, 0xB3A7F971203060DFull, 0xAD770AC0CBCBA128ull,
          0x550F5E9F4BD02BC9ull, 0x8CFEBCE645F6B9F1ull, 0x5DA3E41742CBC199ull,
          0x44E248C094D29F67ull, 0x9F1A5615C05A9527ull, 0xDCFCB8518760DFEFull,
          0x9371573E91CBDD75ull, 0x44E248C094D29F67ull, 0xF128615262110EA3ull,
          0x2E90E293D340A237ull, 0xA5139C23FA93D775ull, 0xABA6CACC645E1DEDull,
          0x24AA06D60C9785DCull, 0x4F19B2DD1436D4C3ull, 0x3337198BF5A69D33ull,
          0xE1E5DC6D81951B47ull, 0x8F7ABC78ACF60D77ull, 0xBA892187C0868CAAull,
      } },
};

} // namespace

TEST(TerrainDeterminism, DriverScriptCoversEverySliderAndIsStable)
{
    const std::vector<SliderChange> a = BuildSliderScript(7, 200);
    const std::vector<SliderChange> b = BuildSliderScript(7, 200);
    ASSERT_EQ(a.size(), 200u);
    std::set<int> seen;
    for (size_t i = 0; i < a.size(); ++i)
    {
        EXPECT_EQ(a[i].slider, b[i].slider);
        EXPECT_EQ(a[i].value, b[i].value);
        seen.insert(a[i].slider);
    }
    EXPECT_EQ(static_cast<int>(seen.size()), kTerrainSliderCount);
    EXPECT_EQ(kTerrainSliderCount, 18);
    // The first block of 18 is a permutation, and the order differs from the identity.
    std::set<int> first;
    for (int i = 0; i < kTerrainSliderCount; ++i) { first.insert(a[i].slider); }
    EXPECT_EQ(static_cast<int>(first.size()), kTerrainSliderCount);
}

TEST(TerrainDeterminism, SpecTablesMatchGenerator)
{
    ASSERT_EQ(kImplementedStageCount, 12);
    for (int s = 0; s < 12; ++s)
    {
        uint32_t mask = 0;
        for (int r : kSpecReads[s]) { mask |= 1u << r; }
        EXPECT_EQ(kStageReads[s], mask) << "stage " << s;
    }
    for (int p = 0; p < kTerrainSliderCount; ++p)
    {
        EXPECT_EQ(static_cast<int>(FirstStageAffected(SliderParam(p))), kSpecFirstStage[p]) << ShapeParamName(SliderParam(p));
    }
}

TEST(TerrainDeterminism, TwoFreshRunsAreByteIdentical)
{
    for (uint64_t seed : { 1ull, 99ull })
    {
        TerrainRunner a, b;
        const TerrainSnapshot sa = a.Run(seed, WorldShapeParams{}, kSmall);
        const TerrainSnapshot sb = b.Run(seed, WorldShapeParams{}, kSmall);
        EXPECT_TRUE(DiffSnapshots(sa, sb).empty()) << Join(DiffSnapshots(sa, sb));
        EXPECT_EQ(a.World().CellCount(), kSmall.width * kSmall.height);
        EXPECT_EQ(0, std::memcmp(a.World().Cells(), b.World().Cells(), a.World().CellCount() * sizeof(Cell)));
        // Invalidate + Update on the same generator reproduces the same bytes.
        const TerrainSnapshot sc = a.RunFresh(seed, WorldShapeParams{}, kSmall);
        EXPECT_TRUE(DiffSnapshots(sa, sc).empty()) << Join(DiffSnapshots(sa, sc));
    }
}

TEST(TerrainDeterminism, DifferentSeedsGiveDifferentWorlds)
{
    TerrainRunner a, b;
    const TerrainSnapshot sa = a.Run(1, WorldShapeParams{}, kSmall);
    const TerrainSnapshot sb = b.Run(2, WorldShapeParams{}, kSmall);
    EXPECT_NE(sa.cellHash, sb.cellHash);
}

TEST(TerrainDeterminism, LiveSliderSequenceMatchesFreshRegeneration)
{
    for (uint64_t seed : { 3ull, 4ull })
    {
        CheckLiveMatchesFresh(seed, 5, kIso, 0x51DE);
    }
}

TEST(TerrainDeterminism, EverySliderIsolation)
{
    TerrainRunner live, fresh;
    const WorldShapeParams base;
    const TerrainSnapshot baseSnap = live.Run(21, base, kIso);
    ASSERT_TRUE(DiffSnapshots(baseSnap, fresh.RunFresh(21, base, kIso)).empty());
    for (int slider = 0; slider < kTerrainSliderCount; ++slider)
    {
        const int v = ValueDifferingFrom(base, slider);
        ASSERT_GE(v, 0) << ShapeParamName(SliderParam(slider));
        CheckSliderIsolation(live, fresh, 21, base, slider, v, kIso, baseSnap);
    }
}

TEST(TerrainDeterminism, LatitudeChangeOnlyChangesLatitudeLayerAndCells)
{
    TerrainRunner live;
    WorldShapeParams p;
    const TerrainSnapshot a = live.Run(5, p, kIso);
    p.latitudeNorthDeg = 75.0f;
    const TerrainSnapshot b = live.Run(5, p, kIso);
    EXPECT_EQ(RanStages(live.Gen()), (std::set<int>{ 11 }));
    const std::vector<std::string> diff = DiffSnapshots(a, b);
    for (const std::string& n : diff)
    {
        EXPECT_TRUE(n == "latitude" || n == "<cells>") << n;
    }
    EXPECT_FALSE(diff.empty());
}

TEST(TerrainDeterminism, RepeatedIdenticalUpdateRunsNothingAndChangesNothing)
{
    TerrainRunner r;
    const TerrainSnapshot a = r.Run(8, WorldShapeParams{}, kIso);
    const TerrainSnapshot b = r.Run(8, WorldShapeParams{}, kIso);
    EXPECT_TRUE(RanStages(r.Gen()).empty());
    EXPECT_TRUE(DiffSnapshots(a, b).empty());
}

TEST(TerrainDeterminism, DimensionChangeDirtiesEverythingAndMatchesFresh)
{
    TerrainRunner live, fresh;
    live.Run(9, WorldShapeParams{}, kIso);
    const GridDimensions other{ 44, 28 };
    const TerrainSnapshot l = live.Run(9, WorldShapeParams{}, other);
    std::set<int> all;
    for (int s = 0; s < kImplementedStageCount; ++s) { all.insert(s); }
    EXPECT_EQ(RanStages(live.Gen()), all);
    const TerrainSnapshot f = fresh.RunFresh(9, WorldShapeParams{}, other);
    EXPECT_TRUE(DiffSnapshots(l, f).empty()) << Join(DiffSnapshots(l, f));
    // Height-only and width-only changes also dirty everything.
    live.Run(9, WorldShapeParams{}, GridDimensions{ 40, 32 });
    EXPECT_EQ(RanStages(live.Gen()), all);
    live.Run(9, WorldShapeParams{}, GridDimensions{ 44, 32 });
    EXPECT_EQ(RanStages(live.Gen()), all);
}

TEST(TerrainDeterminism, SeedChangeDirtiesEverythingAndMatchesFresh)
{
    TerrainRunner live, fresh;
    live.Run(9, WorldShapeParams{}, kIso);
    const TerrainSnapshot l = live.Run(10, WorldShapeParams{}, kIso);
    std::set<int> all;
    for (int s = 0; s < kImplementedStageCount; ++s) { all.insert(s); }
    EXPECT_EQ(RanStages(live.Gen()), all);
    EXPECT_TRUE(DiffSnapshots(l, fresh.RunFresh(10, WorldShapeParams{}, kIso)).empty());
}

TEST(TerrainDeterminism, SmallSizeLiveMatchesFresh)
{
    TerrainRunner live, fresh;
    const GridDimensions dims{ 40, 30 };
    WorldShapeParams p;
    live.Run(77, p, dims);
    p.rivers = 1.0f;
    EXPECT_TRUE(DiffSnapshots(live.Run(77, p, dims), fresh.RunFresh(77, p, dims)).empty());
    p.mountainousness = 0.9f;
    EXPECT_TRUE(DiffSnapshots(live.Run(77, p, dims), fresh.RunFresh(77, p, dims)).empty());
}

TEST(TerrainDeterminism, GoldenHashesAtSmall)
{
    for (const GoldenSet& g : kGolden)
    {
        SCOPED_TRACE(g.seed);
        TerrainRunner r;
        const TerrainSnapshot s = r.Run(g.seed, WorldShapeParams{}, kSmall);
        ASSERT_EQ(TerrainLayers::GetLayerCount(), kGoldenLayers);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "0x%016llXull", static_cast<unsigned long long>(s.cellHash));
        EXPECT_EQ(g.cells, s.cellHash) << "cells actual " << buf;
        for (int l = 0; l < kGoldenLayers; ++l)
        {
            std::snprintf(buf, sizeof(buf), "0x%016llXull", static_cast<unsigned long long>(s.layerHash[l]));
            EXPECT_EQ(g.layers[l], s.layerHash[l]) << "layer " << l << " " << TerrainLayers::GetLayerInfo(l).name << " actual " << buf;
        }
    }
}

// ---- SLOW tier --------------------------------------------------------------------------------
TEST(SLOW_TerrainDeterminism, LiveSliderSequence200ChangesOn5Seeds)
{
    for (uint64_t seed : { 1ull, 12345ull, 777ull, 0xDEADBEEFCAFEBABEull, 424242ull })
    {
        CheckLiveMatchesFresh(seed, 200, kIso, 0xA11CE);
    }
}

TEST(SLOW_TerrainDeterminism, EverySliderIsolationAcrossSeedsAndValuePairs)
{
    for (uint64_t seed : { 21ull, 22ull, 23ull })
    {
        TerrainRunner live, fresh;
        const WorldShapeParams base;
        const TerrainSnapshot baseSnap = live.Run(seed, base, kIso);
        for (int slider = 0; slider < kTerrainSliderCount; ++slider)
        {
            for (int v = 0; v < SliderValueCount(slider); ++v)
            {
                WorldShapeParams probe = base;
                SetSliderValue(probe, slider, v);
                if (probe == base) { continue; }
                CheckSliderIsolation(live, fresh, seed, base, slider, v, kIso, baseSnap);
                if (::testing::Test::HasFatalFailure()) { return; }
            }
        }
        // Same checks from a non-default base (all sliders moved), one pair per slider.
        WorldShapeParams moved;
        for (int slider = 0; slider < kTerrainSliderCount; ++slider) { SetSliderValue(moved, slider, 1 + slider); }
        const TerrainSnapshot movedSnap = live.Run(seed, moved, kIso);
        for (int slider = 0; slider < kTerrainSliderCount; ++slider)
        {
            const int v = ValueDifferingFrom(moved, slider, 2);
            if (v < 0) { continue; }
            CheckSliderIsolation(live, fresh, seed, moved, slider, v, kIso, movedSnap);
        }
    }
}

TEST(SLOW_TerrainDeterminism, LargeSizesLiveMatchesFresh)
{
    for (GridDimensions dims : { GridDimensions{ 104, 64 }, GridDimensions{ 128, 128 } })
    {
        TerrainRunner live, fresh;
        WorldShapeParams p;
        live.Run(31, p, dims);
        p.lakes = 9;
        EXPECT_TRUE(DiffSnapshots(live.Run(31, p, dims), fresh.RunFresh(31, p, dims)).empty()) << dims.width;
        p.ruggedness = 0.9f;
        EXPECT_TRUE(DiffSnapshots(live.Run(31, p, dims), fresh.RunFresh(31, p, dims)).empty()) << dims.width;
        p.latitudeSouthDeg = -30.0f;
        EXPECT_TRUE(DiffSnapshots(live.Run(31, p, dims), fresh.RunFresh(31, p, dims)).empty()) << dims.width;
    }
}
