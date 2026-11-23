#include <gtest/gtest.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using Dia::Core::StringCRC;
    using Draoi::HexWorld::FieldReader;
    using Draoi::HexWorld::WorldCellFields;

    constexpr uint32_t kCols = 6;
    constexpr uint32_t kRows = 4;
    constexpr uint32_t kCells = kCols * kRows; // 24

    // Fixture data: elevation[i] = i (0..23); is_river true when i % 5 == 0 (0, 5, 10, 15, 20).
    struct Fixture
    {
        float elevation[kCells];
        bool  isRiver[kCells];
        Fixture()
        {
            for (uint32_t i = 0; i < kCells; ++i)
            {
                elevation[i] = static_cast<float>(i);
                isRiver[i]   = (i % 5) == 0;
            }
        }
    };

    float ReadFloat(const void* ctx, uint32_t cell) { return static_cast<const float*>(ctx)[cell]; }
    bool  ReadBool (const void* ctx, uint32_t cell) { return static_cast<const bool*>(ctx)[cell]; }

    FieldReader FloatReader(const float* data) { FieldReader r{}; r.getFloat = &ReadFloat; r.ctx = data; return r; }
    FieldReader BoolReader (const bool* data)  { FieldReader r{}; r.getBool  = &ReadBool;  r.ctx = data; return r; }

    const StringCRC kTerrain("terrain");
    const StringCRC kElevation("elevation");
    const StringCRC kIsRiver("is_river");

    void RegisterFixture(WorldCellFields& f, const Fixture& fx)
    {
        f.Register(kTerrain, kElevation, FloatReader(fx.elevation));
        f.Register(kTerrain, kIsRiver, BoolReader(fx.isRiver));
    }
}

TEST(CellFields, RegisteredFieldsReadPerCell)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    for (uint32_t i = 0; i < kCells; ++i)
    {
        f.SetCell(i);
        EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), static_cast<float>(i));
        EXPECT_EQ(f.GetBool(kTerrain, kIsRiver), (i % 5) == 0);
    }
}

TEST(CellFields, ConditionExprEvaluatesToHandComputedCellSet)
{
    // elevation < 12 selects cells 0..11. is_river is true at 0, 5, 10, 15, 20.
    // Intersection = {0, 5, 10}.
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(R"({"op":"and","conditions":[
        {"op":"<","slot":"terrain","field":"elevation","value":12},
        {"op":"==","slot":"terrain","field":"is_river","value":true}]})", root));
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
    ASSERT_TRUE(expr.IsValid());
    EXPECT_TRUE(expr.Validate(f, errors));

    uint32_t hits[kCells];
    uint32_t count = 0;
    for (uint32_t i = 0; i < kCells; ++i)
    {
        f.SetCell(i);
        if (expr.Evaluate(f)) hits[count++] = i;
    }
    ASSERT_EQ(count, 3u);
    EXPECT_EQ(hits[0], 0u);
    EXPECT_EQ(hits[1], 5u);
    EXPECT_EQ(hits[2], 10u);
}

TEST(CellFields, HasAgreesWithRegistration)
{
    Fixture fx;
    WorldCellFields f;
    EXPECT_FALSE(f.Has(kTerrain, kElevation));
    RegisterFixture(f, fx);
    EXPECT_TRUE(f.Has(kTerrain, kElevation));
    EXPECT_TRUE(f.Has(kTerrain, kIsRiver));
    EXPECT_FALSE(f.Has(kTerrain, StringCRC("nope")));
    EXPECT_FALSE(f.Has(StringCRC("other"), kElevation));
}

TEST(CellFields, UnknownPairReturnsZeroAndFalse)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    f.SetCell(7);
    EXPECT_EQ(f.GetFloat(StringCRC("other"), StringCRC("x")), 0.0f);
    EXPECT_FALSE(f.GetBool(StringCRC("other"), StringCRC("x")));
    // Readers fall back to each other: cell 7 has elevation 7 (non-zero -> true) and is_river false (-> 0).
    // (Old behaviour: a float-only field read as bool was always false; a bool-only field read as float was always 0.)
    EXPECT_TRUE(f.GetBool(kTerrain, kElevation));
    EXPECT_EQ(f.GetFloat(kTerrain, kIsRiver), 0.0f);
}

TEST(CellFields, BoolOnlyFieldReadViaGetFloatGivesOneOrZero)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    for (uint32_t i = 0; i < kCells; ++i)
    {
        f.SetCell(i);
        EXPECT_EQ(f.GetFloat(kTerrain, kIsRiver), (i % 5) == 0 ? 1.0f : 0.0f) << i;
    }
}

TEST(CellFields, FloatOnlyFieldReadViaGetBoolIsNonZero)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    for (uint32_t i = 0; i < kCells; ++i)
    {
        f.SetCell(i);
        EXPECT_EQ(f.GetBool(kTerrain, kElevation), i != 0) << i;
    }
}

namespace
{
    float FixedFloat(const void*, uint32_t) { return 5.0f; }
    bool  FixedBool (const void*, uint32_t) { return false; }
    float NaNFloat  (const void*, uint32_t) { return std::numeric_limits<float>::quiet_NaN(); }
}

TEST(CellFields, PrimaryGetterWinsWhenBothSet)
{
    WorldCellFields f;
    FieldReader r{}; r.getFloat = &FixedFloat; r.getBool = &FixedBool;
    f.Register(kTerrain, StringCRC("both"), r);
    f.SetCell(0);
    EXPECT_EQ(f.GetFloat(kTerrain, StringCRC("both")), 5.0f);   // not 0 from getBool
    EXPECT_FALSE(f.GetBool(kTerrain, StringCRC("both")));       // not true from getFloat
}

TEST(CellFields, NaNFloatReadAsBoolIsFalse)
{
    // Documented: NaN is treated as false by the float->bool fallback.
    WorldCellFields f;
    FieldReader r{}; r.getFloat = &NaNFloat;
    f.Register(kTerrain, StringCRC("nan"), r);
    f.SetCell(0);
    EXPECT_FALSE(f.GetBool(kTerrain, StringCRC("nan")));
}

TEST(CellFields, ClearRemovesAllAndReRegisterWorks)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    f.Clear();
    EXPECT_FALSE(f.Has(kTerrain, kElevation));
    EXPECT_FALSE(f.Has(kTerrain, kIsRiver));
    f.SetCell(3);
    EXPECT_EQ(f.GetFloat(kTerrain, kElevation), 0.0f);

    RegisterFixture(f, fx);
    EXPECT_TRUE(f.Has(kTerrain, kElevation));
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 3.0f);
}

TEST(CellFields, ReRegisterSamePairReplacesReader)
{
    // Chosen behaviour: last registration wins; the registry does not grow a duplicate entry.
    Fixture fx;
    float other[kCells];
    for (uint32_t i = 0; i < kCells; ++i) other[i] = 100.0f + static_cast<float>(i);

    WorldCellFields f;
    f.Register(kTerrain, kElevation, FloatReader(fx.elevation));
    f.Register(kTerrain, kElevation, FloatReader(other));
    f.SetCell(2);
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 102.0f);
}

TEST(CellFields, ReadsForbiddenFlagFalseAllowsReads)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    bool forbidden = false;
    f.SetReadsForbiddenFlag(&forbidden);
    f.SetCell(4);
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 4.0f);
    EXPECT_FALSE(f.GetBool(kTerrain, kIsRiver));
}

#ifdef _DEBUG
TEST(CellFields, ReadsForbiddenFlagTrueAssertsInDebug)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    bool forbidden = false;
    f.SetReadsForbiddenFlag(&forbidden);
    f.SetCell(4);
    forbidden = true;
    EXPECT_DEATH(f.GetFloat(kTerrain, kElevation), "");
}
#endif

// ---- D12 edge tests ----

namespace
{
    Dia::Condition::ConditionExpr LoadExpr(const char* json)
    {
        Json::Value root;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(json, root));
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
        EXPECT_TRUE(expr.IsValid());
        return expr;
    }

    std::vector<uint32_t> Select(WorldCellFields& f, const Dia::Condition::ConditionExpr& expr, uint32_t cells)
    {
        std::vector<uint32_t> hits;
        for (uint32_t i = 0; i < cells; ++i)
        {
            f.SetCell(i);
            if (expr.Evaluate(f)) hits.push_back(i);
        }
        return hits;
    }
}

TEST(CellFields, ExactlyCapacityRegistrationsAllWorkAndReplaceAtCapacityIsSafe)
{
    // kMaxFields (128) distinct pairs fit. The 129th would assert in Debug, so it is not exercised here.
    static float values[WorldCellFields::kMaxFields];
    WorldCellFields f;
    for (uint32_t i = 0; i < WorldCellFields::kMaxFields; ++i)
    {
        values[i] = static_cast<float>(i) + 0.5f;
        f.Register(StringCRC("cap"), StringCRC(("f" + std::to_string(i)).c_str()), FloatReader(&values[i]));
    }
    for (uint32_t i = 0; i < WorldCellFields::kMaxFields; ++i)
        EXPECT_TRUE(f.Has(StringCRC("cap"), StringCRC(("f" + std::to_string(i)).c_str()))) << i;

    // Replacing an existing pair while full is not an overflow: last registration wins, no assert.
    static float replacement = 999.0f;
    f.Register(StringCRC("cap"), StringCRC("f127"), FloatReader(&replacement));
    f.SetCell(0);
    EXPECT_FLOAT_EQ(f.GetFloat(StringCRC("cap"), StringCRC("f127")), 999.0f);
    EXPECT_FLOAT_EQ(f.GetFloat(StringCRC("cap"), StringCRC("f0")), 0.5f);
    EXPECT_FLOAT_EQ(f.GetFloat(StringCRC("cap"), StringCRC("f64")), 64.5f);

    // Clear frees every slot again.
    f.Clear();
    EXPECT_FALSE(f.Has(StringCRC("cap"), StringCRC("f0")));
    f.Register(StringCRC("cap"), StringCRC("again"), FloatReader(&replacement));
    EXPECT_TRUE(f.Has(StringCRC("cap"), StringCRC("again")));
}

TEST(CellFields, ClearThenReRegisterWithNewDataReflectsNewData)
{
    Fixture fx;
    float other[kCells];
    for (uint32_t i = 0; i < kCells; ++i) other[i] = 1000.0f + static_cast<float>(i);
    WorldCellFields f;
    f.Register(kTerrain, kElevation, FloatReader(fx.elevation));
    f.SetCell(5);
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 5.0f);
    f.Clear();
    f.Register(kTerrain, kElevation, FloatReader(other));
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 1005.0f);   // cell cursor survives Clear
    EXPECT_EQ(f.GetCell(), 5u);
}

TEST(CellFields, ReaderWithNoGettersIsRegisteredButReadsZeroAndFalse)
{
    WorldCellFields f;
    f.Register(kTerrain, StringCRC("empty"), FieldReader{});
    EXPECT_TRUE(f.Has(kTerrain, StringCRC("empty")));
    f.SetCell(3);
    EXPECT_EQ(f.GetFloat(kTerrain, StringCRC("empty")), 0.0f);
    EXPECT_FALSE(f.GetBool(kTerrain, StringCRC("empty")));
}

TEST(CellFields, HasIsNotGatedByReadsForbiddenFlag)
{
    // Has is registration metadata, not a committed-state read: it must work while reads are forbidden (Load-time validation).
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    bool forbidden = true;
    f.SetReadsForbiddenFlag(&forbidden);
    EXPECT_TRUE(f.Has(kTerrain, kElevation));
    EXPECT_FALSE(f.Has(kTerrain, StringCRC("nope")));
    f.SetReadsForbiddenFlag(nullptr);
    f.SetCell(2);
    EXPECT_FLOAT_EQ(f.GetFloat(kTerrain, kElevation), 2.0f);   // nullptr disables the check
}

TEST(CellFields, ValidateAgainstWorldCellFieldsNamesUnknownPairs)
{
    // AC 16: Validate(const IConditionContext&) rejects unknown pairs through Has, naming them; WorldCellFields overrides Has.
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);

    {
        auto known = LoadExpr(R"({"op":"<","slot":"terrain","field":"elevation","value":3})");
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        EXPECT_TRUE(known.Validate(f, errors));
        EXPECT_EQ(errors.Size(), 0u);
    }
    {
        auto bad = LoadExpr(R"({"op":"and","conditions":[
            {"op":"<","slot":"terrain","field":"elevation","value":3},
            {"op":"==","slot":"terrain","field":"nope","value":true},
            {"op":"<","slot":"ghost","field":"elevation","value":1}]})");
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        EXPECT_FALSE(bad.Validate(f, errors));
        ASSERT_EQ(errors.Size(), 2u);
        EXPECT_NE(std::strstr(errors[0u], "terrain.nope"), nullptr);
        EXPECT_NE(std::strstr(errors[1u], "ghost.elevation"), nullptr);
    }
    {
        // After Clear the previously valid expression no longer validates.
        auto known = LoadExpr(R"({"op":"<","slot":"terrain","field":"elevation","value":3})");
        f.Clear();
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        EXPECT_FALSE(known.Validate(f, errors));
        EXPECT_EQ(errors.Size(), 1u);
    }
}

TEST(CellFields, NaNAndInfinityFloatsFollowIeeeComparisons)
{
    // Cells: 0 = 1.0, 1 = NaN, 2 = +inf, 3 = -inf, 4 = 7.0.
    // IEEE: every ordered comparison and == with NaN is false, so a selector on those ops excludes the NaN cell.
    // Documented consequence: != (and not(<)) is TRUE for NaN, so those selectors INCLUDE the NaN cell.
    static float data[5] = { 1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity(), 7.0f };
    WorldCellFields f;
    f.Register(kTerrain, kElevation, FloatReader(data));
    using V = std::vector<uint32_t>;

    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"<","slot":"terrain","field":"elevation","value":5})"), 5), (V{ 0, 3 }));
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"<=","slot":"terrain","field":"elevation","value":5})"), 5), (V{ 0, 3 }));
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":">","slot":"terrain","field":"elevation","value":5})"), 5), (V{ 2, 4 }));
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":">=","slot":"terrain","field":"elevation","value":5})"), 5), (V{ 2, 4 }));
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"==","slot":"terrain","field":"elevation","value":7})"), 5), (V{ 4 }));
    // NaN != x is true.
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"!=","slot":"terrain","field":"elevation","value":7})"), 5), (V{ 0, 1, 2, 3 }));
    // not(<) includes NaN (documented).
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"not","condition":{"op":"<","slot":"terrain","field":"elevation","value":5}})"), 5),
              (V{ 1, 2, 4 }));
    // Bounded interval excludes NaN and both infinities.
    EXPECT_EQ(Select(f, LoadExpr(R"({"op":"and","conditions":[
        {"op":">=","slot":"terrain","field":"elevation","value":0},
        {"op":"<=","slot":"terrain","field":"elevation","value":10}]})"), 5), (V{ 0, 4 }));
}

TEST(CellFields, NumericValueOnBoolOnlyFieldSelectsSameCellsAsTrue)
{
    Fixture fx;
    WorldCellFields f;
    RegisterFixture(f, fx);
    using V = std::vector<uint32_t>;
    const V asTrue = Select(f, LoadExpr(R"({"op":"==","slot":"terrain","field":"is_river","value":true})"), kCells);
    const V asOne  = Select(f, LoadExpr(R"({"op":"==","slot":"terrain","field":"is_river","value":1})"), kCells);
    EXPECT_EQ(asTrue, (V{ 0, 5, 10, 15, 20 }));
    EXPECT_EQ(asOne, asTrue);
    const V asZero = Select(f, LoadExpr(R"({"op":"==","slot":"terrain","field":"is_river","value":0})"), kCells);
    EXPECT_EQ(asZero.size(), kCells - 5u);
}
