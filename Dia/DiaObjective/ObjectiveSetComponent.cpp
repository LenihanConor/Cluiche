#include <DiaObjective/ObjectiveSetComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::Objective::ObjectiveSetComponent, Dia::Objective::ObjectiveSetComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia
{
    namespace Objective
    {

DIA_COMPONENT_REGISTER(ObjectiveSetComponent, "objective-set-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

void ObjectiveSetComponent::SetObjectiveSet(ObjectiveSet&& set)
{
    mObjectiveSet = std::move(set);
    mHasSet = true;
}

int ObjectiveSetComponent::Evaluate(Dia::Condition::IConditionContext& ctx)
{
    if (!mHasSet) return 0;
    return mObjectiveSet.Evaluate(ctx);
}

ObjectiveState ObjectiveSetComponent::GetState(Dia::Core::StringCRC objectiveId) const
{
    if (!mHasSet) return ObjectiveState::kInactive;
    return mObjectiveSet.GetState(objectiveId);
}

bool ObjectiveSetComponent::AllPrimaryComplete() const
{
    if (!mHasSet) return false;
    return mObjectiveSet.AllPrimaryComplete();
}

bool ObjectiveSetComponent::AnyPrimaryFailed() const
{
    if (!mHasSet) return false;
    return mObjectiveSet.AnyPrimaryFailed();
}

void ObjectiveSetComponent::AddObserver(IObjectiveObserver* observer)
{
    mObjectiveSet.AddObserver(observer);
}

void ObjectiveSetComponent::RemoveObserver(IObjectiveObserver* observer)
{
    mObjectiveSet.RemoveObserver(observer);
}

    } // namespace Objective
} // namespace Dia
