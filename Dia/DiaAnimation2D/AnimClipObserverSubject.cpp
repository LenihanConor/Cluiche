#include "DiaAnimation2D/AnimClipObserverSubject.h"
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Animation2D {

    // -----------------------------------------------------------------------
    // Subscribe / Unsubscribe
    // -----------------------------------------------------------------------

    void AnimClipObserverSubject::Subscribe(IAnimClipObserver* observer)
    {
        DIA_ASSERT(observer != nullptr, "AnimClipObserverSubject::Subscribe: observer must not be null");
        if (mObservers.FindIndex(observer) < 0)
        {
            mObservers.Add(observer);
        }
    }

    void AnimClipObserverSubject::Unsubscribe(IAnimClipObserver* observer)
    {
        mObservers.RemoveFirst(observer);
    }

    // -----------------------------------------------------------------------
    // Notify methods
    // -----------------------------------------------------------------------

    void AnimClipObserverSubject::NotifyClipFinished(const AnimClip& clip)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnClipFinished(clip);
        }
    }

    void AnimClipObserverSubject::NotifyClipLooped(const AnimClip& clip)
    {
        const unsigned int count = mObservers.Size();
        for (unsigned int i = 0; i < count; ++i)
        {
            mObservers[i]->OnClipLooped(clip);
        }
    }

}} // namespace Dia::Animation2D
