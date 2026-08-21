#pragma once

#include "DiaAnimation2D/IAnimClipObserver.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Animation2D {

    // -----------------------------------------------------------------------
    // AnimClipObserverSubject
    // -----------------------------------------------------------------------
    class AnimClipObserverSubject
    {
    public:
        void Subscribe  (IAnimClipObserver* observer);
        void Unsubscribe(IAnimClipObserver* observer);

        void NotifyClipFinished (const AnimClip& clip);
        void NotifyClipLooped   (const AnimClip& clip);

    private:
        Dia::Core::Containers::DynamicArrayC<IAnimClipObserver*, 16> mObservers;
    };

}} // namespace Dia::Animation2D
