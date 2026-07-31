#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCondition/IConditionContext.h>

namespace Dia
{
    namespace UtilityAI
    {
        //-------------------------------------------------------------------------------------------
        // UtilitySetComponent
        //
        // IComponent wrapper that attaches a UtilitySet, a RuleActionRegistry, and an optional
        // GroupConsiderationContext to a DiaEntity.
        //
        // SD-003: Registry is NOT owned by the component — caller passes a pointer, registry must
        //         outlive the component.
        // SD-006: Evaluate() takes IConditionContext&.
        // PD-001: kTypeId uses StringCRC (via DIA_COMPONENT macro).
        // AD-003: Dia::UtilityAI:: namespace.
        // DIA_READONLY: Caller drives evaluation — domain does not tick this component.
        //
        // Cooldown state: owned per-instance in a pimpl struct (no STL in public header).
        // Pass dt > 0.0f to Evaluate() each frame to advance cooldown timers.
        //-------------------------------------------------------------------------------------------
        class UtilitySetComponent : public Dia::Entity::IComponent
        {
            DIA_COMPONENT(UtilitySetComponent, "utility-set-component", 1)
            DIA_READONLY

        public:
            UtilitySetComponent();
            ~UtilitySetComponent();

            // Caller moves a loaded UtilitySet into the component.
            void SetUtilitySet(UtilitySet&& utilitySet);

            // Registry must outlive the component. Caller retains ownership.
            void SetRegistry(const Dia::Rules::RuleActionRegistry* registry);

            // Optional group cap context. Caller retains ownership. Pass nullptr to disable.
            void SetGroupContext(GroupConsiderationContext* group);

            // Evaluate with optional delta-time for cooldown advancement.
            // dt defaults to 0 — pass the frame delta to advance cooldown timers.
            // Returns no-selection (score == 0) if no UtilitySet or registry is set,
            // or if the winning action is currently in cooldown.
            UtilitySelection Evaluate(
                Dia::Condition::IConditionContext& ctx,
                void* actionContext,
                float dt = 0.0f) const;

            const UtilitySet* GetUtilitySet() const;

        private:
            UtilitySet                            mUtilitySet;
            const Dia::Rules::RuleActionRegistry* mRegistry    = nullptr;
            GroupConsiderationContext*             mGroupContext = nullptr;
            bool                                  mHasUtilitySet = false;

            // Per-action cooldown state. Pimpl keeps STL out of the public header.
            struct CooldownState;
            mutable CooldownState* mCooldownState;
        };

    } // namespace UtilityAI
} // namespace Dia
