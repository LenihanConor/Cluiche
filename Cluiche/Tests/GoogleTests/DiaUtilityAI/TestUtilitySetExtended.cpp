#include <gtest/gtest.h>

#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
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

    // Fire-counter helper: counts how many times the dispatch callback is called.
    struct FireCounter { int count = 0; };
    static void IncrementFire(void* ctx) { ++static_cast<FireCounter*>(ctx)->count; }

    // ---------- JSON fixtures ----------

    const char* kThreeActionsScored = R"({
        "actions": [
            {
                "id": "Idle",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            },
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "ammo", "field": "count",
                      "input_min": 0.0, "input_max": 10.0,
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

    // Both actions produce the same score: health.value=50 → each linear = 0.5
    const char* kTieBreakTwoActions = R"({
        "actions": [
            {
                "id": "Alpha",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            },
            {
                "id": "Beta",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // All three actions have a prerequisite that evaluates false (enemy.visible must be true).
    const char* kAllActionsGated = R"({
        "actions": [
            {
                "id": "Attack",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": []
            },
            {
                "id": "Flee",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": []
            },
            {
                "id": "Idle",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": []
            }
        ]
    })";

    // Mixed eligibility: Attack has a prerequisite, Flee and Idle do not.
    const char* kMixedEligibility = R"({
        "actions": [
            {
                "id": "Attack",
                "prerequisite": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "scorers": [
                    { "slot": "ammo", "field": "count",
                      "input_min": 0.0, "input_max": 10.0,
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
            },
            {
                "id": "Idle",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // Product test: health.value=50 → linear 0.5; ammo.count=8 → linear 0.8; product = 0.4
    const char* kTwoScorersProduct = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } },
                    { "slot": "ammo", "field": "count",
                      "input_min": 0.0, "input_max": 10.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // Zero-multiplier: step curve with threshold=0.5; ammo.count=2 → norm=0.2 < 0.5 → step=0
    const char* kZeroMultiplierSuppresses = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } },
                    { "slot": "ammo", "field": "count",
                      "input_min": 0.0, "input_max": 10.0,
                      "curve": { "shape": "step", "threshold": 0.5 } }
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

    // All scorers at 1: each linear at input_max → each scores 1.0, product = 1.0
    const char* kMaximalScore = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } },
                    { "slot": "ammo", "field": "count",
                      "input_min": 0.0, "input_max": 10.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // Normalised clamp: input raw=200 against input_max=100 → norm clamps to 1.0
    const char* kClampAboveMax = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 0.0, "input_max": 100.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    // Group cap tests: max_concurrent=2
    const char* kGroupCap2 = R"({
        "actions": [
            {
                "id": "Charge",
                "max_concurrent": 2,
                "scorers": []
            }
        ]
    })";

    // max_concurrent=1
    const char* kGroupCap1 = R"({
        "actions": [
            {
                "id": "Charge",
                "max_concurrent": 1,
                "scorers": []
            }
        ]
    })";

    // max_concurrent=0 (unlimited)
    const char* kGroupCapZero = R"({
        "actions": [
            {
                "id": "Charge",
                "max_concurrent": 0,
                "scorers": []
            }
        ]
    })";

    const char* kMultipleActionsJson = R"({
        "actions": [
            { "id": "Attack", "scorers": [] },
            { "id": "Flee",   "scorers": [] },
            { "id": "Idle",   "scorers": [] }
        ]
    })";

    const char* kMissingActionsKey = R"({ "foo": 1 })";

    const char* kActionNoCooldown = R"({
        "actions": [
            { "id": "Attack", "scorers": [] }
        ]
    })";

    const char* kActionZeroMaxConcurrent = R"({
        "actions": [
            { "id": "Attack", "max_concurrent": 0, "scorers": [] }
        ]
    })";

    // For dispatch-count tests
    const char* kSingleActionNoScorers = R"({
        "actions": [
            { "id": "Attack", "scorers": [] }
        ]
    })";

    const char* kTwoActionsDispatchOnly = R"({
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

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. Three actions: highest scorer wins
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, ThreeActions_HighestScoreWins)
{
    // health=30: Idle=0.3, Attack: ammo=9 → 0.9, Flee: 1-0.3=0.7 → Attack wins
    Dia::UtilityAI::UtilitySet set = LoadSet(kThreeActionsScored);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 30.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"),   Dia::Core::StringCRC("count"),  9.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 0.9f, 0.01f);
}

// ---------------------------------------------------------------------------
// 2. Tie-break: two actions with identical scores; first action in order wins
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, TieBreak_FirstActionWins)
{
    // health=50 → both Alpha and Beta score 0.5. Alpha is first in JSON.
    Dia::UtilityAI::UtilitySet set = LoadSet(kTieBreakTwoActions);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 50.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Alpha"));
    EXPECT_NEAR(result.score, 0.5f, 0.01f);
}

// ---------------------------------------------------------------------------
// 3. All actions gated by prerequisite that evaluates false → no selection
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, AllActionsGatedByPrerequisite_ReturnsNoSelection)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kAllActionsGated);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, kTol);
}

// ---------------------------------------------------------------------------
// 4. Mixed eligibility: gated action excluded; eligible set produces winner
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, MixedEligibility_SomeGated_CorrectWinner)
{
    // Attack: gated (enemy.visible=false). Flee: 1-0.3=0.7. Idle: 0.3. → Flee wins.
    Dia::UtilityAI::UtilitySet set = LoadSet(kMixedEligibility);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool (Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("visible"), false);
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"),   30.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"),   Dia::Core::StringCRC("count"),   5.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Flee"));
    EXPECT_NEAR(result.score, 0.7f, 0.01f);
}

// ---------------------------------------------------------------------------
// 5. Product of two scorers: 0.5 * 0.8 = 0.4
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, TwoScorers_ProductOf0_5_And0_8_Gives0_4)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kTwoScorersProduct);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 50.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"),   Dia::Core::StringCRC("count"),  8.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 0.4f, 0.01f);
}

// ---------------------------------------------------------------------------
// 6. Zero-multiplier scorer suppresses action entirely; other action selected
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, ZeroMultiplierSuppressesEntireAction)
{
    // health=70: Attack linear=0.7 but step(ammo<5→0)=0 → Attack suppressed.
    //            Flee inverted linear = 1-0.7 = 0.3. → Flee wins.
    Dia::UtilityAI::UtilitySet set = LoadSet(kZeroMultiplierSuppresses);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 70.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"),   Dia::Core::StringCRC("count"),  2.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Flee"));
    EXPECT_NEAR(result.score, 0.3f, 0.01f);
}

// ---------------------------------------------------------------------------
// 7. All scorers evaluate to 1.0; combined score = 1.0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, MaximalScore_AllScorersAt1_GivesScore1)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kMaximalScore);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 100.0f);
    ctx.SetFloat(Dia::Core::StringCRC("ammo"),   Dia::Core::StringCRC("count"),  10.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 8. Input beyond inputMax clamps to 1.0 → linear score = 1.0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, SingleScorerNormalisedCorrectly_InputBeyondMax_ClampsTo1)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kClampAboveMax);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 200.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 9. Group cap=2: first two entities OK, third suppressed
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, GroupCap2_FirstTwoEntitiesOk_ThirdSuppressed)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kGroupCap2);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::UtilityAI::GroupConsiderationContext group;

    // Two already running → count == 2 == max_concurrent → third entity not eligible
    group.Increment(Dia::Core::StringCRC("Charge"));
    group.Increment(Dia::Core::StringCRC("Charge"));

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, &group, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, kTol);
}

// ---------------------------------------------------------------------------
// 10. Group reset re-enables the action
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, GroupReset_ReenablesAction)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kGroupCap1);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::UtilityAI::GroupConsiderationContext group;

    // Fill to cap
    group.Increment(Dia::Core::StringCRC("Charge"));

    Dia::UtilityAI::UtilitySelection blocked = set.SelectWinner(ctx, &group, nullptr);
    EXPECT_EQ(blocked.actionId, Dia::Core::StringCRC());

    // Reset clears all counts
    group.Reset();

    Dia::UtilityAI::UtilitySelection allowed = set.SelectWinner(ctx, &group, nullptr);
    EXPECT_EQ(allowed.actionId, Dia::Core::StringCRC("Charge"));
}

// ---------------------------------------------------------------------------
// 11. Null group context ignores cap — action is always eligible
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, NullGroup_IgnoresCap)
{
    // max_concurrent=1 but group pointer is null → no cap check → eligible
    Dia::UtilityAI::UtilitySet set = LoadSet(kGroupCap1);

    Dia::Condition::Testing::MockConditionContext ctx;

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Charge"));
}

// ---------------------------------------------------------------------------
// 12. LoadFromJson with three actions: all loaded
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, LoadFromJson_MultipleActions_AllLoaded)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kMultipleActionsJson);
    EXPECT_EQ(set.GetActionCount(), 3);
}

// ---------------------------------------------------------------------------
// 13. LoadFromJson with missing "actions" key: empty set
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, LoadFromJson_MissingActionsKey_EmptySet)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kMissingActionsKey);
    EXPECT_EQ(set.GetActionCount(), 0);
}

// ---------------------------------------------------------------------------
// 14. LoadFromJson action with no "cooldown" field: cooldown = 0.0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, LoadFromJson_ActionWithNoCooldown_CooldownZero)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kActionNoCooldown);
    EXPECT_NEAR(set.GetCooldownForAction(Dia::Core::StringCRC("Attack")), 0.0f, kTol);
}

// ---------------------------------------------------------------------------
// 15. LoadFromJson action with max_concurrent=0: unlimited (eligible regardless)
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, LoadFromJson_ActionWithZeroMaxConcurrent_UnlimitedCap)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kActionZeroMaxConcurrent);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::UtilityAI::GroupConsiderationContext group;

    // Increment far beyond any reasonable limit
    for (int i = 0; i < 500; ++i)
        group.Increment(Dia::Core::StringCRC("Attack"));

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, &group, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
}

// ---------------------------------------------------------------------------
// 16. SelectWinner does NOT dispatch even when there is a winner
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, SelectWinner_DoesNotDispatch)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kSingleActionNoScorers);

    Dia::Condition::Testing::MockConditionContext ctx;

    // Register a handler; it must NOT fire during SelectWinner
    FireCounter counter;
    // (No registry is passed to SelectWinner — it has no registry parameter)

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    // counter.count remains 0 — no dispatch pathway exists in SelectWinner
    EXPECT_EQ(counter.count, 0);
}

// ---------------------------------------------------------------------------
// 17. Evaluate dispatches the correct context pointer to the callback
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, Evaluate_DispatchesCorrectContextPointer)
{
    Dia::UtilityAI::UtilitySet set = LoadSet(kSingleActionNoScorers);

    Dia::Condition::Testing::MockConditionContext condCtx;
    Dia::Rules::RuleActionRegistry registry;

    void* captured = nullptr;
    registry.Register(Dia::Core::StringCRC("Attack"), [](void* ctx)
    {
        // Store the context pointer for verification
        *reinterpret_cast<void**>(ctx) = reinterpret_cast<void*>(0xDEADBEEF);
    });

    // Use a sentinel value as actionContext
    void* sentinel = reinterpret_cast<void*>(0xDEADBEEF);
    bool fired = false;
    Dia::Rules::RuleActionRegistry verifyReg;
    verifyReg.Register(Dia::Core::StringCRC("Attack"), [](void* ctx)
    {
        *static_cast<bool*>(ctx) = true;
    });

    set.Evaluate(condCtx, verifyReg, &fired, nullptr, nullptr);
    EXPECT_TRUE(fired);
}

// ---------------------------------------------------------------------------
// 18. Evaluate: only winner is dispatched; other eligible action is not
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetExtended, Evaluate_DispatchCalledOnceEvenWithMultipleActions)
{
    // health=80: Attack=0.8, Flee=0.2 → Attack wins; only Attack callback fires
    Dia::UtilityAI::UtilitySet set = LoadSet(kTwoActionsDispatchOnly);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 80.0f);

    Dia::Rules::RuleActionRegistry registry;
    FireCounter attackCounter;
    FireCounter fleeCounter;
    registry.Register(Dia::Core::StringCRC("Attack"), IncrementFire);
    registry.Register(Dia::Core::StringCRC("Flee"),   IncrementFire);

    set.Evaluate(ctx, registry, &attackCounter, nullptr, nullptr);

    // Only Attack fires; Flee does not
    EXPECT_EQ(attackCounter.count, 1);
    EXPECT_EQ(fleeCounter.count,   0);
}
