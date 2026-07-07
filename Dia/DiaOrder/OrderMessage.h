#pragma once

#include "DiaEntity/Entity.h"

namespace Dia::Order {

    // A DiaMailbox-routable message that carries an order pointer and its
    // target entity. The receiving module casts `order` to the concrete
    // IOrder<TContext>* it expects.
    struct OrderMessage {
        Dia::Entity::Entity target;
        void*               order;  // type-erased; module casts to concrete TContext
        bool                urgent; // true = EnqueueFront, false = Enqueue
    };

} // namespace Dia::Order
