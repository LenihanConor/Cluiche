#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaEntity/IComponent.h"
#include "DiaEntity/Entity.h"
#include "DiaBlackboard/Blackboard.h"
#include "DiaOrder/OrderQueue.h"

namespace Dia::Order {

    // Concrete context type used by all entity-driven orders.
    struct EntityOrderContext {
        Dia::Entity::Entity          entity;
        Dia::Blackboard::Blackboard& board;
    };

    // IComponent wrapper that owns an OrderQueue<EntityOrderContext>.
    // Register via ComponentFactoryRegistry using kUniqueId.
    class OrderQueueComponent : public Dia::Entity::IComponent {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        Dia::Core::StringCRC GetTypeId() const override { return kUniqueId; }

        OrderQueue<EntityOrderContext>&       GetQueue();
        const OrderQueue<EntityOrderContext>& GetQueue() const;

    private:
        OrderQueue<EntityOrderContext> mQueue;
    };

} // namespace Dia::Order
