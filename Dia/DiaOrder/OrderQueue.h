#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaOrder/IOrder.h"
#include "DiaOrder/IOrderQueueObserver.h"

namespace Dia::Order {

    template<typename TContext>
    class OrderQueue {
    public:
        void Enqueue(IOrder<TContext>* order);
        void EnqueueFront(IOrder<TContext>* order);
        void Clear();
        void Cancel();
        void Update(TContext& ctx, float dt);

        bool                     IsEmpty()          const;
        int                      GetQueueDepth()    const;
        const IOrder<TContext>*  GetCurrentOrder()  const;

        void AddObserver(IOrderQueueObserver<TContext>& observer);
        void RemoveObserver(IOrderQueueObserver<TContext>& observer);

    private:
        static const unsigned int kMaxQueueDepth = 16;
        static const unsigned int kMaxObservers  = 8;

        // Start the front pending order as the new current. Caller must ensure
        // mCurrent == nullptr and mPending is non-empty before calling.
        void StartNext(TContext& ctx);

        // Notify observers and log without calling the virtual Cancel callback
        // (used when no TContext is available).
        void NotifyAndDropCurrent();

        IOrder<TContext>* mCurrent = nullptr;

        Dia::Core::Containers::DynamicArrayC<IOrder<TContext>*, kMaxQueueDepth> mPending;
        Dia::Core::Containers::DynamicArrayC<IOrderQueueObserver<TContext>*, kMaxObservers> mObservers;
    };

} // namespace Dia::Order

#include "DiaOrder/OrderQueue.inl"
