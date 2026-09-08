#include "DiaOrder/OrderQueueComponent.h"

namespace Dia::Order {

    const Dia::Core::StringCRC OrderQueueComponent::kUniqueId("OrderQueueComponent");

    OrderQueue<EntityOrderContext>& OrderQueueComponent::GetQueue()
    {
        return mQueue;
    }

    const OrderQueue<EntityOrderContext>& OrderQueueComponent::GetQueue() const
    {
        return mQueue;
    }

} // namespace Dia::Order
