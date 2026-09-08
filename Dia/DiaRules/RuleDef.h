#pragma once

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Rules
    {
        //-------------------------------------------------------------------------------------------
        // RuleDef
        //
        // Value type describing a single rule: an optional id, a guard expression, and a list of
        // action IDs to fire when the guard evaluates to true.
        //
        // SD-001: All-matching evaluation — rules do not stop on first match.
        // SD-002: Rules fire in definition order (no priority in v1).
        // SD-005: RuleSet is immutable after LoadFromJson; RuleDef is consumed by move into RuleSet.
        // PD-001: Action IDs are StringCRC.
        // PD-004: DynamicArrayC for actions; no STL in public API.
        // AD-003: Dia::Rules:: namespace.
        //
        // ConditionExpr is move-only, so RuleDef is move-only as well.
        //-------------------------------------------------------------------------------------------
        struct RuleDef
        {
            // Optional: kZero / default-constructed StringCRC means unnamed rule.
            Dia::Core::StringCRC id;

            // Guard expression — move-only because ConditionExpr is move-only.
            Dia::Condition::ConditionExpr guard;

            // Action IDs to fire when guard passes. Capacity 8 is sufficient for v1.
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> actions;

            RuleDef() = default;
            ~RuleDef() = default;

            // Move-only (ConditionExpr is not copyable)
            RuleDef(RuleDef&&) = default;
            RuleDef& operator=(RuleDef&&) = default;

            RuleDef(const RuleDef&) = delete;
            RuleDef& operator=(const RuleDef&) = delete;
        };

    } // namespace Rules
} // namespace Dia
