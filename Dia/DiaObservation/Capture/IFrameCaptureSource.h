#pragma once
#include <DiaCore/Capture/FrameCapture.h>

namespace Dia { namespace Observation { namespace Capture {

    // Narrow interface exposing only the capture-readback surface of ICanvas.
    // Implemented by DiaBgfx/DiaGraphics and passed into CaptureManager::Initialize(),
    // allowing CaptureManager to live at foundation/services without depending on
    // the full ICanvas interface (domain/visual/core).
    class IFrameCaptureSource
    {
    public:
        virtual ~IFrameCaptureSource() = default;
        virtual Dia::Graphics::FrameCaptureToken  RequestFrameCapture() = 0;
        virtual Dia::Graphics::FrameCaptureResult PollFrameCapture(const Dia::Graphics::FrameCaptureToken& token) = 0;
    };

} } } // namespace Dia::Observation::Capture
