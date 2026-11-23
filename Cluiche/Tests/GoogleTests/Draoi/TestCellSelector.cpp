#include <gtest/gtest.h>

#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/CellSelector.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <chrono>
#include <cstring>
#include <initializer_list>
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
