// S-1 Biome Registry system tests: accessors, masks, hash (plan tasks 3 and 4).
#include <gtest/gtest.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/Logger.h>

#include <string>
#include <utility>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

using namespace CoW::World;
using CoW::WorldGeneration::StratumIndex;
using Dia::Core::StringCRC;

namespace
{
    // Row order: 0 Ocean, 1 Lake, 2 Grassland, 3 Scrub. The last row's last key is "plateau".
    const char* const kFixture = R"J({
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

    std::string Replace(std::string text, const std::string& from, const std::string& to)
    {
        const size_t at = text.find(from);
        EXPECT_NE(at, std::string::npos) << "fixture does not contain: " << from;
        if (at != std::string::npos) { text.replace(at, from.size(), to); }
        return text;
    }

    bool LoadText(BiomeRegistry& r, const std::string& text)
    {
        RegistryError err;
        return r.LoadFromText(text.c_str(), ::CoW::World::Testing::ShippedStratumTable(), err);
    }

    uint64_t HashOf(const std::string& text)
    {
        BiomeRegistry r;
        EXPECT_TRUE(LoadText(r, text));
        return r.Hash();
    }
}

TEST(BiomeRegistry, LoadsFourRows)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.Count(), 4u);
}

TEST(BiomeRegistry, FindReturnsRowOrder)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.Find(StringCRC("Biome.Ocean")), 0);
    EXPECT_EQ(r.Find(StringCRC("Biome.Lake")), 1);
    EXPECT_EQ(r.Find(StringCRC("Biome.Grassland")), 2);
    EXPECT_EQ(r.Find(StringCRC("Biome.Scrub")), 3);
    EXPECT_EQ(r.Find(StringCRC("Biome.Nope")), kNoBiome);
    EXPECT_EQ(r.Find(StringCRC()), kNoBiome);
}

TEST(BiomeRegistry, IdentityAccessors)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.Id(2), StringCRC("Biome.Grassland"));
    EXPECT_STREQ(r.IdText(0), "Biome.Ocean");
    EXPECT_STREQ(r.IdText(3), "Biome.Scrub");
    EXPECT_STREQ(r.Name(1), "Lake");
    EXPECT_STREQ(r.Name(2), "Grassland");
    EXPECT_EQ(r.Colour(0), 0x1F4F8Fu);
    EXPECT_EQ(r.Colour(2), 0x9ACD32u);
    EXPECT_EQ(r.Colour(3), 0x808000u);
}

TEST(BiomeRegistry, GeneratedRolesAndPlateau)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_TRUE(r.IsGenerated(0));
    EXPECT_TRUE(r.IsGenerated(2));
    EXPECT_FALSE(r.IsGenerated(3));
    EXPECT_EQ(r.Role(BiomeRole::Ocean), 0);
    EXPECT_EQ(r.Role(BiomeRole::Lake), 1);
    EXPECT_EQ(r.Role(BiomeRole::Fallback), 2);
    EXPECT_EQ(r.Plateau(false), 2);
    EXPECT_EQ(r.Plateau(true), 3);
}

TEST(BiomeRegistry, PlateauAbsentIsNoBiome)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, Replace(kFixture, ",\"plateau\":\"wet\"", "")));
    EXPECT_EQ(r.Plateau(true), kNoBiome);
    EXPECT_EQ(r.Plateau(false), 2);
}

TEST(BiomeRegistry, OutOfRangeIndexIsSafe)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    for (BiomeIndex bad : { BiomeIndex(4), BiomeIndex(200), kNoBiome })
    {
        EXPECT_EQ(r.Id(bad), StringCRC());
        EXPECT_STREQ(r.IdText(bad), "");
        EXPECT_STREQ(r.Name(bad), "");
        EXPECT_EQ(r.Colour(bad), 0u);
        EXPECT_FALSE(r.IsGenerated(bad));
        EXPECT_FALSE(r.HasTag(bad, 0));
        EXPECT_EQ(r.TagMask(bad), 0u);
        EXPECT_FALSE(r.IsValidInStratum(bad, kNoStratum));
        EXPECT_FALSE(r.AreForbidden(bad, 0));
        EXPECT_FALSE(r.AreForbidden(0, bad));
        EXPECT_EQ(r.Climate(bad), nullptr);
    }
}

TEST(BiomeRegistry, EmptyRegistryIsSafe)
{
    BiomeRegistry r;
    EXPECT_EQ(r.Count(), 0u);
    EXPECT_EQ(r.Find(StringCRC("Biome.Ocean")), kNoBiome);
    EXPECT_STREQ(r.IdText(0), "");
    EXPECT_EQ(r.Role(BiomeRole::Ocean), kNoBiome);
    EXPECT_EQ(r.Plateau(true), kNoBiome);
    uint32_t n = 99;
    EXPECT_EQ(r.ClimateBiomes(n), nullptr);
    EXPECT_EQ(n, 0u);
}

TEST(BiomeRegistry, FailedSecondLoadLeavesContents)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    RegistryError err;
    const uint64_t before = r.Hash();
    EXPECT_FALSE(r.LoadFromText("{ not json", ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_FALSE(r.LoadFromText(Replace(kFixture, "\"water\",\"aquatic\"", "\"nonsense\"").c_str(), ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_FALSE(r.LoadFromText(Replace(kFixture, "\"FirstShores\"", "\"Nowhere\"").c_str(), ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_FALSE(r.LoadFromText(Replace(kFixture, "\"water_ocean\"", "\"bogus_rule\"").c_str(), ::CoW::World::Testing::ShippedStratumTable(), err,
                                [](StringCRC rule) { return rule == StringCRC("water_lake") || rule == StringCRC("wetland"); }));
    EXPECT_FALSE(r.LoadFromText(Replace(kFixture, "\"colour\":\"#808000\",", "").c_str(), ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_EQ(r.Count(), 4u);
    EXPECT_EQ(r.Hash(), before);
    EXPECT_STREQ(r.IdText(2), "Biome.Grassland");
}

TEST(BiomeRegistry, FailedFirstLoadStaysEmpty)
{
    BiomeRegistry r;
    RegistryError err;
    EXPECT_FALSE(r.LoadFromText("{}", ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_EQ(r.Count(), 0u);
}

TEST(BiomeRegistry, MoveAndSwap)
{
    BiomeRegistry a;
    ASSERT_TRUE(LoadText(a, kFixture));
    BiomeRegistry b(std::move(a));
    EXPECT_EQ(b.Count(), 4u);
    EXPECT_EQ(a.Count(), 0u);
    BiomeRegistry c;
    c.Swap(b);
    EXPECT_EQ(c.Count(), 4u);
    EXPECT_EQ(b.Count(), 0u);
    EXPECT_EQ(c.Find(StringCRC("Biome.Lake")), 1);
}

TEST(BiomeRegistry, TagsAndMasks)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    // Vocabulary order: land, water, aquatic, wet, cold, hot, forest, open, fuel, mountain, volcanic, succession.
    EXPECT_EQ(r.FindTag(StringCRC("land")), 0);
    EXPECT_EQ(r.FindTag(StringCRC("water")), 1);
    EXPECT_EQ(r.FindTag(StringCRC("aquatic")), 2);
    EXPECT_EQ(r.FindTag(StringCRC("open")), 7);
    EXPECT_EQ(r.FindTag(StringCRC("succession")), 11);
    EXPECT_EQ(r.FindTag(StringCRC("bogus")), kNoTag);
    EXPECT_EQ(r.TagMask(0), (1ull << 1) | (1ull << 2));
    EXPECT_EQ(r.TagMask(2), (1ull << 0) | (1ull << 7) | (1ull << 8));
    EXPECT_TRUE(r.HasTag(0, 1));
    EXPECT_FALSE(r.HasTag(0, 0));
    EXPECT_TRUE(r.HasTag(3, 11));
    EXPECT_FALSE(r.HasTag(2, 11));
    EXPECT_FALSE(r.HasTag(2, kNoTag));
}

TEST(BiomeRegistry, ZoneValidity)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_TRUE(r.IsValidInStratum(0, kNoStratum));
    EXPECT_FALSE(r.IsValidInStratum(0, kStratumFirst));
    EXPECT_FALSE(r.IsValidInStratum(2, kNoStratum));
    EXPECT_TRUE(r.IsValidInStratum(2, kStratumFirst));
    EXPECT_TRUE(r.IsValidInStratum(2, kStratumWild));
    EXPECT_FALSE(r.IsValidInStratum(2, kStratumAncient));
    for (StratumIndex z : { kNoStratum, kStratumFirst, kStratumWild, kStratumAncient })
    {
        EXPECT_FALSE(r.IsValidInStratum(3, z));   // empty zones
    }
}

TEST(BiomeRegistry, ForbiddenIsSymmetric)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_TRUE(r.AreForbidden(0, 2));
    EXPECT_TRUE(r.AreForbidden(2, 0));
    EXPECT_TRUE(r.AreForbidden(3, 1));
    EXPECT_TRUE(r.AreForbidden(1, 3));
    EXPECT_FALSE(r.AreForbidden(0, 1));
    EXPECT_FALSE(r.AreForbidden(2, 3));
    EXPECT_FALSE(r.AreForbidden(0, 0));
}

TEST(BiomeRegistry, ClimateAndOrder)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.Climate(0), nullptr);
    ASSERT_NE(r.Climate(2), nullptr);
    EXPECT_EQ(r.Climate(2)->t0, 0.5f);
    EXPECT_EQ(r.Climate(2)->m0, 0.25f);
    EXPECT_EQ(r.Climate(2)->wt, 0.125f);
    EXPECT_EQ(r.Climate(2)->wm, 0.75f);
    ASSERT_NE(r.Climate(3), nullptr);
    EXPECT_EQ(r.Climate(3)->wm, 0.375f);
    uint32_t n = 0;
    const BiomeIndex* list = r.ClimateBiomes(n);
    ASSERT_EQ(n, 2u);
    EXPECT_EQ(list[0], 2);
    EXPECT_EQ(list[1], 3);
}

TEST(BiomeRegistry, PinsHighestPriorityFirst)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    uint32_t n = 0;
    const BiomeIndex* pins = r.PinsByPriority(n);
    ASSERT_EQ(n, 3u);
    EXPECT_EQ(pins[0], 1);   // priority 200
    EXPECT_EQ(pins[1], 2);   // 150
    EXPECT_EQ(pins[2], 0);   // 100
    EXPECT_EQ(r.Pin(1), StringCRC("water_lake"));
    EXPECT_EQ(r.Pin(2), StringCRC("wetland"));
    EXPECT_EQ(r.Pin(0), StringCRC("water_ocean"));
    EXPECT_STREQ(r.PinRuleText(1), "water_lake");
}

TEST(BiomeRegistry, HashStableAcrossLoads)
{
    EXPECT_EQ(HashOf(kFixture), HashOf(kFixture));
    EXPECT_NE(HashOf(kFixture), 0u);
}

TEST(BiomeRegistry, HashChangesWithClassificationFields)
{
    const uint64_t base = HashOf(kFixture);
    const std::pair<const char*, const char*> edits[] = {
        { "\"land\",\"open\",\"fuel\"", "\"land\",\"open\"" },                       // tag
        { "[\"FirstShores\",\"WildReaches\"]", "[\"FirstShores\"]" },                // zone
        { "\"generated\":false", "\"generated\":true" },                             // generated
        { "\"t0\":0.5,", "\"t0\":0.625," },                                          // climate float
        { "\"priority\":150", "\"priority\":151" },                                  // pin priority
        { "\"rule\":\"wetland\"", "\"rule\":\"vent\"" },                             // pin rule
        { ",\"plateau\":\"dry\"", "" },                                              // plateau removed
        { "[\"Biome.Scrub\",\"Biome.Lake\"]", "[\"Biome.Scrub\",\"Biome.Ocean\"]" }, // forbidden pair
                { ",\"plateau\":\"wet\"", "" },                                              // last row's last field removed
        { "\"wm\":0.375", "\"wm\":0.5" },                                            // last row's climate
    };
    for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i)
    {
        EXPECT_NE(HashOf(Replace(kFixture, edits[i].first, edits[i].second)), base) << "edit " << i;
    }
    // Id rename (row and forbidden pair).
    EXPECT_NE(HashOf(Replace(Replace(kFixture, "{\"id\":\"Biome.Scrub\"", "{\"id\":\"Biome.Shrub\""),
                             "[\"Biome.Scrub\",", "[\"Biome.Shrub\",")), base);
    // Role swap: ocean and lake exchange roles.
    std::string swapped = Replace(kFixture, "\"roles\":[\"ocean\"]", "\"roles\":[\"TMP\"]");
    swapped = Replace(swapped, "\"roles\":[\"lake\"]", "\"roles\":[\"ocean\"]");
    swapped = Replace(swapped, "\"roles\":[\"TMP\"]", "\"roles\":[\"lake\"]");
    EXPECT_NE(HashOf(swapped), base);
}

TEST(BiomeRegistry, HashIgnoresCosmeticFields)
{
    const uint64_t base = HashOf(kFixture);
    EXPECT_EQ(HashOf(Replace(kFixture, "\"name\":\"Lake\"", "\"name\":\"Mere\"")), base);
    EXPECT_EQ(HashOf(Replace(kFixture, "Open plains", "Rolling plains")), base);
    EXPECT_EQ(HashOf(Replace(kFixture, "#808000", "#808001")), base);
}

namespace
{
    // Rows "Biome.N0".."Biome.N<rows-1>", each tagged land with zone FirstShores; the last row also has
    // the "succession" tag and the AncientInteriors zone. Forbidden: (last, 0) and (1, last-1).
    std::string BigFixture(uint32_t rows)
    {
        std::string s = "{\"format\":\"cow.biomes\",\"version\":1,\"biomes\":[";
        for (uint32_t i = 0; i < rows; ++i)
        {
            if (i) { s += ","; }
            const bool last = (i == rows - 1);
            const char* role = i == 0 ? ",\"roles\":[\"ocean\"]" : i == 1 ? ",\"roles\":[\"lake\"]" : i == 2 ? ",\"roles\":[\"fallback\"]" : "";
            s += "{\"id\":\"Biome.N" + std::to_string(i) + "\",\"name\":\"N\",\"colour\":\"#000001\",\"description\":\"d\","
                 "\"tags\":[\"land\"" + std::string(last ? ",\"succession\"" : "") + "],\"generated\":true,"
                 "\"zones\":[\"FirstShores\"" + std::string(last ? ",\"AncientInteriors\"" : "") + "]" + role + "}";
        }
        s += "],\"forbidden\":[[\"Biome.N" + std::to_string(rows - 1) + "\",\"Biome.N0\"],[\"Biome.N1\",\"Biome.N" +
             std::to_string(rows - 2) + "\"]]}";
        return s;
    }
}

TEST(BiomeRegistry, CapacityBoundary254Rows)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, BigFixture(254)));
    EXPECT_EQ(r.Count(), 254u);
    EXPECT_EQ(r.Find(StringCRC("Biome.N253")), 253);
    EXPECT_STREQ(r.IdText(253), "Biome.N253");
    EXPECT_TRUE(r.AreForbidden(253, 0));
    EXPECT_TRUE(r.AreForbidden(0, 253));
    EXPECT_TRUE(r.AreForbidden(1, 252));
    EXPECT_TRUE(r.AreForbidden(252, 1));
    EXPECT_FALSE(r.AreForbidden(253, 1));
    EXPECT_FALSE(r.AreForbidden(252, 0));
    EXPECT_TRUE(r.HasTag(253, 11));
    EXPECT_FALSE(r.HasTag(252, 11));
    EXPECT_TRUE(r.IsValidInStratum(253, kStratumAncient));
    EXPECT_FALSE(r.IsValidInStratum(252, kStratumAncient));
    EXPECT_TRUE(r.IsValidInStratum(253, kStratumFirst));
}

TEST(BiomeRegistry, MoreThan254RowsFails)
{
    BiomeRegistry r;
    EXPECT_FALSE(LoadText(r, BigFixture(255)));
    EXPECT_EQ(r.Count(), 0u);
}

namespace
{
    // Counts log entries whose text contains the ResolveOrFallback warning marker.
    class UnknownIdSink : public Dia::Observation::Log::ISink
    {
    public:
        void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
        {
            if (std::string(e.message).find("BiomeRegistry unknown id") != std::string::npos) { ++mCount; }
        }
        const char* GetName() const override { return "BiomeRegistryUnknownIdSink"; }
        unsigned int mCount = 0;
    };

    class BiomeRegistryResolve : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            auto& l = Dia::Observation::Log::Logger::Instance();
            l.RegisterThreadBuffer();
            l.RegisterSink(&mSink);
        }
        void TearDown() override
        {
            auto& l = Dia::Observation::Log::Logger::Instance();
            l.FlushSync();
            l.UnregisterSink(&mSink);
            l.UnregisterThreadBuffer();
        }
        unsigned int Warnings()
        {
            Dia::Observation::Log::Logger::Instance().FlushSync();
            return mSink.mCount;
        }
        UnknownIdSink mSink;
    };
}

TEST_F(BiomeRegistryResolve, UnknownIdReturnsFallbackIndex)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Nope")), 2);
    EXPECT_EQ(Warnings(), 1u);
}

TEST_F(BiomeRegistryResolve, WarnsOncePerDistinctUnknownId)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.A")), 2); }
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.B")), 2);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.C")), 2);
    EXPECT_EQ(Warnings(), 3u);
}

TEST_F(BiomeRegistryResolve, KnownIdLeavesCountUnchanged)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Ocean")), 0);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Scrub")), 3);
    EXPECT_EQ(Warnings(), 0u);
}

TEST_F(BiomeRegistryResolve, SecondInstanceWarnsAgain)
{
    BiomeRegistry a, b;
    ASSERT_TRUE(LoadText(a, kFixture));
    ASSERT_TRUE(LoadText(b, kFixture));
    a.ResolveOrFallback(StringCRC("Biome.Nope"));
    b.ResolveOrFallback(StringCRC("Biome.Nope"));
    EXPECT_EQ(Warnings(), 2u);
}

TEST_F(BiomeRegistryResolve, ReloadResetsWarnedList)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    r.ResolveOrFallback(StringCRC("Biome.Nope"));
    r.ResolveOrFallback(StringCRC("Biome.Nope"));
    EXPECT_EQ(Warnings(), 1u);
    ASSERT_TRUE(LoadText(r, kFixture));
    r.ResolveOrFallback(StringCRC("Biome.Nope"));
    EXPECT_EQ(Warnings(), 2u);
}

TEST_F(BiomeRegistryResolve, EmptyRegistryReturnsNoBiome)
{
    BiomeRegistry r;
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Nope")), kNoBiome);
}

TEST_F(BiomeRegistryResolve, ZeroCrcIsDistinctUnknownId)
{
    BiomeRegistry r;
    ASSERT_TRUE(LoadText(r, kFixture));
    EXPECT_EQ(r.ResolveOrFallback(StringCRC()), 2);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC()), 2);
    EXPECT_EQ(r.ResolveOrFallback(StringCRC("Biome.Nope")), 2);
    EXPECT_EQ(Warnings(), 2u);
}
