#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include <DiaRules/Testing/RulesTestHelpers.h>
#include <DiaRules/RuleSet.h>
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
// DiaRules_TestHelpers — FireRuleSet
// ---------------------------------------------------------------------------

TEST(DiaRules_TestHelpers, FireRuleSet_OneRuleFires_ReturnsOne_ActionCollected)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    int count = Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    EXPECT_EQ(count, 1);
    EXPECT_EQ(firedActions.Size(), 1u);
    EXPECT_EQ(firedActions[0u].Value(), Dia::Core::StringCRC("AttackAction").Value());
}

TEST(DiaRules_TestHelpers, FireRuleSet_NoRulesFire_ReturnsZero_EmptyActions)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": "<", "slot": "hero", "field": "health", "value": 0.0 },
                "actions": ["FleeAction"]
            }
        ]
    })");

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    int count = Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    EXPECT_EQ(count, 0);
    EXPECT_EQ(firedActions.Size(), 0u);
}

TEST(DiaRules_TestHelpers, FireRuleSet_TwoRulesFire_ReturnsTwoActionsBothCollected)
{
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

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  80.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    int count = Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    EXPECT_EQ(count, 2);
    EXPECT_EQ(firedActions.Size(), 2u);

    // Both action IDs must be present (order-independent)
    Dia::Rules::Testing::AssertActionsFired(firedActions,
        {Dia::Core::StringCRC("AttackAction"), Dia::Core::StringCRC("SetAggressive")});
}

// ---------------------------------------------------------------------------
// DiaRules_TestHelpers — AssertActionsFired
// ---------------------------------------------------------------------------

TEST(DiaRules_TestHelpers, AssertActionsFired_CorrectExpectedSet_NoFailure)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));
    firedActions.Add(Dia::Core::StringCRC("FleeAction"));

    // Exact set matches — should produce no failures
    Dia::Rules::Testing::AssertActionsFired(firedActions,
        {Dia::Core::StringCRC("AttackAction"), Dia::Core::StringCRC("FleeAction")});
}

TEST(DiaRules_TestHelpers, AssertActionsFired_MissingAction_TriggersNonfatalFailure)
{
    // Only AttackAction fired — but we expect FleeAction as well
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));

    const Dia::Core::StringCRC attackId("AttackAction");
    const Dia::Core::StringCRC fleeId("FleeAction");
    auto doAssert = [&]()
    {
        Dia::Rules::Testing::AssertActionsFired(firedActions, {attackId, fleeId});
    };
    EXPECT_NONFATAL_FAILURE(doAssert(), "");
}

// ---------------------------------------------------------------------------
// DiaRules_TestHelpers — AssertActionNotFired
// ---------------------------------------------------------------------------

TEST(DiaRules_TestHelpers, AssertActionNotFired_ActionNotPresent_NoFailure)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));

    // FleeAction did not fire — no failure expected
    Dia::Rules::Testing::AssertActionNotFired(firedActions, Dia::Core::StringCRC("FleeAction"));
}

TEST(DiaRules_TestHelpers, AssertActionNotFired_ActionPresent_TriggersNonfatalFailure)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));

    const Dia::Core::StringCRC attackId("AttackAction");
    auto doAssert = [&]()
    {
        Dia::Rules::Testing::AssertActionNotFired(firedActions, attackId);
    };
    EXPECT_NONFATAL_FAILURE(doAssert(), "");
}

// ---------------------------------------------------------------------------
// DiaRules_TestHelpers — additional coverage
// ---------------------------------------------------------------------------

TEST(DiaRules_TestHelpers, FireRuleSet_OneRuleTwoActions_FiredCountOneActionCountTwo)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["A", "B"]
            }
        ]
    })");

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    int count = Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    EXPECT_EQ(count, 1);
    EXPECT_EQ(firedActions.Size(), 2u);
}

TEST(DiaRules_TestHelpers, FireRuleSet_EmptyRuleSet_ReturnsZero)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({ "rules": [] })");

    Dia::Condition::Testing::MockConditionContext ctx;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    int count = Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    EXPECT_EQ(count, 0);
    EXPECT_EQ(firedActions.Size(), 0u);
}

TEST(DiaRules_TestHelpers, FireRuleSet_AppendsToExistingBuffer)
{
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    // Pre-populate with one entry
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("PreExisting"));

    Dia::Rules::Testing::FireRuleSet(rs, ctx, firedActions);

    // Should now have 2 entries: the pre-existing one plus the newly fired one
    EXPECT_EQ(firedActions.Size(), 2u);
}

TEST(DiaRules_TestHelpers, AssertActionsFired_EmptyExpected_NoFailure)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));

    // Empty expected list — vacuous truth, no failure
    Dia::Rules::Testing::AssertActionsFired(firedActions, {});
}

TEST(DiaRules_TestHelpers, AssertActionsFired_ExtraFiredActions_SubsetCheckPasses)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    firedActions.Add(Dia::Core::StringCRC("AttackAction"));
    firedActions.Add(Dia::Core::StringCRC("FleeAction"));

    const Dia::Core::StringCRC attackId("AttackAction");

    // Subset semantics: only assert AttackAction is present — FleeAction being extra is fine
    Dia::Rules::Testing::AssertActionsFired(firedActions, {attackId});
}

TEST(DiaRules_TestHelpers, AssertActionNotFired_EmptyFiredActions_NoFailure)
{
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> firedActions;
    // firedActions is empty

    const Dia::Core::StringCRC someId("SomeAction");

    // Nothing in firedActions — action definitely did not fire, no failure
    Dia::Rules::Testing::AssertActionNotFired(firedActions, someId);
}
