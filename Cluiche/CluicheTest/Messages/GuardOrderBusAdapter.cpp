#include "Messages/GuardOrderBusAdapter.h"
#include "Messages/order_messages.h"
#include "Modules/TestStages/BehaviourTreeTestStageModule.h"

#include <DiaMessageBus/Bus.h>

namespace CluicheTest {

    GuardOrderBusAdapter::GuardOrderBusAdapter(Dia::MessageBus::Bus& bus)
        : mBus(bus)
    {
        // Registers all four message types + producer metadata. RegisterType
        // is a no-op (returns false, no assert) if already registered, so
        // this is safe even if something else already registered these types.
        Messages::RegisterMessages(mBus, Messages::Handlers{});
    }

    void GuardOrderBusAdapter::OnOrderStarted(const Dia::Order::IOrder<GuardOrderContext>& order)
    {
        Messages::OrderStartedEvent msg;
        msg.orderId = order.GetOrderId();
        msg.target  = Dia::Maths::Vector2D{};
        msg.speed   = 0.0f;

        // GuardMoveOrder is the only concrete IOrder<GuardOrderContext> subtype
        // today; recover its extra fields when the started order is one.
        if (const GuardMoveOrder* moveOrder = dynamic_cast<const GuardMoveOrder*>(&order))
        {
            msg.target = moveOrder->target;
            msg.speed  = moveOrder->speed;
        }

        mBus.Broadcast(msg);
    }

    void GuardOrderBusAdapter::OnOrderFinished(const Dia::Order::IOrder<GuardOrderContext>& order)
    {
        Messages::OrderFinishedEvent msg;
        msg.orderId = order.GetOrderId();
        mBus.Broadcast(msg);
    }

    void GuardOrderBusAdapter::OnOrderCancelled(const Dia::Order::IOrder<GuardOrderContext>& order)
    {
        Messages::OrderCancelledEvent msg;
        msg.orderId = order.GetOrderId();
        mBus.Broadcast(msg);
    }

    void GuardOrderBusAdapter::OnQueueEmpty()
    {
        mBus.Broadcast(Messages::OrderQueueEmptyEvent{});
    }

} // namespace CluicheTest
