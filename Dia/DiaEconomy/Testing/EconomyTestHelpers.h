#pragma once

#include "DiaEconomy/EconomySystem.h"
#include "DiaEconomy/EconomyInstance.h"
#include "DiaEconomy/EconomySchema.h"
#include "DiaEconomy/IEconomyObserver.h"
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Economy { namespace Testing {

    // -----------------------------------------------------------------------
    // AssertPoolValue — inline helper to check a pool value within tolerance
    // -----------------------------------------------------------------------
    // Usage: AssertPoolValue(instance, "gold"_crc, 100.0f, 0.001f);
    inline void AssertPoolValue(const EconomyInstance& instance,
                                Dia::Core::StringCRC resource_name,
                                float expected,
                                float tolerance = 0.001f,
                                const char* msg = "")
    {
        const float actual = instance.GetValue(resource_name);
        const float diff = std::fabsf(actual - expected);
        DIA_ASSERT(diff <= tolerance,
                   "AssertPoolValue failed: pool value out of tolerance");
        (void)msg; // msg is for human readability only
    }

    // -----------------------------------------------------------------------
    // EventCapture — subscribes to an EconomySystem and records all events
    // -----------------------------------------------------------------------
    // Usage:
    //   EventCapture capture;
    //   capture.Subscribe(system);
    //   system.Earn(instance, "gold"_crc, 50.0f);
    //   DIA_ASSERT(capture.poolChangedCount == 1, "Expected pool changed event");
    //   capture.Unsubscribe(system);
    class EventCapture : public IEconomyObserver
    {
    public:
        unsigned int poolChangedCount         = 0;
        unsigned int transactionClampedCount  = 0;
        unsigned int transferCompletedCount   = 0;
        unsigned int poolReachedMaximumCount  = 0;
        unsigned int poolReachedMinimumCount  = 0;

        // Last event payloads
        PoolChangedEvent         lastPoolChanged{};
        TransactionClampedEvent  lastTransactionClamped{};
        TransferCompletedEvent   lastTransferCompleted{};

        void Subscribe(EconomySystem& system)
        {
            system.GetObserverSubject().Subscribe(this);
        }

        void Unsubscribe(EconomySystem& system)
        {
            system.GetObserverSubject().Unsubscribe(this);
        }

        void Reset()
        {
            poolChangedCount = 0;
            transactionClampedCount = 0;
            transferCompletedCount = 0;
            poolReachedMaximumCount = 0;
            poolReachedMinimumCount = 0;
        }

        void OnPoolChanged(const PoolChangedEvent& e) override
        {
            ++poolChangedCount;
            lastPoolChanged = e;
        }

        void OnTransactionClamped(const TransactionClampedEvent& e) override
        {
            ++transactionClampedCount;
            lastTransactionClamped = e;
        }

        void OnTransferCompleted(const TransferCompletedEvent& e) override
        {
            ++transferCompletedCount;
            lastTransferCompleted = e;
        }

        void OnPoolReachedMaximum(const PoolReachedMaximumEvent&) override
        {
            ++poolReachedMaximumCount;
        }

        void OnPoolReachedMinimum(const PoolReachedMinimumEvent&) override
        {
            ++poolReachedMinimumCount;
        }
    };

    // -----------------------------------------------------------------------
    // AssertEventFired — check event count
    // -----------------------------------------------------------------------
    // Usage: AssertEventFired(capture.poolChangedCount, 1, "pool changed");
    inline void AssertEventFired(unsigned int actual_count, unsigned int expected_count,
                                  const char* event_name = "event")
    {
        DIA_ASSERT(actual_count == expected_count,
                   "AssertEventFired: event count mismatch");
        (void)event_name; // event_name is for human readability only
    }

}}} // namespace Dia::Economy::Testing
