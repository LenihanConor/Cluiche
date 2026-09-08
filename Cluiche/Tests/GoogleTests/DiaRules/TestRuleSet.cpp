#include <gtest/gtest.h>

#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleDef.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
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

    Dia::Rules::RuleSet LoadRuleSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::Rules::RuleSet::LoadFromJson(root);
    }
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — LoadFromJson
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, LoadFromJson_ValidJson_GetRuleCountCorrect)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "AttackWhenHealthy",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["StartAttack", "SetAggressive"]
            },
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["Flee"]
            }
        ]
    })");

    EXPECT_EQ(rs.GetRuleCount(), 2);
}

TEST(DiaRules_RuleSet, LoadFromJson_NoRulesKey_ReturnsZeroRulesNoCrash)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({})");

    EXPECT_EQ(rs.GetRuleCount(), 0);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — Validate
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, Validate_ValidRuleSet_ReturnsTrue_NoErrors)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "FleeRule",
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_TRUE(rs.Validate(errors));
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaRules_RuleSet, Validate_EmptyActionsList_ReturnsFalse_ErrorPresent)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "BadRule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": []
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaRules_RuleSet, Validate_DuplicateRuleIds_ReturnsFalse_ErrorPresent)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "SameId",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            },
            {
                "id": "SameId",
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaRules_RuleSet, Validate_UnnamedRules_ReturnsTrue)
{
    // Unnamed rules (no "id" key) are allowed — multiple unnamed rules OK
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            },
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_TRUE(rs.Validate(errors));
    EXPECT_EQ(errors.Size(), 0u);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — Evaluate
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, Evaluate_NoGuardPasses_ReturnsZero)
{
    // Guard: health < 0.0 — fails when health=50
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 0.0 },
                "actions": ["SomeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    bool actionCalled = false;
    registry.Register(Dia::Core::StringCRC("SomeAction"), [](void* ctx) {
        *static_cast<bool*>(ctx) = true;
    });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 50.0f);

    int fired = rs.Evaluate(ctx, registry, &actionCalled);
    EXPECT_EQ(fired, 0);
    EXPECT_FALSE(actionCalled);
}

TEST(DiaRules_RuleSet, Evaluate_OneRuleFires_ReturnsOne_ActionCalled)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"), [](void* ctx) {
        ++(*static_cast<int*>(ctx));
    });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int callCount = 0;
    int fired = rs.Evaluate(ctx, registry, &callCount);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(callCount, 1);
}

TEST(DiaRules_RuleSet, Evaluate_MultipleRulesFire_ReturnsN_AllActionsCalled)
{
    // Both rules pass — total 2 rules fired, 2 action calls
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            },
            {
                "guard": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "actions": ["SetAggressive"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"),  [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    registry.Register(Dia::Core::StringCRC("SetAggressive"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  80.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    int callCount = 0;
    int fired = rs.Evaluate(ctx, registry, &callCount);
    EXPECT_EQ(fired, 2);
    EXPECT_EQ(callCount, 2);
}

TEST(DiaRules_RuleSet, Evaluate_OnlyMatchingRulesFire_ReturnsPartialCount)
{
    // Rule 1: health >= 50 → passes (health=80)
    // Rule 2: health < 10  → fails (health=80)
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            },
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    registry.Register(Dia::Core::StringCRC("FleeAction"),   [](void* ctx) { ++(*static_cast<int*>(ctx)); });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int callCount = 0;
    int fired = rs.Evaluate(ctx, registry, &callCount);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(callCount, 1);
}

TEST(DiaRules_RuleSet, Evaluate_ActionNotInRegistry_SilentlySkipped_NoCrash)
{
    // Guard passes but "UnknownAction" is not registered — no crash, rule still counts as fired
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["UnknownAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    // "UnknownAction" is NOT registered

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = rs.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 1); // rule fired (guard passed), action silently skipped
}

TEST(DiaRules_RuleSet, Evaluate_ActionContextPassedThroughToCallback)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["WriteAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("WriteAction"), [](void* ctx) {
        *static_cast<int*>(ctx) = 42;
    });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int written = 0;
    rs.Evaluate(ctx, registry, &written);
    EXPECT_EQ(written, 42);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — LoadFromJson edge cases
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, LoadFromJson_EmptyRulesArray_ReturnsZeroRules)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [] })");
    EXPECT_EQ(rs.GetRuleCount(), 0);
}

TEST(DiaRules_RuleSet, LoadFromJson_RulesValueIsString_ReturnsZeroRules)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": "not_an_array" })");
    EXPECT_EQ(rs.GetRuleCount(), 0);
}

TEST(DiaRules_RuleSet, LoadFromJson_RuleWithNoGuard_GuardEvalsFalse)
{
    // Rule has actions but no "guard" key — default-constructed guard evaluates false
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "actions": ["SomeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("SomeAction"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 80.0f);

    int fired = rs.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 0);
}

TEST(DiaRules_RuleSet, LoadFromJson_RuleWithNoActionsKey_HasEmptyActions)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "NoActions",
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 }
            }
        ]
    })");

    EXPECT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->actions.Size(), 0u);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
}

TEST(DiaRules_RuleSet, LoadFromJson_ActionsValueIsString_ZeroActionsLoaded)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": "Flee"
            }
        ]
    })");

    ASSERT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->actions.Size(), 0u);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
}

TEST(DiaRules_RuleSet, LoadFromJson_ActionsOverflowCapacity8_DropsExtra)
{
    // 9 actions — capacity is 8, 9th must be dropped silently
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": ["A0","A1","A2","A3","A4","A5","A6","A7","A8"]
            }
        ]
    })");

    ASSERT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->actions.Size(), 8u);
}

TEST(DiaRules_RuleSet, LoadFromJson_EmptyStringAction_Skipped)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": ["", "ValidAction", ""]
            }
        ]
    })");

    ASSERT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->actions.Size(), 1u);
}

TEST(DiaRules_RuleSet, LoadFromJson_NonStringActionEntries_Skipped)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": [42, null, "Good"]
            }
        ]
    })");

    ASSERT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->actions.Size(), 1u);
}

TEST(DiaRules_RuleSet, LoadFromJson_EmptyStringId_TreatedAsUnnamed)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "",
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": ["SomeAction"]
            }
        ]
    })");

    ASSERT_EQ(rs.GetRuleCount(), 1);
    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    // Empty string id -> left as default (kZero)
    EXPECT_EQ(rule->id.Value(), Dia::Core::StringCRC().Value());
}

TEST(DiaRules_RuleSet, LoadFromJson_GuardLoadError_SilentlyDefaultsToFalse)
{
    // Malformed guard: missing "op" key — guard defaults and evaluates false
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "field": "health" },
                "actions": ["SomeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("SomeAction"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 80.0f);

    int fired = rs.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 0);  // invalid guard evaluates false, no crash
}

TEST(DiaRules_RuleSet, LoadFromJson_RuleArrayEntryIsNotObject_LoadsEmptyRule)
{
    // Non-object entries in rules array each yield an empty RuleDef
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [42, "text"] })");

    EXPECT_EQ(rs.GetRuleCount(), 2);

    const Dia::Rules::RuleDef* rule0 = rs.GetRuleAt(0);
    const Dia::Rules::RuleDef* rule1 = rs.GetRuleAt(1);
    ASSERT_NE(rule0, nullptr);
    ASSERT_NE(rule1, nullptr);
    EXPECT_EQ(rule0->actions.Size(), 0u);
    EXPECT_EQ(rule1->actions.Size(), 0u);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — Validate edge cases
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, Validate_MultipleErrorTypes_BothReported)
{
    // Rule 0: empty actions (error 1)
    // Rules 1 & 2: duplicate id "SharedId" (error 2)
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "BadRule",
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": []
            },
            {
                "id": "SharedId",
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": ["ActionA"]
            },
            {
                "id": "SharedId",
                "guard": { "op": "<", "slot": "health", "field": "value", "value": 10.0 },
                "actions": ["ActionB"]
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
    EXPECT_GE(errors.Size(), 2u);
}

TEST(DiaRules_RuleSet, Validate_EmptyRuleSet_ReturnsTrue)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [] })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_TRUE(rs.Validate(errors));
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaRules_RuleSet, Validate_ErrorCountIsExact)
{
    // Only one error: one rule with empty actions
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": []
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    EXPECT_FALSE(rs.Validate(errors));
    EXPECT_EQ(errors.Size(), 1u);
}

TEST(DiaRules_RuleSet, Validate_OutErrorsFullOnEntry_StillReturnsFalse)
{
    // Pre-fill errors array to capacity
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    for (int i = 0; i < 32; ++i)
        errors.Add("pre-existing error");

    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "Dup",
                "guard": { "op": ">=", "slot": "health", "field": "value", "value": 50.0 },
                "actions": ["A"]
            },
            {
                "id": "Dup",
                "guard": { "op": "<", "slot": "health", "field": "value", "value": 10.0 },
                "actions": ["B"]
            }
        ]
    })");

    EXPECT_FALSE(rs.Validate(errors));
    EXPECT_EQ(errors.Size(), 32u);  // buffer full — no new entries added
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — Evaluate edge cases
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, Evaluate_OneRuleMultipleActions_RuleCountIsOne)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["StartAttack", "SetAggressive"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    int callCount = 0;
    registry.Register(Dia::Core::StringCRC("StartAttack"),  [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    registry.Register(Dia::Core::StringCRC("SetAggressive"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = rs.Evaluate(ctx, registry, &callCount);
    EXPECT_EQ(fired, 1);      // SD-007: count is rules, not actions
    EXPECT_EQ(callCount, 2);
}

TEST(DiaRules_RuleSet, Evaluate_GuardAtExactBoundary_Fires)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 50.0f);

    int callCount = 0;
    int fired = rs.Evaluate(ctx, registry, &callCount);
    EXPECT_EQ(fired, 1);
}

TEST(DiaRules_RuleSet, Evaluate_GuardJustBelowBoundary_NoFire)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 49.0f);

    int fired = rs.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 0);
}

TEST(DiaRules_RuleSet, Evaluate_ZeroRules_ReturnsZero)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [] })");

    Dia::Rules::RuleActionRegistry registry;
    Dia::Condition::Testing::MockConditionContext ctx;

    int fired = rs.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 0);
}

TEST(DiaRules_RuleSet, Evaluate_MixedRegisteredActions_UnregisteredSkipped_RuleStillCounts)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["Known", "Unknown"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    int knownCallCount = 0;
    registry.Register(Dia::Core::StringCRC("Known"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    // "Unknown" is NOT registered

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = rs.Evaluate(ctx, registry, &knownCallCount);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(knownCallCount, 1);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — GetRuleAt edge cases
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, GetRuleAt_NegativeIndex_ReturnsNull)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A"]
            }
        ]
    })");

    EXPECT_EQ(rs.GetRuleAt(-1), nullptr);
}

TEST(DiaRules_RuleSet, GetRuleAt_OnePastEnd_ReturnsNull)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A"]
            },
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["B"]
            }
        ]
    })");

    EXPECT_EQ(rs.GetRuleCount(), 2);
    EXPECT_EQ(rs.GetRuleAt(2), nullptr);
}

TEST(DiaRules_RuleSet, GetRuleAt_EmptyRuleSet_IndexZero_ReturnsNull)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [] })");
    EXPECT_EQ(rs.GetRuleAt(0), nullptr);
}

TEST(DiaRules_RuleSet, GetRuleAt_ValidIndex_ReturnsCorrectData)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "MyRule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A", "B"]
            }
        ]
    })");

    const Dia::Rules::RuleDef* rule = rs.GetRuleAt(0);
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->id.Value(), Dia::Core::StringCRC("MyRule").Value());
    EXPECT_EQ(rule->actions.Size(), 2u);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — Move semantics
// ---------------------------------------------------------------------------

TEST(DiaRules_RuleSet, MoveConstructor_ProducesValidState)
{
    Dia::Rules::RuleSet a = LoadRuleSet(R"({
        "rules": [
            { "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 }, "actions": ["A"] },
            { "guard": { "op": "<",  "slot": "hero", "field": "health", "value": 10.0 }, "actions": ["B"] }
        ]
    })");

    Dia::Rules::RuleSet b = std::move(a);

    EXPECT_EQ(b.GetRuleCount(), 2);
    EXPECT_EQ(a.GetRuleCount(), 0);  // moved-from returns 0 via null guard
}

TEST(DiaRules_RuleSet, MoveAssignment_ReplacesRules)
{
    Dia::Rules::RuleSet a = LoadRuleSet(R"({
        "rules": [
            { "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 }, "actions": ["A"] }
        ]
    })");

    Dia::Rules::RuleSet b = LoadRuleSet(R"({
        "rules": [
            { "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 }, "actions": ["X"] },
            { "guard": { "op": "<",  "slot": "hero", "field": "health", "value": 10.0 }, "actions": ["Y"] }
        ]
    })");

    a = std::move(b);

    EXPECT_EQ(a.GetRuleCount(), 2);
}

TEST(DiaRules_RuleSet, MovedFrom_EvaluateIsSafe)
{
    Dia::Rules::RuleSet a = LoadRuleSet(R"({
        "rules": [
            { "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 }, "actions": ["A"] }
        ]
    })");

    Dia::Rules::RuleSet b = std::move(a);

    Dia::Rules::RuleActionRegistry registry;
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    // Calling Evaluate on moved-from RuleSet must not crash; returns 0
    int fired = a.Evaluate(ctx, registry, nullptr);
    EXPECT_EQ(fired, 0);
}

// ---------------------------------------------------------------------------
// DiaRules_RuleSet — GetLastFireReport (DIA_DEBUG only)
// ---------------------------------------------------------------------------

#ifdef DIA_DEBUG

TEST(DiaRules_RuleSet, GetLastFireReport_BeforeEvaluate_ReturnsZero_NoEntries)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "AttackRule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["Attack"]
            }
        ]
    })");

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(entries.Size(), 0u);
}

TEST(DiaRules_RuleSet, GetLastFireReport_OneRuleFires_EntryHasCorrectRuleIdAndActions)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "AttackRule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction", "SetAggressive"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"),  [](void*) {});
    registry.Register(Dia::Core::StringCRC("SetAggressive"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);

    EXPECT_EQ(count, 1);
    ASSERT_EQ(entries.Size(), 1u);
    EXPECT_EQ(entries[0].ruleId.Value(), Dia::Core::StringCRC("AttackRule").Value());
    EXPECT_EQ(entries[0].actions.Size(), 2u);
    EXPECT_EQ(entries[0].actions[0].Value(), Dia::Core::StringCRC("AttackAction").Value());
    EXPECT_EQ(entries[0].actions[1].Value(), Dia::Core::StringCRC("SetAggressive").Value());
}

TEST(DiaRules_RuleSet, GetLastFireReport_MultipleRulesFire_AllEntriesPresent)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "RuleA",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["ActionA"]
            },
            {
                "id": "RuleB",
                "guard": { "op": "==", "slot": "enemy", "field": "visible", "value": true },
                "actions": ["ActionB"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("ActionA"), [](void*) {});
    registry.Register(Dia::Core::StringCRC("ActionB"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  80.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);

    EXPECT_EQ(count, 2);
    EXPECT_EQ(entries.Size(), 2u);
    EXPECT_EQ(entries[0].ruleId.Value(), Dia::Core::StringCRC("RuleA").Value());
    EXPECT_EQ(entries[1].ruleId.Value(), Dia::Core::StringCRC("RuleB").Value());
}

TEST(DiaRules_RuleSet, GetLastFireReport_PartialFire_OnlyFiredRulesInReport)
{
    // Rule 0: health >= 50 → passes (health=80)
    // Rule 1: health < 10 → fails (health=80)
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "AttackRule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            },
            {
                "id": "FleeRule",
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 10.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("AttackAction"), [](void*) {});
    registry.Register(Dia::Core::StringCRC("FleeAction"),   [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);

    EXPECT_EQ(count, 1);
    ASSERT_EQ(entries.Size(), 1u);
    EXPECT_EQ(entries[0].ruleId.Value(), Dia::Core::StringCRC("AttackRule").Value());
}

TEST(DiaRules_RuleSet, GetLastFireReport_ClearedBetweenEvaluateCalls)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "Rule1",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A1"]
            },
            {
                "id": "Rule2",
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 30.0 },
                "actions": ["A2"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("A1"), [](void*) {});
    registry.Register(Dia::Core::StringCRC("A2"), [](void*) {});

    // First evaluate: health=80 → Rule1 fires, Rule2 does not
    Dia::Condition::Testing::MockConditionContext ctx1;
    ctx1.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);
    rs.Evaluate(ctx1, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries1;
    EXPECT_EQ(rs.GetLastFireReport(entries1), 1);

    // Second evaluate: health=20 → Rule2 fires, Rule1 does not
    Dia::Condition::Testing::MockConditionContext ctx2;
    ctx2.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);
    rs.Evaluate(ctx2, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries2;
    int count2 = rs.GetLastFireReport(entries2);

    // Report should only contain Rule2 (Rule1 did not fire in second Evaluate)
    EXPECT_EQ(count2, 1);
    ASSERT_EQ(entries2.Size(), 1u);
    EXPECT_EQ(entries2[0].ruleId.Value(), Dia::Core::StringCRC("Rule2").Value());
}

TEST(DiaRules_RuleSet, GetLastFireReport_AnonymousRule_RuleIdIsZero)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["SomeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("SomeAction"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);

    EXPECT_EQ(count, 1);
    ASSERT_EQ(entries.Size(), 1u);
    EXPECT_EQ(entries[0].ruleId.Value(), Dia::Core::StringCRC().Value());  // kZero
}

TEST(DiaRules_RuleSet, GetLastFireReport_NoRulesFire_ReturnsZero_EmptyEntries)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "NeverFire",
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 0.0 },
                "actions": ["SomeAction"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("SomeAction"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 50.0f);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = rs.GetLastFireReport(entries);

    EXPECT_EQ(count, 0);
    EXPECT_EQ(entries.Size(), 0u);
}

TEST(DiaRules_RuleSet, GetLastFireReport_RuleWithMultipleActions_ActionsAllPresent)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "id": "MultiAction",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A", "B", "C"]
            }
        ]
    })");

    Dia::Rules::RuleActionRegistry registry;
    registry.Register(Dia::Core::StringCRC("A"), [](void*) {});
    registry.Register(Dia::Core::StringCRC("B"), [](void*) {});
    registry.Register(Dia::Core::StringCRC("C"), [](void*) {});

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    rs.Evaluate(ctx, registry, nullptr);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    rs.GetLastFireReport(entries);

    ASSERT_EQ(entries.Size(), 1u);
    EXPECT_EQ(entries[0].actions.Size(), 3u);
    EXPECT_EQ(entries[0].actions[0].Value(), Dia::Core::StringCRC("A").Value());
    EXPECT_EQ(entries[0].actions[1].Value(), Dia::Core::StringCRC("B").Value());
    EXPECT_EQ(entries[0].actions[2].Value(), Dia::Core::StringCRC("C").Value());
}

TEST(DiaRules_RuleSet, GetLastFireReport_MovedFrom_ReturnsZero)
{
    Dia::Rules::RuleSet a = LoadRuleSet(R"({
        "rules": [
            {
                "id": "Rule",
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A"]
            }
        ]
    })");

    Dia::Rules::RuleSet b = std::move(a);

    Dia::Core::Containers::DynamicArrayC<Dia::Rules::RuleSet::RuleFireEntry, 16> entries;
    int count = a.GetLastFireReport(entries);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(entries.Size(), 0u);
}

#endif // DIA_DEBUG
