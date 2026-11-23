// Spec: docs/specs/features/draoi/worldsim-kit-draoi.md (AC 10a), plan task D4b.
// Field contributions: RegisterBiomeFields (core) and RegisterTerrainFields (DraoiHexWorldTerrain).
// The biome tests use a hand-built world over the fixture registry. The terrain tests need a generated Small seed-1 world:
// a Draoi-only world is not possible (generation needs the terrain_rules sidecar, step seed names and node kinds, which the
// application owns), so they use the CoW test session helper from the same GoogleTests project. The package under test
// (and its headers) names no CoW identifier; only this test fixture does.
#include <gtest/gtest.h>
#include <cstdio>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>
#include <DraoiHexWorld/BiomeFieldContribution.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorld/WorldCellFields.h>
#include <DraoiHexWorld/WorldStructure.h>
#include <DraoiHexWorldTerrain/TerrainFieldContribution.h>
#include <DraoiHexWorldTerrain/Testing/SessionDrive.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <cctype>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace Draoi::HexWorld;
using Dia::Core::StringCRC;

namespace
{
    // Row order: 0 Ocean, 1 Lake, 2 Grassland, 3 Scrub (same shape as the BiomeRegistry fixture).
    const char* const kBiomeFixture = R"J({
  "format":"cow.biomes","version":1,
  "biomes":[
    {"id":"Biome.Ocean","name":"Ocean","colour":"#1f4f8f","description":"Open sea","tags":["water","aquatic"],
     "generated":true,"zones":["None"],"roles":["ocean"],"pin":{"rule":"water_ocean","priority":100}},
    {"id":"Biome.Lake","name":"Lake","colour":"#3a7fd0","description":"Inland water","tags":["water","aquatic"],
     "generated":true,"zones":["None"],"roles":["lake"],"pin":{"rule":"water_lake","priority":200}},
    {"id":"Biome.Grassland","name":"Grassland","colour":"#9acd32","description":"Open plains","tags":["land","open","fuel"],
     "generated":true,"zones":["FirstShores","WildReaches"],"roles":["fallback"],
     "climate":{"t0":0.5,"m0":0.25,"wt":0.125,"wm":0.75},"pin":{"rule":"wetland","priority":150},"plateau":"dry"},
    {"id":"Biome.Scrub","name":"Scrub","colour":"#808000","description":"Succession scrub","tags":["land","succession"],
     "generated":false,"zones":[],
     "climate":{"t0":0.75,"m0":0.5,"wt":0.25,"wm":0.375},"plateau":"wet"}
  ],
  "forbidden":[["Biome.Ocean","Biome.Grassland"],["Biome.Scrub","Biome.Lake"]]
})J";

    // The closed tag vocabulary, written out here independently of the implementation.
    const char* const kTags[] = { "land", "water", "aquatic", "wet", "cold", "hot",
                                  "forest", "open", "fuel", "mountain", "volcanic", "succession" };

    StringCRC Slot(const char* s) { return StringCRC(s); }

    std::string SnakeCase(const std::string& in)
    {
        std::string out;
        for (char c : in)
        {
            if (std::isupper(static_cast<unsigned char>(c))) { out += '_'; out += static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            else { out += c; }
        }
        return out;
    }

    std::vector<uint32_t> EvalSet(const WorldCellFields& constFields, const char* json, uint32_t cells)
    {
        WorldCellFields& f = const_cast<WorldCellFields&>(constFields);
        Json::Value root;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(json, root));
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
        EXPECT_TRUE(expr.IsValid());
        EXPECT_TRUE(expr.Validate(f, errors));
        std::vector<uint32_t> hits;
        for (uint32_t i = 0; i < cells; ++i)
        {
            f.SetCell(i);
            if (expr.Evaluate(f)) { hits.push_back(i); }
        }
        return hits;
    }

    struct Gen
    {
        CoW::WorldGeneration::TestSession session;
        Dia::Maths::RandomStream          rng{ 1 };

        void Regenerate(uint32_t seed)
        {
            Json::Value args(Json::objectValue);
            args["seed"] = seed;
            session.Regenerate(args);
            EXPECT_TRUE(Testing::TickUntilIdle(session, rng));
            EXPECT_EQ(session.State(), WorldGenState::Complete);
        }
    };

    // Protected site kinds read from the shipped guarantee data (reach_any endpoints and variant_cover kinds), never by C++ name.
    std::vector<uint32_t> DataProtectedKinds()
    {
        std::vector<uint32_t> kinds;
        for (const GuaranteeEntry& e : CoW::WorldGeneration::Testing::ShippedGuaranteeSpec().guarantees)
        {
            if (e.type == GuaranteeRuleType::ReachAny) { kinds.push_back(StringCRC(e.from.c_str()).Value()); kinds.push_back(StringCRC(e.to.c_str()).Value()); }
            else if (e.type == GuaranteeRuleType::VariantCover) { kinds.push_back(StringCRC(e.kind.c_str()).Value()); }
        }
        return kinds;
    }

    // Expected is_protected, computed from the world's sites and flags only.
    std::vector<bool> ExpectedProtected(const WorldStructure& w, const std::vector<uint32_t>& kinds, uint32_t corridor)
    {
        std::vector<bool> out(static_cast<size_t>(w.CellCount()), false);
        for (int i = 0; i < w.CellCount(); ++i)
        {
            const uint8_t f = w.Cells()[i].terrainFlags;
            if ((f & TerrainFlag::Pass) != 0 || (f & TerrainFlag::RiverCrossesBarrier) != 0) { out[static_cast<size_t>(i)] = true; }
        }
        std::set<uint32_t> kindSet(kinds.begin(), kinds.end());
        kindSet.insert(corridor);
        for (const Site& s : w.sites.All())
        {
            if (kindSet.count(s.kindCrc) != 0) { out[s.cell] = true; }
        }
        return out;
    }

    void ConfigureSource(TerrainFieldSource& src)
    {
        for (uint32_t k : DataProtectedKinds()) { EXPECT_TRUE(src.AddProtectedKind(k)); }
    }

    void ExpectTerrainMatchesWorld(const WorldCellFields& fields, const WorldGenSession& s)
    {
        const WorldStructure& w = s.World();
        const TerrainLayers& layers = s.Generator().GetLayers();
        const uint32_t n = static_cast<uint32_t>(w.CellCount());
        ASSERT_GT(n, 0u);

        // Every committed layer (except biomeIndex, owned by biome.index) is a terrain.<snake_case> float field.
        for (int li = 0; li < TerrainLayers::GetLayerCount(); ++li)
        {
            const std::string layerName = TerrainLayers::GetLayerInfo(li).name;
            if (layerName == "biomeIndex") { continue; }
            const StringCRC field(SnakeCase(layerName).c_str());
            ASSERT_TRUE(fields.Has(Slot("terrain"), field)) << layerName;
            const float* f = layers.GetFloat(li);
            const uint32_t* u = layers.GetUInt(li);
            ASSERT_TRUE(f != nullptr || u != nullptr) << layerName;
            WorldCellFields& mf = const_cast<WorldCellFields&>(fields);
            for (uint32_t c = 0; c < n; ++c)
            {
                mf.SetCell(c);
                const float expected = f ? f[c] : static_cast<float>(u[c]);
                ASSERT_EQ(mf.GetFloat(Slot("terrain"), field), expected) << layerName << " cell " << c;
            }
        }

        const char* required[] = { "elevation", "is_river", "flow_direction", "water_source", "is_protected", "region" };
        for (const char* r : required) { EXPECT_TRUE(fields.Has(Slot("terrain"), StringCRC(r))) << r; }
        EXPECT_FALSE(fields.Has(Slot("terrain"), StringCRC("biome_index")));
        EXPECT_FALSE(fields.Has(Slot("terrain"), StringCRC("is_crossing")));

        const uint32_t corridor = CoW::WorldGeneration::CoWNodeKindIds().corridor.Value();
        const std::vector<bool> expectedProt = ExpectedProtected(w, DataProtectedKinds(), corridor);
        const int riverIdx = layers.FindLayer("riverMask");
        const int flowIdx  = layers.FindLayer("flowDirection");
        WorldCellFields& mf = const_cast<WorldCellFields&>(fields);
        for (uint32_t c = 0; c < n; ++c)
        {
            mf.SetCell(c);
            // is_river is the committed cell classification (waterSource River); the raw riverMask layer also marks
            // cells later classified as lake/ocean, and is exposed unchanged as terrain.river_mask.
            const bool river = w.Cells()[c].waterSource == WaterSource::River;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("river_mask")), static_cast<float>(layers.GetUInt(riverIdx)[c])) << c;
            ASSERT_EQ(mf.GetBool(Slot("terrain"), StringCRC("is_river")), river) << c;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("is_river")), river ? 1.0f : 0.0f) << c;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("region")), static_cast<float>(w.Cells()[c].region)) << c;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("water_source")), static_cast<float>(w.Cells()[c].waterSource)) << c;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("flow_direction")), static_cast<float>(layers.GetUInt(flowIdx)[c])) << c;
            ASSERT_EQ(mf.GetBool(Slot("terrain"), StringCRC("is_protected")), static_cast<bool>(expectedProt[c])) << c;
            ASSERT_EQ(mf.GetFloat(Slot("terrain"), StringCRC("is_protected")), expectedProt[c] ? 1.0f : 0.0f) << c;
        }
    }
}

// ---- Biome ----

TEST(FieldContributions, BiomeIndexAndTagsMatchHandComputedCells)
{
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(kBiomeFixture, Testing::ShippedStratumTable(), err));
    WorldStructure world;
    world.Resize({ 4, 2 });
    const BiomeIndex biomes[8] = { 0, 1, 2, 3, 2, 3, 0, 1 };
    for (int i = 0; i < 8; ++i) { world.Cells()[i].biome = biomes[i]; }

    BiomeFieldSource source(reg, world);
    WorldCellFields fields;
    RegisterBiomeFields(fields, source);

    ASSERT_TRUE(fields.Has(Slot("biome"), StringCRC("index")));
    for (uint32_t c = 0; c < 8; ++c)
    {
        fields.SetCell(c);
        EXPECT_EQ(fields.GetFloat(Slot("biome"), StringCRC("index")), static_cast<float>(biomes[c]));
    }

    for (const char* tag : kTags)
    {
        const StringCRC field((std::string("tag.") + tag).c_str());
        ASSERT_TRUE(fields.Has(Slot("biome"), field)) << tag;
        const TagId id = reg.FindTag(StringCRC(tag));
        ASSERT_NE(id, kNoTag) << tag;
        for (uint32_t c = 0; c < 8; ++c)
        {
            fields.SetCell(c);
            EXPECT_EQ(fields.GetBool(Slot("biome"), field), reg.HasTag(biomes[c], id)) << tag << " cell " << c;
            EXPECT_EQ(fields.GetFloat(Slot("biome"), field), reg.HasTag(biomes[c], id) ? 1.0f : 0.0f) << tag << " cell " << c;
        }
    }

    // Hand-computed: land = Grassland or Scrub = cells 2,3,4,5; water = Ocean or Lake = 0,1,6,7.
    const std::vector<uint32_t> land = EvalSet(fields, R"({"op":"==","slot":"biome","field":"tag.land","value":true})", 8);
    EXPECT_EQ(land, (std::vector<uint32_t>{ 2, 3, 4, 5 }));
    const std::vector<uint32_t> water = EvalSet(fields, R"({"op":"==","slot":"biome","field":"tag.water","value":1})", 8);
    EXPECT_EQ(water, (std::vector<uint32_t>{ 0, 1, 6, 7 }));
}

TEST(FieldContributions, BiomeTagVocabularyIsEnumerableAndRegisteredInFull)
{
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(kBiomeFixture, Testing::ShippedStratumTable(), err));
    ASSERT_EQ(BiomeRegistry::TagCount(), 12u);
    EXPECT_EQ(BiomeRegistry::TagName(static_cast<TagId>(BiomeRegistry::TagCount())), nullptr);
    WorldStructure world;
    world.Resize({ 2, 2 });
    BiomeFieldSource source(reg, world);
    WorldCellFields fields;
    RegisterBiomeFields(fields, source);
    for (uint32_t i = 0; i < BiomeRegistry::TagCount(); ++i)
    {
        const char* name = BiomeRegistry::TagName(static_cast<TagId>(i));
        ASSERT_NE(name, nullptr);
        EXPECT_EQ(reg.FindTag(StringCRC(name)), static_cast<TagId>(i)) << name;
        char field[96];
        std::snprintf(field, sizeof(field), "tag.%s", name);
        EXPECT_TRUE(fields.Has(Slot("biome"), StringCRC(field))) << field;
    }
}

TEST(FieldContributions, BiomeUnknownTagIsNotRegistered)
{
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(kBiomeFixture, Testing::ShippedStratumTable(), err));
    WorldStructure world;
    world.Resize({ 2, 2 });
    BiomeFieldSource source(reg, world);
    WorldCellFields fields;
    EXPECT_FALSE(fields.Has(Slot("biome"), StringCRC("index")));
    RegisterBiomeFields(fields, source);
    EXPECT_FALSE(fields.Has(Slot("biome"), StringCRC("tag.nonsense")));
    EXPECT_FALSE(fields.Has(Slot("biome"), StringCRC("tag.")));
    fields.Clear();
    EXPECT_FALSE(fields.Has(Slot("biome"), StringCRC("index")));
    EXPECT_FALSE(fields.Has(Slot("biome"), StringCRC("tag.land")));
}

TEST(FieldContributions, BiomeFieldsFollowWorldChangesAfterReregister)
{
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(kBiomeFixture, Testing::ShippedStratumTable(), err));
    WorldStructure world;
    world.Resize({ 2, 1 });
    world.Cells()[0].biome = 0;
    world.Cells()[1].biome = 2;
    BiomeFieldSource source(reg, world);
    WorldCellFields fields;
    RegisterBiomeFields(fields, source);
    fields.SetCell(1);
    EXPECT_TRUE(fields.GetBool(Slot("biome"), StringCRC("tag.land")));
    world.Cells()[1].biome = 1;
    fields.Clear();
    RegisterBiomeFields(fields, source);
    fields.SetCell(1);
    EXPECT_FALSE(fields.GetBool(Slot("biome"), StringCRC("tag.land")));
    EXPECT_EQ(fields.GetFloat(Slot("biome"), StringCRC("index")), 1.0f);
}

// ---- Terrain (generated Small seed-1 world) ----

TEST(FieldContributions, TerrainFieldsReturnCommittedLayerValuesOnSeedOneWorld)
{
    Gen g;
    g.Regenerate(1);
    ASSERT_EQ(g.session.World().dimensions.width, 66);
    ASSERT_EQ(g.session.World().dimensions.height, 42);

    TerrainFieldSource source(g.session);
    ConfigureSource(source);
    WorldCellFields fields;
    RegisterTerrainFields(fields, source);
    ExpectTerrainMatchesWorld(fields, g.session);
}

TEST(FieldContributions, TerrainIsProtectedCoversEverySourceOnSeedOneWorld)
{
    Gen g;
    g.Regenerate(1);
    TerrainFieldSource source(g.session);
    ConfigureSource(source);
    WorldCellFields fields;
    RegisterTerrainFields(fields, source);

    const WorldStructure& w = g.session.World();
    const std::vector<bool> expected = ExpectedProtected(w, DataProtectedKinds(), CoW::WorldGeneration::CoWNodeKindIds().corridor.Value());
    int total = 0, byFlag = 0, bySite = 0;
    std::set<uint32_t> siteCells;
    std::set<uint32_t> kindSet;
    for (uint32_t k : DataProtectedKinds()) { kindSet.insert(k); }
    kindSet.insert(CoW::WorldGeneration::CoWNodeKindIds().corridor.Value());
    for (const Site& s : w.sites.All()) { if (kindSet.count(s.kindCrc) != 0) { siteCells.insert(s.cell); } }
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const bool flag = (w.Cells()[i].terrainFlags & (TerrainFlag::Pass | TerrainFlag::RiverCrossesBarrier)) != 0;
        total += expected[i] ? 1 : 0;
        byFlag += flag ? 1 : 0;
        bySite += siteCells.count(static_cast<uint32_t>(i)) != 0 ? 1 : 0;
    }
    EXPECT_GT(total, 0);
    EXPECT_GT(bySite, 0) << "seed 1 produced no protected-kind sites";
    EXPECT_GT(byFlag, 0) << "seed 1 produced no Pass/RiverCrossesBarrier cells";
    EXPECT_LT(total, w.CellCount());

    // A protected set that omits the data-declared kinds is strictly smaller (kinds are an input, not hard-coded).
    TerrainFieldSource bare(g.session);
    WorldCellFields bareFields;
    RegisterTerrainFields(bareFields, bare);
    int bareTotal = 0;
    for (uint32_t c = 0; c < static_cast<uint32_t>(w.CellCount()); ++c)
    {
        bareFields.SetCell(c);
        bareTotal += bareFields.GetBool(Slot("terrain"), StringCRC("is_protected")) ? 1 : 0;
    }
    EXPECT_LE(bareTotal, total);
}

TEST(FieldContributions, TerrainFieldsReresolveAfterRegenerate)
{
    Gen g;
    g.Regenerate(1);
    TerrainFieldSource source(g.session);
    ConfigureSource(source);
    WorldCellFields fields;
    RegisterTerrainFields(fields, source);
    ExpectTerrainMatchesWorld(fields, g.session);

    std::vector<float> oldElevation;
    for (int i = 0; i < g.session.World().CellCount(); ++i) { oldElevation.push_back(g.session.World().Cells()[i].elevation); }

    g.Regenerate(7);
    fields.Clear();
    EXPECT_FALSE(fields.Has(Slot("terrain"), StringCRC("elevation")));
    RegisterTerrainFields(fields, source);
    ExpectTerrainMatchesWorld(fields, g.session);

    int differing = 0;
    for (int i = 0; i < g.session.World().CellCount(); ++i)
    {
        fields.SetCell(static_cast<uint32_t>(i));
        if (fields.GetFloat(Slot("terrain"), StringCRC("elevation")) != oldElevation[static_cast<size_t>(i)]) { ++differing; }
    }
    EXPECT_GT(differing, 0) << "fields still show the old world";
}

TEST(FieldContributions, TerrainConditionExpressionSelectsExpectedCells)
{
    Gen g;
    g.Regenerate(1);
    TerrainFieldSource source(g.session);
    ConfigureSource(source);
    WorldCellFields fields;
    RegisterTerrainFields(fields, source);

    const WorldStructure& w = g.session.World();
    const uint32_t n = static_cast<uint32_t>(w.CellCount());
    const TerrainLayers& layers = g.session.Generator().GetLayers();
    const float* elevation = layers.GetFloat(layers.FindLayer("elevation"));
    const std::vector<bool> prot = ExpectedProtected(w, DataProtectedKinds(), CoW::WorldGeneration::CoWNodeKindIds().corridor.Value());

    std::vector<uint32_t> expectRiver, expectRiverLow, expectProt;
    for (uint32_t c = 0; c < n; ++c)
    {
        const bool isRiver = w.Cells()[c].waterSource == WaterSource::River;
        if (isRiver) { expectRiver.push_back(c); }
        if (isRiver && elevation[c] < 0.5f) { expectRiverLow.push_back(c); }
        if (prot[c]) { expectProt.push_back(c); }
    }
    ASSERT_FALSE(expectRiver.empty());
    ASSERT_FALSE(expectProt.empty());

    EXPECT_EQ(EvalSet(fields, R"({"op":"==","slot":"terrain","field":"is_river","value":true})", n), expectRiver);
    EXPECT_EQ(EvalSet(fields, R"({"op":"==","slot":"terrain","field":"is_river","value":1})", n), expectRiver);
    EXPECT_EQ(EvalSet(fields, R"({"op":"==","slot":"terrain","field":"is_protected","value":true})", n), expectProt);
    EXPECT_EQ(EvalSet(fields, R"({"op":"==","slot":"terrain","field":"is_protected","value":1})", n), expectProt);
    EXPECT_EQ(EvalSet(fields, R"({"op":"and","conditions":[
        {"op":"<","slot":"terrain","field":"elevation","value":0.5},
        {"op":"==","slot":"terrain","field":"is_river","value":true}]})", n), expectRiverLow);
}

TEST(FieldContributions, TerrainFieldsReadZeroBeforeAnyWorldExists)
{
    CoW::WorldGeneration::TestSession session;
    TerrainFieldSource source(session);
    WorldCellFields fields;
    RegisterTerrainFields(fields, source);
    EXPECT_TRUE(fields.Has(Slot("terrain"), StringCRC("elevation")));
    fields.SetCell(0);
    EXPECT_EQ(fields.GetFloat(Slot("terrain"), StringCRC("elevation")), 0.0f);
    EXPECT_FALSE(fields.GetBool(Slot("terrain"), StringCRC("is_protected")));
    fields.SetCell(999999);
    EXPECT_EQ(fields.GetFloat(Slot("terrain"), StringCRC("region")), 0.0f);
}
