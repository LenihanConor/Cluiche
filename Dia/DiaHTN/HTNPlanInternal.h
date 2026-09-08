#pragma once

// Internal header — NOT part of DiaHTN's public API.
// Exposes HTNPlan mutation helpers for HTNPlanner.cpp only.

#include <DiaHTN/HTNPlan.h>

namespace Dia
{
    namespace HTN
    {
        void HTNPlanAddTask(HTNPlan& plan, HTNTask&& task);

        void HTNPlanAddSnapFloat(HTNPlan& plan,
                                 Dia::Core::StringCRC slot,
                                 Dia::Core::StringCRC field,
                                 float value);

        void HTNPlanAddSnapBool(HTNPlan& plan,
                                Dia::Core::StringCRC slot,
                                Dia::Core::StringCRC field,
                                bool value);

    } // namespace HTN
} // namespace Dia
