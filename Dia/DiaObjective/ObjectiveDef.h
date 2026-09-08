#pragma once

#include <DiaObjective/IObjectiveObserver.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Objective
    {
        enum class ObjectiveClassification
        {
            kPrimary,
            kSecondary,
            kOptional
        };

        enum class ObjectiveState
        {
            kInactive,
            kActive,
            kComplete,
            kFailed
        };

        struct ObjectiveDef
        {
            Dia::Core::StringCRC      id;
            ObjectiveClassification   classification;
            Dia::Condition::ConditionExpr completion;
            Dia::Condition::ConditionExpr failure;
            RewardPayload             reward;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> prerequisites;

            ObjectiveDef() = default;
            ~ObjectiveDef() = default;

            ObjectiveDef(ObjectiveDef&&) = default;
            ObjectiveDef& operator=(ObjectiveDef&&) = default;

            ObjectiveDef(const ObjectiveDef&) = delete;
            ObjectiveDef& operator=(const ObjectiveDef&) = delete;
        };

    } // namespace Objective
} // namespace Dia
