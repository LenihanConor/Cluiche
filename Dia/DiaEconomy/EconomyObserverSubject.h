#pragma once

#include "DiaEconomy/IEconomyObserver.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // EconomyObserverSubject
    // -----------------------------------------------------------------------
    class EconomyObserverSubject
    {
    public:
        void Subscribe  (IEconomyObserver* observer);
        void Unsubscribe(IEconomyObserver* observer);

        void NotifyPoolChanged        (const PoolChangedEvent&);
        void NotifyTransactionClamped (const TransactionClampedEvent&);
        void NotifyTransferCompleted  (const TransferCompletedEvent&);
        void NotifyPoolReachedMaximum (const EconomyInstance&, Dia::Core::StringCRC resource_name);
        void NotifyPoolReachedMinimum (const EconomyInstance&, Dia::Core::StringCRC resource_name);

    private:
        Dia::Core::Containers::DynamicArrayC<IEconomyObserver*, 16> mObservers;
    };

}} // namespace Dia::Economy
