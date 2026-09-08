#pragma once

#include <DiaUtilityAI/ScorerDef.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace UtilityAI
    {
        struct ActionDef
        {
            Dia::Core::StringCRC actionId;
            Dia::Condition::ConditionExpr prerequisite;
            Dia::Core::Containers::DynamicArrayC<ScorerDef, 8> scorers;
            float cooldownSeconds;    // 0 = no cooldown
            int   maxConcurrent;      // 0 = unlimited

            ActionDef() : cooldownSeconds(0.0f), maxConcurrent(0) {}

            // Move-only because ConditionExpr is move-only
            ActionDef(ActionDef&&) noexcept = default;
            ActionDef& operator=(ActionDef&&) noexcept = default;
            ActionDef(const ActionDef&) = delete;
            ActionDef& operator=(const ActionDef&) = delete;
        };

    } // namespace UtilityAI
} // namespace Dia
