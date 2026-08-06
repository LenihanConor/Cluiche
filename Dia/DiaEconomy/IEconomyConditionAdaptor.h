#pragma once

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // IEconomyConditionAdaptor
    //
    // Thin virtual interface that bridges the modifier stack's `when` field to
    // whatever condition-evaluation backend the host application provides.
    //
    // DiaEconomy has no compile-time dependency on DiaCondition.  Host code
    // creates a concrete subclass that wraps DiaCondition (or any other
    // evaluator), then calls EconomySystem::SetConditionAdaptor.
    //
    // When no adaptor is installed, conditional modifiers (those whose
    // when_condition field is non-empty) are silently skipped.
    // -----------------------------------------------------------------------
    class IEconomyConditionAdaptor
    {
    public:
        virtual ~IEconomyConditionAdaptor() = default;

        // Returns true if the expression evaluates to true.
        // expression is the raw when_condition string from ModifierDef.
        virtual bool Evaluate(const char* expression) const = 0;
    };

}} // namespace Dia::Economy
