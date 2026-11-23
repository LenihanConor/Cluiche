#include <gtest/gtest.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaPathfinding/HexTopology.h>
#include <DraoiHexWorld/CellSelector.h>
#include <DraoiHexWorld/CellVocabulary.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

// D10: coverage of today's A4 Weather / A5 Hydrology selector clauses as CellSelectorSet expressions.
//
// Conventions (the reading of each clause that follows the spec text literally):
//  - bool-like fields (is_river, is_protected) register BOTH getFloat and getBool. The clauses below use the bool form
//    `"value": true` (GetBool); the numeric form `"value": 1` / `0` is exercised for `river` too.
//  - terrain.water_source is the WaterSource ordinal (None 0, Ocean 1, Lake 2, River 3), read as a float.
//  - hydro.basin_kind (fixture only; A5 does not exist): 0 = none, 1 = lake, 2 = dry. `any` is `!= none`.
//  - "water" for distanceToWater = every cell with water_source != None (A4: "every water-source cell; water cells are 0").
//  - elevation / latitude are plain [0, 1] floats, tested on binary-exact values so the inclusive bounds are exact.
//  - `crossing` is expressed as is_protected AND is_river (A5 text: an isCrossing *river* cell).
namespace
{
    using Dia::Core::StringCRC;
    using Dia::Pathfinding::HexTopology;
    using Draoi::HexWorld::CellSelectorSet;
    using Draoi::HexWorld::FieldReader;
    using Draoi::HexWorld::SelectorError;
    using Draoi::HexWorld::WaterSource;
    using Draoi::HexWorld::WorldCellFields;

    using Cells = std::vector<uint32_t>;

    float F32 (const void* ctx, uint32_t c) { return static_cast<const float*>(ctx)[c]; }
    float U8F (const void* ctx, uint32_t c) { return static_cast<float>(static_cast<const uint8_t*>(ctx)[c]); }
    bool  BoolB(const void* ctx, uint32_t c) { return static_cast<const bool*>(ctx)[c]; }
    float BoolF(const void* ctx, uint32_t c) { return static_cast<const bool*>(ctx)[c] ? 1.0f : 0.0f; }

    FieldReader FloatField(const float* a)   { FieldReader r{}; r.getFloat = &F32; r.ctx = a; return r; }
    FieldReader EnumField(const uint8_t* a)  { FieldReader r{}; r.getFloat = &U8F; r.ctx = a; return r; }
    FieldReader BoolField(const bool* a)     { FieldReader r{}; r.getFloat = &BoolF; r.getBool = &BoolB; r.ctx = a; return r; }

    Json::Value Parse(const std::string& text)
    {
        Json::Value v;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, v)) << text;
        return v;
    }

    Cells Resolved(const CellSelectorSet& set, const char* name)
    {
        const int s = set.FindSelector(StringCRC(name));
        EXPECT_GE(s, 0) << name;
        Cells out;
        if (s < 0) return out;
        for (uint32_t k = 0; k < set.ResolvedCount(static_cast<uint32_t>(s)); ++k)
            out.push_back(set.ResolvedCells(static_cast<uint32_t>(s))[k]);
        return out;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // 6 x 4 flat-top world, index = row * 6 + col, 24 cells.
    //   elevation[i] = (i % 8) / 8        -> 0, .125, .25, .375, .5, .625, .75, .875 repeating
    //   latitude[i]  = (i / 6) / 4        -> .0 / .25 / .5 / .75 by row
    //   water_source: Ocean {0}, Lake {23}, River {11, 17}, None elsewhere
    //   is_river {11, 17}; is_protected {3, 11, 22}
    //   basin_kind: lake {1, 2, 7}, dry {13, 14, 19, 20}, none elsewhere
    // Hex-step distances (cube coords q = col, r = row - floor(col / 2); d = max(|dq|, |dr|, |dq + dr|)):
    //   distance to water (sources 0, 11, 17, 23)     distance to river (sources 11, 17)
    //     row 0:  0 1 2 2 2 1                            5 4 3 2 2 1
    //     row 1:  1 2 2 2 1 0                            5 4 3 2 1 0
    //     row 2:  2 3 3 2 1 0                            5 4 3 2 1 0
    //     row 3:  3 4 3 2 1 0                            5 4 3 2 1 1
    // -----------------------------------------------------------------------------------------------------------------
    constexpr uint32_t kCells = 24;

    struct World
    {
        float   elevation[kCells];
        float   latitude[kCells];
        uint8_t waterSource[kCells] = {};
        bool    isRiver[kCells] = {};
        bool    isProtected[kCells] = {};
        uint8_t basinKind[kCells] = {};
        float   flow[kCells];
        WorldCellFields fields;

        World()
        {
            for (uint32_t i = 0; i < kCells; ++i)
            {
                elevation[i] = static_cast<float>(i % 8) / 8.0f;
                latitude[i]  = static_cast<float>(i / 6) / 4.0f;
                flow[i] = 6.0f;
            }
            waterSource[0]  = static_cast<uint8_t>(WaterSource::Ocean);
            waterSource[23] = static_cast<uint8_t>(WaterSource::Lake);
            waterSource[11] = waterSource[17] = static_cast<uint8_t>(WaterSource::River);
            isRiver[11] = isRiver[17] = true;
            for (uint32_t i : { 3u, 11u, 22u }) isProtected[i] = true;
            for (uint32_t i : { 1u, 2u, 7u }) basinKind[i] = 1;
            for (uint32_t i : { 13u, 14u, 19u, 20u }) basinKind[i] = 2;

            fields.Register(StringCRC("terrain"), StringCRC("elevation"), FloatField(elevation));
            fields.Register(StringCRC("terrain"), StringCRC("latitude"), FloatField(latitude));
            fields.Register(StringCRC("terrain"), StringCRC("water_source"), EnumField(waterSource));
            fields.Register(StringCRC("terrain"), StringCRC("is_river"), BoolField(isRiver));
            fields.Register(StringCRC("terrain"), StringCRC("is_protected"), BoolField(isProtected));
            fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), FloatField(flow));
            fields.Register(StringCRC("hydro"), StringCRC("basin_kind"), EnumField(basinKind));
        }
    };

    // One block holding every clause as a named selector (the shape a preset's selector block takes).
    const char* kBlock = R"({
      "derived": {
        "dist_water": { "distance": { "from": "water" } },
        "dist_river": { "distance": { "from": "river" } } },
      "selectors": {
        "water": { "op": "!=", "slot": "terrain", "field": "water_source", "value": 0 },
        "river": { "op": "==", "slot": "terrain", "field": "is_river", "value": true },

        "elev": { "op": "and", "conditions": [
            { "op": ">=", "slot": "terrain", "field": "elevation", "value": 0.25 },
            { "op": "<=", "slot": "terrain", "field": "elevation", "value": 0.5 } ] },
        "lat": { "op": "and", "conditions": [
            { "op": ">=", "slot": "terrain", "field": "latitude", "value": 0.25 },
            { "op": "<=", "slot": "terrain", "field": "latitude", "value": 0.5 } ] },

        "ws_ocean_lake": { "op": "or", "conditions": [
            { "op": "==", "slot": "terrain", "field": "water_source", "value": 1 },
            { "op": "==", "slot": "terrain", "field": "water_source", "value": 2 } ] },
        "ws_river": { "op": "==", "slot": "terrain", "field": "water_source", "value": 3 },
        "ws_none_ocean": { "op": "or", "conditions": [
            { "op": "==", "slot": "terrain", "field": "water_source", "value": 0 },
            { "op": "==", "slot": "terrain", "field": "water_source", "value": 1 } ] },

        "river_num": { "op": "==", "slot": "terrain", "field": "is_river", "value": 1 },
        "not_river": { "op": "not", "condition":
            { "op": "==", "slot": "terrain", "field": "is_river", "value": true } },
        "river_zero": { "op": "==", "slot": "terrain", "field": "is_river", "value": 0 },

        "prot": { "op": "==", "slot": "terrain", "field": "is_protected", "value": true },

        "basin_lake": { "op": "==", "slot": "hydro", "field": "basin_kind", "value": 1 },
        "basin_dry":  { "op": "==", "slot": "hydro", "field": "basin_kind", "value": 2 },
        "basin_any":  { "op": "!=", "slot": "hydro", "field": "basin_kind", "value": 0 },
        "basin_any_union": { "op": "or", "conditions": [
            { "op": "==", "slot": "hydro", "field": "basin_kind", "value": 1 },
            { "op": "==", "slot": "hydro", "field": "basin_kind", "value": 2 } ] },

        "dw_2_3": { "op": "and", "conditions": [
            { "op": ">=", "slot": "derived", "field": "dist_water", "value": 2 },
            { "op": "<=", "slot": "derived", "field": "dist_water", "value": 3 } ] },
        "dw_min3": { "op": ">=", "slot": "derived", "field": "dist_water", "value": 3 },
        "dr_2_4": { "op": "and", "conditions": [
            { "op": ">=", "slot": "derived", "field": "dist_river", "value": 2 },
            { "op": "<=", "slot": "derived", "field": "dist_river", "value": 4 } ] },

        "combo": { "op": "and", "conditions": [
            { "op": ">=", "slot": "terrain", "field": "elevation", "value": 0.25 },
            { "op": "<=", "slot": "terrain", "field": "elevation", "value": 0.5 },
            { "op": "or", "conditions": [
                { "op": "==", "slot": "terrain", "field": "water_source", "value": 0 },
                { "op": "==", "slot": "terrain", "field": "water_source", "value": 1 } ] },
            { "op": "<=", "slot": "derived", "field": "dist_water", "value": 2 } ] }
      }})";

    struct Resolution
    {
        World w;
        CellSelectorSet set;
        HexTopology topo{ 6, 4 };

        Resolution()
        {
            SelectorError err;
            const bool loaded = set.Load(Parse(kBlock), w.fields, err);
            EXPECT_TRUE(loaded) << err.item.AsCStr() << ": " << err.reason.AsCStr();
            const bool resolved = loaded && set.Resolve(w.fields, topo, err);
            EXPECT_TRUE(resolved) << err.item.AsCStr() << ": " << err.reason.AsCStr();
        }
    };

    struct Row { const char* selector; Cells expected; };

    void ExpectRows(const Resolution& r, std::initializer_list<Row> rows)
    {
        for (const Row& row : rows)
            EXPECT_EQ(Resolved(r.set, row.selector), row.expected) << row.selector;
    }

    Cells Range(uint32_t first, uint32_t last)
    {
        Cells c;
        for (uint32_t i = first; i <= last; ++i) c.push_back(i);
        return c;
    }
}

// elevation {min 0.25, max 0.5} inclusive: i % 8 in {2, 3, 4} -> 2,3,4, 10,11,12, 18,19,20. Cells 2 and 4 (and 10, 12,
// 18, 20) sit exactly on the min and max bounds.
TEST(SelectorClauses, ElevationInclusiveBounds)
{
    Resolution r;
    ExpectRows(r, { { "elev", { 2, 3, 4, 10, 11, 12, 18, 19, 20 } } });
}

// latitude {min 0.25, max 0.5}: rows 1 and 2 (latitude .25 and .5, both on a bound) -> cells 6..17.
TEST(SelectorClauses, LatitudeInclusiveBounds)
{
    Resolution r;
    ExpectRows(r, { { "lat", Range(6, 17) } });
}

// waterSource lists: [Ocean, Lake] -> {0, 23}; [River] -> {11, 17}; [None, Ocean] -> everything but Lake and River.
TEST(SelectorClauses, WaterSourceListAndSingle)
{
    Resolution r;
    ExpectRows(r, {
        { "ws_ocean_lake", { 0, 23 } },
        { "ws_river", { 11, 17 } },
        { "ws_none_ocean", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22 } },
        { "water", { 0, 11, 17, 23 } } });
}

// river true -> {11, 17}; river false via `not` and via `== 0` -> the other 22 cells; numeric `== 1` equals the bool form.
TEST(SelectorClauses, RiverBoolTrueAndFalse)
{
    Resolution r;
    Cells notRiver;
    for (uint32_t i = 0; i < kCells; ++i) if (i != 11 && i != 17) notRiver.push_back(i);
    ExpectRows(r, {
        { "river", { 11, 17 } },
        { "river_num", { 11, 17 } },
        { "not_river", notRiver },
        { "river_zero", notRiver } });
}

TEST(SelectorClauses, ProtectedViaIsProtected)
{
    Resolution r;
    ExpectRows(r, { { "prot", { 3, 11, 22 } } });
}

// basin lake {1, 2, 7}; dry {13, 14, 19, 20}; any = != none = lake union dry.
TEST(SelectorClauses, BasinLakeDryAny)
{
    Resolution r;
    ExpectRows(r, {
        { "basin_lake", { 1, 2, 7 } },
        { "basin_dry", { 13, 14, 19, 20 } },
        { "basin_any", { 1, 2, 7, 13, 14, 19, 20 } } });
}

TEST(SelectorClauses, AnyEqualsLakeUnionDry)
{
    Resolution r;
    Cells united = Resolved(r.set, "basin_lake");
    const Cells dry = Resolved(r.set, "basin_dry");
    united.insert(united.end(), dry.begin(), dry.end());
    std::sort(united.begin(), united.end());
    EXPECT_EQ(Resolved(r.set, "basin_any"), united);
    EXPECT_EQ(Resolved(r.set, "basin_any"), Resolved(r.set, "basin_any_union"));
    EXPECT_EQ(Resolved(r.set, "basin_any").size(), 7u);
}

// distanceToWater {2,3} from the water table: row0 2,3,4; row1 7,8,9; row2 12,13,14,15; row3 18,20,21 (19 is 4, out).
// distanceToWater >= 3 (A4 drought style): 13, 14, 18, 19, 20.
// distanceToRiver {2,4} from the river table: row0 1,2,3,4; row1 7,8,9; row2 13,14,15; row3 19,20,21.
TEST(SelectorClauses, DistanceToWaterAndRiver)
{
    Resolution r;
    ExpectRows(r, {
        { "dw_2_3", { 2, 3, 4, 7, 8, 9, 12, 13, 14, 15, 18, 20, 21 } },
        { "dw_min3", { 13, 14, 18, 19, 20 } },
        { "dr_2_4", { 1, 2, 3, 4, 7, 8, 9, 13, 14, 15, 19, 20, 21 } } });
}

// Clauses AND together: elevation {.25,.5} = {2,3,4,10,11,12,18,19,20}; waterSource [None, Ocean] removes River cell 11;
// distanceToWater <= 2 keeps 2,3,4,10,12 (18, 19, 20 are 3, 4, 3 away). Intersection = {2, 3, 4, 10, 12}.
TEST(SelectorClauses, ClausesAndCombine)
{
    Resolution r;
    ExpectRows(r, { { "combo", { 2, 3, 4, 10, 12 } } });
}

TEST(SelectorClauses, AllPredefinedSelectsEveryCell)
{
    Resolution r;
    ExpectRows(r, { { "all", Range(0, kCells - 1) } });
}

// -----------------------------------------------------------------------------------------------------------------
// upstreamOfCrossing {maxSteps}: 3 x 3 flow tree, index = row * 3 + col, flow_direction 0..5 (6 = no outflow).
//   Flow: 1->0  2->1  3->0  4->3  5->4  6->3  7->6  8->7 (cell 0 is the sink).
// crossing = is_protected AND is_river. is_river {3, 4}; is_protected {0, 3}: cell 0 is protected but not a river, so it
// is NOT a crossing; the only crossing is cell 3.
// Steps along flow to 3: 3:0 4:1 6:1 5:2 7:2 8:3 (0, 1, 2 never reach 3).
// -----------------------------------------------------------------------------------------------------------------
namespace
{
    struct TreeWorld
    {
        float flow[9] = { 6, 2, 3, 1, 2, 3, 1, 2, 3 };
        bool  isRiver[9] = {};
        bool  isProtected[9] = {};
        WorldCellFields fields;

        explicit TreeWorld(std::initializer_list<uint32_t> riverCells, std::initializer_list<uint32_t> protectedCells)
        {
            for (uint32_t c : riverCells) isRiver[c] = true;
            for (uint32_t c : protectedCells) isProtected[c] = true;
            fields.Register(StringCRC("terrain"), StringCRC("is_river"), BoolField(isRiver));
            fields.Register(StringCRC("terrain"), StringCRC("is_protected"), BoolField(isProtected));
            fields.Register(StringCRC("terrain"), StringCRC("flow_direction"), FloatField(flow));
        }
    };

    Cells UpstreamOfCrossing(TreeWorld& w, int maxSteps)
    {
        const HexTopology topo(3, 3);
        CellSelectorSet set; SelectorError err;
        const std::string block = std::string(R"({"derived":{"up":{"upstream":{"of":"crossing","maxSteps":)") +
            std::to_string(maxSteps) + R"(}}},"selectors":{
              "crossing": { "op": "and", "conditions": [
                { "op": "==", "slot": "terrain", "field": "is_protected", "value": true },
                { "op": "==", "slot": "terrain", "field": "is_river", "value": true } ] },
              "upstream_of_crossing": { "op": "==", "slot": "derived", "field": "up", "value": 1 } }})";
        EXPECT_TRUE(set.Load(Parse(block), w.fields, err)) << err.reason.AsCStr();
        EXPECT_TRUE(set.Resolve(w.fields, topo, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
        return Resolved(set, "upstream_of_crossing");
    }
}

TEST(SelectorClauses, UpstreamOfCrossingWithinMaxStepsOnly)
{
    TreeWorld w({ 3, 4 }, { 0, 3 });
    EXPECT_EQ(UpstreamOfCrossing(w, 1), (Cells{ 3, 4, 6 }));
    EXPECT_EQ(UpstreamOfCrossing(w, 2), (Cells{ 3, 4, 5, 6, 7 }));
    EXPECT_EQ(UpstreamOfCrossing(w, 3), (Cells{ 3, 4, 5, 6, 7, 8 }));
    EXPECT_EQ(UpstreamOfCrossing(w, 6), (Cells{ 3, 4, 5, 6, 7, 8 }));
}

// A terrain with no crossing (protected cells exist but none is a river): the crossing selector is empty, the derived
// field is all zero and the upstream selector resolves to the empty list.
TEST(SelectorClauses, UpstreamOfCrossingWithNoCrossingSelectsNothing)
{
    TreeWorld w({ 3, 4 }, { 0, 7 });
    EXPECT_TRUE(UpstreamOfCrossing(w, 3).empty());
    TreeWorld none({}, {});
    EXPECT_TRUE(UpstreamOfCrossing(none, 3).empty());
}
