#pragma once

#include <DiaObjective/ObjectiveDef.h>
#include <DiaObjective/IObjectiveObserver.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace Objective
    {
        class ObjectiveSet
        {
        public:
            ObjectiveSet();
            ~ObjectiveSet();

            ObjectiveSet(ObjectiveSet&&) noexcept;
            ObjectiveSet& operator=(ObjectiveSet&&) noexcept;

            ObjectiveSet(const ObjectiveSet&) = delete;
            ObjectiveSet& operator=(const ObjectiveSet&) = delete;

            static ObjectiveSet LoadFromJson(
                const Json::Value& root,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

            bool Validate(const Dia::Condition::ConditionRegistry& registry,
                          Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            int Evaluate(Dia::Condition::IConditionContext& ctx);

            void AddObserver   (IObjectiveObserver* observer);
            void RemoveObserver(IObjectiveObserver* observer);

            // Register objective transition metrics with MetricRegistry.
            // Call once after loading. Idempotent if already registered.
            void InitMetrics();

            ObjectiveState      GetState  (Dia::Core::StringCRC objectiveId) const;
            int                 GetCount  () const;
            const ObjectiveDef* GetAt     (int index) const;

            bool AllPrimaryComplete() const;
            bool AnyPrimaryFailed  () const;

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace Objective
} // namespace Dia
