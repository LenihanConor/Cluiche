#include <gtest/gtest.h>

#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/CellSelector.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <cstring>
#include <string>

namespace
{
    using Dia::Core::StringCRC;
    using Draoi::HexWorld::CellSelectorSet;
    using Draoi::HexWorld::DerivedKind;
    using Draoi::HexWorld::FieldReader;
    using Draoi::HexWorld::SelectorError;
    using Draoi::HexWorld::WorldCellFields;

    float ConstFloat(const void*, uint32_t) { return 0.0f; }
    bool  ConstBool (const void*, uint32_t) { return false; }

    FieldReader F() { FieldReader r{}; r.getFloat = &ConstFloat; return r; }
    FieldReader B() { FieldReader r{}; r.getBool = &ConstBool; return r; }

    void RegisterFields(WorldCellFields& f)
    {
        f.Register(StringCRC("terrain"), StringCRC("elevation"), F());
        f.Register(StringCRC("terrain"), StringCRC("is_river"), B());
        f.Register(StringCRC("terrain"), StringCRC("flow_direction"), F());
        f.Register(StringCRC("biome"), StringCRC("tag.cold"), B());
    }

    Json::Value Parse(const std::string& text)
    {
        Json::Value v;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, v)) << text;
        return v;
    }

    std::string Contains(const char* haystack, const char* needle)
    {
        return std::strstr(haystack, needle) ? "" : std::string("missing '") + needle + "' in '" + haystack + "'";
    }

    struct FailRow
    {
        const char* label;
        const char* json;
        const char* kind;
        const char* item;
    };

    const char* kSpecExample = R"({
      "derived": {
        "dist_river":    { "distance": { "from": "river" } },
        "near_crossing": { "upstream": { "of": "crossing", "maxSteps": 6 } }
      },
      "selectors": {
        "river":    { "op": "==", "slot": "terrain", "field": "is_river", "value": 1 },
        "crossing": { "op": "==", "slot": "terrain", "field": "is_river", "value": 1 },
        "wet_low": { "op": "and", "conditions": [
            { "op": "<",  "slot": "terrain", "field": "elevation", "value": 0.2 },
            { "op": "<=", "slot": "derived", "field": "dist_river", "value": 3 },
            { "op": "not", "condition": { "op": "==", "slot": "biome", "field": "tag.cold", "value": 1 } } ] }
      }
    })";
}

TEST(CellSelector, SpecExampleLoads)
{
    WorldCellFields fields; RegisterFields(fields);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kSpecExample), fields, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_EQ(set.SelectorCount(), 4u);   // all + river + crossing + wet_low
    EXPECT_EQ(set.DerivedCount(), 2u);
    EXPECT_GE(set.FindSelector(StringCRC("wet_low")), 0);
    EXPECT_GE(set.FindSelector(StringCRC("all")), 0);
    EXPECT_LT(set.FindSelector(StringCRC("nope")), 0);
    const int d = set.FindDerived(StringCRC("near_crossing"));
    ASSERT_GE(d, 0);
    EXPECT_EQ(set.Derived(static_cast<uint32_t>(d)).kind, DerivedKind::Upstream);
    EXPECT_EQ(set.Derived(static_cast<uint32_t>(d)).maxSteps, 6);
    EXPECT_TRUE(set.Derived(static_cast<uint32_t>(d)).sourceName == "crossing");
    const int e = set.FindDerived(StringCRC("dist_river"));
    ASSERT_GE(e, 0);
    EXPECT_EQ(set.Derived(static_cast<uint32_t>(e)).kind, DerivedKind::Distance);
    EXPECT_TRUE(set.Derived(static_cast<uint32_t>(e)).sourceName == "river");
}

TEST(CellSelector, EmptyAndAbsentBlocksLoadWithAllOnly)
{
    WorldCellFields fields; RegisterFields(fields);
    for (const char* text : { "{}", R"({"selectors":{}})", R"({"derived":{}, "selectors":{}})" })
    {
        CellSelectorSet set; SelectorError err;
        ASSERT_TRUE(set.Load(Parse(text), fields, err)) << text;
        EXPECT_EQ(set.SelectorCount(), 1u) << text;
        EXPECT_EQ(set.DerivedCount(), 0u) << text;
        EXPECT_EQ(set.FindSelector(StringCRC("all")), 0);
    }
}

TEST(CellSelector, AllUsableAsDerivedFrom)
{
    WorldCellFields fields; RegisterFields(fields);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({"derived":{"d":{"distance":{"from":"all"}}}})"), fields, err)) << err.reason.AsCStr();
    EXPECT_EQ(set.DerivedCount(), 1u);
}

TEST(CellSelector, ExactlyAtCapsLoad)
{
    WorldCellFields fields; RegisterFields(fields);
    std::string sel = R"({"selectors":{)";
    for (int i = 0; i < CellSelectorSet::kMaxSelectors; ++i)
        sel += std::string(i ? "," : "") + "\"s" + std::to_string(i) + R"(":{"op":"==","slot":"terrain","field":"is_river","value":1})";
    sel += "}, \"derived\":{";
    for (int i = 0; i < CellSelectorSet::kMaxDerived; ++i)
        sel += std::string(i ? "," : "") + "\"d" + std::to_string(i) + R"(":{"distance":{"from":"s0"}})";
    sel += "}}";
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(sel), fields, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_EQ(set.SelectorCount(), CellSelectorSet::kMaxSelectors + 1u);   // all not counted against the cap
    EXPECT_EQ(set.DerivedCount(), static_cast<uint32_t>(CellSelectorSet::kMaxDerived));
}

TEST(CellSelector, OverCapsFail)
{
    WorldCellFields fields; RegisterFields(fields);
    {
        std::string sel = R"({"selectors":{)";
        for (int i = 0; i < CellSelectorSet::kMaxSelectors + 1; ++i)
            sel += std::string(i ? "," : "") + "\"s" + std::to_string(i) + R"(":{"op":"==","slot":"terrain","field":"is_river","value":1})";
        sel += "}}";
        CellSelectorSet set; SelectorError err;
        EXPECT_FALSE(set.Load(Parse(sel), fields, err));
        EXPECT_TRUE(err.kind == "selectors");
        EXPECT_TRUE(err.item == "selectors");
        EXPECT_EQ(set.SelectorCount(), 1u);
    }
    {
        std::string sel = R"({"derived":{)";
        for (int i = 0; i < CellSelectorSet::kMaxDerived + 1; ++i)
            sel += std::string(i ? "," : "") + "\"d" + std::to_string(i) + R"(":{"distance":{"from":"all"}})";
        sel += "}}";
        CellSelectorSet set; SelectorError err;
        EXPECT_FALSE(set.Load(Parse(sel), fields, err));
        EXPECT_TRUE(err.kind == "derived");
        EXPECT_TRUE(err.item == "derived");
        EXPECT_EQ(set.DerivedCount(), 0u);
    }
}

TEST(CellSelector, FailureRowsNameTheItem)
{
    WorldCellFields fields; RegisterFields(fields);
    const FailRow rows[] = {
        { "unknown slot",
          R"({"selectors":{"bad":{"op":"==","slot":"nope","field":"is_river","value":1}}})", "selector", "bad" },
        { "unknown field",
          R"({"selectors":{"bad":{"op":"==","slot":"terrain","field":"nope","value":1}}})", "selector", "bad" },
        { "selector references unknown derived",
          R"({"selectors":{"bad":{"op":"<","slot":"derived","field":"ghost","value":1}}})", "selector", "bad" },
        { "unknown field nested in and/not",
          R"({"selectors":{"deep":{"op":"and","conditions":[{"op":"not","condition":{"op":"==","slot":"terrain","field":"zzz","value":1}}]}}})", "selector", "deep" },
        { "derived from unknown selector",
          R"({"derived":{"d":{"distance":{"from":"ghost"}}}})", "derived", "d" },
        { "derived of unknown selector",
          R"({"derived":{"u":{"upstream":{"of":"ghost","maxSteps":3}}}})", "derived", "u" },
        { "2-cycle",
          R"({"derived":{"d":{"distance":{"from":"s"}}},
              "selectors":{"s":{"op":"<","slot":"derived","field":"d","value":2}}})", "derived", "d" },
        { "self through all is fine but 4-cycle D1-S1-D2-S2-D1 fails",
          R"({"derived":{"d1":{"distance":{"from":"s1"}},"d2":{"distance":{"from":"s2"}}},
              "selectors":{"s1":{"op":"<","slot":"derived","field":"d2","value":2},
                           "s2":{"op":"<","slot":"derived","field":"d1","value":2}}})", "derived", "d1" },
        { "maxSteps 0",
          R"({"derived":{"u":{"upstream":{"of":"all","maxSteps":0}}}})", "derived", "u" },
        { "maxSteps -1",
          R"({"derived":{"u":{"upstream":{"of":"all","maxSteps":-1}}}})", "derived", "u" },
        { "maxSteps missing",
          R"({"derived":{"u":{"upstream":{"of":"all"}}}})", "derived", "u" },
        { "maxSteps non-integer",
          R"({"derived":{"u":{"upstream":{"of":"all","maxSteps":"six"}}}})", "derived", "u" },
        { "unsupported kind",
          R"({"derived":{"w":{"teleport":{"from":"all"}}}})", "derived", "w" },
        { "derived entry not an object",
          R"({"derived":{"w":5}})", "derived", "w" },
        { "derived from missing",
          R"({"derived":{"w":{"distance":{}}}})", "derived", "w" },
        { "selectors not an object",
          R"({"selectors":[1,2]})", "selectors", "selectors" },
        { "derived not an object",
          R"({"derived":3})", "derived", "derived" },
        { "block not an object",
          R"([])", "block", "block" },
        { "selector expr malformed",
          R"({"selectors":{"m":{"op":"bogus"}}})", "selector", "m" },
        { "reserved name all",
          R"({"selectors":{"all":{"op":"==","slot":"terrain","field":"is_river","value":1}}})", "selector", "all" },
    };
    for (const FailRow& row : rows)
    {
        CellSelectorSet set; SelectorError err;
        EXPECT_FALSE(set.Load(Parse(row.json), fields, err)) << row.label;
        EXPECT_TRUE(err.kind == row.kind) << row.label << " kind=" << err.kind.AsCStr();
        EXPECT_TRUE(err.item == row.item) << row.label << " item=" << err.item.AsCStr();
        EXPECT_GT(err.reason.Length(), 0u) << row.label;
    }
}

TEST(CellSelector, FailureReasonsNameTheCulprit)
{
    WorldCellFields fields; RegisterFields(fields);
    {
        CellSelectorSet set; SelectorError err;
        ASSERT_FALSE(set.Load(Parse(R"({"derived":{"d":{"distance":{"from":"ghost"}}}})"), fields, err));
        EXPECT_EQ(Contains(err.reason.AsCStr(), "ghost"), "");
    }
    {
        CellSelectorSet set; SelectorError err;
        ASSERT_FALSE(set.Load(Parse(R"({"derived":{"d":{"distance":{"from":"s"}}},
            "selectors":{"s":{"op":"<","slot":"derived","field":"d","value":2}}})"), fields, err));
        EXPECT_EQ(Contains(err.reason.AsCStr(), "cycle"), "");
        EXPECT_EQ(Contains(err.reason.AsCStr(), "s"), "");
    }
}

TEST(CellSelector, FailureLeavesPreviousSetUnchanged)
{
    WorldCellFields fields; RegisterFields(fields);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kSpecExample), fields, err));
    EXPECT_FALSE(set.Load(Parse(R"({"selectors":{"bad":{"op":"==","slot":"nope","field":"x","value":1}}})"), fields, err));
    EXPECT_EQ(set.SelectorCount(), 4u);
    EXPECT_EQ(set.DerivedCount(), 2u);
    EXPECT_GE(set.FindSelector(StringCRC("wet_low")), 0);
    EXPECT_LT(set.FindSelector(StringCRC("bad")), 0);
    // Success replaces.
    ASSERT_TRUE(set.Load(Parse("{}"), fields, err));
    EXPECT_EQ(set.SelectorCount(), 1u);
    EXPECT_EQ(set.DerivedCount(), 0u);
}

TEST(CellSelector, DependencyOrderPutsDependenciesFirst)
{
    WorldCellFields fields; RegisterFields(fields);
    // b's source selector (sb) uses derived a. Names chosen so alphabetical order would put "a" first anyway; so use
    // z as the dependency and a as the dependent to prove ordering is by dependency, not name.
    const char* json = R"({
      "derived": { "a_dep": { "distance": { "from": "sa" } }, "z_base": { "distance": { "from": "river" } } },
      "selectors": {
        "river": { "op": "==", "slot": "terrain", "field": "is_river", "value": 1 },
        "sa":    { "op": "<=", "slot": "derived", "field": "z_base", "value": 3 }
      }})";
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(json), fields, err)) << err.reason.AsCStr();
    ASSERT_EQ(set.DerivedCount(), 2u);
    const int a = set.FindDerived(StringCRC("a_dep"));
    const int z = set.FindDerived(StringCRC("z_base"));
    ASSERT_GE(a, 0); ASSERT_GE(z, 0);
    int posA = -1, posZ = -1;
    for (uint32_t i = 0; i < set.DerivedCount(); ++i)
    {
        const uint32_t idx = set.DerivedOrder(i);
        if (static_cast<int>(idx) == a) posA = static_cast<int>(i);
        if (static_cast<int>(idx) == z) posZ = static_cast<int>(i);
    }
    EXPECT_LT(posZ, posA);
    EXPECT_TRUE(set.Derived(static_cast<uint32_t>(a)).DependsOn(static_cast<uint32_t>(z)));
    EXPECT_FALSE(set.Derived(static_cast<uint32_t>(z)).DependsOn(static_cast<uint32_t>(a)));
}
