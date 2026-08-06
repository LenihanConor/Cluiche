#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Economy {

    // Forward declarations
    class EconomyInstance;

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

        // TODO Task 7: RegisterDerivedResource goes here
    };

}} // namespace Dia::Economy
