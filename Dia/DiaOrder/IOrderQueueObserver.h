#pragma once

#include "DiaOrder/IOrder.h"

namespace Dia::Order {

    template<typename TContext>
    class IOrderQueueObserver {
    public:
        virtual ~IOrderQueueObserver() = default;

        virtual void OnOrderStarted(const IOrder<TContext>& order)   {}
        virtual void OnOrderFinished(const IOrder<TContext>& order)  {}
        virtual void OnOrderCancelled(const IOrder<TContext>& order) {}
        virtual void OnQueueEmpty()                                   {}
    };

} // namespace Dia::Order
