// S-1 Biome Registry: loading and parsing biome data - strict load faults (plan task 5).
//
// Every fault fails LoadFromText, fills RegistryError and leaves a previously loaded registry unchanged.
// Stable RegistryError.field strings:
//   "document"       text is not valid JSON (row 0)
//   "format"         format is not "cow.biomes" (row 0)
//   "version"        version is not 1 (row 0)
//   "biomes"         biomes is not an array, or has more than 254 rows (row 0)
//   "id"             missing/empty/too long, or duplicate id (row = the offending row)
//   "tags" "zones"   unknown tag / zone name (row = the row)
//   "roles"          unknown role, a role held by two rows (row = the second holder),
//                    or a role held by no row (row 0, id empty)
//   "climate"        climate object malformed (row = the row)
//   "climate.wt"     wt <= 0 (row = the row);  "climate.wm"  wm <= 0 (row = the row)
//   "pin"            unknown pin rule / malformed pin (row = the row)
//   "pin.priority"   priority already used by an earlier row (row = the later row)
//   "plateau"        bad plateau value, or plateau already held by an earlier row (row = the later row)
//   "forbidden"      malformed pair, unknown id, duplicate pair (either order), or a biome paired with
//                    itself (row = index of the pair in the forbidden list)
// A "sidecar validation failure" fault is omitted: no C++ validator exists.
#include <gtest/gtest.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <cstring>
#include <string>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

using namespace CoW::World;
using Dia::Core::StringCRC;

namespace
{
    // Rows: 0 Ocean (ocean), 1 Lake (lake), 2 Grassland (fallback, climate, dry), 3 Scrub (climate, wet).
    const char* const kBase = R"J({
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

    // The registry accepts any pin rule id unless the caller supplies a rule table; this stands in for the generator's.
    bool KnownRule(StringCRC rule)
    {
        return rule == StringCRC("water_ocean") || rule == StringCRC("water_lake") || rule == StringCRC("wetland");
    }

    // One mutation of the base: replace the first occurrence of `from` with `to`.
    std::string Mutate(const char* from, const char* to)
    {
        std::string text = kBase;
        const size_t at = text.find(from);
        EXPECT_NE(at, std::string::npos) << "base does not contain: " << from;
        if (at != std::string::npos) { text.replace(at, std::strlen(from), to); }
        return text;
    }

    struct Fault
    {
        const char* name;
        std::string json;
        const char* expectedField;
        uint32_t    expectedRow;
        const char* expectedId;   // nullptr = do not check
    };

    std::vector<Fault> Faults()
    {
        std::vector<Fault> f;
        f.push_back({ "WrongFormat", Mutate("cow.biomes", "cow.other"), "format", 0, nullptr });
        f.push_back({ "WrongVersion", Mutate("\"version\":1", "\"version\":2"), "version", 0, nullptr });
        f.push_back({ "UnknownTag", Mutate("\"water\",\"aquatic\"", "\"nonsense\""), "tags", 0, "Biome.Ocean" });
        f.push_back({ "UnknownZone", Mutate("\"FirstShores\"", "\"Nowhere\""), "zones", 2, "Biome.Grassland" });
        f.push_back({ "UnknownPinRule", Mutate("\"water_lake\"", "\"bogus_rule\""), "pin", 1, "Biome.Lake" });
        f.push_back({ "ForbiddenUnknownId", Mutate("[\"Biome.Scrub\",\"Biome.Lake\"]", "[\"Biome.Scrub\",\"Biome.Nope\"]"),
                      "forbidden", 1, "Biome.Nope" });

        f.push_back({ "DuplicateId", Mutate("{\"id\":\"Biome.Scrub\"", "{\"id\":\"Biome.Lake\""), "id", 3, "Biome.Lake" });

        f.push_back({ "ForbiddenPairRepeatedSameOrder",
                      Mutate("[\"Biome.Scrub\",\"Biome.Lake\"]]", "[\"Biome.Scrub\",\"Biome.Lake\"],[\"Biome.Scrub\",\"Biome.Lake\"]]"),
                      "forbidden", 2, nullptr });
        f.push_back({ "ForbiddenPairRepeatedReversed",
                      Mutate("[\"Biome.Scrub\",\"Biome.Lake\"]]", "[\"Biome.Scrub\",\"Biome.Lake\"],[\"Biome.Lake\",\"Biome.Scrub\"]]"),
                      "forbidden", 2, nullptr });
        f.push_back({ "ForbiddenSelfPair",
                      Mutate("[\"Biome.Scrub\",\"Biome.Lake\"]]", "[\"Biome.Scrub\",\"Biome.Lake\"],[\"Biome.Lake\",\"Biome.Lake\"]]"),
                      "forbidden", 2, "Biome.Lake" });

        f.push_back({ "OceanRoleHeldByNone", Mutate("\"roles\":[\"ocean\"],", ""), "roles", 0, nullptr });
        f.push_back({ "LakeRoleHeldByNone", Mutate("\"roles\":[\"lake\"],", ""), "roles", 0, nullptr });
        f.push_back({ "FallbackRoleHeldByNone", Mutate("\"roles\":[\"fallback\"],", ""), "roles", 0, nullptr });
        f.push_back({ "OceanRoleHeldByTwo", Mutate("\"roles\":[\"fallback\"]", "\"roles\":[\"fallback\",\"ocean\"]"),
                      "roles", 2, "Biome.Grassland" });
        f.push_back({ "LakeRoleHeldByTwo", Mutate("\"generated\":false,\"zones\":[],", "\"generated\":false,\"zones\":[],\"roles\":[\"lake\"],"),
                      "roles", 3, "Biome.Scrub" });
        f.push_back({ "FallbackRoleHeldByTwo", Mutate("\"generated\":false,\"zones\":[],", "\"generated\":false,\"zones\":[],\"roles\":[\"fallback\"],"),
                      "roles", 3, "Biome.Scrub" });

        f.push_back({ "PinsSamePriority", Mutate("\"priority\":150", "\"priority\":100"), "pin.priority", 2, "Biome.Grassland" });

        f.push_back({ "ClimateWtZero", Mutate("\"wt\":0.125", "\"wt\":0"), "climate.wt", 2, "Biome.Grassland" });
        f.push_back({ "ClimateWtNegative", Mutate("\"wt\":0.125", "\"wt\":-0.5"), "climate.wt", 2, "Biome.Grassland" });
        f.push_back({ "ClimateWmZero", Mutate("\"wm\":0.75}", "\"wm\":0}"), "climate.wm", 2, "Biome.Grassland" });
        f.push_back({ "ClimateWmNegative", Mutate("\"wm\":0.75}", "\"wm\":-0.5}"), "climate.wm", 2, "Biome.Grassland" });

        f.push_back({ "TwoDryPlateaus", Mutate("\"plateau\":\"wet\"", "\"plateau\":\"dry\""), "plateau", 3, "Biome.Scrub" });
        f.push_back({ "TwoWetPlateaus", Mutate("\"plateau\":\"dry\"", "\"plateau\":\"wet\""), "plateau", 3, "Biome.Scrub" });
        return f;
    }

    // 255 rows, each otherwise valid (roles on the first three).
    std::string TooManyRows()
    {
        std::string s = "{\"format\":\"cow.biomes\",\"version\":1,\"biomes\":[";
        for (uint32_t i = 0; i < 255; ++i)
        {
            if (i) { s += ","; }
            const char* role = i == 0 ? ",\"roles\":[\"ocean\"]" : i == 1 ? ",\"roles\":[\"lake\"]" : i == 2 ? ",\"roles\":[\"fallback\"]" : "";
            s += "{\"id\":\"Biome.N" + std::to_string(i) + "\",\"name\":\"N\",\"colour\":\"#000001\",\"description\":\"d\","
                 "\"tags\":[\"land\"],\"generated\":true,\"zones\":[\"FirstShores\"]" + role + "}";
        }
        s += "]}";
        return s;
    }
}

TEST(BiomeRegistryLoad, BaseFixtureLoads)
{
    BiomeRegistry r;
    RegistryError err;
    EXPECT_TRUE(r.LoadFromText(kBase, ::CoW::World::Testing::ShippedStratumTable(), err)) << err.field.AsCStr() << " row " << err.row << ": " << err.reason.AsCStr();
    EXPECT_EQ(r.Count(), 4u);
}

TEST(BiomeRegistryLoad, FaultTable)
{
    const std::vector<Fault> faults = Faults();
    for (const Fault& fault : faults)
    {
        SCOPED_TRACE(fault.name);

        BiomeRegistry r;
        RegistryError loadErr;
        ASSERT_TRUE(r.LoadFromText(kBase, ::CoW::World::Testing::ShippedStratumTable(), loadErr));
        const uint32_t count = r.Count();
        const uint64_t hash = r.Hash();

        RegistryError err;
        EXPECT_FALSE(r.LoadFromText(fault.json.c_str(), ::CoW::World::Testing::ShippedStratumTable(), err, &KnownRule));
        EXPECT_STREQ(err.field.AsCStr(), fault.expectedField);
        EXPECT_EQ(err.row, fault.expectedRow);
        if (fault.expectedId != nullptr) { EXPECT_STREQ(err.id.AsCStr(), fault.expectedId); }
        EXPECT_GT(std::strlen(err.reason.AsCStr()), 0u);

        // Previously loaded registry answers identically.
        EXPECT_EQ(r.Count(), count);
        EXPECT_EQ(r.Hash(), hash);
        EXPECT_EQ(r.Find(StringCRC("Biome.Ocean")), 0);
        EXPECT_EQ(r.Find(StringCRC("Biome.Lake")), 1);
        EXPECT_EQ(r.Find(StringCRC("Biome.Grassland")), 2);
        EXPECT_EQ(r.Find(StringCRC("Biome.Scrub")), 3);
        EXPECT_EQ(r.Role(BiomeRole::Fallback), 2);
    }
}

TEST(BiomeRegistryLoad, FaultOnFirstLoadStaysEmpty)
{
    const std::vector<Fault> faults = Faults();
    for (const Fault& fault : faults)
    {
        SCOPED_TRACE(fault.name);
        BiomeRegistry r;
        RegistryError err;
        EXPECT_FALSE(r.LoadFromText(fault.json.c_str(), ::CoW::World::Testing::ShippedStratumTable(), err, &KnownRule));
        EXPECT_EQ(r.Count(), 0u);
        EXPECT_EQ(r.Hash(), 0u);
    }
}

TEST(BiomeRegistryLoad, MoreThan254RowsFails)
{
    BiomeRegistry r;
    RegistryError loadErr;
    ASSERT_TRUE(r.LoadFromText(kBase, ::CoW::World::Testing::ShippedStratumTable(), loadErr));
    const uint64_t hash = r.Hash();
    RegistryError err;
    EXPECT_FALSE(r.LoadFromText(TooManyRows().c_str(), ::CoW::World::Testing::ShippedStratumTable(), err));
    EXPECT_STREQ(err.field.AsCStr(), "biomes");
    EXPECT_EQ(r.Count(), 4u);
    EXPECT_EQ(r.Hash(), hash);
}
