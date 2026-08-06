#pragma once

#include <DiaObjective/ObjectiveSet.h>
#include <DiaObjective/IObjectiveObserver.h>
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace Objective
    {
        class ObjectiveSetComponent : public Dia::Entity::IComponent
        {
            DIA_COMPONENT(ObjectiveSetComponent, "objective-set-component", 1)
            DIA_READONLY

        public:
            void SetObjectiveSet(ObjectiveSet&& set);

            int            Evaluate          (Dia::Condition::IConditionContext& ctx);
            ObjectiveState GetState          (Dia::Core::StringCRC objectiveId) const;
            bool           AllPrimaryComplete() const;
            bool           AnyPrimaryFailed  () const;

            void AddObserver   (IObjectiveObserver* observer);
            void RemoveObserver(IObjectiveObserver* observer);

        private:
            ObjectiveSet mObjectiveSet;
            bool         mHasSet = false;
        };

    } // namespace Objective
} // namespace Dia
