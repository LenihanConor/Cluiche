#include "DiaEconomy/EconomyBusAdapter.h"
#include "DiaEconomy/Messages/economy_messages.h"
#include <DiaMessageBus/Bus.h>

namespace Dia { namespace Economy {

    EconomyBusAdapter::EconomyBusAdapter(Dia::MessageBus::Bus& bus)
        : mBus(bus)
    {
        // Registers all five message types + producer metadata. RegisterType
        // is a no-op (returns false, no assert) if already registered, so
        // this is safe even if something else already registered these types.
        Messages::RegisterMessages(mBus, Messages::Handlers{});
    }

    void EconomyBusAdapter::OnPoolChanged(const PoolChangedEvent& e)
    {
        Messages::PoolChangedEvent msg;
        msg.instanceName = e.instanceName;
        msg.resourceName = e.resourceName;
        msg.newValue     = e.newValue;
        msg.delta        = e.delta;
        mBus.Broadcast(msg);
    }

    void EconomyBusAdapter::OnTransactionClamped(const TransactionClampedEvent& e)
    {
        Messages::TransactionClampedEvent msg;
        msg.instanceName    = e.instanceName;
        msg.resourceName    = e.resourceName;
        msg.requestedAmount = e.requestedAmount;
        msg.actualAmount    = e.actualAmount;
        mBus.Broadcast(msg);
    }

    void EconomyBusAdapter::OnTransferCompleted(const TransferCompletedEvent& e)
    {
        Messages::TransferCompletedEvent msg;
        msg.fromInstanceName = e.fromInstanceName;
        msg.toInstanceName   = e.toInstanceName;
        msg.resourceName     = e.resourceName;
        msg.amount           = e.amount;
        mBus.Broadcast(msg);
    }

    void EconomyBusAdapter::OnPoolReachedMaximum(const PoolReachedMaximumEvent& e)
    {
        Messages::PoolReachedMaximumEvent msg;
        msg.instanceName = e.instanceName;
        msg.resourceName = e.resourceName;
        mBus.Broadcast(msg);
    }

    void EconomyBusAdapter::OnPoolReachedMinimum(const PoolReachedMinimumEvent& e)
    {
        Messages::PoolReachedMinimumEvent msg;
        msg.instanceName = e.instanceName;
        msg.resourceName = e.resourceName;
        mBus.Broadcast(msg);
    }

}} // namespace Dia::Economy
