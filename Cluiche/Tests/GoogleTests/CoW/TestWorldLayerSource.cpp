// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <DiaObservation/Log/DiaLog.h>
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/WorldLayerSource.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;

namespace
{
Json::Value Obj(const char* key, const Json::Value& v)
{
    Json::Value o(Json::objectValue);
    o[key] = v;
    return o;
}

void Generate(WorldGenSession& s, uint64_t seed)
{
    Dia::Maths::RandomStream rng(seed);
    s.Regenerate(Obj("seed", Json::Value(static_cast<Json::UInt64>(seed))));
    ASSERT_TRUE(s.Tick(rng));
}

std::vector<float> Read(const WorldLayerDesc& d, const WorldGenSession& s, uint32_t n)
{
    std::vector<float> out(n, -123.0f);
    d.read(s, out.data(), n);
    return out;
}

// Expected value of layer 'id' at cell i, computed independently of the readers.
float Expected(const WorldGenSession& s, const std::string& id, uint32_t i)
{
    const TerrainLayers& L = s.Generator().GetLayers();
    const Cell& c = s.World().Cells()[i];
    if (id == "relief") return L.GetFloat(L.FindLayer("elevation"))[i];
    if (id == "terrainType") return static_cast<float>(c.terrain);
    if (id == "waterSource") return static_cast<float>(c.waterSource);
    if (id == "stratum")   return static_cast<float>(c.stratum);
    if (id == "landmass")    return static_cast<float>(c.landmass);
    if (id == "regions")     return c.region == 0 ? 0.0f : static_cast<float>(1 + (c.region - 1) % 63);
    const int idx = L.FindLayer(id.c_str());
    if (const float* f = L.GetFloat(idx)) return f[i];
    if (const uint32_t* u = L.GetUInt(idx)) return static_cast<float>(u[i]);
    return -9999.0f;
}
}

TEST(WorldLayerSource, DescriptorIdsAreUniqueAndFindable)
{
    std::set<std::string> ids;
    std::set<std::string> keys;
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(i);
        EXPECT_TRUE(ids.insert(d.id).second) << d.id;
        EXPECT_TRUE(keys.insert(d.schemaKey).second) << d.schemaKey;
        EXPECT_EQ(WorldLayerSource::Find(d.id), &d);
        EXPECT_EQ(std::string(d.schemaKey), std::string(kWorldLayerSchemaFor) + "/" + d.id);
        EXPECT_NE(d.read, nullptr);
        if (d.kind == WorldLayerKind::Category) EXPECT_GE(WorldLayerSource::CategoryCount(d, Draoi::HexWorld::Testing::ShippedBiomeRegistry()), 2u) << d.id;
        else EXPECT_EQ(d.categoryCount, 0u) << d.id;
    }
    EXPECT_EQ(WorldLayerSource::Find("nope"), nullptr);
    EXPECT_EQ(WorldLayerSource::Find(nullptr), nullptr);
}

TEST(WorldLayerSource, ExactlyOneDefaultBaseIsRelief)
{
    int n = 0;
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
        if (WorldLayerSource::At(i).defaultBase) { ++n; EXPECT_STREQ(WorldLayerSource::At(i).id, "relief"); }
    EXPECT_EQ(n, 1);
}

TEST(WorldLayerSource, EveryReaderMatchesGeneratedData)
{
    TestSession s;
    Generate(s, 12345);
    const uint32_t n = static_cast<uint32_t>(s.World().CellCount());
    ASSERT_EQ(n, 66u * 42u);
    for (uint32_t li = 0; li < WorldLayerSource::Count(); ++li)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(li);
        const std::vector<float> out = Read(d, s, n);
        for (uint32_t i = 0; i < n; ++i)
            ASSERT_EQ(out[i], Expected(s, d.id, i)) << d.id << " cell " << i;
    }
}

TEST(WorldLayerSource, CategoryValuesStayInsideCategoryCount)
{
    TestSession s;
    Generate(s, 777);
    const uint32_t n = static_cast<uint32_t>(s.World().CellCount());
    for (uint32_t li = 0; li < WorldLayerSource::Count(); ++li)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(li);
        if (d.kind != WorldLayerKind::Category) continue;
        for (float v : Read(d, s, n))
            ASSERT_TRUE(v >= 0.0f && v < static_cast<float>(WorldLayerSource::CategoryCount(d, *s.BiomeRegistryPtr()))) << d.id << " value " << v;
    }
}

TEST(WorldLayerSource, VersionBumpsOnlyWhenTickChangesWorld)
{
    TestSession s;
    Dia::Maths::RandomStream rng(1);
    EXPECT_EQ(WorldLayerSource::Version(s), 0u);
    ASSERT_TRUE(s.Tick(rng));
    const uint32_t v1 = WorldLayerSource::Version(s);
    EXPECT_EQ(v1, 1u);
    EXPECT_FALSE(s.Tick(rng));
    EXPECT_EQ(WorldLayerSource::Version(s), v1);
    s.Set(Obj("landFraction", Json::Value(0.5)));
    EXPECT_EQ(WorldLayerSource::Version(s), v1);   // pending action alone does not bump
    ASSERT_TRUE(s.Tick(rng));
    EXPECT_EQ(WorldLayerSource::Version(s), v1 + 1u);
    EXPECT_FALSE(s.Tick(rng));
    EXPECT_EQ(WorldLayerSource::Version(s), v1 + 1u);
}

TEST(WorldLayerSource, ZeroFillBeforeFirstGeneration)
{
    TestSession s;
    ASSERT_EQ(s.Revision(), 0u);
    for (uint32_t li = 0; li < WorldLayerSource::Count(); ++li)
    {
        const WorldLayerDesc& d = WorldLayerSource::At(li);
        for (float v : Read(d, s, 66u * 42u))
            ASSERT_EQ(v, 0.0f) << d.id;
    }
}

TEST(WorldLayerSource, BufferLargerThanWorldIsZeroPadded)
{
    TestSession s;
    Generate(s, 5);
    const uint32_t n = static_cast<uint32_t>(s.World().CellCount());
    const WorldLayerDesc* d = WorldLayerSource::Find("elevation");
    ASSERT_NE(d, nullptr);
    const std::vector<float> out = Read(*d, s, n + 10);
    for (uint32_t i = n; i < n + 10; ++i)
        EXPECT_EQ(out[i], 0.0f);
}

TEST(WorldLayerSource, LargeReadAllLayersTiming)
{
    TestSession s;
    s.SetSize(Obj("size", Json::Value("Large")));
    Generate(s, 99);
    const uint32_t n = static_cast<uint32_t>(s.World().CellCount());
    ASSERT_EQ(n, 104u * 64u);
    std::vector<float> buf(n);
    const auto t0 = std::chrono::steady_clock::now();
    for (uint32_t li = 0; li < WorldLayerSource::Count(); ++li)
        WorldLayerSource::At(li).read(s, buf.data(), n);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    DIA_LOG_INFO("Test", "[ INFO ] WorldLayerSource Large read of all %zu layers: %.2f ms", static_cast<size_t>(WorldLayerSource::Count()), static_cast<double>(ms));
    SUCCEED();
}

// ---- Regions plan task 10: region colour layer ----

TEST(WorldLayerSource, RegionLayerIsCategoricalWithA64SlotPalette)
{
    const WorldLayerDesc* d = WorldLayerSource::Find("regions");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->kind, WorldLayerKind::Category);
    EXPECT_EQ(d->categoryCount, kRegionPaletteSlots);
    EXPECT_EQ(kRegionPaletteSlots, 64u) << "CellField palettes hold 64 colours";
    EXPECT_STREQ(d->group, "Regions");
}

TEST(WorldLayerSource, RegionPaletteSlotKeepsZeroAndWrapsPastSlot63)
{
    EXPECT_EQ(RegionPaletteSlot(0), 0u);
    EXPECT_EQ(RegionPaletteSlot(1), 1u);
    EXPECT_EQ(RegionPaletteSlot(63), 63u);
    EXPECT_EQ(RegionPaletteSlot(64), 1u);
    for (uint32_t r = 1; r <= 64; ++r)
        EXPECT_LT(RegionPaletteSlot(static_cast<RegionId>(r)), kRegionPaletteSlots);
}

TEST(WorldLayerSource, RegionLayerReadsFreshRegionsAndIsZeroAfterLivePreview)
{
    TestSession s;
    Generate(s, 4321);
    ASSERT_TRUE(s.RegionsFresh());
    const WorldLayerDesc* d = WorldLayerSource::Find("regions");
    ASSERT_NE(d, nullptr);
    const uint32_t n = static_cast<uint32_t>(s.World().CellCount());
    std::set<float> seen;
    for (float v : Read(*d, s, n)) seen.insert(v);
    EXPECT_GE(seen.size(), 11u) << "region 0 plus at least 10 mainland regions";

    Dia::Maths::RandomStream rng(4321);
    s.Set(Obj("rivers", 0.31));         // live preview clears regions (SD-S1-004)
    ASSERT_TRUE(s.Tick(rng));
    ASSERT_FALSE(s.RegionsFresh());
    for (float v : Read(*d, s, n)) ASSERT_EQ(v, 0.0f);
}
