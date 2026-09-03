#include <gtest/gtest.h>

#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaUtilityAI/PersonalityProfile.h>
#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaUtilityAI/Testing/UtilityTestHelpers.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float kTol = 0.001f;

    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    Dia::UtilityAI::UtilitySet LoadSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }

    Dia::UtilityAI::PersonalityProfile LoadProfile(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::PersonalityProfile::LoadFromJson(root);
    }

    // Fire counter
    struct FireCounter { int count = 0; };
    static void IncrementFire(void* ctx) { ++static_cast<FireCounter*>(ctx)->count; }

    // Two-action set with linear scorers, each reading health.value
    const char* kTwoActionsLinear = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ],
                "cooldown": 0.5
            },
            {
                "id": "Flee",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear", "invert": true } }
                ],
                "cooldown": 0.5
            }
        ]
    })";

    // Action A (max_concurrent=2) and Action B (unlimited)
    const char* kGroupCapTwoActions = R"({
        "actions": [
            {
                "id": "ActionA",
                "max_concurrent": 2,
                "scorers": [
                    { "slot": "score", "field": "val",
                      "input_min": 0.0, "input_max": 1.0,
                      "curve": { "shape": "linear" } }
                ]
            },
            {
                "id": "ActionB",
                "scorers": [
                    { "slot": "score", "field": "val",
                      "input_min": 0.0, "input_max": 1.0,
                      "curve": { "shape": "linear", "invert": true } }
                ]
            }
        ]
    })";

    // Three actions all with cooldown=0.3s for cooldown expiry test
    const char* kThreeActionsWithCooldown = R"({
        "actions": [
            { "id": "ActionX", "scorers": [
                { "slot": "x", "field": "v", "input_min": 0.0, "input_max": 10.0,
                  "curve": { "shape": "linear" } }
              ], "cooldown": 0.3 },
            { "id": "ActionY", "scorers": [
                { "slot": "y", "field": "v", "input_min": 0.0, "input_max": 10.0,
                  "curve": { "shape": "linear" } }
              ], "cooldown": 0.3 },
            { "id": "ActionZ", "scorers": [
                { "slot": "z", "field": "v", "input_min": 0.0, "input_max": 10.0,
                  "curve": { "shape": "linear" } }
              ], "cooldown": 0.3 }
        ]
    })";

    // Single Attack, no cooldown, no scorers (score always 1.0)
    const char* kAttackNoCooldown = R"({
        "actions": [ { "id": "Attack", "scorers": [] } ]
    })";

    // Async test set: two actions scoring against health
    const char* kAsyncTwoActions = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            },
            {
                "id": "Flee",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear", "invert": true } }
                ]
            }
        ]
    })";

    // AsyncResult for async integration test
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
// 1. Personality changes winner; cooldown suppresses on second eval;
//    third eval (after dt > cooldown) allows it again.
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, FullEval_PersonalityChangesWinner_And_CooldownEnforced)
{
    // health=40: raw Attack=0.4, raw Flee=0.6 → without personality Flee wins.
    // Aggressive: Attack*2.0=0.8, Flee*0.5=0.3 → Attack wins.
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kTwoActionsLinear));

    Dia::Rules::RuleActionRegistry reg;
    FireCounter attackFire;
    FireCounter fleeFire;
    reg.Register(Dia::Core::StringCRC("Attack"), IncrementFire);
    reg.Register(Dia::Core::StringCRC("Flee"),   [](void*){});
    comp.SetRegistry(&reg);

    Dia::UtilityAI::PersonalityProfile profile = LoadProfile(R"({
        "name": "Aggressive",
        "biases": [
            { "action": "Attack", "multiplier": 2.0 },
            { "action": "Flee",   "multiplier": 0.5 }
        ]
    })");
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 40.0f);

    // First eval: Attack wins (personality-biased); cooldown = 0.5s starts
    Dia::UtilityAI::UtilitySelection r1 = comp.Evaluate(ctx, &attackFire, 0.0f);
    EXPECT_EQ(r1.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_EQ(attackFire.count, 1);

    // Second eval (dt=0): Attack still in cooldown → suppressed
    Dia::UtilityAI::UtilitySelection r2 = comp.Evaluate(ctx, &attackFire, 0.0f);
    EXPECT_EQ(r2.actionId, Dia::Core::StringCRC());
    EXPECT_EQ(attackFire.count, 1);

    // Third eval (dt=0.6): cooldown 0.5s expires → Attack wins again
    Dia::UtilityAI::UtilitySelection r3 = comp.Evaluate(ctx, &attackFire, 0.6f);
    EXPECT_EQ(r3.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_EQ(attackFire.count, 2);
}

// ---------------------------------------------------------------------------
// 2. Group cap and personality both apply: cap enforcement overrides bias
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, GroupCap_And_PersonalityBothApply)
{
    // ActionA: max_concurrent=2; with Aggressive profile scored high.
    // Two already active → cap hit → falls back to ActionB.
    // score.val=0.3 → ActionA raw=0.3, ActionB raw=0.7
    // Aggressive: ActionA*4.0=1.2 → but cap is hit → ActionB wins at 0.7
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kGroupCapTwoActions));

    Dia::Rules::RuleActionRegistry reg;
    reg.Register(Dia::Core::StringCRC("ActionA"), [](void*){});
    reg.Register(Dia::Core::StringCRC("ActionB"), [](void*){});
    comp.SetRegistry(&reg);

    Dia::UtilityAI::GroupConsiderationContext group;
    group.Increment(Dia::Core::StringCRC("ActionA"));
    group.Increment(Dia::Core::StringCRC("ActionA"));  // at cap=2
    comp.SetGroupContext(&group);

    Dia::UtilityAI::PersonalityProfile profile = LoadProfile(R"({
        "name": "Aggressive",
        "biases": [
            { "action": "ActionA", "multiplier": 4.0 }
        ]
    })");
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("score"), Dia::Core::StringCRC("val"), 0.3f);

    Dia::UtilityAI::UtilitySelection result = comp.Evaluate(ctx, nullptr, 0.0f);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("ActionB"));
}

// ---------------------------------------------------------------------------
// 3. AsyncEval with personality: callback receives personality-biased winner
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, AsyncEval_PersonalityApplied_CallbackGetsCorrectWinner)
{
    // health=40: raw Attack=0.4, raw Flee=0.6 → without personality Flee wins.
    // Aggressive profile: Attack*2.0=0.8 → Attack wins in async callback.
    Dia::UtilityAI::UtilitySet set = LoadSet(kAsyncTwoActions);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 40.0f);

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void*){});
    registry.Register(Dia::Core::StringCRC("Flee"),   [](void*){});

    Dia::UtilityAI::PersonalityProfile profile = LoadProfile(R"({
        "name": "Aggressive",
        "biases": [
            { "action": "Attack", "multiplier": 2.0 },
            { "action": "Flee",   "multiplier": 0.5 }
        ]
    })");

    Dia::SimTime::SimTimeBudget budget;
    AsyncResult result;

    set.EvaluateAsync(ctx, registry, nullptr, budget, AsyncCallback, &result,
                      nullptr, &profile);

    budget.RunOneShots(1000.0f);

    EXPECT_EQ(result.callCount, 1);
    EXPECT_EQ(result.sel.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.sel.score, 0.8f, 0.01f);
}

// ---------------------------------------------------------------------------
// 4. ComponentEvalPeriod × cooldown: period=2; cooldown blocks on same tick
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, ComponentEvalPeriod_CooldownInteraction)
{
    // period=2: evaluations fire on calls 2, 4, 6, …
    // cooldown=0.5s; no dt passed → second evaluation (call 4) still in cooldown
    const char* kAttackCooldown = R"({
        "actions": [
            { "id": "Attack", "scorers": [], "cooldown": 0.5 }
        ]
    })";

    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kAttackCooldown));

    Dia::Rules::RuleActionRegistry reg;
    FireCounter fire;
    reg.Register(Dia::Core::StringCRC("Attack"), IncrementFire);
    comp.SetRegistry(&reg);

    Dia::UtilityAI::PersonalityProfile profile = LoadProfile(
        R"({"name":"Throttled","eval_period_ticks":2,"biases":[]})");
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;

    // Call 1: skipped (counter=1, 1%2 != 0)
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 0);

    // Call 2: evaluates (counter=2, 2%2==0) → Attack fires, cooldown starts
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 1);

    // Call 3: skipped (counter=3)
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 1);

    // Call 4: evaluates (counter=4), still in cooldown (dt=0) → suppressed
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 1);

    // Call 5: skipped
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 1);

    // Call 6: evaluates (counter=6), dt=0.6 → cooldown expires → fires again
    comp.Evaluate(ctx, &fire, 0.6f);
    EXPECT_EQ(fire.count, 2);
}

// ---------------------------------------------------------------------------
// 5. SetPersonality resets the eval counter
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, ComponentSetPersonality_ResetsCounter)
{
    // period=3: counter must reach 3 to evaluate.
    // After 2 calls (counter=2), SetPersonality resets counter to 0.
    // Next 3 calls: call1=1%3 skip, call2=2%3 skip, call3=3%3 fire → fireCount=1
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kAttackNoCooldown));

    Dia::Rules::RuleActionRegistry reg;
    FireCounter fire;
    reg.Register(Dia::Core::StringCRC("Attack"), IncrementFire);
    comp.SetRegistry(&reg);

    Dia::UtilityAI::PersonalityProfile profile = LoadProfile(
        R"({"name":"Slow","eval_period_ticks":3,"biases":[]})");
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;

    // Two calls advance counter to 2 — no fire yet
    comp.Evaluate(ctx, &fire, 0.0f);
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 0);

    // SetPersonality resets counter back to 0
    comp.SetPersonality(&profile);

    // Now 3 more calls: 1, 2, 3 → fires on third
    comp.Evaluate(ctx, &fire, 0.0f);
    comp.Evaluate(ctx, &fire, 0.0f);
    comp.Evaluate(ctx, &fire, 0.0f);
    EXPECT_EQ(fire.count, 1);
}

// ---------------------------------------------------------------------------
// 6. All actions on cooldown: Evaluate returns no selection
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, MultipleActionsAllOnCooldown_ReturnsNoSelection)
{
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kThreeActionsWithCooldown));

    Dia::Rules::RuleActionRegistry reg;
    reg.Register(Dia::Core::StringCRC("ActionX"), [](void*){});
    reg.Register(Dia::Core::StringCRC("ActionY"), [](void*){});
    reg.Register(Dia::Core::StringCRC("ActionZ"), [](void*){});
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("v"), 9.0f);
    ctx.SetFloat(Dia::Core::StringCRC("y"), Dia::Core::StringCRC("v"), 7.0f);
    ctx.SetFloat(Dia::Core::StringCRC("z"), Dia::Core::StringCRC("v"), 5.0f);

    // First eval: ActionX wins (score=0.9), starts cooldown
    comp.Evaluate(ctx, nullptr, 0.0f);

    // Make ActionX score 0 so ActionY wins next; ActionY fires + cooldown starts
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("v"), 0.0f);
    comp.Evaluate(ctx, nullptr, 0.0f);

    // Make ActionY score 0 so ActionZ wins; ActionZ fires + cooldown starts
    ctx.SetFloat(Dia::Core::StringCRC("y"), Dia::Core::StringCRC("v"), 0.0f);
    comp.Evaluate(ctx, nullptr, 0.0f);

    // Restore all scores; all three are now in cooldown → no selection
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("v"), 9.0f);
    ctx.SetFloat(Dia::Core::StringCRC("y"), Dia::Core::StringCRC("v"), 7.0f);

    Dia::UtilityAI::UtilitySelection result = comp.Evaluate(ctx, nullptr, 0.0f);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, kTol);
}

// ---------------------------------------------------------------------------
// 7. Cooldown expiry: after advancing dt past cooldown, correct winner re-fires
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, CooldownExpiry_AllActionsRefire_CorrectWinner)
{
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(LoadSet(kThreeActionsWithCooldown));

    Dia::Rules::RuleActionRegistry reg;
    FireCounter xFire;
    reg.Register(Dia::Core::StringCRC("ActionX"), IncrementFire);
    reg.Register(Dia::Core::StringCRC("ActionY"), [](void*){});
    reg.Register(Dia::Core::StringCRC("ActionZ"), [](void*){});
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("x"), Dia::Core::StringCRC("v"), 9.0f);
    ctx.SetFloat(Dia::Core::StringCRC("y"), Dia::Core::StringCRC("v"), 5.0f);
    ctx.SetFloat(Dia::Core::StringCRC("z"), Dia::Core::StringCRC("v"), 3.0f);

    // ActionX wins on first eval; cooldown=0.3s starts
    comp.Evaluate(ctx, &xFire, 0.0f);
    EXPECT_EQ(xFire.count, 1);

    // Second eval: ActionX still in cooldown (dt=0) → no selection
    Dia::UtilityAI::UtilitySelection blocked = comp.Evaluate(ctx, &xFire, 0.0f);
    EXPECT_EQ(blocked.actionId, Dia::Core::StringCRC());
    EXPECT_EQ(xFire.count, 1);

    // Third eval: dt=0.4 → 0.4 > 0.3s cooldown → ActionX wins again
    Dia::UtilityAI::UtilitySelection allowed = comp.Evaluate(ctx, &xFire, 0.4f);
    EXPECT_EQ(allowed.actionId, Dia::Core::StringCRC("ActionX"));
    EXPECT_EQ(xFire.count, 2);
}

// ---------------------------------------------------------------------------
// 8. Validate with real ConditionRegistry: no errors when registry has the slot
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, Validate_WithRealConditionRegistry_Passes)
{
    // Action has a prerequisite: enemy.visible == true
    // And a scorer reading health.value
    // Both registered → Validate returns true
    const char* kValidatableSet = R"({
        "actions": [
            {
                "id": "Attack",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    Dia::UtilityAI::UtilitySet set = LoadSet(kValidatableSet);

    // Set up a real ConditionRegistry with both slots registered
    struct TestData { bool visible; float health; };
    TestData data{ true, 80.0f };

    // Bare function pointers required — no captures
    static auto GetVisible = [](void* d) -> bool  { return static_cast<TestData*>(d)->visible; };
    static auto GetHealth  = [](void* d) -> float { return static_cast<TestData*>(d)->health;  };

    Dia::Condition::ConditionRegistry registry(&data);
    registry.RegisterBool (Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("visible"), GetVisible);
    registry.RegisterFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"),   GetHealth);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    const bool valid = set.Validate(registry, errors);

    EXPECT_TRUE(valid);
    EXPECT_EQ(static_cast<int>(errors.Size()), 0);
}

// ---------------------------------------------------------------------------
// 10. Full schema example: Attack vs Flee; correct winner with combined score
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_Integration, LoadFromJson_FullSchemaExample_AllFieldsParsed)
{
    // Attack:
    //   prereq: enemy.visible == true (met)
    //   scorer 1: health.value=80, range=100, norm=0.8, linear → 0.8
    //   scorer 2: enemy.distance=5, range=20, norm=0.25, quadratic(exp=2) inverted
    //             → 1 - 0.25^2 = 1 - 0.0625 = 0.9375
    //   Attack score = 0.8 * 0.9375 = 0.75
    //
    // Flee:
    //   prereq: health.value < 20.0 → 80 < 20 = false → not eligible
    //
    // Expected: Attack wins with score ≈ 0.75
    const char* kFullExample = R"({
        "actions": [
            {
                "id": "Attack",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } },
                    { "slot": "enemy", "field": "distance",
                      "input_min": 0.0, "input_max": 20.0,
                      "curve": { "shape": "quadratic", "exponent": 2.0, "invert": true } }
                ],
                "cooldown": 0.5,
                "max_concurrent": 2
            },
            {
                "id": "Flee",
                "prerequisite": { "op": "<", "slot": "health", "field": "value", "value": 20.0 },
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 20.0,
                      "curve": { "shape": "logistic", "invert": true } }
                ],
                "cooldown": 1.0,
                "max_concurrent": 0
            }
        ]
    })";

    Dia::UtilityAI::UtilitySet set = LoadSet(kFullExample);
    EXPECT_EQ(set.GetActionCount(), 2);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool (Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("visible"),  true);
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"),    80.0f);
    ctx.SetFloat(Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("distance"),  5.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    // 0.8 * (1 - 0.0625) = 0.8 * 0.9375 = 0.75
    EXPECT_NEAR(result.score, 0.75f, 0.01f);
}
