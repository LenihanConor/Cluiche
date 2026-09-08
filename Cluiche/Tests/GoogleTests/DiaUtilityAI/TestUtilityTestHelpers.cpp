#include <gtest/gtest.h>

#include <DiaUtilityAI/Testing/UtilityTestHelpers.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/ResponseCurve.h>
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
}

// ---------------------------------------------------------------------------
// DiaUtilityAI_TestHelpers
// ---------------------------------------------------------------------------

// Meta-test 1: AssertWinner with a one-action set and the correct expected ID.
// The action has no prerequisite and no scorers (score defaults to 1.0).
TEST(DiaUtilityAI_TestHelpers, AssertWinner_CorrectAction_NoFailure)
{
    Json::Value root = ParseJson(R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": []
            }
        ]
    })");

    Dia::UtilityAI::UtilitySet set = Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    // Should pass without triggering any EXPECT failure
    Dia::UtilityAI::Testing::AssertWinner(set, ctx, registry, Dia::Core::StringCRC("Attack"));
}

// Meta-test 2: AssertNoSelection with an empty UtilitySet.
// No actions means SelectWinner returns score=0 and kInvalidCRC.
TEST(DiaUtilityAI_TestHelpers, AssertNoSelection_EmptySet_NoFailure)
{
    Json::Value root = ParseJson(R"({ "actions": [] })");

    Dia::UtilityAI::UtilitySet set = Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Rules::RuleActionRegistry registry;

    // Should pass without triggering any EXPECT failure
    Dia::UtilityAI::Testing::AssertNoSelection(set, ctx, registry);
}

// Meta-test 3: AssertCurveOutput with a linear curve at input=0.5, expected=0.5.
TEST(DiaUtilityAI_TestHelpers, AssertCurveOutput_Linear_Half_NoFailure)
{
    Json::Value curveNode = ParseJson(R"({ "shape": "linear" })");
    Dia::UtilityAI::ResponseCurve curve = Dia::UtilityAI::ResponseCurve::LoadFromJson(curveNode);

    // Should pass without triggering any EXPECT failure
    Dia::UtilityAI::Testing::AssertCurveOutput(curve, 0.5f, 0.5f, 0.001f);
}
