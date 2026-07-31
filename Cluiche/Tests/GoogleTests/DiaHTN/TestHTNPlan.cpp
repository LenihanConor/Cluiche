#include <gtest/gtest.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_Plan
// Covers IsEmpty/IsComplete, Advance, cursor semantics, and HasDiverged.

namespace
{
    constexpr const char* kLinearDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["A", "B", "C"] }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] },
            "B": { "type": "primitive", "operator": "OpB", "params": [] },
            "C": { "type": "primitive", "operator": "OpC", "params": [] }
        }
    })";

    Dia::HTN::HTNDomain LoadDomain(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        return Dia::HTN::HTNDomain::LoadFromJson(root, errors);
    }
}

TEST(DiaHTN_Plan, DefaultConstruct_IsEmpty)
{
    Dia::HTN::HTNPlan plan;
    EXPECT_TRUE(plan.IsEmpty());
    EXPECT_TRUE(plan.IsComplete());
}

TEST(DiaHTN_Plan, AfterPlanning_NotEmpty)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    EXPECT_FALSE(plan.IsEmpty());
    EXPECT_EQ(plan.GetTaskCount(), 3);
}

TEST(DiaHTN_Plan, IsComplete_BeforeAdvance_False)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    EXPECT_FALSE(plan.IsComplete());
}

TEST(DiaHTN_Plan, Advance_ThroughAllTasks_IsComplete)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_EQ(plan.GetTaskCount(), 3);

    plan.Advance(); // A
    EXPECT_FALSE(plan.IsComplete());
    plan.Advance(); // B
    EXPECT_FALSE(plan.IsComplete());
    plan.Advance(); // C
    EXPECT_TRUE(plan.IsComplete());
}

TEST(DiaHTN_Plan, Advance_BeyondEnd_NoOp)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    plan.Advance(); plan.Advance(); plan.Advance();
    ASSERT_TRUE(plan.IsComplete());

    plan.Advance(); // should not crash
    EXPECT_TRUE(plan.IsComplete());
}

TEST(DiaHTN_Plan, CurrentTask_FirstOp_IsOpA)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    EXPECT_EQ(plan.CurrentTask().operatorId.Value(), Dia::Core::StringCRC("OpA").Value());
}

TEST(DiaHTN_Plan, CurrentTask_AfterAdvance_IsOpB)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    plan.Advance();
    EXPECT_EQ(plan.CurrentTask().operatorId.Value(), Dia::Core::StringCRC("OpB").Value());
}

TEST(DiaHTN_Plan, HasDiverged_UnchangedContext_ReturnsFalse)
{
    constexpr const char* kCondDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "agent", "field": "hp", "op": ">", "value": 0.0 },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kCondDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("agent"), Dia::Core::StringCRC("hp"), 10.0f);

    Dia::HTN::HTNPlanner planner;
    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_FALSE(plan.IsEmpty());

    EXPECT_FALSE(plan.HasDiverged(ctx));
}

TEST(DiaHTN_Plan, HasDiverged_ChangedFloat_ReturnsTrue)
{
    constexpr const char* kCondDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "agent", "field": "hp", "op": ">", "value": 0.0 },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kCondDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("agent"), Dia::Core::StringCRC("hp"), 10.0f);

    Dia::HTN::HTNPlanner planner;
    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_FALSE(plan.IsEmpty());

    // Mutate the context after planning
    ctx.SetFloat(Dia::Core::StringCRC("agent"), Dia::Core::StringCRC("hp"), 5.0f);

    EXPECT_TRUE(plan.HasDiverged(ctx));
}

TEST(DiaHTN_Plan, MoveConstruct_PreservesTaskCount)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto planA = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_EQ(planA.GetTaskCount(), 3);

    auto planB = std::move(planA);
    EXPECT_EQ(planB.GetTaskCount(), 3);
    EXPECT_TRUE(planA.IsEmpty());
}

TEST(DiaHTN_Plan, MoveAssign_PreservesTaskCount)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto planA = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_EQ(planA.GetTaskCount(), 3);

    Dia::HTN::HTNPlan planB;
    planB = std::move(planA);

    EXPECT_EQ(planB.GetTaskCount(), 3);
    EXPECT_TRUE(planA.IsEmpty());
}

TEST(DiaHTN_Plan, HasDiverged_EmptyPlan_ReturnsFalse)
{
    Dia::HTN::HTNPlan plan; // default-constructed, no snapshot
    Dia::HTN::Testing::MockHTNContext ctx;
    // An empty plan can't have diverged — no snapshot to check.
    EXPECT_FALSE(plan.HasDiverged(ctx));
}

TEST(DiaHTN_Plan, HasDiverged_BoolChanges_ReturnsTrue)
{
    constexpr const char* kBoolCondDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "unit", "field": "alive", "op": "==", "value": true },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kBoolCondDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive"), true);

    Dia::HTN::HTNPlanner planner;
    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_FALSE(plan.IsEmpty());

    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive"), false);
    EXPECT_TRUE(plan.HasDiverged(ctx));
}
