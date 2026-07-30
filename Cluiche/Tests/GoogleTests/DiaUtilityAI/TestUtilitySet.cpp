#include <gtest/gtest.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaRules/RuleActionRegistry.h>
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

    // Build a UtilitySet from a JSON string
    Dia::UtilityAI::UtilitySet MakeSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }

    // Single action with no prerequisite and no scorers (always score=1)
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

    // Two actions with linear scorers reading health.value
    const char* kTwoActionsLinearScorer = R"({
        "actions": [
            {
                "id": "Flee",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear", "invert": true } }
                ]
            },
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // Single action with two scorers: linear (0.5 output) and step (returns 0.0 when below threshold)
    const char* kActionTwoScorersZeroSuppresses = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear" } },
                    { "slot": "ammo", "field": "count", "input_min": 0.0, "input_max": 10.0, "curve": { "shape": "step", "threshold": 0.5 } }
                ]
            }
        ]
    })";

    // Single action with max_concurrent = 1
    const char* kActionMaxConcurrentOne = R"({
        "actions": [
            {
                "id": "Charge",
                "max_concurrent": 1,
                "scorers": []
            }
        ]
    })";

    // Single action with max_concurrent = 0 (unlimited)
    const char* kActionMaxConcurrentZero = R"({
        "actions": [
            {
                "id": "Charge",
                "max_concurrent": 0,
                "scorers": []
            }
        ]
    })";

    // Two JSON actions for count test
    const char* kTwoActionsJson = R"({
        "actions": [
            { "id": "Attack", "scorers": [] },
            { "id": "Flee",   "scorers": [] }
        ]
    })";

    // Single action with scorer, for ActionId parse test
    const char* kFleeActionJson = R"({
        "actions": [
            {
                "id": "Flee",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. Empty UtilitySet, Evaluate returns no selection
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, Empty_Evaluate_ReturnsNoSelection)
{
    Dia::UtilityAI::UtilitySet set;
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 2. Single action with no prerequisite and no scorers wins with score 1.0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, SingleAction_NoPrerequisite_NoScorers_WinsWithScoreOne)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kSingleActionNoScorers);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 3. Single action with failing prerequisite returns no selection
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, SingleAction_FailPrerequisite_ReturnsNoSelection)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kSingleActionWithPrereq);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    // enemy.visible = false => prerequisite fails
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 4. Two actions: score-based winner is selected
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, TwoActions_ScoreBasedWinner)
{
    // Attack has linear scorer (high health = high score)
    // Flee has inverted linear scorer (low health = high score)
    // With health=80: Attack score = 0.8, Flee score = 0.2 => Attack wins
    Dia::UtilityAI::UtilitySet set = MakeSet(kTwoActionsLinearScorer);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 80.0f);

    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 0.8f, 0.01f);
}

// ---------------------------------------------------------------------------
// 5. Product of scorers: one scorer returns 0.0 => action suppressed
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, ProductOfScorers_ZeroSuppressesAction)
{
    // health.value = 50 => linear scorer normalised = 0.5 => output 0.5
    // ammo.count = 2 => step scorer normalised = 0.2 (below 0.5 threshold) => output 0.0
    // product = 0.5 * 0.0 = 0.0 => action ineligible
    Dia::UtilityAI::UtilitySet set = MakeSet(kActionTwoScorersZeroSuppresses);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 50.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"), Dia::Core::StringCRC("count"), 2.0f);

    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 6. Group cap enforced: action not eligible when at max_concurrent
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, GroupCap_Enforced)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kActionMaxConcurrentOne);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::GroupConsiderationContext group;
    group.Increment(Dia::Core::StringCRC("Charge"));  // count = 1, max = 1 => not eligible

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, &group);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 7. Group cap = 0 means unlimited: action still eligible at any count
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, GroupCap_Zero_Unlimited)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kActionMaxConcurrentZero);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    Dia::UtilityAI::GroupConsiderationContext group;
    for (int i = 0; i < 100; ++i)
        group.Increment(Dia::Core::StringCRC("Charge"));

    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, &group);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Charge"));
    EXPECT_NEAR(result.score, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// 8. Evaluate dispatches winning action callback
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, Evaluate_DispatchesWinner)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kSingleActionNoScorers);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    bool dispatched = false;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void* context)
    {
        *static_cast<bool*>(context) = true;
    });

    set.Evaluate(ctx, registry, &dispatched, nullptr);

    EXPECT_TRUE(dispatched);
}

// ---------------------------------------------------------------------------
// 9. Evaluate does not dispatch when no eligible action
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, Evaluate_DoesNotDispatch_NoEligible)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kSingleActionWithPrereq);

    Dia::Condition::Testing::MockConditionContext ctx;
    // enemy.visible = false => prerequisite fails
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);

    Dia::Rules::RuleActionRegistry registry;

    bool dispatched = false;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void* context)
    {
        *static_cast<bool*>(context) = true;
    });

    set.Evaluate(ctx, registry, &dispatched, nullptr);

    EXPECT_FALSE(dispatched);
}

// ---------------------------------------------------------------------------
// 10. GetActionCount after loading 2 actions
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, GetActionCount_AfterLoad)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kTwoActionsJson);
    EXPECT_EQ(set.GetActionCount(), 2);
}

// ---------------------------------------------------------------------------
// 11. LoadFromJson parses action id correctly
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySet, LoadFromJson_ParsesActionId)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kFleeActionJson);

    EXPECT_EQ(set.GetActionCount(), 1);

    // With health.value = 100 (fully normalised to 1.0 via linear scorer), Flee wins
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 100.0f);

    Dia::Rules::RuleActionRegistry registry;
    Dia::UtilityAI::UtilitySelection result = set.Evaluate(ctx, registry, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Flee"));
}
