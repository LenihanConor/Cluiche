#pragma once

#include "DiaOrder/IOrder.h"
#include "DiaOrder/IOrderQueueObserver.h"
#include "DiaOrder/OrderQueue.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/CRC/StringCRC.h"

namespace Dia { namespace Order { namespace Testing {

    // ---------------------------------------------------------------------------
    // Free-function assertions
    // ---------------------------------------------------------------------------

    template<typename TContext>
    inline void AssertOrderPending(const Dia::Order::OrderQueue<TContext>& queue, Dia::Core::StringCRC orderId)
    {
        DIA_ASSERT(!queue.IsEmpty(), "Expected queue to have a pending order but it was empty");
        DIA_ASSERT(queue.GetCurrentOrder() != nullptr, "Expected a current order but GetCurrentOrder() returned null");
        DIA_ASSERT(queue.GetCurrentOrder()->GetOrderId() == orderId, "Expected current order id to match the provided orderId");
    }

    template<typename TContext>
    inline void AssertQueueEmpty(const Dia::Order::OrderQueue<TContext>& queue)
    {
        DIA_ASSERT(queue.IsEmpty(), "Expected queue to be empty but it was not");
    }

    // ---------------------------------------------------------------------------
    // MockOrder<TContext>
    // ---------------------------------------------------------------------------

    template<typename TContext>
    class MockOrder : public Dia::Order::IOrder<TContext>
    {
    public:
        Dia::Core::StringCRC id;
        int  startCount  = 0;
        int  updateCount = 0;
        int  finishCount = 0;
        int  cancelCount = 0;
        bool shouldFinish = false;

        explicit MockOrder(Dia::Core::StringCRC id_, bool shouldFinish_ = false)
            : id(id_)
            , shouldFinish(shouldFinish_)
        {}

        Dia::Core::StringCRC GetOrderId() const override { return id; }

        void Start(TContext&) override
        {
            ++startCount;
        }

        bool Update(TContext&, float) override
        {
            ++updateCount;
            return shouldFinish;
        }

        void Finish(TContext&) override
        {
            ++finishCount;
        }

        void Cancel(TContext&) override
        {
            ++cancelCount;
        }
    };

    // ---------------------------------------------------------------------------
    // MockOrderQueueObserver<TContext>
    // ---------------------------------------------------------------------------

    template<typename TContext>
    class MockOrderQueueObserver : public Dia::Order::IOrderQueueObserver<TContext>
    {
    public:
        int startedCount   = 0;
        int finishedCount  = 0;
        int cancelledCount = 0;
        int emptyCount     = 0;

        const Dia::Order::IOrder<TContext>* lastStarted   = nullptr;
        const Dia::Order::IOrder<TContext>* lastFinished  = nullptr;
        const Dia::Order::IOrder<TContext>* lastCancelled = nullptr;

        void OnOrderStarted(const Dia::Order::IOrder<TContext>& order) override
        {
            ++startedCount;
            lastStarted = &order;
        }

        void OnOrderFinished(const Dia::Order::IOrder<TContext>& order) override
        {
            ++finishedCount;
            lastFinished = &order;
        }

        void OnOrderCancelled(const Dia::Order::IOrder<TContext>& order) override
        {
            ++cancelledCount;
            lastCancelled = &order;
        }

        void OnQueueEmpty() override
        {
            ++emptyCount;
        }
    };

}}} // namespace Dia::Order::Testing
