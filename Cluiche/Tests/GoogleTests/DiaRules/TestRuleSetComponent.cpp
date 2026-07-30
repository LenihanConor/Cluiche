#include <gtest/gtest.h>

#include <DiaRules/RuleSetComponent.h>
#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
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
// DiaRules_Component — identity
// ---------------------------------------------------------------------------

TEST(DiaRules_Component, GetTypeId_ReturnsExpectedStringCRC)
{
    Dia::Rules::RuleSetComponent comp;
    EXPECT_EQ(comp.GetTypeId(), Dia::Core::StringCRC("rule-set-component"));
}

// ---------------------------------------------------------------------------
// DiaRules_Component — default construction
// ---------------------------------------------------------------------------

TEST(DiaRules_Component, DefaultConstruct_GetRuleSet_ReturnsNull)
{
    Dia::Rules::RuleSetComponent comp;
    EXPECT_EQ(comp.GetRuleSet(), nullptr);
}

TEST(DiaRules_Component, DefaultConstruct_GetRegistry_ReturnsNull)
{
    Dia::Rules::RuleSetComponent comp;
    EXPECT_EQ(comp.GetRegistry(), nullptr);
}

TEST(DiaRules_Component, DefaultConstruct_Evaluate_ReturnsZero)
{
    Dia::Rules::RuleSetComponent comp;
    Dia::Condition::Testing::MockConditionContext ctx;
    int result = comp.Evaluate(ctx, nullptr);
    EXPECT_EQ(result, 0);
}

// ---------------------------------------------------------------------------
// DiaRules_Component — SetRuleSet / SetRegistry
// ---------------------------------------------------------------------------

TEST(DiaRules_Component, SetRuleSet_GetRuleSet_NonNull)
{
    Dia::Rules::RuleSetComponent comp;
    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");

    comp.SetRuleSet(std::move(rs));
    EXPECT_NE(comp.GetRuleSet(), nullptr);
}

TEST(DiaRules_Component, SetRegistry_GetRegistry_ReturnsSamePointer)
{
    Dia::Rules::RuleSetComponent comp;
    Dia::Rules::RuleActionRegistry reg;

    comp.SetRegistry(&reg);
    EXPECT_EQ(comp.GetRegistry(), &reg);
}

// ---------------------------------------------------------------------------
// DiaRules_Component — Evaluate
// ---------------------------------------------------------------------------

TEST(DiaRules_Component, Evaluate_ValidRuleSetAndRegistry_FiresMatchingRules)
{
    Dia::Rules::RuleSetComponent comp;

    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");
    comp.SetRuleSet(std::move(rs));

    Dia::Rules::RuleActionRegistry reg;
    int callCount = 0;
    reg.Register(Dia::Core::StringCRC("AttackAction"), [](void* ctx) {
        ++(*static_cast<int*>(ctx));
    });
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = comp.Evaluate(ctx, &callCount);
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(callCount, 1);
}

TEST(DiaRules_Component, Evaluate_NoRegistry_ReturnsZero)
{
    Dia::Rules::RuleSetComponent comp;

    Dia::Rules::RuleSet rs = LoadRuleSet(R"({
        "rules": [
            {
                "guard": { "op": ">=", "slot": "hero", "field": "health", "value": 50.0 },
                "actions": ["AttackAction"]
            }
        ]
    })");
    comp.SetRuleSet(std::move(rs));
    // registry NOT set

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = comp.Evaluate(ctx, nullptr);
    EXPECT_EQ(fired, 0);
}

TEST(DiaRules_Component, Evaluate_NoRuleSet_ReturnsZero)
{
    Dia::Rules::RuleSetComponent comp;

    Dia::Rules::RuleActionRegistry reg;
    reg.Register(Dia::Core::StringCRC("AttackAction"), [](void* ctx) {
        (void)ctx;
    });
    comp.SetRegistry(&reg);
    // RuleSet NOT set

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 80.0f);

    int fired = comp.Evaluate(ctx, nullptr);
    EXPECT_EQ(fired, 0);
}
