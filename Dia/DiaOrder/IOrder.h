#pragma once

#include "DiaCore/CRC/StringCRC.h"

namespace Dia::Order {

    template<typename TContext>
    struct IOrder {
        virtual ~IOrder() = default;

        virtual Dia::Core::StringCRC GetOrderId() const = 0;
        virtual void Start(TContext& ctx)             = 0;
        virtual bool Update(TContext& ctx, float dt)  = 0; // true = finished
        virtual void Finish(TContext& ctx)             = 0;
        virtual void Cancel(TContext& ctx)             = 0;
    };

} // namespace Dia::Order
