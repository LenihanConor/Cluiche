#pragma once

#include <DiaHTN/OperatorRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace HTN
    {
        //-------------------------------------------------------------------------------------------
        // RegisterRuleActionAsOperator
        //
        // Bridge adapter: wraps a DiaRules RuleActionFn as an instant-succeed OperatorFn.
        // The RuleActionFn receives the operatorContext pointer as its void* data.
        // Registered actions always return kSucceeded — use for fire-and-forget operators.
        //
        // SD-003: Bridge lives in the higher-level system (DiaHTN), not DiaRules.
        //         Same pattern as ConditionGuardAdapter in DiaCondition (SD-008).
        //-------------------------------------------------------------------------------------------
        void RegisterRuleActionAsOperator(Dia::Core::StringCRC operatorId,
                                          Dia::Rules::RuleActionFn action,
                                          OperatorRegistry& registry);

    } // namespace HTN
} // namespace Dia
