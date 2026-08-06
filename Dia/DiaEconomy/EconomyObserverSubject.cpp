#include "DiaEconomy/EconomyObserverSubject.h"
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // Subscribe / Unsubscribe
    // -----------------------------------------------------------------------

    void EconomyObserverSubject::Subscribe(IEconomyObserver* observer)
    {
        DIA_ASSERT(observer != nullptr, "EconomyObserverSubject::Subscribe: observer must not be null");
        if (mObservers.FindIndex(observer) < 0)
        {
            mObservers.Add(observer);
        }
    }

    void EconomyObserverSubject::Unsubscribe(IEconomyObserver* observer)
    {
        mObservers.RemoveFirst(observer);
    }

    // -----------------------------------------------------------------------
    // Notify methods
    // -----------------------------------------------------------------------

    void EconomyObserverSubject::NotifyPoolChanged(const PoolChangedEvent& e)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnPoolChanged(e);
        }
    }

    void EconomyObserverSubject::NotifyTransactionClamped(const TransactionClampedEvent& e)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnTransactionClamped(e);
        }
    }

    void EconomyObserverSubject::NotifyTransferCompleted(const TransferCompletedEvent& e)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnTransferCompleted(e);
        }
    }

    void EconomyObserverSubject::NotifyPoolReachedMaximum(const EconomyInstance& instance,
                                                          Dia::Core::StringCRC resource_name)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnPoolReachedMaximum(instance, resource_name);
        }
    }

    void EconomyObserverSubject::NotifyPoolReachedMinimum(const EconomyInstance& instance,
                                                          Dia::Core::StringCRC resource_name)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnPoolReachedMinimum(instance, resource_name);
        }
    }

}} // namespace Dia::Economy
