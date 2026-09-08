#include <gtest/gtest.h>

#include <DiaUtilityAI/PersonalityProfile.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaUtilityAI/Testing/UtilityTestHelpers.h>
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

    Dia::UtilityAI::PersonalityProfile MakeProfile(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::PersonalityProfile::LoadFromJson(root);
    }

    Dia::UtilityAI::UtilitySet MakeSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }

    // Two actions: Attack (raw score 0.4), Flee (raw score 0.6) at health.value=40
    const char* kTwoActionsLinearScorer = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear" } }
                ]
            },
            {
                "id": "Flee",
                "scorers": [
                    { "slot": "health", "field": "value", "input_min": 0.0, "input_max": 100.0, "curve": { "shape": "linear", "invert": true } }
                ]
            }
        ]
    })";

    // Single action Attack with no scorers (always score=1.0), no cooldown
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
// PersonalityProfile unit tests
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_PersonalityProfile, LoadFromJson_SetsName)
{
    const char* json = R"({"name":"Aggressive","eval_period_ticks":1,"biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_EQ(profile.GetName(), Dia::Core::StringCRC("Aggressive"));
}

TEST(DiaUtilityAI_PersonalityProfile, LoadFromJson_SetsBiases)
{
    const char* json = R"({
        "name": "Aggressive",
        "eval_period_ticks": 1,
        "biases": [
            { "action": "Attack", "multiplier": 1.5 },
            { "action": "Flee",   "multiplier": 0.2 }
        ]
    })";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_NEAR(profile.GetScoreMultiplier(Dia::Core::StringCRC("Attack")), 1.5f, 0.001f);
    EXPECT_NEAR(profile.GetScoreMultiplier(Dia::Core::StringCRC("Flee")),   0.2f, 0.001f);
}

TEST(DiaUtilityAI_PersonalityProfile, GetScoreMultiplier_UnlistedAction_ReturnsOne)
{
    const char* json = R"({"name":"Aggressive","biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_NEAR(profile.GetScoreMultiplier(Dia::Core::StringCRC("Patrol")), 1.0f, 0.001f);
}

TEST(DiaUtilityAI_PersonalityProfile, GetEvalPeriodTicks_Default_ReturnsOne)
{
    const char* json = R"({"name":"Neutral","biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_EQ(profile.GetEvalPeriodTicks(), 1);
}

TEST(DiaUtilityAI_PersonalityProfile, GetEvalPeriodTicks_Custom_Returns3)
{
    const char* json = R"({"name":"Lazy","eval_period_ticks":3,"biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_EQ(profile.GetEvalPeriodTicks(), 3);
}

TEST(DiaUtilityAI_PersonalityProfile, IsValid_AfterLoad_True)
{
    const char* json = R"({"name":"Aggressive","biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(json);
    EXPECT_TRUE(profile.IsValid());
}

TEST(DiaUtilityAI_PersonalityProfile, IsValid_DefaultConstruct_False)
{
    Dia::UtilityAI::PersonalityProfile profile;
    EXPECT_FALSE(profile.IsValid());
}

// ---------------------------------------------------------------------------
// PersonalityIntegration tests
// ---------------------------------------------------------------------------

// Test 8: PersonalityProfile changes winner
// Without profile: Attack=0.4, Flee=0.6 at health=40 → Flee wins
// With Aggressive profile: Attack*2.0=0.8, Flee*0.5=0.3 → Attack wins
TEST(DiaUtilityAI_PersonalityIntegration, PersonalityProfile_ChangesWinner)
{
    Dia::UtilityAI::UtilitySet set = MakeSet(kTwoActionsLinearScorer);

    // Set health.value = 40 so Attack raw=0.4, Flee raw=0.6
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 40.0f);

    // Without personality — Flee wins
    Dia::UtilityAI::UtilitySelection noProfile = set.SelectWinner(ctx, nullptr, nullptr);
    EXPECT_EQ(noProfile.actionId, Dia::Core::StringCRC("Flee"));

    // Build Aggressive profile: Attack * 2.0, Flee * 0.5
    const char* profileJson = R"({
        "name": "Aggressive",
        "biases": [
            { "action": "Attack", "multiplier": 2.0 },
            { "action": "Flee",   "multiplier": 0.5 }
        ]
    })";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(profileJson);
    EXPECT_TRUE(profile.IsValid());

    // Use test helper — Attack must win
    Dia::Rules::RuleActionRegistry registry;
    Dia::UtilityAI::Testing::AssertPersonalityChangesWinner(
        set, profile, ctx, registry, Dia::Core::StringCRC("Attack"));
}

// Test 9: EvalPeriod skips ticks
// period=3: evaluations happen on calls 3, 6, ... → 6 calls → fireCount = 2
TEST(DiaUtilityAI_PersonalityIntegration, EvalPeriod_SkipsTicks)
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

    const char* profileJson = R"({"name":"Throttled","eval_period_ticks":3,"biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(profileJson);
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;

    // 6 calls; evaluations fire on calls 3 and 6
    for (int i = 0; i < 6; ++i)
        comp.Evaluate(ctx, &fireCount, 0.0f);

    EXPECT_EQ(fireCount, 2);
}

// Test 10: EvalPeriod of 1 fires every call
TEST(DiaUtilityAI_PersonalityIntegration, EvalPeriod_One_FiresEveryCall)
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

    const char* profileJson = R"({"name":"Active","eval_period_ticks":1,"biases":[]})";
    Dia::UtilityAI::PersonalityProfile profile = MakeProfile(profileJson);
    comp.SetPersonality(&profile);

    Dia::Condition::Testing::MockConditionContext ctx;

    comp.Evaluate(ctx, &fireCount, 0.0f);
    comp.Evaluate(ctx, &fireCount, 0.0f);
    comp.Evaluate(ctx, &fireCount, 0.0f);

    EXPECT_EQ(fireCount, 3);
}
