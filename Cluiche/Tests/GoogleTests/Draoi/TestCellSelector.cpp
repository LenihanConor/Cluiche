#include <gtest/gtest.h>

#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/CellSelector.h>
#include <DiaPathfinding/HexTopology.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <chrono>
#include <cstring>
#include <initializer_list>
#include <limits>
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

// ---------------------------------------------------------------------------------------------------------------------
// Resolve (D8). Hand-built 6 x 4 world, 24 cells (index = row * 6 + col).
// elevation[i] = i; is_river = {2,3,8,14,15,20}; tag.cold = {0,1,2,3,4,5}.
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
    constexpr uint32_t kResolveCells = 24;

    float ArrFloat(const void* ctx, uint32_t cell) { return static_cast<const float*>(ctx)[cell]; }
    bool  ArrBool (const void* ctx, uint32_t cell) { return static_cast<const bool*>(ctx)[cell]; }

    struct SmallWorld
    {
        float elevation[kResolveCells];
        bool  river[kResolveCells] = {};
        bool  cold[kResolveCells] = {};
        WorldCellFields fields;

        SmallWorld()
        {
            for (uint32_t i = 0; i < kResolveCells; ++i) elevation[i] = static_cast<float>(i);
            for (uint32_t i : { 2u, 3u, 8u, 14u, 15u, 20u }) river[i] = true;
            for (uint32_t i = 0; i < 6; ++i) cold[i] = true;
            FieldReader fe{}; fe.getFloat = &ArrFloat; fe.ctx = elevation;
            FieldReader fr{}; fr.getBool = &ArrBool; fr.ctx = river;
            FieldReader fc{}; fc.getBool = &ArrBool; fc.ctx = cold;
            fields.Register(StringCRC("terrain"), StringCRC("elevation"), fe);
            fields.Register(StringCRC("terrain"), StringCRC("is_river"), fr);
            fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), F());
            fields.Register(StringCRC("biome"), StringCRC("tag.cold"), fc);
        }
    };

    const char* kResolveBlock = R"({
      "selectors": {
        "low":           { "op": "<",  "slot": "terrain", "field": "elevation", "value": 5 },
        "river":         { "op": "==", "slot": "terrain", "field": "is_river", "value": true },
        "low_and_river": { "op": "and", "conditions": [
            { "op": "<",  "slot": "terrain", "field": "elevation", "value": 5 },
            { "op": "==", "slot": "terrain", "field": "is_river", "value": true } ] },
        "low_or_river":  { "op": "or", "conditions": [
            { "op": "<",  "slot": "terrain", "field": "elevation", "value": 5 },
            { "op": "==", "slot": "terrain", "field": "is_river", "value": true } ] },
        "not_cold":      { "op": "not", "condition": { "op": "==", "slot": "biome", "field": "tag.cold", "value": true } },
        "none":          { "op": "<",  "slot": "terrain", "field": "elevation", "value": -1 },
        "everything":    { "op": ">=", "slot": "terrain", "field": "elevation", "value": 0 }
      }})";

    bool ListEquals(const CellSelectorSet& set, const char* name, std::initializer_list<uint32_t> expected)
    {
        const int s = set.FindSelector(StringCRC(name));
        if (s < 0) return false;
        const uint32_t n = set.ResolvedCount(static_cast<uint32_t>(s));
        if (n != expected.size()) return false;
        uint32_t k = 0;
        for (uint32_t e : expected)
            if (set.ResolvedCells(static_cast<uint32_t>(s))[k++] != e) return false;
        return true;
    }
}

TEST(CellSelector, ResolveLeafAndOrNotAllNoneEverything)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err)) << err.reason.AsCStr();

    EXPECT_TRUE(ListEquals(set, "low", { 0, 1, 2, 3, 4 }));
    EXPECT_TRUE(ListEquals(set, "river", { 2, 3, 8, 14, 15, 20 }));
    EXPECT_TRUE(ListEquals(set, "low_and_river", { 2, 3 }));
    EXPECT_TRUE(ListEquals(set, "low_or_river", { 0, 1, 2, 3, 4, 8, 14, 15, 20 }));
    EXPECT_TRUE(ListEquals(set, "not_cold", { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 }));
    EXPECT_TRUE(ListEquals(set, "everything", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 }));
    EXPECT_TRUE(ListEquals(set, "all", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 }));

    const int none = set.FindSelector(StringCRC("none"));
    ASSERT_GE(none, 0);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(none)), 0u);
    EXPECT_NE(set.ResolvedCells(static_cast<uint32_t>(none)), nullptr);   // safe to hold, count 0
}

TEST(CellSelector, ResolveListsAreStrictlyIncreasing)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err));
    for (uint32_t s = 0; s < set.SelectorCount(); ++s)
        for (uint32_t k = 1; k < set.ResolvedCount(s); ++k)
            EXPECT_LT(set.ResolvedCells(s)[k - 1], set.ResolvedCells(s)[k]) << set.SelectorName(s);
}

TEST(CellSelector, ResolveIsByteIdenticalAcrossResolvesAndSets)
{
    SmallWorld w;
    CellSelectorSet a, b; SelectorError err;
    ASSERT_TRUE(a.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(b.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(a.Resolve(w.fields, kResolveCells, err));
    ASSERT_TRUE(b.Resolve(w.fields, kResolveCells, err));
    // Snapshot a's first resolve, then resolve again.
    uint32_t snap[8][kResolveCells] = {};
    uint32_t snapCount[8] = {};
    ASSERT_LE(a.SelectorCount(), 8u);
    for (uint32_t s = 0; s < a.SelectorCount(); ++s)
    {
        snapCount[s] = a.ResolvedCount(s);
        std::memcpy(snap[s], a.ResolvedCells(s), snapCount[s] * sizeof(uint32_t));
    }
    ASSERT_TRUE(a.Resolve(w.fields, kResolveCells, err));
    for (uint32_t s = 0; s < a.SelectorCount(); ++s)
    {
        ASSERT_EQ(a.ResolvedCount(s), snapCount[s]);
        EXPECT_EQ(std::memcmp(a.ResolvedCells(s), snap[s], snapCount[s] * sizeof(uint32_t)), 0) << a.SelectorName(s);
        ASSERT_EQ(a.ResolvedCount(s), b.ResolvedCount(s));
        EXPECT_EQ(std::memcmp(a.ResolvedCells(s), b.ResolvedCells(s), a.ResolvedCount(s) * sizeof(uint32_t)), 0) << a.SelectorName(s);
    }
}

TEST(CellSelector, ResolveRereadsFieldDataAndReplacesResults)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err));
    EXPECT_TRUE(ListEquals(set, "river", { 2, 3, 8, 14, 15, 20 }));
    w.river[3] = false; w.river[21] = true;
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err));
    EXPECT_TRUE(ListEquals(set, "river", { 2, 8, 14, 15, 20, 21 }));
    EXPECT_TRUE(ListEquals(set, "low_and_river", { 2 }));
    // A smaller world replaces results too.
    ASSERT_TRUE(set.Resolve(w.fields, 10, err));
    EXPECT_TRUE(ListEquals(set, "river", { 2, 8 }));
    EXPECT_TRUE(ListEquals(set, "all", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
}

TEST(CellSelector, ResolveZeroCellsGivesEmptyLists)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, 0, err));
    for (uint32_t s = 0; s < set.SelectorCount(); ++s)
        EXPECT_EQ(set.ResolvedCount(s), 0u) << set.SelectorName(s);
}

TEST(CellSelector, ResolveWithOnlyAllSelector)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse("{}"), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err));
    EXPECT_EQ(set.ResolvedCount(0), kResolveCells);
}

// Seam for D9: derived values are not computed by the base-field Resolve, so a selector that reads slot "derived"
// must fail loudly (kind "resolve", item = selector name) and leave no resolved data.
TEST(CellSelector, ResolveRefusesSelectorsReadingDerivedFields)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({
      "derived": { "dist_river": { "distance": { "from": "river" } } },
      "selectors": {
        "river": { "op": "==", "slot": "terrain", "field": "is_river", "value": true },
        "near":  { "op": "<=", "slot": "derived", "field": "dist_river", "value": 2 } } })"), w.fields, err)) << err.reason.AsCStr();
    EXPECT_FALSE(set.Resolve(w.fields, kResolveCells, err));
    EXPECT_TRUE(err.kind == "resolve");
    EXPECT_TRUE(err.item == "near");
    EXPECT_GT(err.reason.Length(), 0u);
    const int nearSel = set.FindSelector(StringCRC("near"));
    ASSERT_GE(nearSel, 0);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(nearSel)), 0u);   // nothing resolved (no stale or wrong data)
}

TEST(CellSelector, ResolvePerfBallparkDebug)
{
    constexpr uint32_t kW = 104, kH = 64, kN = kW * kH;
    static float elev[kN];
    for (uint32_t i = 0; i < kN; ++i) elev[i] = static_cast<float>(i % 100);
    WorldCellFields fields;
    FieldReader fe{}; fe.getFloat = &ArrFloat; fe.ctx = elev;
    fields.Register(StringCRC("terrain"), StringCRC("elevation"), fe);

    std::string json = R"({"selectors":{)";
    for (int i = 0; i < CellSelectorSet::kMaxSelectors; ++i)
        json += std::string(i ? "," : "") + "\"s" + std::to_string(i) + R"(":{"op":"<","slot":"terrain","field":"elevation","value":)" + std::to_string(i * 3) + "}";
    json += "}}";
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(json), fields, err)) << err.reason.AsCStr();
    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(set.Resolve(fields, kN, err));
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 1000) << "resolve took " << ms << " ms";
    EXPECT_EQ(set.ResolvedCount(0), kN);
    // s1 is elevation < 3: values 0,1,2 of each 100-cycle. 6656 = 66*100 + 56 -> 66*3 + 3 = 201.
    const int s1 = set.FindSelector(StringCRC("s1"));
    ASSERT_GE(s1, 0);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(s1)), 201u);
}

// ---------------------------------------------------------------------------------------------------------------------
// Derived fields (D9). Resolve(fields, topology, err).
//
// Distance world: 6 x 4 flat-top odd-q hex grid, index = row * 6 + col. Rivers (seeds) at cells 0 (col 0,row 0) and
// 23 (col 5,row 3). Hex-step distance to the NEAREST river cell, computed by hand through cube coordinates
// (q = col, r = row - floor(col / 2), dist = (|dq| + |dr| + |dq + dr|) / 2), min over the two seeds:
//   row 0 (idx  0.. 5): 0 1 2 3 4 3
//   row 1 (idx  6..11): 1 2 2 3 3 2
//   row 2 (idx 12..17): 2 3 3 2 2 1
//   row 3 (idx 18..23): 3 4 3 2 1 0
// Cells 4 and 19 are 4 from both seeds; cell 5 is 3 from seed 23 but 5 from seed 0, so the min over seeds matters.
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
    using Dia::Pathfinding::HexTopology;

    constexpr uint32_t kDistCells = 24;
    const float kDistExpected[kDistCells] = {
        0, 1, 2, 3, 4, 3,
        1, 2, 2, 3, 3, 2,
        2, 3, 3, 2, 2, 1,
        3, 4, 3, 2, 1, 0 };

    struct DistWorld
    {
        float elevation[kDistCells];
        bool  river[kDistCells] = {};
        float flow[kDistCells];
        WorldCellFields fields;

        explicit DistWorld(std::initializer_list<uint32_t> rivers = { 0u, 23u })
        {
            for (uint32_t i = 0; i < kDistCells; ++i) { elevation[i] = static_cast<float>(i); flow[i] = 6.0f; }
            for (uint32_t i : rivers) river[i] = true;
            FieldReader fe{}; fe.getFloat = &ArrFloat; fe.ctx = elevation;
            FieldReader fr{}; fr.getBool = &ArrBool; fr.ctx = river;
            FieldReader ff{}; ff.getFloat = &ArrFloat; ff.ctx = flow;
            fields.Register(StringCRC("terrain"), StringCRC("elevation"), fe);
            fields.Register(StringCRC("terrain"), StringCRC("is_river"), fr);
            fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), ff);
            fields.Register(StringCRC("biome"), StringCRC("tag.cold"), B());
        }
    };

    const float kInf = std::numeric_limits<float>::infinity();

    const char* kDistBlock = R"({
      "derived": {
        "a_near":     { "distance": { "from": "near" } },
        "dist_river": { "distance": { "from": "river" } } },
      "selectors": {
        "river": { "op": "==", "slot": "terrain", "field": "is_river", "value": true },
        "near":  { "op": "<=", "slot": "derived", "field": "dist_river", "value": 1 },
        "wet_low": { "op": "and", "conditions": [
            { "op": "<",  "slot": "terrain", "field": "elevation", "value": 18 },
            { "op": "<=", "slot": "derived", "field": "dist_river", "value": 1 } ] },
        "far":   { "op": ">=", "slot": "derived", "field": "a_near", "value": 2 } } })";

    bool DerivedEquals(const CellSelectorSet& set, const char* name, const float* expected, uint32_t n)
    {
        const int d = set.FindDerived(StringCRC(name));
        if (d < 0) return false;
        const float* v = set.DerivedValues(static_cast<uint32_t>(d));
        if (v == nullptr) return false;
        for (uint32_t i = 0; i < n; ++i)
            if (!(v[i] == expected[i])) return false;   // inf == inf is true
        return true;
    }
}

TEST(CellSelector, DerivedDistanceMatchesHandComputedSteps)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, topo, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(DerivedEquals(set, "dist_river", kDistExpected, kDistCells));
    EXPECT_TRUE(ListEquals(set, "river", { 0, 23 }));
}

TEST(CellSelector, DerivedLeqOnDerivedFieldAndWithBaseField)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    // dist <= 1: {0, 1, 6, 17, 22, 23}; elevation (= index) < 18 keeps {0, 1, 6, 17}.
    EXPECT_TRUE(ListEquals(set, "near", { 0, 1, 6, 17, 22, 23 }));
    EXPECT_TRUE(ListEquals(set, "wet_low", { 0, 1, 6, 17 }));
}

TEST(CellSelector, DerivedDependingOnAnotherResolvesInOrder)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    // a_near (index 0 by name) depends on dist_river; it must still resolve correctly.
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    // Every cell with dist_river = k >= 1 lies k - 1 from the set {dist <= 1}; so a_near = max(0, dist_river - 1).
    float expected[kDistCells];
    for (uint32_t i = 0; i < kDistCells; ++i) expected[i] = kDistExpected[i] > 1.0f ? kDistExpected[i] - 1.0f : 0.0f;
    EXPECT_TRUE(DerivedEquals(set, "a_near", expected, kDistCells));
    // far: a_near >= 2 <=> dist_river >= 3.
    EXPECT_TRUE(ListEquals(set, "far", { 3, 4, 5, 9, 10, 13, 14, 18, 19, 20 }));
}

TEST(CellSelector, DerivedEmptySourceGivesInfinityAndZero)
{
    DistWorld w({});   // no river cells
    const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({
      "derived": { "d": { "distance": { "from": "river" } }, "u": { "upstream": { "of": "river", "maxSteps": 3 } } },
      "selectors": { "river": { "op": "==", "slot": "terrain", "field": "is_river", "value": true },
                     "near": { "op": "<=", "slot": "derived", "field": "d", "value": 100 } } })"), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    float inf[kDistCells], zero[kDistCells] = {};
    for (float& f : inf) f = kInf;
    EXPECT_TRUE(DerivedEquals(set, "d", inf, kDistCells));
    EXPECT_TRUE(DerivedEquals(set, "u", zero, kDistCells));
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("near")))), 0u);   // inf <= 100 is false
}

TEST(CellSelector, DerivedResolveIsByteIdenticalAcrossResolves)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    float snapA[kDistCells], snapD[kDistCells];
    const uint32_t dA = static_cast<uint32_t>(set.FindDerived(StringCRC("a_near")));
    const uint32_t dD = static_cast<uint32_t>(set.FindDerived(StringCRC("dist_river")));
    std::memcpy(snapA, set.DerivedValues(dA), sizeof(snapA));
    std::memcpy(snapD, set.DerivedValues(dD), sizeof(snapD));
    uint32_t lists[8][kDistCells] = {}; uint32_t counts[8] = {};
    ASSERT_LE(set.SelectorCount(), 8u);
    for (uint32_t s = 0; s < set.SelectorCount(); ++s)
    {
        counts[s] = set.ResolvedCount(s);
        std::memcpy(lists[s], set.ResolvedCells(s), counts[s] * sizeof(uint32_t));
    }
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    EXPECT_EQ(std::memcmp(snapA, set.DerivedValues(dA), sizeof(snapA)), 0);
    EXPECT_EQ(std::memcmp(snapD, set.DerivedValues(dD), sizeof(snapD)), 0);
    for (uint32_t s = 0; s < set.SelectorCount(); ++s)
    {
        ASSERT_EQ(counts[s], set.ResolvedCount(s));
        EXPECT_EQ(std::memcmp(lists[s], set.ResolvedCells(s), counts[s] * sizeof(uint32_t)), 0) << set.SelectorName(s);
    }
}

TEST(CellSelector, DerivedOldResolveOverloadStillRefuses)
{
    DistWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    EXPECT_FALSE(set.Resolve(w.fields, kDistCells, err));
    EXPECT_TRUE(err.kind == "resolve");
}

TEST(CellSelector, DerivedSelectorsWithoutDerivedLeavesResolveWithTopology)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    EXPECT_TRUE(ListEquals(set, "low", { 0, 1, 2, 3, 4 }));
    EXPECT_TRUE(ListEquals(set, "river", { 0, 23 }));
    EXPECT_EQ(set.ResolvedCount(0), kDistCells);
}

// ---------------------------------------------------------------------------------------------------------------------
// Upstream: 3 x 3 flow tree, index = row * 3 + col, flow_direction = hex direction index 0..5 (0=NE 1=N 2=NW 3=SW 4=S
// 5=SE), 6 = no outflow. Cell 0 is the sink. Flow (cell -> outflow neighbour, direction):
//   1->0 (2)  2->1 (3)  3->0 (1)  4->3 (2)  5->4 (3)  6->3 (1)  7->6 (2)  8->7 (3)
// Steps along flow to cell 0: 0:0 1:1 2:2 3:1 4:2 5:3 6:2 7:3 8:4. Steps along flow to cell 3: 3:0 4:1 6:1 5:2 7:2 8:3
// (cells 0, 1, 2 never reach 3).
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
    struct TreeWorld
    {
        float flow[9] = { 6, 2, 3, 1, 2, 3, 1, 2, 3 };
        bool  match[9] = {};
        WorldCellFields fields;

        explicit TreeWorld(uint32_t matchCell)
        {
            match[matchCell] = true;
            FieldReader fr{}; fr.getBool = &ArrBool; fr.ctx = match;
            FieldReader ff{}; ff.getFloat = &ArrFloat; ff.ctx = flow;
            fields.Register(StringCRC("terrain"), StringCRC("is_river"), fr);
            fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), ff);
        }
    };

    bool UpstreamEquals(uint32_t matchCell, int maxSteps, std::initializer_list<uint32_t> inside)
    {
        TreeWorld w(matchCell);
        const HexTopology topo(3, 3);
        CellSelectorSet set; SelectorError err;
        const std::string block = std::string(R"({"derived":{"up":{"upstream":{"of":"m","maxSteps":)") + std::to_string(maxSteps) +
            R"(}}},"selectors":{"m":{"op":"==","slot":"terrain","field":"is_river","value":true},)"
            R"("in":{"op":">=","slot":"derived","field":"up","value":1}}})";
        if (!set.Load(Parse(block), w.fields, err)) return false;
        if (!set.Resolve(w.fields, topo, err)) return false;
        float expected[9] = {};
        for (uint32_t c : inside) expected[c] = 1.0f;
        if (!DerivedEquals(set, "up", expected, 9)) return false;
        return ListEquals(set, "in", inside);
    }
}

TEST(CellSelector, DerivedUpstreamWithinMaxStepsAlongFlow)
{
    EXPECT_TRUE(UpstreamEquals(0, 1, { 0, 1, 3 }));
    EXPECT_TRUE(UpstreamEquals(0, 2, { 0, 1, 2, 3, 4, 6 }));
    EXPECT_TRUE(UpstreamEquals(0, 3, { 0, 1, 2, 3, 4, 5, 6, 7 }));   // 8 is exactly 4 away: excluded at 3
    EXPECT_TRUE(UpstreamEquals(0, 4, { 0, 1, 2, 3, 4, 5, 6, 7, 8 })); // exactly at maxSteps: included
    EXPECT_TRUE(UpstreamEquals(0, 6, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }));
}

TEST(CellSelector, DerivedUpstreamFollowsFlowDirectionNotAdjacency)
{
    // Match cell 3: cell 0 is adjacent to 3 but flows nowhere, 1 and 2 drain to 0; none of 0, 1, 2 are upstream of 3.
    EXPECT_TRUE(UpstreamEquals(3, 1, { 3, 4, 6 }));
    EXPECT_TRUE(UpstreamEquals(3, 2, { 3, 4, 5, 6, 7 }));
    EXPECT_TRUE(UpstreamEquals(3, 6, { 3, 4, 5, 6, 7, 8 }));
}

TEST(CellSelector, DerivedUpstreamMatchingCellsAreStepZero)
{
    TreeWorld w(8);   // leaf: nothing flows into 8
    const HexTopology topo(3, 3);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({"derived":{"up":{"upstream":{"of":"m","maxSteps":6}}},
      "selectors":{"m":{"op":"==","slot":"terrain","field":"is_river","value":true}}})"), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    const float expected[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_TRUE(DerivedEquals(set, "up", expected, 9));
}

// ---------------------------------------------------------------------------------------------------------------------
// D12 edge tests.
// ---------------------------------------------------------------------------------------------------------------------

// `all` as a derived source: every cell is a source, so distance is 0 everywhere and every cell is upstream of itself.
TEST(CellSelector, DerivedFromAllGivesZeroDistanceAndFullUpstream)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({
      "derived": { "d": { "distance": { "from": "all" } }, "u": { "upstream": { "of": "all", "maxSteps": 1 } } },
      "selectors": {
        "here":   { "op": "<=", "slot": "derived", "field": "d", "value": 0 },
        "inside": { "op": ">=", "slot": "derived", "field": "u", "value": 1 } } })"), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, topo, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();

    float zero[kDistCells] = {}, one[kDistCells];
    for (float& f : one) f = 1.0f;
    EXPECT_TRUE(DerivedEquals(set, "d", zero, kDistCells));
    EXPECT_TRUE(DerivedEquals(set, "u", one, kDistCells));
    std::initializer_list<uint32_t> everyCell = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
    EXPECT_TRUE(ListEquals(set, "all", everyCell));
    EXPECT_TRUE(ListEquals(set, "here", everyCell));
    EXPECT_TRUE(ListEquals(set, "inside", everyCell));
}

// An empty source selector: selectors using the derived fields resolve without failure. distance is +inf (so `>` tests
// select every cell, `<=` tests none) and upstream is 0 (so `== 0` selects every cell).
TEST(CellSelector, EmptyDerivedSourceSelectorsResolveWithoutCrash)
{
    DistWorld w({});   // no river cells
    const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({
      "derived": { "d": { "distance": { "from": "river" } }, "u": { "upstream": { "of": "river", "maxSteps": 3 } } },
      "selectors": {
        "river":     { "op": "==", "slot": "terrain", "field": "is_river", "value": true },
        "far":       { "op": ">",  "slot": "derived", "field": "d", "value": 1000 },
        "unreached": { "op": "==", "slot": "derived", "field": "u", "value": 0 },
        "reached":   { "op": ">=", "slot": "derived", "field": "u", "value": 1 } } })"), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, topo, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("river")))), 0u);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("far")))), kDistCells);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("unreached")))), kDistCells);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("reached")))), 0u);
}

// WorldCellFields::Clear between two resolves: re-registered data replaces the old data.
TEST(CellSelector, ClearAndReRegisterBetweenResolvesReflectsNewData)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err));
    EXPECT_TRUE(ListEquals(set, "low", { 0, 1, 2, 3, 4 }));

    static float shifted[kResolveCells];
    for (uint32_t i = 0; i < kResolveCells; ++i) shifted[i] = static_cast<float>(i) - 10.0f;   // elevation < 5 -> 0..14
    static bool noRiver[kResolveCells] = {};
    noRiver[9] = true;
    w.fields.Clear();
    FieldReader fe{}; fe.getFloat = &ArrFloat; fe.ctx = shifted;
    FieldReader fr{}; fr.getBool = &ArrBool; fr.ctx = noRiver;
    FieldReader fc{}; fc.getBool = &ArrBool; fc.ctx = w.cold;
    w.fields.Register(StringCRC("terrain"), StringCRC("elevation"), fe);
    w.fields.Register(StringCRC("terrain"), StringCRC("is_river"), fr);
    w.fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), F());
    w.fields.Register(StringCRC("biome"), StringCRC("tag.cold"), fc);

    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err)) << err.reason.AsCStr();
    EXPECT_TRUE(ListEquals(set, "low", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 }));
    EXPECT_TRUE(ListEquals(set, "river", { 9 }));
    EXPECT_TRUE(ListEquals(set, "low_and_river", { 9 }));
    EXPECT_TRUE(ListEquals(set, "everything", { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 }));
}

// A field removed after Load (Clear without re-register): the base-field Resolve does not re-validate, so it must not
// crash or read garbage. Missing pairs read 0 / false (WorldCellFields contract), so the result is deterministic.
TEST(CellSelector, FieldsRemovedAfterLoadResolveAgainstZeroValues)
{
    SmallWorld w;
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kResolveBlock), w.fields, err));
    w.fields.Clear();
    ASSERT_TRUE(set.Resolve(w.fields, kResolveCells, err)) << err.reason.AsCStr();
    // elevation reads 0: low (< 5) and everything (>= 0) select every cell; is_river reads false: river selects none.
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("low")))), kResolveCells);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("everything")))), kResolveCells);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("river")))), 0u);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("none")))), 0u);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("not_cold")))), kResolveCells);

    // Re-registering and resolving again recovers the real answer.
    SmallWorld fresh;
    ASSERT_TRUE(set.Resolve(fresh.fields, kResolveCells, err));
    EXPECT_TRUE(ListEquals(set, "river", { 2, 3, 8, 14, 15, 20 }));
}

// With topology: an upstream field needs terrain.flow_direction. Removed after Load -> a clean "resolve" failure naming the
// derived field, with no stale results (not UB, not a half-filled pool).
TEST(CellSelector, UpstreamResolveAfterFlowFieldRemovedFailsCleanly)
{
    TreeWorld w(0);
    const HexTopology topo(3, 3);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({"derived":{"up":{"upstream":{"of":"m","maxSteps":3}}},
      "selectors":{"m":{"op":"==","slot":"terrain","field":"is_river","value":true},
                   "in":{"op":">=","slot":"derived","field":"up","value":1}}})"), w.fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    EXPECT_GT(set.ResolvedCount(static_cast<uint32_t>(set.FindSelector(StringCRC("in")))), 0u);

    w.fields.Clear();
    FieldReader fr{}; fr.getBool = &ArrBool; fr.ctx = w.match;
    w.fields.Register(StringCRC("terrain"), StringCRC("is_river"), fr);   // flow_direction deliberately not re-registered
    EXPECT_FALSE(set.Resolve(w.fields, topo, err));
    EXPECT_TRUE(err.kind == "resolve");
    EXPECT_TRUE(err.item == "up");
    EXPECT_GT(err.reason.Length(), 0u);
    for (uint32_t s = 0; s < set.SelectorCount(); ++s) EXPECT_EQ(set.ResolvedCount(s), 0u) << set.SelectorName(s);
    EXPECT_EQ(set.DerivedValues(0), nullptr);
}

// A successful Load discards earlier results (counts reset, derived values gone).
TEST(CellSelector, LoadAfterResolveClearsResolvedData)
{
    DistWorld w; const HexTopology topo(6, 4);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    EXPECT_GT(set.ResolvedCount(0), 0u);
    ASSERT_TRUE(set.Load(Parse(kDistBlock), w.fields, err));
    for (uint32_t s = 0; s < set.SelectorCount(); ++s) EXPECT_EQ(set.ResolvedCount(s), 0u) << set.SelectorName(s);
    EXPECT_EQ(set.DerivedValues(0), nullptr);
    ASSERT_TRUE(set.Resolve(w.fields, topo, err));
    EXPECT_TRUE(ListEquals(set, "river", { 0, 23 }));
}

// NaN / infinity in float fields: ordered comparisons and == exclude the cell; != includes it (IEEE; documented).
TEST(CellSelector, NaNAndInfinityInFloatFieldsFollowIeee)
{
    constexpr uint32_t n = 6;
    static float elev[n] = { 1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity(), 3.0f, 9.0f };
    WorldCellFields fields;
    FieldReader fe{}; fe.getFloat = &ArrFloat; fe.ctx = elev;
    fields.Register(StringCRC("terrain"), StringCRC("elevation"), fe);
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(Parse(R"({"selectors":{
      "lt":  { "op": "<",  "slot": "terrain", "field": "elevation", "value": 5 },
      "ge":  { "op": ">=", "slot": "terrain", "field": "elevation", "value": 5 },
      "eq":  { "op": "==", "slot": "terrain", "field": "elevation", "value": 3 },
      "ne":  { "op": "!=", "slot": "terrain", "field": "elevation", "value": 3 },
      "band": { "op": "and", "conditions": [
          { "op": ">=", "slot": "terrain", "field": "elevation", "value": 0 },
          { "op": "<=", "slot": "terrain", "field": "elevation", "value": 10 } ] } }})"), fields, err)) << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(fields, n, err));
    EXPECT_TRUE(ListEquals(set, "lt", { 0, 3, 4 }));
    EXPECT_TRUE(ListEquals(set, "ge", { 2, 5 }));
    EXPECT_TRUE(ListEquals(set, "eq", { 4 }));
    EXPECT_TRUE(ListEquals(set, "ne", { 0, 1, 2, 3, 5 }));
    EXPECT_TRUE(ListEquals(set, "band", { 0, 4, 5 }));
    EXPECT_TRUE(ListEquals(set, "all", { 0, 1, 2, 3, 4, 5 }));
}

// NaN / out-of-range flow_direction means "no outflow": the cell never drains into anything, nothing crashes.
TEST(CellSelector, UpstreamTreatsNaNInfNegativeAndTooLargeFlowAsNoOutflow)
{
    TreeWorld w(0);   // match cell 0; normal tree: everything drains to 0 eventually
    const float bad[4] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f, 5.6f };
    const HexTopology topo(3, 3);
    for (float b : bad)
    {
        for (uint32_t c = 1; c < 9; ++c) w.flow[c] = b;   // every non-sink cell has no outflow
        CellSelectorSet set; SelectorError err;
        ASSERT_TRUE(set.Load(Parse(R"({"derived":{"up":{"upstream":{"of":"m","maxSteps":6}}},
          "selectors":{"m":{"op":"==","slot":"terrain","field":"is_river","value":true}}})"), w.fields, err));
        ASSERT_TRUE(set.Resolve(w.fields, topo, err)) << b;
        const float expected[9] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };   // only the source cell itself
        EXPECT_TRUE(DerivedEquals(set, "up", expected, 9)) << b;
    }
}
