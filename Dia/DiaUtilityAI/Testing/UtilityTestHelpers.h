#pragma once

#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaUtilityAI/PersonalityProfile.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <gtest/gtest.h>

namespace Dia { namespace UtilityAI { namespace Testing {

    // Assert that SelectWinner returns a specific action.
    // Uses SelectWinner (score-only) — does not dispatch.
    inline void AssertWinner(
        const Dia::UtilityAI::UtilitySet& set,
        Dia::Condition::IConditionContext& ctx,
        const Dia::Rules::RuleActionRegistry& /*registry*/,
        Dia::Core::StringCRC expectedActionId)
    {
        Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr);
        EXPECT_EQ(result.actionId, expectedActionId)
            << "Expected winner '" << expectedActionId.AsChar()
            << "' but got '" << result.actionId.AsChar() << "'";
        EXPECT_GT(result.score, 0.0f) << "Expected non-zero score for winner";
    }

    // Assert that SelectWinner returns no eligible action.
    inline void AssertNoSelection(
        const Dia::UtilityAI::UtilitySet& set,
        Dia::Condition::IConditionContext& ctx,
        const Dia::Rules::RuleActionRegistry& /*registry*/)
    {
        Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr);
        EXPECT_EQ(result.score, 0.0f) << "Expected no eligible action but got score " << result.score;
        EXPECT_EQ(result.actionId.Value(), Dia::Core::StringCRC().Value())
            << "Expected kInvalidCRC but got '" << result.actionId.AsChar() << "'";
    }

    // Evaluate a single ResponseCurve and assert output within tolerance.
    inline void AssertCurveOutput(
        const Dia::UtilityAI::ResponseCurve& curve,
        float input,
        float expectedOutput,
        float tolerance = 0.001f)
    {
        float actual = curve.Evaluate(input);
        EXPECT_NEAR(actual, expectedOutput, tolerance)
            << "ResponseCurve::Evaluate(" << input << ") = " << actual
            << ", expected " << expectedOutput << " +/- " << tolerance;
    }

    // Assert that applying a PersonalityProfile changes the winner to the expected action.
    // Uses SelectWinner (score-only) — does not dispatch.
    inline void AssertPersonalityChangesWinner(
        const Dia::UtilityAI::UtilitySet& set,
        const Dia::UtilityAI::PersonalityProfile& profile,
        Dia::Condition::IConditionContext& ctx,
        const Dia::Rules::RuleActionRegistry& /*registry*/,
        Dia::Core::StringCRC expectedWinnerWithProfile)
    {
        Dia::UtilityAI::UtilitySelection withProfile = set.SelectWinner(ctx, nullptr, &profile);
        EXPECT_EQ(withProfile.actionId, expectedWinnerWithProfile)
            << "Expected winner with personality '" << expectedWinnerWithProfile.AsChar()
            << "' but got '" << withProfile.actionId.AsChar() << "'";
    }

}}} // namespace Dia::UtilityAI::Testing
