// S-1 Biome Registry: Task 32 edge cases (fallback not row 0, no tags, zones [], 254 rows end to end).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md (ACs 2, 3, 4, 6)
#include <gtest/gtest.h>
#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/Testing/BiomeTestGrids.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <Shared/World/Testing/StratumTestIndices.h>

using namespace CoW::World;
using namespace CoW::World::Testing;
using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{
    std::string ShippedText()
    {
        std::ifstream     in(ShippedBiomesPath(), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Applies textual edits (first occurrence each) to the shipped file; returns false if a target is missing.
    bool Edit(std::string& text, const std::string& from, const std::string& to)
    {
        const size_t at = text.find(from);
        if (at == std::string::npos) { return false; }
        text.replace(at, from.size(), to);
        return true;
    }

    bool Load(BiomeRegistry& r, const std::string& text, RegistryError& err) { return r.LoadFromText(text.c_str(), ::CoW::World::Testing::ShippedStratumTable(), err); }

    BiomeIndex NearestEnvelope(const BiomeRegistry& reg, float t, float m)
    {
        BiomeIndex best = kNoBiome;
        float      bestD = 0.0f;
        for (uint32_t b = 0; b < reg.Count(); ++b)
        {
            const ClimateEnvelope* e = reg.Climate(static_cast<BiomeIndex>(b));
            if (e == nullptr) { continue; }
            const float dt = (t - e->t0) / e->wt;
            const float dm = (m - e->m0) / e->wm;
            const float d  = dt * dt + dm * dm;
            if (best == kNoBiome || d < bestD) { best = static_cast<BiomeIndex>(b); bestD = d; }
        }
        return best;
    }
}

TEST(BiomeRegistryEdge, FallbackRoleOnANonZeroRowIsTheResolveTargetAndClassificationStillWorks)
{
    std::string text = ShippedText();
    ASSERT_TRUE(Edit(text, "\"wm\": 0.2}, \"roles\": [\"fallback\"]}", "\"wm\": 0.2}}"));   // drop it from Grassland
    ASSERT_TRUE(Edit(text, "\"m0\": 0.6, \"wt\": 0.25, \"wm\": 0.2}}", "\"m0\": 0.6, \"wt\": 0.25, \"wm\": 0.2}, \"roles\": [\"fallback\"]}"));
    BiomeRegistry r;
    RegistryError err;
    ASSERT_TRUE(Load(r, text, err)) << err.field.AsCStr() << " " << err.reason.AsCStr();

    const BiomeIndex forest = r.Find(StringCRC("Biome.TemperateForest"));
    ASSERT_NE(forest, 0);
    EXPECT_EQ(r.Role(BiomeRole::Fallback), forest);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.DoesNotExist")), forest);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Grassland")), r.Find(StringCRC("Biome.Grassland")));

    // The fallback role never leaks into classification: a Grassland-perfect cell is still Grassland.
    BiomeGrid g(5, 5);
    g.registry = &r;
    const BiomeResult res = RunBiomes(g);
    EXPECT_EQ(res.At(g.Index(2, 2)), r.Find(StringCRC("Biome.Grassland")));
}

TEST(BiomeRegistryEdge, BiomeWithNoTagsLoadsWithAnEmptyMask)
{
    std::string text = ShippedText();
    ASSERT_TRUE(Edit(text, "\"tags\": [\"land\", \"open\", \"fuel\", \"succession\"]", "\"tags\": []"));   // Scrub
    BiomeRegistry r;
    RegistryError err;
    ASSERT_TRUE(Load(r, text, err)) << err.field.AsCStr() << " " << err.reason.AsCStr();
    const BiomeIndex scrub = r.Find(StringCRC("Biome.Scrub"));
    ASSERT_NE(scrub, kNoBiome);
    EXPECT_EQ(r.TagMask(scrub), 0ull);
    for (const char* tag : { "land", "water", "aquatic", "wet", "cold", "hot", "forest", "open", "fuel", "mountain", "volcanic", "succession" })
    {
        const TagId t = r.FindTag(StringCRC(tag));
        ASSERT_NE(t, kNoTag) << tag;
        EXPECT_FALSE(r.HasTag(scrub, t)) << tag;
    }
    // Neighbouring rows keep their own tags.
    EXPECT_NE(r.TagMask(r.Find(StringCRC("Biome.BurnedAshland"))), 0ull);
}

TEST(BiomeRegistryEdge, BiomeWithEmptyZonesIsNeverPlacedEvenWithAPerfectClimateEnvelope)
{
    // Give Scrub (zones: []) a climate envelope that nothing else covers; zone validity must still exclude it.
    std::string text = ShippedText();
    ASSERT_TRUE(Edit(text, "\"generated\": false, \"zones\": []},", "\"generated\": false, \"zones\": [], \"climate\": {\"t0\": 0.95, \"m0\": 0.5, \"wt\": 0.05, \"wm\": 0.05}},"));
    BiomeRegistry r;
    RegistryError err;
    ASSERT_TRUE(Load(r, text, err)) << err.field.AsCStr() << " " << err.reason.AsCStr();
    const BiomeIndex scrub = r.Find(StringCRC("Biome.Scrub"));
    ASSERT_NE(r.Climate(scrub), nullptr);

    const StratumIndex zones[] = { kStratumFirst, kStratumWild, kStratumAncient };
    for (StratumIndex zone : zones)
    {
        ASSERT_FALSE(r.IsValidInStratum(scrub, zone));
        BiomeGrid g(31, 31);
        g.registry = &r;
        for (int c = 0; c < g.dims.width * g.dims.height; ++c) { g.SetZone(c, zone); }
        // Sample climates (lattice three apart, never adjacent) including the exact Scrub envelope centre.
        for (int j = 0; j < 10; ++j)
        {
            for (int i = 0; i < 10; ++i)
            {
                g.SetClimate(g.Index(1 + i * 3, 1 + j * 3), 0.5f + 0.05f * static_cast<float>(i), 0.45f + 0.01f * static_cast<float>(j));
            }
        }
        const BiomeResult res = RunBiomes(g);
        for (int c = 0; c < g.dims.width * g.dims.height; ++c)
        {
            EXPECT_NE(res.At(c), scrub) << "cell " << c << " zone " << static_cast<int>(zone);
        }
    }
}

namespace
{
    // 254 rows: 0 ocean, 1 lake, 2 fallback; rows 2..253 carry distinct climate envelopes valid in every land zone.
    std::string Big254()
    {
        std::string s = "{\"format\":\"cow.biomes\",\"version\":1,\"biomes\":[";
        for (int i = 0; i < 254; ++i)
        {
            if (i > 0) { s += ","; }
            s += "{\"id\":\"Biome.N" + std::to_string(i) + "\",\"name\":\"N" + std::to_string(i) + "\",\"colour\":\"#000001\",\"description\":\"d\",";
            if (i < 2)
            {
                s += "\"tags\":[\"water\"],\"generated\":true,\"zones\":[\"None\"],\"roles\":[\"" + std::string(i == 0 ? "ocean" : "lake") + "\"]";
            }
            else
            {
                const float t0 = static_cast<float>(i - 2) / 251.0f;
                const float m0 = static_cast<float>(((i - 2) * 7) % 252) / 251.0f;
                s += "\"tags\":[\"land\"],\"generated\":true,\"zones\":[\"FirstShores\",\"WildReaches\",\"AncientInteriors\"],"
                     "\"climate\":{\"t0\":" + std::to_string(t0) + ",\"m0\":" + std::to_string(m0) + ",\"wt\":0.1,\"wm\":0.1}";
                if (i == 2) { s += ",\"roles\":[\"fallback\"]"; }
            }
            s += "}";
        }
        s += "],\"forbidden\":[]}";
        return s;
    }
}

TEST(BiomeRegistryEdge, TwoHundredFiftyFourRowRegistryClassifiesEveryCellByItsNearestEnvelope)
{
    BiomeRegistry r;
    RegistryError err;
    ASSERT_TRUE(Load(r, Big254(), err)) << "row " << err.row << " " << err.field.AsCStr() << " " << err.reason.AsCStr();
    ASSERT_EQ(r.Count(), 254u);
    uint32_t climateCount = 0;
    r.ClimateBiomes(climateCount);
    EXPECT_EQ(climateCount, 252u);

    BiomeGrid g(49, 49);
    g.registry = &r;
    for (int j = 0; j < 16; ++j)
    {
        for (int i = 0; i < 16; ++i)
        {
            g.SetClimate(g.Index(1 + i * 3, 1 + j * 3), 0.01f + 0.065f * static_cast<float>(i), 0.01f + 0.065f * static_cast<float>(j));
        }
    }
    const BiomeResult res = RunBiomes(g);
    bool sawHighRow = false;
    for (int c = 0; c < g.dims.width * g.dims.height; ++c)
    {
        EXPECT_EQ(res.At(c), NearestEnvelope(r, g.temperature[c], g.moisture[c])) << "cell " << c;
        EXPECT_LT(res.At(c), 254) << "cell " << c;
        sawHighRow = sawHighRow || res.At(c) > 200;
    }
    EXPECT_TRUE(sawHighRow) << "rows above 200 should win some cells";
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Missing")), 2);
}
