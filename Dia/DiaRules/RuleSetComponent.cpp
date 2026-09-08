#include <DiaRules/RuleSetComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::Rules::RuleSetComponent, Dia::Rules::RuleSetComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia
{
    namespace Rules
    {

DIA_COMPONENT_REGISTER(RuleSetComponent, "rule-set-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

void RuleSetComponent::SetRuleSet(RuleSet&& ruleSet)
{
    mRuleSet    = std::move(ruleSet);
    mHasRuleSet = true;
}

void RuleSetComponent::SetRegistry(const RuleActionRegistry* registry)
{
    mRegistry = registry;
}

int RuleSetComponent::Evaluate(Dia::Condition::IConditionContext& context,
                                void* actionContext) const
{
    if (!mHasRuleSet || mRegistry == nullptr)
    {
        return 0;
    }
    return mRuleSet.Evaluate(context, *mRegistry, actionContext);
}

const RuleSet* RuleSetComponent::GetRuleSet() const
{
    return mHasRuleSet ? &mRuleSet : nullptr;
}

const RuleActionRegistry* RuleSetComponent::GetRegistry() const
{
    return mRegistry;
}

    } // namespace Rules
} // namespace Dia
