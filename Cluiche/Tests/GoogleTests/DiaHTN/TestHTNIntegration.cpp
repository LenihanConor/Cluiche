#include <gtest/gtest.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/TaskResult.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaCore/Json/external/json/json.h>

// DiaHTN_Integration
// End-to-end tests: load domain → register operators → replan → tick to completion.

namespace
{
    constexpr const char* kPatrolDomain = R"({
        "tasks": {
            "Patrol": {
                "type": "compound",
                "methods": [
                    {
                        "id": "Patrol_Ready",
                        "precondition": { "slot": "unit", "field": "ready", "op": "==", "value": true },
                        "subtasks": ["MoveToA", "MoveToB", "MoveToC"]
                    },
                    {
                        "id": "Patrol_Wait",
                        "subtasks": ["WaitOp"]
                    }
                ]
            },
            "MoveToA": { "type": "primitive", "operator": "Move", "params": ["pointA"] },
            "MoveToB": { "type": "primitive", "operator": "Move", "params": ["pointB"] },
            "MoveToC": { "type": "primitive", "operator": "Move", "params": ["pointC"] },
            "WaitOp":  { "type": "primitive", "operator": "Wait", "params": [] }
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

// Full sync path: load → validate → plan → tick to IsComplete.
TEST(DiaHTN_Integration, FullSyncPlan_TicksToCompletion)
{
    auto domain = LoadDomain(kPatrolDomain);
    ASSERT_TRUE(domain.IsValid());

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    ASSERT_TRUE(domain.Validate(validateErrors));

    Dia::HTN::OperatorRegistry registry;
    struct MoveLog { Dia::Core::StringCRC lastParam; int callCount = 0; };
    MoveLog moveLog;

    registry.Register(Dia::Core::StringCRC("Move"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params)
        -> Dia::HTN::TaskResult
        {
            auto* log = static_cast<MoveLog*>(ctx);
            ++log->callCount;
            if (params.Size() > 0) log->lastParam = params[0];
            return Dia::HTN::TaskResult::kSucceeded;
        });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("ready"), true);

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Patrol"));
    comp.Replan(ctx);

    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 3);
    EXPECT_FALSE(comp.HasDiverged(ctx));

    // Tick all three Move operators to completion.
    auto r = comp.Tick(&moveLog);
    EXPECT_EQ(r, Dia::HTN::TaskResult::kSucceeded);
    r = comp.Tick(&moveLog);
    EXPECT_EQ(r, Dia::HTN::TaskResult::kSucceeded);
    r = comp.Tick(&moveLog);
    EXPECT_EQ(r, Dia::HTN::TaskResult::kSucceeded);

    // Plan is complete; further ticks are no-ops.
    r = comp.Tick(&moveLog);
    EXPECT_EQ(r, Dia::HTN::TaskResult::kSucceeded);

    EXPECT_EQ(moveLog.callCount, 3);
    EXPECT_EQ(moveLog.lastParam.Value(), Dia::Core::StringCRC("pointC").Value());

    EXPECT_TRUE(comp.GetActivePlan()->IsComplete());
    EXPECT_FALSE(comp.HasDiverged(ctx));
}

// Full async path: PlanAsync → scheduler.Update → tick to completion.
TEST(DiaHTN_Integration, FullAsyncPlan_TicksToCompletion)
{
    auto domain = LoadDomain(kPatrolDomain);
    ASSERT_TRUE(domain.IsValid());

    Dia::HTN::OperatorRegistry registry;
    int callCount = 0;
    registry.Register(Dia::Core::StringCRC("Move"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { ++(*static_cast<int*>(ctx)); return Dia::HTN::TaskResult::kSucceeded; });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("ready"), true);
    Dia::AIBudget::AIBudgetScheduler scheduler;

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Patrol"));
    comp.ReplanAsync(ctx, scheduler);

    EXPECT_FALSE(comp.HasActivePlan()); // callback not yet fired
    scheduler.Update(100.0f);           // drains item and fires callback
    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 3);

    // Tick all three.
    comp.Tick(&callCount); comp.Tick(&callCount); comp.Tick(&callCount);
    EXPECT_EQ(callCount, 3);
    EXPECT_TRUE(comp.GetActivePlan()->IsComplete());
}

// Fallback method is chosen when first method's precondition fails.
TEST(DiaHTN_Integration, FallbackPath_WaitOperatorSelected)
{
    auto domain = LoadDomain(kPatrolDomain);
    ASSERT_TRUE(domain.IsValid());

    Dia::HTN::OperatorRegistry registry;
    bool waitCalled = false;
    registry.Register(Dia::Core::StringCRC("Wait"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { *static_cast<bool*>(ctx) = true; return Dia::HTN::TaskResult::kSucceeded; });
    registry.Register(Dia::Core::StringCRC("Move"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("ready"), false);

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Patrol"));
    comp.Replan(ctx);

    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 1);

    comp.Tick(&waitCalled);
    EXPECT_TRUE(waitCalled);
}

// Divergence detected mid-execution triggers correct HasDiverged result.
TEST(DiaHTN_Integration, Divergence_ContextChanges_DetectedMidPlan)
{
    auto domain = LoadDomain(kPatrolDomain);
    ASSERT_TRUE(domain.IsValid());

    Dia::HTN::OperatorRegistry registry;
    registry.Register(Dia::Core::StringCRC("Move"),
        [](void*, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult { return Dia::HTN::TaskResult::kSucceeded; });

    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::Testing::MockHTNContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("ready"), true);

    comp.SetDomain(&domain);
    comp.SetRegistry(&registry);
    comp.SetRootTask(Dia::Core::StringCRC("Patrol"));
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_FALSE(comp.HasDiverged(ctx));

    // Execute first task, then change the world state.
    comp.Tick(nullptr);
    ctx.SetBool(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("ready"), false);

    EXPECT_TRUE(comp.HasDiverged(ctx));

    // Replan with updated state — divergence should clear.
    comp.Replan(ctx);
    ASSERT_TRUE(comp.HasActivePlan());
    EXPECT_FALSE(comp.HasDiverged(ctx));
    EXPECT_EQ(comp.GetActivePlan()->GetTaskCount(), 1); // fallback: Wait
}
