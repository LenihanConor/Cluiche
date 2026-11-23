#include <gtest/gtest.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <cstdint>

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
    // A float-only field read as bool (and vice versa) is also 0/false.
    EXPECT_FALSE(f.GetBool(kTerrain, kElevation));
    EXPECT_EQ(f.GetFloat(kTerrain, kIsRiver), 0.0f);
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
