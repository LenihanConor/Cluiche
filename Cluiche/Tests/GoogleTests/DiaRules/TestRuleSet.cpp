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
