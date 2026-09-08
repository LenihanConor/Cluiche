#include <gtest/gtest.h>

#include <DiaUtilityAI/UtilitySetComponent.h>
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

    // Single action with no scorers (always score=1.0), cooldown=1.0s
    const char* kAttackWithCooldown = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [],
                "cooldown": 1.0
            }
        ]
    })";

    // Single action with no scorers (always score=1.0), cooldown=0 (no cooldown)
    const char* kAttackNoCooldown = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": []
            }
        ]
    })";

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. GetTypeId returns expected string CRC
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, GetTypeId_ReturnsExpectedStringCRC)
{
    Dia::UtilityAI::UtilitySetComponent comp;
    EXPECT_EQ(comp.GetTypeId(), Dia::Core::StringCRC("utility-set-component"));
}

// ---------------------------------------------------------------------------
// 2. Default construction: GetUtilitySet returns null
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, DefaultConstruct_GetUtilitySet_ReturnsNull)
{
    Dia::UtilityAI::UtilitySetComponent comp;
    EXPECT_EQ(comp.GetUtilitySet(), nullptr);
}

// ---------------------------------------------------------------------------
// 3. No registry set — Evaluate returns no-selection
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, SetRegistry_Nullptr_EvaluateReturnsNoSelection)
{
    Dia::UtilityAI::UtilitySetComponent comp;

    comp.SetUtilitySet(MakeSet(kAttackNoCooldown));
    // registry NOT set

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::UtilityAI::UtilitySelection result = comp.Evaluate(ctx, nullptr);

    EXPECT_NEAR(result.score, 0.0f, 0.001f);
    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
}

// ---------------------------------------------------------------------------
// 4. SetUtilitySet then Evaluate — returns winner
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, SetUtilitySet_Evaluate_ReturnsWinner)
{
    Dia::UtilityAI::UtilitySetComponent comp;

    comp.SetUtilitySet(MakeSet(kAttackNoCooldown));

    Dia::Rules::RuleActionRegistry reg;
    bool dispatched = false;
    reg.Register(Dia::Core::StringCRC("Attack"), [](void* ctx)
    {
        *static_cast<bool*>(ctx) = true;
    });
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::UtilityAI::UtilitySelection result = comp.Evaluate(ctx, &dispatched);

    EXPECT_EQ(result.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(result.score, 1.0f, 0.001f);
    EXPECT_TRUE(dispatched);
}

// ---------------------------------------------------------------------------
// 5. GetUtilitySet returns non-null after SetUtilitySet
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, SetUtilitySet_GetUtilitySet_NonNull)
{
    Dia::UtilityAI::UtilitySetComponent comp;
    comp.SetUtilitySet(MakeSet(kAttackNoCooldown));
    EXPECT_NE(comp.GetUtilitySet(), nullptr);
}

// ---------------------------------------------------------------------------
// 6. Cooldown blocks repeat dispatch within window
//    - First Evaluate (dt=0) → winner fires, cooldown starts (1.0s)
//    - Second Evaluate (dt=0) → still in cooldown, returns no-selection
//    - Third Evaluate (dt=1.1f) → cooldown expires, fires again
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, Cooldown_BlocksRepeatDispatch_WithinWindow)
{
    Dia::UtilityAI::UtilitySetComponent comp;

    comp.SetUtilitySet(MakeSet(kAttackWithCooldown));

    Dia::Rules::RuleActionRegistry reg;
    int fireCount = 0;
    reg.Register(Dia::Core::StringCRC("Attack"), [](void* ctx)
    {
        ++(*static_cast<int*>(ctx));
    });
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;

    // First evaluate — winner fires, cooldown starts
    Dia::UtilityAI::UtilitySelection r1 = comp.Evaluate(ctx, &fireCount, 0.0f);
    EXPECT_EQ(r1.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(r1.score, 1.0f, 0.001f);
    EXPECT_EQ(fireCount, 1);

    // Second evaluate (dt=0) — still in cooldown, suppressed
    Dia::UtilityAI::UtilitySelection r2 = comp.Evaluate(ctx, &fireCount, 0.0f);
    EXPECT_NEAR(r2.score, 0.0f, 0.001f);
    EXPECT_EQ(r2.actionId, Dia::Core::StringCRC());
    EXPECT_EQ(fireCount, 1);  // no additional fire

    // Third evaluate (dt=1.1f) — advances timer past 1.0s, cooldown expires, fires again
    Dia::UtilityAI::UtilitySelection r3 = comp.Evaluate(ctx, &fireCount, 1.1f);
    EXPECT_EQ(r3.actionId, Dia::Core::StringCRC("Attack"));
    EXPECT_NEAR(r3.score, 1.0f, 0.001f);
    EXPECT_EQ(fireCount, 2);
}

// ---------------------------------------------------------------------------
// 7. Zero cooldown — action fires every Evaluate
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, CooldownZero_NoBlock_FiresEveryEvaluate)
{
    Dia::UtilityAI::UtilitySetComponent comp;

    comp.SetUtilitySet(MakeSet(kAttackNoCooldown));

    Dia::Rules::RuleActionRegistry reg;
    int fireCount = 0;
    reg.Register(Dia::Core::StringCRC("Attack"), [](void* ctx)
    {
        ++(*static_cast<int*>(ctx));
    });
    comp.SetRegistry(&reg);

    Dia::Condition::Testing::MockConditionContext ctx;

    // Three consecutive evaluates with no dt — all must fire
    comp.Evaluate(ctx, &fireCount, 0.0f);
    comp.Evaluate(ctx, &fireCount, 0.0f);
    comp.Evaluate(ctx, &fireCount, 0.0f);

    EXPECT_EQ(fireCount, 3);
}

// ---------------------------------------------------------------------------
// 8. SetUtilitySet called twice — second replaces first; cooldowns reset
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_UtilitySetComponent, SetUtilitySet_CalledTwice_SecondReplaces)
{
    Dia::UtilityAI::UtilitySetComponent comp;

    Dia::Rules::RuleActionRegistry reg;
    int attackCount = 0;
    int fleeCount = 0;
    reg.Register(Dia::Core::StringCRC("Attack"), [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    reg.Register(Dia::Core::StringCRC("Flee"),   [](void* ctx) { ++(*static_cast<int*>(ctx)); });
    comp.SetRegistry(&reg);

    // First UtilitySet: Attack with cooldown 1.0s
    comp.SetUtilitySet(MakeSet(kAttackWithCooldown));

    Dia::Condition::Testing::MockConditionContext ctx;

    // Fire Attack once to start its cooldown
    comp.Evaluate(ctx, &attackCount, 0.0f);
    EXPECT_EQ(attackCount, 1);

    // Replace with a new UtilitySet — cooldowns are cleared
    const char* kFleeNoCooldown = R"({
        "actions": [
            { "id": "Flee", "scorers": [] }
        ]
    })";
    comp.SetUtilitySet(MakeSet(kFleeNoCooldown));

    // Evaluate again — Flee fires (no cooldown applied from previous set)
    Dia::UtilityAI::UtilitySelection r = comp.Evaluate(ctx, &fleeCount, 0.0f);
    EXPECT_EQ(r.actionId, Dia::Core::StringCRC("Flee"));
    EXPECT_EQ(fleeCount, 1);
}
