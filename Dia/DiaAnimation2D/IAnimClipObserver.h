#pragma once

namespace Dia { namespace Animation2D {

    // Forward declarations
    class AnimClip;

    // -----------------------------------------------------------------------
    // IAnimClipObserver
    // -----------------------------------------------------------------------
    class IAnimClipObserver
    {
    public:
        virtual ~IAnimClipObserver() = default;

        virtual void OnClipFinished (const AnimClip&) {}
        virtual void OnClipLooped   (const AnimClip&) {}
    };

}} // namespace Dia::Animation2D
