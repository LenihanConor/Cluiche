#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Economy {

    // Forward declarations
    class EconomyInstance;

    // -----------------------------------------------------------------------
    // Event structs
    // -----------------------------------------------------------------------

    struct PoolChangedEvent
    {
        const EconomyInstance* instance;
        Dia::Core::StringCRC   resource_name;
        float                  new_value;
        float                  delta;
    };

    struct TransactionClampedEvent
    {
        const EconomyInstance* instance;
        Dia::Core::StringCRC   resource_name;
        float                  requested_amount;
        float                  actual_amount;
    };

    struct TransferCompletedEvent
    {
        const EconomyInstance* from_instance;
        const EconomyInstance* to_instance;
        Dia::Core::StringCRC   resource_name;
        float                  amount;
    };

    // -----------------------------------------------------------------------
    // IEconomyObserver
    // -----------------------------------------------------------------------
    class IEconomyObserver
    {
    public:
        virtual ~IEconomyObserver() = default;

        virtual void OnPoolChanged        (const PoolChangedEvent&)                                            {}
        virtual void OnTransactionClamped (const TransactionClampedEvent&)                                     {}
        virtual void OnTransferCompleted  (const TransferCompletedEvent&)                                      {}
        virtual void OnPoolReachedMaximum (const EconomyInstance&, Dia::Core::StringCRC resource_name)         {}
        virtual void OnPoolReachedMinimum (const EconomyInstance&, Dia::Core::StringCRC resource_name)         {}
    };

}} // namespace Dia::Economy
