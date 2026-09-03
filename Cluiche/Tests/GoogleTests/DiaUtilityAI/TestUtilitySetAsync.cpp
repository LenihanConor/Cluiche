#include <gtest/gtest.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    Dia::UtilityAI::UtilitySet MakeAsyncSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }

    // Single action with no prerequisite and no scorers (always score = 1.0)
    const char* kSingleActionNoScorers = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": []
            }
        ]
    })";

    // Single action with a prerequisite that checks enemy.visible == true
    const char* kSingleActionWithPrereq = R"({
        "actions": [
            {
                "id": "Attack",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": []
            }
        ]
    })";

    // Callback result capture struct (no captures — function pointer only).
    struct AsyncResult
    {
        Dia::UtilityAI::UtilitySelection sel;
        int callCount = 0;
    };

    static void AsyncCallback(Dia::UtilityAI::UtilitySelection result, void* ud)
    {
        auto* r = reinterpret_cast<AsyncResult*>(ud);
        r->sel = result;
        r->callCount++;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. Callback fires with correct winner when scheduler is drained once
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetAsync, EvaluateAsync_CallbackFires_WhenSchedulerDrained)
{
    Dia::UtilityAI::UtilitySet set = MakeAsyncSet(kSingleActionNoScorers);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void*) {});

    Dia::SimTime::SimTimeBudget budget;

    AsyncResult result;
    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &result);

    EXPECT_EQ(budget.GetPendingOneShotCount(), 1);

    budget.RunOneShots(1000.0f);

    EXPECT_EQ(result.callCount, 1);
    EXPECT_EQ(result.sel.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.sel.score, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 2. Work item is one-shot: callback fires exactly once even when scheduler
//    is drained a second time
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetAsync, EvaluateAsync_OneShot_CallbackFiresOnce)
{
    Dia::UtilityAI::UtilitySet set = MakeAsyncSet(kSingleActionNoScorers);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void*) {});

    Dia::SimTime::SimTimeBudget budget;

    AsyncResult result;
    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &result);

    // Drain twice — callback must fire only once.
    budget.RunOneShots(1000.0f);
    budget.RunOneShots(1000.0f);

    EXPECT_EQ(result.callCount, 1);
}

// ---------------------------------------------------------------------------
// 3. Work item is removed from the queue once drained (SimTimeBudget::
//    RunOneShots removes an item as soon as its Step() returns true).
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetAsync, EvaluateAsync_SchedulerEmptyAfterDrain)
{
    Dia::UtilityAI::UtilitySet set = MakeAsyncSet(kSingleActionNoScorers);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    Dia::SimTime::SimTimeBudget budget;

    AsyncResult result;
    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &result);

    EXPECT_EQ(budget.GetPendingOneShotCount(), 1);
    budget.RunOneShots(1000.0f);
    EXPECT_EQ(budget.GetPendingOneShotCount(), 0);
}

// ---------------------------------------------------------------------------
// 4. No eligible action: callback receives zero actionId and zero score
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetAsync, EvaluateAsync_NoEligible_CallbackReceivesNoSelection)
{
    Dia::UtilityAI::UtilitySet set = MakeAsyncSet(kSingleActionWithPrereq);

    Dia::Condition::Testing::MockConditionContext ctx;
    // enemy.visible = false => prerequisite fails => no eligible action
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);

    Dia::Rules::RuleActionRegistry registry;
    Dia::SimTime::SimTimeBudget budget;

    AsyncResult result;
    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &result);

    budget.RunOneShots(1000.0f);

    EXPECT_EQ(result.callCount, 1);
    EXPECT_EQ(result.sel.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.sel.score, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 5. Multiple async submissions: each fires exactly once. SimTimeBudget::
//    RunOneShots drains its queue in submission order within a single call
//    (it does not skip when an earlier item is removed mid-loop), so both
//    items complete in one drain here.
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetAsync, EvaluateAsync_MultipleSubmissions_EachFiresExactlyOnce)
{
    Dia::UtilityAI::UtilitySet set = MakeAsyncSet(kSingleActionNoScorers);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void*) {});

    Dia::SimTime::SimTimeBudget budget;

    AsyncResult resultA;
    AsyncResult resultB;

    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &resultA);
    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &resultB);

    EXPECT_EQ(budget.GetPendingOneShotCount(), 2);

    budget.RunOneShots(1000.0f);
    budget.RunOneShots(1000.0f); // harmless extra drain; queue is already empty

    EXPECT_EQ(resultA.callCount, 1);
    EXPECT_EQ(resultB.callCount, 1);
    EXPECT_EQ(budget.GetPendingOneShotCount(), 0);
}
