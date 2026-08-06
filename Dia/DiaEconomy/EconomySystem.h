#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "DiaEconomy/EconomyObserverSubject.h"
#include "DiaEconomy/IEconomyConditionAdaptor.h"

namespace Dia { namespace Economy {

    // Forward declarations
    class EconomyInstance;
    struct ModifierDef;

    // -----------------------------------------------------------------------
    // DerivedResourceFn
    // -----------------------------------------------------------------------
    using DerivedResourceFn = float(*)(const EconomyInstance&);

    // -----------------------------------------------------------------------
    // TransactionResult
    // -----------------------------------------------------------------------
    enum class TransactionResult
    {
        Success,         // full amount applied
        Clamped,         // amount reduced due to min/max limit; event fired
        UnknownResource  // resource_name not in schema; DIA_ASSERT in debug
    };

    // -----------------------------------------------------------------------
    // EconomySystem
    // -----------------------------------------------------------------------
    class EconomySystem
    {
    public:
        // Called by game/test code each sim tick — applies income rules and
        // modifiers for one instance.
        void Tick(EconomyInstance& instance, float delta_seconds);

        // Adds amount to pool; clamps to maximum; returns Clamped if truncated.
        [[nodiscard]] TransactionResult Earn(EconomyInstance& instance,
                                             Dia::Core::StringCRC resource_name,
                                             float amount);

        // Subtracts amount; clamps to minimum (zero); returns Clamped if
        // insufficient funds.
        [[nodiscard]] TransactionResult Spend(EconomyInstance& instance,
                                              Dia::Core::StringCRC resource_name,
                                              float amount);

        // Atomic deduct-then-deposit; returns result of the deduct phase.
        [[nodiscard]] TransactionResult Transfer(EconomyInstance& from,
                                                 EconomyInstance& to,
                                                 Dia::Core::StringCRC resource_name,
                                                 float amount);

        // Directly sets value (clamped to [min, max]); returns Clamped if
        // value was adjusted.
        [[nodiscard]] TransactionResult SetValue(EconomyInstance& instance,
                                                 Dia::Core::StringCRC resource_name,
                                                 float value);

        // Observer subscription point — game code calls Subscribe/Unsubscribe here.
        EconomyObserverSubject& GetObserverSubject() { return mObserverSubject; }

        // Optional condition adaptor for evaluating `when` fields on ModifierDefs.
        // When null, any modifier whose when_condition is non-empty is skipped.
        void SetConditionAdaptor(IEconomyConditionAdaptor* adaptor) { mConditionAdaptor = adaptor; }

        // Registers a derived (computed) resource with a C++ callback function.
        void RegisterDerivedResource(Dia::Core::StringCRC resource_name, DerivedResourceFn fn);

        // Queries a derived resource; invokes the registered callback.
        // If unregistered, logs a warning and returns 0.0f.
        float QueryDerived(const EconomyInstance& instance, Dia::Core::StringCRC resource_name) const;

    private:
        // Returns true if the modifier should be applied this tick.
        // Always-on modifiers (empty when_condition) return true.
        // Conditional modifiers return false when no adaptor is installed.
        bool IsModifierConditionMet(const ModifierDef& mod) const;

        // Derived resource registry entry.
        struct DerivedEntry
        {
            Dia::Core::StringCRC key;
            DerivedResourceFn    fn;
        };

        EconomyObserverSubject                                      mObserverSubject;
        IEconomyConditionAdaptor*                                   mConditionAdaptor = nullptr;
        Dia::Core::Containers::DynamicArrayC<DerivedEntry, 32>     mDerivedResources;
    };

}} // namespace Dia::Economy
