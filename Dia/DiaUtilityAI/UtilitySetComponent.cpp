#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <unordered_map>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::UtilityAI::UtilitySetComponent, Dia::UtilityAI::UtilitySetComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia
{
    namespace UtilityAI
    {

DIA_COMPONENT_REGISTER(UtilitySetComponent, "utility-set-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

struct UtilitySetComponent::CooldownState
{
    std::unordered_map<unsigned int, float> timers;
};

UtilitySetComponent::UtilitySetComponent()
    : mCooldownState(new CooldownState())
{
}

UtilitySetComponent::~UtilitySetComponent()
{
    delete mCooldownState;
}

void UtilitySetComponent::SetUtilitySet(UtilitySet&& utilitySet)
{
    mUtilitySet    = std::move(utilitySet);
    mHasUtilitySet = true;
    mCooldownState->timers.clear();  // reset cooldowns when set changes
}

void UtilitySetComponent::SetRegistry(const Dia::Rules::RuleActionRegistry* registry)
{
    mRegistry = registry;
}

void UtilitySetComponent::SetGroupContext(GroupConsiderationContext* group)
{
    mGroupContext = group;
}

void UtilitySetComponent::SetPersonality(const PersonalityProfile* profile)
{
    mPersonality = profile;
    mEvalCounter = 0;  // reset counter when personality changes
}

const PersonalityProfile* UtilitySetComponent::GetPersonality() const
{
    return mPersonality;
}

UtilitySelection UtilitySetComponent::Evaluate(
    Dia::Condition::IConditionContext& ctx,
    void* actionContext,
    float dt) const
{
    if (!mHasUtilitySet || mRegistry == nullptr)
    {
        return UtilitySelection{};
    }

    // Eval period throttle: skip ticks when personality has eval_period_ticks > 1
    if (mPersonality != nullptr)
    {
        ++mEvalCounter;
        if (mEvalCounter % mPersonality->GetEvalPeriodTicks() != 0)
            return UtilitySelection{};
    }

    // Advance cooldown timers
    if (dt > 0.0f)
    {
        for (auto& kv : mCooldownState->timers)
        {
            kv.second -= dt;
            if (kv.second < 0.0f)
                kv.second = 0.0f;
        }
    }

    // Score only (no dispatch) so we can enforce cooldown before committing
    UtilitySelection result = mUtilitySet.SelectWinner(ctx, mGroupContext, mPersonality);

    if (result.score > 0.0f)
    {
        const unsigned int id = result.actionId.Value();
        auto it = mCooldownState->timers.find(id);
        if (it != mCooldownState->timers.end() && it->second > 0.0f)
        {
            // Still in cooldown — suppress without dispatch
            return UtilitySelection{};
        }

        // Winner cleared cooldown check — dispatch now
        Dia::Rules::RuleActionFn fn = mRegistry->Find(result.actionId);
        if (fn != nullptr)
            fn(actionContext);

        // Start cooldown if the action has one
        const float cooldownDur = mUtilitySet.GetCooldownForAction(result.actionId);
        if (cooldownDur > 0.0f)
            mCooldownState->timers[id] = cooldownDur;
    }

    return result;
}

const UtilitySet* UtilitySetComponent::GetUtilitySet() const
{
    return mHasUtilitySet ? &mUtilitySet : nullptr;
}

    } // namespace UtilityAI
} // namespace Dia
