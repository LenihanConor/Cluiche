#pragma once

#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleDef.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <gtest/gtest.h>
#include <initializer_list>

namespace Dia
{
    namespace Rules
    {
        namespace Testing
        {
            //-------------------------------------------------------------------------------------------
            // RulesTestHelpers
            //
            // Utility functions for testing RuleSet behaviour.  Designed for use inside GoogleTests.
            // Header-only (all inline) so consumers opt in by including this header — no gtest
            // dependency is introduced into DiaRules.lib itself.
            //
            // SD-009: Lives in DiaRules/Testing/.
            //-------------------------------------------------------------------------------------------

            // Evaluate a RuleSet against a context and collect all fired action IDs.
            // Returns count of rules that fired (same as RuleSet::Evaluate semantics).
            inline int FireRuleSet(const RuleSet& ruleSet,
                                   Dia::Condition::IConditionContext& context,
                                   Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64>& outFiredActions)
            {
                int fired = 0;

                const int ruleCount = ruleSet.GetRuleCount();
                for (int i = 0; i < ruleCount; ++i)
                {
                    const RuleDef* rule = ruleSet.GetRuleAt(i);
                    if (!rule)
                        continue;

                    if (rule->guard.Evaluate(context))
                    {
                        ++fired;
                        for (unsigned int j = 0; j < rule->actions.Size(); ++j)
                        {
                            if (!outFiredActions.IsFull())
                                outFiredActions.Add(rule->actions[j]);
                        }
                    }
                }

                return fired;
            }

            // Assert that every action in 'expected' is present in 'firedActions' (order-independent).
            // Uses gtest EXPECT_* so failures are non-fatal.
            inline void AssertActionsFired(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64>& firedActions,
                                           std::initializer_list<Dia::Core::StringCRC> expected)
            {
                for (const Dia::Core::StringCRC& expectedId : expected)
                {
                    bool found = false;
                    for (unsigned int i = 0; i < firedActions.Size(); ++i)
                    {
                        if (firedActions[i].Value() == expectedId.Value())
                        {
                            found = true;
                            break;
                        }
                    }
                    EXPECT_TRUE(found) << "Expected action was not fired: CRC=" << expectedId.Value();
                }
            }

            // Assert that 'actionId' is NOT present in 'firedActions'.
            // Uses gtest EXPECT_* so failure is non-fatal.
            inline void AssertActionNotFired(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64>& firedActions,
                                             Dia::Core::StringCRC actionId)
            {
                for (unsigned int i = 0; i < firedActions.Size(); ++i)
                {
                    if (firedActions[i].Value() == actionId.Value())
                    {
                        EXPECT_TRUE(false) << "Action was not expected to fire but did: CRC=" << actionId.Value();
                        return;
                    }
                }
            }

        } // namespace Testing
    } // namespace Rules
} // namespace Dia
