#pragma once

#include "DiaEconomy/IEconomyObserver.h"

namespace Dia { namespace MessageBus { class Bus; } }

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // EconomyBusAdapter
    //
    // One more IEconomyObserver subscriber — registered on an
    // EconomyObserverSubject alongside any other direct observers, it does
    // not replace them. Forwards each synchronous Notify() callback onto the
    // shared DiaMessageBus::Bus via Bus::Broadcast<T>(). The event payloads
    // it forwards are snapshot values only (see IEconomyObserver.h), so they
    // remain safe even if a bus-queued message is delivered on a later pass.
    //
    // Bus::Broadcast<T> requires T::kTypeId, so this adapter broadcasts the
    // codegen'd Dia::Economy::Messages::* structs (Messages/economy_messages.h),
    // converting field-by-field from the plain IEconomyObserver.h event on
    // each callback. The constructor registers all five message types (and
    // their producer metadata) with the bus.
    //
    // DiaEconomy's core types (IEconomyObserver.h, EconomyInstance, etc.)
    // have no DiaMessageBus include/dependency — only this adapter does.
    // -----------------------------------------------------------------------
    class EconomyBusAdapter : public IEconomyObserver
    {
    public:
        explicit EconomyBusAdapter(Dia::MessageBus::Bus& bus);

        void OnPoolChanged        (const PoolChangedEvent&)        override;
        void OnTransactionClamped (const TransactionClampedEvent&) override;
        void OnTransferCompleted  (const TransferCompletedEvent&)  override;
        void OnPoolReachedMaximum (const PoolReachedMaximumEvent&) override;
        void OnPoolReachedMinimum (const PoolReachedMinimumEvent&) override;

    private:
        Dia::MessageBus::Bus& mBus;
    };

}} // namespace Dia::Economy
