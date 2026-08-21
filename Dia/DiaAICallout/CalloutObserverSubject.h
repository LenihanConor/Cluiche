#pragma once

#include <DiaAICallout/ICalloutObserver.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::AICallout {

    struct Callout;

    // -----------------------------------------------------------------------
    // CalloutObserverSubject
    //
    // Fixed-capacity subscriber list for ICalloutObserver — no heap allocation.
    // Same shape as Dia::Economy::EconomyObserverSubject.
    // -----------------------------------------------------------------------
    class CalloutObserverSubject
    {
    public:
        void Subscribe  (ICalloutObserver* observer);
        void Unsubscribe(ICalloutObserver* observer);

        void NotifyCalloutEmitted  (const Callout& callout, CalloutHandle handle);
        void NotifyCalloutClaimed  (CalloutHandle handle, Dia::Core::StringCRC claimerEntityId);
        void NotifyCalloutReleased (CalloutHandle handle, Dia::Core::StringCRC claimerEntityId);

    private:
        Dia::Core::Containers::DynamicArrayC<ICalloutObserver*, 16> mObservers;
    };

} // namespace Dia::AICallout
