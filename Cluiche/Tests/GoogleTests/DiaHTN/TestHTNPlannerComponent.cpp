#include <gtest/gtest.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/TaskResult.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_PlannerComponent
// Covers SetDomain/SetRegistry/SetRootTask, Replan, Tick lifecycle, and
// HasActivePlan/GetActivePlan accessors.

namespace
{
    Dia::HTN::HTNDomain LoadDomain(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        return Dia::HTN::HTNDomain::LoadFromJson(root, errors);
    }

    constexpr const char* kTwoPrimDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["A", "B"] }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] },
            "B": { "type": "primitive", "operator": "OpB", "params": [] }
        }
    })";
}

TEST(DiaHTN_PlannerComponent, DefaultConstruct_NoActivePlan)
{
    Dia::HTN::HTNPlannerComponent comp;
    EXPECT_FALSE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan(), nullptr);
}

TEST(DiaHTN_PlannerComponent, Tick_NoPlan_ReturnsSucceeded)
{
    Dia::HTN::HTNPlannerComponent comp;
    EXPECT_EQ(comp.Tick(nullptr), Dia::HTN::TaskResult::kSucceeded);
}

TEST(DiaHTN_PlannerComponent, Replan_NoDomain_NoOp)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx); // no crash, no plan
    EXPECT_FALSE(comp.HasActivePlan());
}

TEST(DiaHTN_PlannerComponent, Replan_ValidDomain_HasPlan)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;

    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    EXPECT_TRUE(comp.HasActivePlan());
    ASSERT_NE(comp.GetActivePlan(), nullptr);
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 2);
}

TEST(DiaHTN_PlannerComponent, Tick_NoRegistry_ReturnsFailed)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;

    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());

    // No registry set — operator lookup fails
    const auto result = comp.Tick(nullptr);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kFailed);
}

TEST(DiaHTN_PlannerComponent, Tick_OperatorSucceeded_AdvancesCursor)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::OperatorRegistry registry;

    int opACount = 0;
    int opBCount = 0;

    registry.Register(Dia::Core::StringCRC("OpA"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult
        {
            ++(*static_cast<int*>(ctx));
            return Dia::HTN::TaskResult::kSucceeded;
        });
    registry.Register(Dia::Core::StringCRC("OpB"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult
        {
            ++(*(static_cast<int*>(ctx) + 1));
            return Dia::HTN::TaskResult::kSucceeded;
        });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext planCtx;

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(planCtx);

    int counts[2] = {0, 0};
    auto result = comp.Tick(counts);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kSucceeded);
    EXPECT_EQ(counts[0], 1); // OpA called once

    result = comp.Tick(counts);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kSucceeded);
    EXPECT_EQ(counts[1], 1); // OpB called once

    // Plan complete — further ticks return kSucceeded (no-op)
    result = comp.Tick(counts);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kSucceeded);
}

TEST(DiaHTN_PlannerComponent, Tick_OperatorRunning_DoesNotAdvance)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::OperatorRegistry registry;

    registry.Register(Dia::Core::StringCRC("OpA"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kRunning; });
    registry.Register(Dia::Core::StringCRC("OpB"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    // OpA returns kRunning — cursor stays at first task
    auto r1 = comp.Tick(nullptr);
    EXPECT_EQ(r1, Dia::HTN::TaskResult::kRunning);

    auto r2 = comp.Tick(nullptr);
    EXPECT_EQ(r2, Dia::HTN::TaskResult::kRunning);

    ASSERT_NE(comp.GetActivePlan(), nullptr);
    EXPECT_EQ(comp.GetActivePlan()->CurrentTask().operatorId.Value(),
              Dia::Core::StringCRC("OpA").Value());
}

TEST(DiaHTN_PlannerComponent, Tick_OperatorFailed_ReturnsFailed)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::OperatorRegistry registry;

    registry.Register(Dia::Core::StringCRC("OpA"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kFailed; });
    registry.Register(Dia::Core::StringCRC("OpB"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    EXPECT_EQ(comp.Tick(nullptr), Dia::HTN::TaskResult::kFailed);
}

TEST(DiaHTN_PlannerComponent, HasDiverged_UnchangedContext_ReturnsFalse)
{
    constexpr const char* kCondDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "u", "field": "alive", "op": "==", "value": true },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kCondDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("u"), Dia::Core::StringCRC("alive"), true);

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());

    EXPECT_FALSE(comp.HasDiverged(ctx));
}

TEST(DiaHTN_PlannerComponent, HasDiverged_WorldChanged_ReturnsTrue)
{
    constexpr const char* kCondDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{
                    "id": "m",
                    "precondition": { "slot": "u", "field": "alive", "op": "==", "value": true },
                    "subtasks": ["A"]
                }]
            },
            "A": { "type": "primitive", "operator": "OpA", "params": [] }
        }
    })";

    auto domain = LoadDomain(kCondDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("u"), Dia::Core::StringCRC("alive"), true);

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());

    ctx.SetBool(Dia::Core::StringCRC("u"), Dia::Core::StringCRC("alive"), false);
    EXPECT_TRUE(comp.HasDiverged(ctx));
}

TEST(DiaHTN_PlannerComponent, HasDiverged_NoPlan_ReturnsFalse)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    // No Replan called — no plan, no snapshot.
    EXPECT_FALSE(comp.HasDiverged(ctx));
}

TEST(DiaHTN_PlannerComponent, Replan_ReplacesExistingPlan)
{
    // First plan has 2 tasks; second plan (after SetRootTask swap) has 1.
    constexpr const char* kTwoPlansDomain = R"({
        "tasks": {
            "BigTask": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["P1", "P2"] }]
            },
            "SmallTask": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["P1"] }]
            },
            "P1": { "type": "primitive", "operator": "OpA", "params": [] },
            "P2": { "type": "primitive", "operator": "OpB", "params": [] }
        }
    })";

    auto domain = LoadDomain(kTwoPlansDomain);
    Dia::HTN::Testing::MockHTNContext ctx;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);

    comp.SetRootTask(Dia::Core::StringCRC("BigTask"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 2);

    comp.SetRootTask(Dia::Core::StringCRC("SmallTask"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 1);
}

TEST(DiaHTN_PlannerComponent, SetRootTask_Swap_UsedOnNextReplan)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::Testing::MockHTNContext ctx;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());
    const int firstCount = comp.GetActivePlan()->GetTaskCount();

    // Swap to an unknown root — replanning should yield no plan
    comp.SetRootTask(Dia::Core::StringCRC("DoesNotExist"));
    comp.Replan(ctx);
    EXPECT_FALSE(comp.HasActivePlan());
    (void)firstCount;
}

TEST(DiaHTN_PlannerComponent, Tick_ParamsPassedToOperator)
{
    constexpr const char* kParamDomain = R"({
        "tasks": {
            "Root": {
                "type": "compound",
                "methods": [{ "id": "m", "subtasks": ["Move"] }]
            },
            "Move": { "type": "primitive", "operator": "MoveOp", "params": ["waypoint_a"] }
        }
    })";

    auto domain = LoadDomain(kParamDomain);
    Dia::HTN::OperatorRegistry registry;

    Dia::Core::StringCRC capturedParam;
    registry.Register(Dia::Core::StringCRC("MoveOp"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params)
        -> Dia::HTN::TaskResult
        {
            if (params.Size() > 0)
                *static_cast<Dia::Core::StringCRC*>(ctx) = params[0];
            return Dia::HTN::TaskResult::kSucceeded;
        });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext planCtx;

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(planCtx);

    comp.Tick(&capturedParam);
    EXPECT_EQ(capturedParam.Value(), Dia::Core::StringCRC("waypoint_a").Value());
}

TEST(DiaHTN_PlannerComponent, ReplanAsync_CancelOnDestroy_NoUAF)
{
    // If the component is destroyed before the scheduler drains, the callback must be
    // a no-op — no write into freed memory.
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::SimTime::SimTimeBudget budget;

    {
        Dia::HTN::HTNPlannerComponent comp;
        comp.SetDomain(&domain);
        comp.SetRootTask(Dia::Core::StringCRC("Root"));
        comp.ReplanAsync(ctx, budget);
        // comp destructs here — cancels the pending token.
    }

    // Budget drains after the component is gone. Must not crash.
    budget.RunOneShots(100.0f);
    // If we reach here without a crash or assertion, the cancel guard worked.
    SUCCEED();
}

TEST(DiaHTN_PlannerComponent, ReplanAsync_DoubleSubmit_SecondCancelsFirst)
{
    // A second ReplanAsync call before budget drains should cancel the first
    // work item's token so only the second plan is installed.
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::SimTime::SimTimeBudget budget;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));

    comp.ReplanAsync(ctx, budget); // first submission
    comp.ReplanAsync(ctx, budget); // second submission — cancels first token

    // SimTimeBudget::RunOneShots drains its queue in submission order within a
    // single call (unlike the old AIBudgetScheduler, it does not skip an item
    // when an earlier one is removed mid-loop), so one drain is now enough to
    // process both items. A second call is a harmless no-op kept here for
    // extra safety.
    budget.RunOneShots(100.0f); // drains item1 (cancelled → no-op) and item2 (installs plan)
    budget.RunOneShots(100.0f); // no-op; queue already empty
    EXPECT_TRUE(comp.HasActivePlan());
}

TEST(DiaHTN_PlannerComponent, ReplanAsync_CallbackUpdatesActivePlan)
{
    auto domain = LoadDomain(kTwoPrimDomain);
    Dia::HTN::Testing::MockHTNContext ctx;
    Dia::SimTime::SimTimeBudget budget;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domain);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));

    EXPECT_FALSE(comp.HasActivePlan());

    comp.ReplanAsync(ctx, budget);
    EXPECT_FALSE(comp.HasActivePlan()); // callback not yet fired

    budget.RunOneShots(100.0f);         // drains work item, fires OnAsyncPlanReady
    EXPECT_TRUE(comp.HasActivePlan());
    ASSERT_NE(comp.GetActivePlan(), nullptr);
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 2);
}
