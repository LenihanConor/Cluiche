#pragma once

#include "DiaCore/Core/Assert.h"
#include "DiaObservation/Log/DiaLog.h"
#include "DiaOrder/OrderQueue.h"

namespace Dia::Order {

    // ---------------------------------------------------------------------------
    // Public interface
    // ---------------------------------------------------------------------------

    template<typename TContext>
    void OrderQueue<TContext>::Enqueue(IOrder<TContext>* order)
    {
        DIA_ASSERT(order != nullptr, "OrderQueue::Enqueue — null order pointer");
        DIA_ASSERT(!mPending.IsFull(), "OrderQueue::Enqueue — queue is full");

        mPending.Add(order);
    }

    template<typename TContext>
    void OrderQueue<TContext>::EnqueueFront(IOrder<TContext>* order)
    {
        DIA_ASSERT(order != nullptr, "OrderQueue::EnqueueFront — null order pointer");

        // Cancel the running order (no TContext available here — drop without
        // calling the virtual Cancel callback; observer is still notified).
        if (mCurrent != nullptr)
        {
            NotifyAndDropCurrent();
        }

        // Insert at front of pending queue.
        // DynamicArrayC has no InsertAt; we rebuild by swapping.
        if (mPending.IsEmpty())
        {
            mPending.Add(order);
        }
        else
        {
            // Swap existing entries into a temporary, then rebuild with order first.
            Dia::Core::Containers::DynamicArrayC<IOrder<TContext>*, kMaxQueueDepth> temp;
            for (unsigned int i = 0; i < mPending.Size(); ++i)
            {
                temp.Add(mPending[i]);
            }
            mPending.RemoveAll();
            mPending.Add(order);
            for (unsigned int i = 0; i < temp.Size(); ++i)
            {
                mPending.Add(temp[i]);
            }
        }
    }

    template<typename TContext>
    void OrderQueue<TContext>::Clear()
    {
        if (mCurrent != nullptr)
        {
            NotifyAndDropCurrent();
        }

        mPending.RemoveAll();

        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            mObservers[i]->OnQueueEmpty();
        }
    }

    template<typename TContext>
    void OrderQueue<TContext>::Cancel()
    {
        if (mCurrent == nullptr)
        {
            return;
        }

        NotifyAndDropCurrent();

        if (!mPending.IsEmpty())
        {
            // Advance will happen on the next Update tick when StartNext is called.
            // We can't call StartNext here because we have no TContext.
        }
        else
        {
            for (unsigned int i = 0; i < mObservers.Size(); ++i)
            {
                mObservers[i]->OnQueueEmpty();
            }
        }
    }

    template<typename TContext>
    void OrderQueue<TContext>::Update(TContext& ctx, float dt)
    {
        // Promote the first pending order if nothing is running.
        if (mCurrent == nullptr && !mPending.IsEmpty())
        {
            StartNext(ctx);
        }

        if (mCurrent == nullptr)
        {
            return;
        }

        bool done = mCurrent->Update(ctx, dt);

        if (done)
        {
            IOrder<TContext>* finished = mCurrent;
            finished->Finish(ctx);

            DIA_LOG_INFO("Order", "Order finished: %s", finished->GetOrderId().AsChar());

            for (unsigned int i = 0; i < mObservers.Size(); ++i)
            {
                mObservers[i]->OnOrderFinished(*finished);
            }

            mCurrent = nullptr;

            if (!mPending.IsEmpty())
            {
                StartNext(ctx);
            }
            else
            {
                for (unsigned int i = 0; i < mObservers.Size(); ++i)
                {
                    mObservers[i]->OnQueueEmpty();
                }
            }
        }
    }

    template<typename TContext>
    bool OrderQueue<TContext>::IsEmpty() const
    {
        return mCurrent == nullptr && mPending.IsEmpty();
    }

    template<typename TContext>
    int OrderQueue<TContext>::GetQueueDepth() const
    {
        int depth = static_cast<int>(mPending.Size());
        if (mCurrent != nullptr)
        {
            depth += 1;
        }
        return depth;
    }

    template<typename TContext>
    const IOrder<TContext>* OrderQueue<TContext>::GetCurrentOrder() const
    {
        return mCurrent;
    }

    template<typename TContext>
    void OrderQueue<TContext>::AddObserver(IOrderQueueObserver<TContext>& observer)
    {
        DIA_ASSERT(!mObservers.IsFull(), "OrderQueue::AddObserver — observer capacity exceeded");
        mObservers.Add(&observer);
    }

    template<typename TContext>
    void OrderQueue<TContext>::RemoveObserver(IOrderQueueObserver<TContext>& observer)
    {
        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            if (mObservers[i] == &observer)
            {
                mObservers.RemoveAt(i);
                return;
            }
        }
        DIA_ASSERT(false, "OrderQueue::RemoveObserver — observer not found");
    }

    // ---------------------------------------------------------------------------
    // Private helpers
    // ---------------------------------------------------------------------------

    template<typename TContext>
    void OrderQueue<TContext>::StartNext(TContext& ctx)
    {
        DIA_ASSERT(!mPending.IsEmpty(), "OrderQueue::StartNext — pending queue is empty");
        DIA_ASSERT(mCurrent == nullptr,  "OrderQueue::StartNext — an order is already running");

        mCurrent = mPending[0u];
        mPending.RemoveAt(0u);

        mCurrent->Start(ctx);

        DIA_LOG_INFO("Order", "Order started: %s", mCurrent->GetOrderId().AsChar());

        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            mObservers[i]->OnOrderStarted(*mCurrent);
        }
    }

    template<typename TContext>
    void OrderQueue<TContext>::NotifyAndDropCurrent()
    {
        DIA_ASSERT(mCurrent != nullptr, "OrderQueue::NotifyAndDropCurrent — no current order");

        IOrder<TContext>* cancelled = mCurrent;
        mCurrent = nullptr;

        DIA_LOG_INFO("Order", "Order cancelled: %s", cancelled->GetOrderId().AsChar());

        for (unsigned int i = 0; i < mObservers.Size(); ++i)
        {
            mObservers[i]->OnOrderCancelled(*cancelled);
        }
    }

} // namespace Dia::Order
