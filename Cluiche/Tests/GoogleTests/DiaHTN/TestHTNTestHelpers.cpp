#include <gtest/gtest.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_TestHelpers
// Covers MockHTNContext, AssertPlanOperators, and AssertPlanFails.

namespace
{
    Dia::HTN::HTNDomain LoadDomain(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        return Dia::HTN::HTNDomain::LoadFromJson(root, errors);
    }

    constexpr const char* kDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["A"] }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";
}

TEST(DiaHTN_TestHelpers, MockHTNContext_GetFloat_ReturnsSingleValue)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("agent"), Dia::Core::StringCRC("hp"), 42.0f);
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("agent"), Dia::Core::StringCRC("hp")), 42.0f);
}

TEST(DiaHTN_TestHelpers, MockHTNContext_GetBool_ReturnsSingleValue)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive"), true);
    EXPECT_TRUE(ctx.GetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive")));
}

TEST(DiaHTN_TestHelpers, MockHTNContext_GetFloat_MissingKey_ReturnsZero)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("y")), 0.0f);
}

TEST(DiaHTN_TestHelpers, MockHTNContext_GetBool_MissingKey_ReturnsFalse)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    EXPECT_FALSE(ctx.GetBool(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("y")));
}

TEST(DiaHTN_TestHelpers, MockHTNContext_MultipleSlots_Independent)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("a"), Dia::Core::StringCRC("f"), 1.0f);
    ctx.SetFloat(Dia::Core::StringCRC("b"), Dia::Core::StringCRC("f"), 2.0f);
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("a"), Dia::Core::StringCRC("f")), 1.0f);
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("b"), Dia::Core::StringCRC("f")), 2.0f);
}

TEST(DiaHTN_TestHelpers, MockHTNContext_SameSlotDifferentFields_Independent)
{
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("s"), Dia::Core::StringCRC("f1"), 1.0f);
    ctx.SetFloat(Dia::Core::StringCRC("s"), Dia::Core::StringCRC("f2"), 2.0f);
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("s"), Dia::Core::StringCRC("f1")), 1.0f);
    EXPECT_FLOAT_EQ(ctx.GetFloat(Dia::Core::StringCRC("s"), Dia::Core::StringCRC("f2")), 2.0f);
}

TEST(DiaHTN_TestHelpers, AssertPlanOperators_CorrectPlan_Passes)
{
    auto domain = LoadDomain(kDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    // If this would fail, gtest would report it here.
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("OpA") });
}

TEST(DiaHTN_TestHelpers, AssertPlanFails_UnknownRoot_Passes)
{
    auto domain = LoadDomain(kDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanFails(planner,
        Dia::Core::StringCRC("NonExistent"), domain, ctx);
}
