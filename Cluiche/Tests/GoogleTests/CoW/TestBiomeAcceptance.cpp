// S-1 Biome Classification task 7: acceptance tests.
// AC10 (diversity, tuning-gated), AC11 (performance, Release only), AC12 (A4 contract).
// AC1's 200-change sequence is SLOW_BiomeGenerator.LiveSliderSequenceEqualsFreshGenerationLong (TestBiomeGenerator.cpp).
// AC14 (imported worlds) is deferred until export/import exists (system plan task 16).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/BiomeTypes.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    constexpr int kTemperature = static_cast<int>(TerrainStage::Temperature);
    constexpr int kMoisture    = static_cast<int>(TerrainStage::Moisture);
    constexpr int kBiomes      = static_cast<int>(TerrainStage::Biomes);

    struct Gen
    {
        TerrainGenerator          gen;
        Dia::Maths::RandomStream  rng{ 1 };
        Gen() : gen(TerrainRules::Defaults()) {}
    };

    double Median(std::vector<double> v)
    {
        std::sort(v.begin(), v.end());
        return v.empty() ? 0.0 : v[v.size() / 2];
    }

    // ---- AC10 --------------------------------------------------------------------------------------
    void CheckDiversity(uint64_t firstSeed, int count)
    {
        int seedsWithFive = 0;
        for (int i = 0; i < count; ++i)
        {
            const uint64_t seed = firstSeed + static_cast<uint64_t>(i);
            Gen            g;
            g.gen.Update(seed, WorldShapeParams{}, kSmall, g.rng);
            const TerrainLayers& L     = g.gen.GetLayers();
            const uint32_t*      biome = L.GetUInt(L.FindLayer("biomeIndex"));
            ASSERT_NE(biome, nullptr);

            int counts[kBiomeCount] = {};
            int land                = 0;
            for (int c = 0; c < kSmall.width * kSmall.height; ++c)
            {
                const uint32_t b = biome[c];
                ASSERT_LT(b, static_cast<uint32_t>(kBiomeCount));
                if (b == static_cast<uint32_t>(Biome::Ocean) || b == static_cast<uint32_t>(Biome::Lake)) { continue; }
                ++counts[b];
                ++land;
            }
            int    distinct = 0;
            int    maxCount = 0;
            Biome  maxBiome = Biome::Ocean;
            std::string line;
            for (int b = 0; b < kBiomeCount; ++b)
            {
                if (b == static_cast<int>(Biome::Ocean) || b == static_cast<int>(Biome::Lake)) { continue; }
                if (counts[b] > 0) { ++distinct; }
                if (counts[b] > maxCount) { maxCount = counts[b]; maxBiome = static_cast<Biome>(b); }
                char buf[64];
                std::snprintf(buf, sizeof(buf), " %s=%d", BiomeName(static_cast<Biome>(b)), counts[b]);
                line += buf;
            }
            const double maxShare = land > 0 ? static_cast<double>(maxCount) / land : 0.0;
            std::printf("[BiomeAcceptance] AC10 seed %llu: land=%d distinct=%d max=%s %.1f%% |%s\n",
                        static_cast<unsigned long long>(seed), land, distinct, BiomeName(maxBiome), maxShare * 100.0, line.c_str());
            if (distinct >= 5) { ++seedsWithFive; }
            EXPECT_LE(maxShare, 0.60) << "seed " << seed << " dominant " << BiomeName(maxBiome);
        }
        const double frac = static_cast<double>(seedsWithFive) / count;
        std::printf("[BiomeAcceptance] AC10 %d/%d seeds (%.0f%%) have >= 5 distinct land biomes\n", seedsWithFive, count, frac * 100.0);
        EXPECT_GE(frac, 0.90);
    }
}

TEST(BiomeAcceptance, DiversityAtDefaultsCheapSubset)       { CheckDiversity(1, 5); }
TEST(SLOW_BiomeAcceptance, DiversityAtDefaultsTwentySeeds)  { CheckDiversity(1, 20); }

// ---- AC11 (Release only) --------------------------------------------------------------------------
TEST(BiomeAcceptancePerf, StagesTwelveToFourteenAndClimateSliders)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    {
        Gen                 g;
        const WorldShapeParams s;
        std::vector<double> total;
        for (int i = 0; i < kRuns; ++i)
        {
            g.gen.Invalidate();
            g.gen.Update(7000 + static_cast<uint64_t>(i), s, kSmall, g.rng);
            const StageTiming* t = g.gen.GetTimings();
            total.push_back(static_cast<double>(t[kTemperature].milliseconds) + t[kMoisture].milliseconds + t[kBiomes].milliseconds);
        }
        const double med = Median(total);
        std::printf("[BiomeAcceptancePerf] stages 12-14 full Invalidate+Update median %.3f ms (66x42, %d runs)\n", med, kRuns);
        EXPECT_LT(med, 5.0);
    }

    struct Slider { const char* name; void (*set)(WorldShapeParams&, bool); };
    const Slider sliders[] =
    {
        { "temperatureOffset", [](WorldShapeParams& p, bool hi) { p.temperatureOffset = hi ? 0.2f : -0.2f; } },
        { "moistureOffset",    [](WorldShapeParams& p, bool hi) { p.moistureOffset = hi ? 0.2f : -0.2f; } },
    };
    for (const Slider& s : sliders)
    {
        Gen              g;
        WorldShapeParams p;
        g.gen.Update(11, p, kSmall, g.rng);
        std::vector<double> ms;
        for (int i = 0; i < kRuns; ++i)
        {
            s.set(p, (i & 1) == 0);
            const auto t0 = std::chrono::steady_clock::now();
            g.gen.Update(11, p, kSmall, g.rng);
            const auto t1 = std::chrono::steady_clock::now();
            ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        const double med = Median(ms);
        std::printf("[BiomeAcceptancePerf] %s change median %.3f ms (66x42, %d runs)\n", s.name, med, kRuns);
        EXPECT_LT(med, 3.0) << s.name;
    }
#else
    GTEST_SKIP() << "Release-only performance gate (AC11)";
#endif
}

// ---- AC12 -----------------------------------------------------------------------------------------
// A4 Feature 1 (TableBiomeSeasonalBaseline + Assets/CoW/Global/Weather/weather_parameters.json) is not built.
// When it exists this test must load the default baseline_table and fail if any of these ten ids has no entry:
//   Biome.Ocean, Biome.Lake, Biome.Grassland, Biome.TemperateForest, Biome.Wetland, Biome.Desert,
//   Biome.Alpine, Biome.Tundra, Biome.Rainforest, Biome.Volcanic
// (the spec says the default table is "the same for every biome"; unknown ids fall back to 0.5 per EC-19).
TEST(BiomeAcceptance, DISABLED_A4BaselineTableCoversAllTenBiomeIds)
{
    // A4 Feature 1 not built yet.
    const char* const kIds[] = { "Biome.Ocean", "Biome.Lake", "Biome.Grassland", "Biome.TemperateForest", "Biome.Wetland",
                                 "Biome.Desert", "Biome.Alpine", "Biome.Tundra", "Biome.Rainforest", "Biome.Volcanic" };
    std::set<uint32_t> ours;
    for (int b = 0; b < kBiomeCount; ++b) { ours.insert(static_cast<uint32_t>(BiomeId(static_cast<Biome>(b)).Value())); }
    for (const char* id : kIds)
    {
        EXPECT_TRUE(ours.count(static_cast<uint32_t>(Dia::Core::StringCRC(id).Value()))) << id;
    }
    FAIL() << "A4 Feature 1 not built yet: load baseline_table and check each id has an entry";
}
