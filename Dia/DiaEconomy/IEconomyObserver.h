#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Economy {

    // Forward declarations
    class EconomyInstance;

    // -----------------------------------------------------------------------
    // Event structs
    //
    // Snapshot values only — no pointer/reference to a live EconomyInstance.
    // These payloads are safe to forward onto DiaMessageBus (a bus-queued
    // message can sit until the next flush pass, by which time a raw
    // instance pointer could be stale or dangling). Instance identity is
    // carried via EconomyInstance::GetInstanceName()'s StringCRC.
    // -----------------------------------------------------------------------

    struct PoolChangedEvent
    {
        Dia::Core::StringCRC instanceName;
        Dia::Core::StringCRC resourceName;
        float                newValue;
        float                delta;
    };

    struct TransactionClampedEvent
    {
        Dia::Core::StringCRC instanceName;
        Dia::Core::StringCRC resourceName;
        float                requestedAmount;
        float                actualAmount;
    };

    struct TransferCompletedEvent
    {
        Dia::Core::StringCRC fromInstanceName;
        Dia::Core::StringCRC toInstanceName;
        Dia::Core::StringCRC resourceName;
        float                amount;
    };

    struct PoolReachedMaximumEvent
    {
        Dia::Core::StringCRC instanceName;
        Dia::Core::StringCRC resourceName;
    };

    struct PoolReachedMinimumEvent
    {
        Dia::Core::StringCRC instanceName;
        Dia::Core::StringCRC resourceName;
    };

    // -----------------------------------------------------------------------
    // IEconomyObserver
    // -----------------------------------------------------------------------
    class IEconomyObserver
    {
    public:
        virtual ~IEconomyObserver() = default;

        virtual void OnPoolChanged        (const PoolChangedEvent&)        {}
        virtual void OnTransactionClamped (const TransactionClampedEvent&) {}
        virtual void OnTransferCompleted  (const TransferCompletedEvent&)  {}
        virtual void OnPoolReachedMaximum (const PoolReachedMaximumEvent&) {}
        virtual void OnPoolReachedMinimum (const PoolReachedMinimumEvent&) {}
    };

}} // namespace Dia::Economy
