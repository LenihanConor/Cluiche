#include <DiaAICallout/CalloutObserverSubject.h>
#include <DiaAICallout/Callout.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::AICallout {

    // -----------------------------------------------------------------------
    // Subscribe / Unsubscribe
    // -----------------------------------------------------------------------

    void CalloutObserverSubject::Subscribe(ICalloutObserver* observer)
    {
        DIA_ASSERT(observer != nullptr, "CalloutObserverSubject::Subscribe: observer must not be null");
        if (mObservers.FindIndex(observer) < 0)
        {
            mObservers.Add(observer);
        }
    }

    void CalloutObserverSubject::Unsubscribe(ICalloutObserver* observer)
    {
        mObservers.RemoveFirst(observer);
    }

    // -----------------------------------------------------------------------
    // Notify methods
    // -----------------------------------------------------------------------

    void CalloutObserverSubject::NotifyCalloutEmitted(const Callout& callout, CalloutHandle handle)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnCalloutEmitted(callout, handle);
        }
    }

    void CalloutObserverSubject::NotifyCalloutClaimed(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnCalloutClaimed(handle, claimerEntityId);
        }
    }

    void CalloutObserverSubject::NotifyCalloutReleased(CalloutHandle handle, Dia::Core::StringCRC claimerEntityId)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnCalloutReleased(handle, claimerEntityId);
        }
    }

} // namespace Dia::AICallout
