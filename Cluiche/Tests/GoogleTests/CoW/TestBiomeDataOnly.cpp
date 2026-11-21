// S-1 Biome Registry: biome-only data tests without registry coupling.
// Task 29 (AC 9): appending one climate row to the registry changes classification with no code change.
#include <gtest/gtest.h>
#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/Testing/BiomeTestGrids.h>

#include <vector>

using namespace CoW::World;
using namespace CoW::World::Testing;
using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;

namespace
{
    // Independent brute-force nearest-envelope scan: the climate row (in row order, ties to the lowest row) with the
    // smallest normalised squared distance to (t, m). Written from the envelope fields, not via the classifier.
    BiomeIndex NearestEnvelope(const BiomeRegistry& reg, float t, float m)
    {
        BiomeIndex best = kNoBiome;
        float      bestD = 0.0f;
        for (uint32_t b = 0; b < reg.Count(); ++b)
        {
            const ClimateEnvelope* e = reg.Climate(static_cast<BiomeIndex>(b));
            if (e == nullptr)
            {
                continue;
            }
            const float dt = (t - e->t0) / e->wt;
            const float dm = (m - e->m0) / e->wm;
            const float d  = dt * dt + dm * dm;
            if (best == kNoBiome || d < bestD)
            {
                best = static_cast<BiomeIndex>(b);
                bestD = d;
            }
        }
        return best;
    }
}

TEST(BiomeDataOnly, ExtraClimateRowTakesExactlyItsBestMatchCells)
{
    const BiomeRegistry& shipped = ShippedBiomeRegistry();
    const BiomeRegistry& extra   = ExtraBiomeRegistry();
    const BiomeIndex     savanna = B(extra, "Biome.Savanna");
    ASSERT_EQ(extra.Count(), shipped.Count() + 1u);
    ASSERT_EQ(static_cast<uint32_t>(savanna), shipped.Count());   // appended last

    // Land cells on a lattice three apart (never hex-adjacent, so no forbidden pair can form) sweeping the climate
    // square; every other cell is Grassland-perfect. All WildReaches land with no pins, so only the climate score
    // and nothing else decides the biome.
    constexpr int kSteps = 9;
    BiomeGrid     g(kSteps * 3 + 1, kSteps * 3 + 1);
    std::vector<int> sampled;
    for (int j = 0; j < kSteps; ++j)
    {
        for (int i = 0; i < kSteps; ++i)
        {
            const int c = g.Index(1 + i * 3, 1 + j * 3);
            g.SetClimate(c, 0.02f + 0.96f * static_cast<float>(i) / (kSteps - 1), 0.02f + 0.96f * static_cast<float>(j) / (kSteps - 1));
            sampled.push_back(c);
        }
    }
    const int n = g.dims.width * g.dims.height;

    g.registry = &extra;
    const BiomeResult withExtra = RunBiomes(g);
    g.registry = &shipped;
    const BiomeResult withShipped = RunBiomes(g);

    int savannaCells = 0;
    for (int c = 0; c < n; ++c)
    {
        const BiomeIndex wantExtra   = NearestEnvelope(extra, g.temperature[c], g.moisture[c]);
        const BiomeIndex wantShipped = NearestEnvelope(shipped, g.temperature[c], g.moisture[c]);
        EXPECT_EQ(withExtra.At(c), wantExtra) << "cell " << c;
        EXPECT_EQ(withShipped.At(c), wantShipped) << "cell " << c;
        EXPECT_EQ(withExtra.fixup[c], static_cast<uint32_t>(BiomeFixup::None)) << "cell " << c;
        EXPECT_LT(withShipped.biome[c], shipped.Count()) << "shipped run produced a 14th index at cell " << c;
        if (wantExtra == savanna)
        {
            ++savannaCells;
        }
        else
        {
            // Cells the extra row does not win are unchanged by adding it.
            EXPECT_EQ(withExtra.At(c), withShipped.At(c)) << "cell " << c;
        }
    }
    EXPECT_GT(savannaCells, 0);

    // Meaningful: with the extra row ignored (shipped registry) none of those cells is Savanna, and the cells the extra
    // row wins are exactly the ones that differ between the two runs.
    int differing = 0;
    for (int c = 0; c < n; ++c)
    {
        if (withExtra.biome[c] != withShipped.biome[c])
        {
            ++differing;
            EXPECT_EQ(withExtra.At(c), savanna) << "cell " << c;
        }
    }
    EXPECT_EQ(differing, savannaCells);
}
