#include <gtest/gtest.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_Planner
// Covers sync planning: linear chains, method fallthrough, preconditions,
// nested compound tasks, parameter binding, and plan failure cases.

namespace
{
    Dia::HTN::HTNDomain LoadDomain(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        return Dia::HTN::HTNDomain::LoadFromJson(root, errors);
    }

    constexpr const char* kLinearDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["A", "B"] }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] },
            "B": { "type": "primitive", "operator": "OpB", "params": [] }
        }
    })";

    // Two methods: first requires army.size >= 5, fallback has no precondition.
    constexpr const char* kFallbackDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [
                    {
                        "id": "m_strong",
                        "precondition": { "slot": "army", "field": "size", "op": ">=", "value": 5.0 },
                        "subtasks": ["AttackOp"]
                    },
                    {
                        "id": "m_retreat",
                        "subtasks": ["RetreatOp"]
                    }
                ]
            },
            "AttackOp": { "type": "primitive", "operator": "Attack", "params": [] },
            "RetreatOp": { "type": "primitive", "operator": "Retreat", "params": [] }
        }
    })";

    constexpr const char* kNestedDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Sub", "Final"] }]
            },
            "Sub": {
                "type": "compound",
                "methods": [{ "id": "s", "subtasks": ["OpX", "OpY"] }]
            },
            "Final": { "type": "primitive", "operator": "OpFinal", "params": [] },
            "OpX": { "type": "primitive", "operator": "X", "params": [] },
            "OpY": { "type": "primitive", "operator": "Y", "params": [] }
        }
    })";
}

TEST(DiaHTN_Planner, LinearDomain_ProducesCorrectSequence)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("OpA"), Dia::Core::StringCRC("OpB") });
}

TEST(DiaHTN_Planner, UnknownRootTask_ReturnsEmptyPlan)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanFails(planner,
        Dia::Core::StringCRC("DoesNotExist"), domain, ctx);
}

TEST(DiaHTN_Planner, PreconditionPasses_FirstMethodChosen)
{
    auto domain = LoadDomain(kFallbackDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("army"), Dia::Core::StringCRC("size"), 10.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("Attack") });
}

TEST(DiaHTN_Planner, PreconditionFails_FallbackMethodChosen)
{
    auto domain = LoadDomain(kFallbackDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("army"), Dia::Core::StringCRC("size"), 2.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("Retreat") });
}

TEST(DiaHTN_Planner, AllMethodsFail_ReturnsEmptyPlan)
{
    constexpr const char* kAllFailDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [
                    {
                        "id": "m1",
                        "precondition": { "slot": "x", "field": "f", "op": ">", "value": 10.0 },
                        "subtasks": ["OpA"]
                    },
                    {
                        "id": "m2",
                        "precondition": { "slot": "x", "field": "f", "op": ">", "value": 20.0 },
                        "subtasks": ["OpB"]
                    }
                ]
            },
            "OpA": { "type": "primitive", "operator": "A", "params": [] },
            "OpB": { "type": "primitive", "operator": "B", "params": [] }
        }
    })";

    auto domain = LoadDomain(kAllFailDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("f"), 0.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanFails(planner,
        Dia::Core::StringCRC("Root"), domain, ctx);
}

TEST(DiaHTN_Planner, NestedCompound_FlattensCorrectly)
{
    auto domain = LoadDomain(kNestedDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        {
            Dia::Core::StringCRC("X"),
            Dia::Core::StringCRC("Y"),
            Dia::Core::StringCRC("OpFinal")
        });
}

TEST(DiaHTN_Planner, Stateless_TwoCalls_ProduceSamePlan)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto planA = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    auto planB = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    EXPECT_EQ(planA.GetTaskCount(), planB.GetTaskCount());
}

TEST(DiaHTN_Planner, InvalidDomain_ReturnsEmptyPlan)
{
    Dia::HTN::HTNDomain domain; // default-constructed, not valid
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    EXPECT_TRUE(plan.IsEmpty());
}

TEST(DiaHTN_Planner, PrimitiveWithParams_TaskHasParams)
{
    constexpr const char* kParamDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Move"] }]
            },
            "Move": { "type": "primitive", "operator": "MoveOp", "params": ["staging_point"] }
        }
    })";

    auto domain = LoadDomain(kParamDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    auto plan = planner.Plan(Dia::Core::StringCRC("Root"), domain, ctx);
    ASSERT_EQ(plan.GetTaskCount(), 1);
    EXPECT_EQ(plan.CurrentTask().params.Size(), 1u);
    EXPECT_EQ(plan.CurrentTask().params[0].Value(),
              Dia::Core::StringCRC("staging_point").Value());
}

TEST(DiaHTN_Planner, BoolPrecondition_TrueValue_PlanSucceeds)
{
    constexpr const char* kBoolDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [
                    {
                        "id": "m",
                        "precondition": { "slot": "unit", "field": "alive", "op": "==", "value": true },
                        "subtasks": ["Attack"]
                    }
                ]
            },
            "Attack": { "type": "primitive", "operator": "AttackOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kBoolDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive"), true);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("AttackOp") });
}

TEST(DiaHTN_Planner, BoolPrecondition_FalseValue_PlanFails)
{
    constexpr const char* kBoolDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [
                    {
                        "id": "m",
                        "precondition": { "slot": "unit", "field": "alive", "op": "==", "value": true },
                        "subtasks": ["Attack"]
                    }
                ]
            },
            "Attack": { "type": "primitive", "operator": "AttackOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kBoolDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("alive"), false);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanFails(planner,
        Dia::Core::StringCRC("Root"), domain, ctx);
}

// The spec example: CaptureBase domain from the JSON schema section.
TEST(DiaHTN_Planner, CaptureBase_ArmyReadyAndScouted_UsesAttackPath)
{
    constexpr const char* kCaptureBaseDomain = R"({
        "tasks": {
            "CaptureBase": {
                "type": "compound",
                "methods": [
                    {
                        "id": "CaptureBase_ArmyReady",
                        "precondition": {
                            "op": "and", "conditions": [
                                { "slot": "army", "field": "size", "op": ">=", "value": 5.0 },
                                { "slot": "base", "field": "scouted", "op": "==", "value": true }
                            ]
                        },
                        "subtasks": ["MoveToStaging", "Attack", "Hold"]
                    },
                    {
                        "id": "CaptureBase_Scout",
                        "precondition": { "slot": "base", "field": "scouted", "op": "==", "value": false },
                        "subtasks": ["SendScout"]
                    }
                ]
            },
            "MoveToStaging": { "type": "primitive", "operator": "MoveToPosition", "params": ["staging_point"] },
            "Attack":        { "type": "primitive", "operator": "AttackTarget",   "params": [] },
            "Hold":          { "type": "primitive", "operator": "HoldPosition",   "params": [] },
            "SendScout":     { "type": "primitive", "operator": "SpawnScout",     "params": [] }
        }
    })";

    auto domain = LoadDomain(kCaptureBaseDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("army"), Dia::Core::StringCRC("size"), 6.0f);
    ctx.SetBool(Dia::Core::StringCRC("base"), Dia::Core::StringCRC("scouted"), true);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("CaptureBase"), domain, ctx,
        {
            Dia::Core::StringCRC("MoveToPosition"),
            Dia::Core::StringCRC("AttackTarget"),
            Dia::Core::StringCRC("HoldPosition")
        });
}

// --- PrimitiveRootTask ---

TEST(DiaHTN_Planner, PrimitiveRootTask_ProducesSingleTask)
{
    constexpr const char* kPrimRootDomain = R"({
        "tasks": {
            "JustPatrol": { "type": "primitive", "operator": "PatrolOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kPrimRootDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("JustPatrol"), domain, ctx,
        { Dia::Core::StringCRC("PatrolOp") });
}

// --- ZeroMethod compound ---

TEST(DiaHTN_Planner, ZeroMethodCompound_ReturnsEmptyPlan)
{
    constexpr const char* kZeroMethodDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": []
            }
        }
    })";

    auto domain = LoadDomain(kZeroMethodDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanFails(planner,
        Dia::Core::StringCRC("Root"), domain, ctx);
}

// --- Three-level nesting ---

TEST(DiaHTN_Planner, ThreeLevelNesting_FlattensCorrectly)
{
    constexpr const char* kDeepDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Mid"] }]
            },
            "Mid": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Leaf"] }]
            },
            "Leaf": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Prim"] }]
            },
            "Prim": { "type": "primitive", "operator": "DeepOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kDeepDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;

    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("DeepOp") });
}

// --- Additional comparison operators ---

TEST(DiaHTN_Planner, NotEqualOp_ConditionPasses_PlanSucceeds)
{
    constexpr const char* kNotEqualDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "unit", "field": "state", "op": "!=", "value": 2.0 },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kNotEqualDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("state"), 1.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("OpA") });
}

TEST(DiaHTN_Planner, LessThanOp_ConditionPasses_PlanSucceeds)
{
    constexpr const char* kLtDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "unit", "field": "hp", "op": "<", "value": 50.0 },
                    "subtasks": ["Heal"]
                }]
            },
            "Heal": { "type": "primitive", "operator": "HealOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kLtDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("hp"), 30.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("HealOp") });
}

TEST(DiaHTN_Planner, LessEqualOp_ExactBoundary_PlanSucceeds)
{
    constexpr const char* kLeDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "unit", "field": "hp", "op": "<=", "value": 50.0 },
                    "subtasks": ["Heal"]
                }]
            },
            "Heal": { "type": "primitive", "operator": "HealOp", "params": [] }
        }
    })";

    auto domain = LoadDomain(kLeDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("hp"), 50.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("Root"), domain, ctx,
        { Dia::Core::StringCRC("HealOp") });
}

// --- PlanAsync ---

TEST(DiaHTN_Planner, PlanAsync_Submit_CallbackFires)
{
    auto domain = LoadDomain(kLinearDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::HTN::HTNPlanner planner;
    Dia::AIBudget::AIBudgetScheduler scheduler;

    struct Result { bool fired = false; int taskCount = 0; };
    Result result;

    const bool submitted = planner.PlanAsync(
        Dia::Core::StringCRC("Root"), domain, ctx, scheduler,
        [](Dia::HTN::HTNPlan plan, void* ud)
        {
            auto* r = static_cast<Result*>(ud);
            r->fired     = true;
            r->taskCount = plan.GetTaskCount();
        },
        &result);

    EXPECT_TRUE(submitted);
    EXPECT_FALSE(result.fired); // not yet — scheduler hasn't ticked

    scheduler.Update(100.0f);   // drains the work item
    EXPECT_TRUE(result.fired);
    EXPECT_EQ(result.taskCount, 2);
}

TEST(DiaHTN_Planner, PlanAsync_FailingDomain_CallbackFiresWithEmptyPlan)
{
    constexpr const char* kAllFailDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "x", "field": "f", "op": ">", "value": 100.0 },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kAllFailDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("f"), 0.0f);

    Dia::HTN::HTNPlanner planner;
    Dia::AIBudget::AIBudgetScheduler scheduler;

    struct Result { bool fired = false; bool planEmpty = false; };
    Result result;

    planner.PlanAsync(
        Dia::Core::StringCRC("Root"), domain, ctx, scheduler,
        [](Dia::HTN::HTNPlan plan, void* ud)
        {
            auto* r = static_cast<Result*>(ud);
            r->fired     = true;
            r->planEmpty = plan.IsEmpty();
        },
        &result);

    scheduler.Update(100.0f);
    EXPECT_TRUE(result.fired);
    EXPECT_TRUE(result.planEmpty);
}

TEST(DiaHTN_Planner, CaptureBase_NotScouted_UsesScoutPath)
{
    constexpr const char* kCaptureBaseDomain = R"({
        "tasks": {
            "CaptureBase": {
                "type": "compound",
                "methods": [
                    {
                        "id": "CaptureBase_ArmyReady",
                        "precondition": {
                            "op": "and", "conditions": [
                                { "slot": "army", "field": "size", "op": ">=", "value": 5.0 },
                                { "slot": "base", "field": "scouted", "op": "==", "value": true }
                            ]
                        },
                        "subtasks": ["Attack"]
                    },
                    {
                        "id": "CaptureBase_Scout",
                        "precondition": { "slot": "base", "field": "scouted", "op": "==", "value": false },
                        "subtasks": ["SendScout"]
                    }
                ]
            },
            "Attack":    { "type": "primitive", "operator": "AttackTarget", "params": [] },
            "SendScout": { "type": "primitive", "operator": "SpawnScout",   "params": [] }
        }
    })";

    auto domain = LoadDomain(kCaptureBaseDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("army"), Dia::Core::StringCRC("size"), 6.0f);
    ctx.SetBool(Dia::Core::StringCRC("base"), Dia::Core::StringCRC("scouted"), false);

    Dia::HTN::HTNPlanner planner;
    Dia::HTN::Testing::AssertPlanOperators(planner,
        Dia::Core::StringCRC("CaptureBase"), domain, ctx,
        { Dia::Core::StringCRC("SpawnScout") });
}
