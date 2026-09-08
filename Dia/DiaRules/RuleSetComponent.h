#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCondition/IConditionContext.h>

namespace Dia
{
    namespace Rules
    {
        //-------------------------------------------------------------------------------------------
        // RuleSetComponent
        //
        // IComponent wrapper that attaches a RuleSet and a RuleActionRegistry to a DiaEntity.
        //
        // SD-003: Registry is NOT owned by the component — caller passes a pointer, registry must
        //         outlive the component.
        // SD-006: Evaluate() takes IConditionContext&.
        // SD-007: Evaluate() returns count of rules fired (0 = no match / no-op).
        // PD-001: kTypeId uses StringCRC (via DIA_COMPONENT macro).
        // AD-003: Dia::Rules:: namespace.
        // DIA_READONLY: Caller drives evaluation — domain does not tick this component.
        //-------------------------------------------------------------------------------------------
        class RuleSetComponent : public Dia::Entity::IComponent
        {
            DIA_COMPONENT(RuleSetComponent, "rule-set-component", 1)
            DIA_READONLY

        public:
            // Caller moves a loaded RuleSet into the component.
            void SetRuleSet(RuleSet&& ruleSet);

            // Registry must outlive the component. Caller retains ownership.
            void SetRegistry(const RuleActionRegistry* registry);

            // Evaluate all rules with the given context and action context.
            // No-op (returns 0) if no RuleSet or no registry has been set.
            int Evaluate(Dia::Condition::IConditionContext& context,
                         void* actionContext) const;

            const RuleSet*             GetRuleSet()  const;
            const RuleActionRegistry*  GetRegistry() const;

        private:
            RuleSet                    mRuleSet;
            const RuleActionRegistry*  mRegistry    = nullptr;
            bool                       mHasRuleSet  = false;
        };

    } // namespace Rules
} // namespace Dia
