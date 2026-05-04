////////////////////////////////////////////////////////////////////////////////
// Filename: IFixedPrimitiveBuffer.h
// Description: Write-only sink passed to IObjectRenderer::BuildPrimitives().
//              Mirrors the DebugFrameData request API so renderers can be
//              written without knowing which target they are writing to.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia
{
    namespace Debug
    {
        class IFixedPrimitiveBuffer
        {
        public:
            virtual ~IFixedPrimitiveBuffer() = default;

            virtual void RequestDraw(const Dia::Maths::Vector2D& from,
                                     const Dia::Maths::Vector2D& to,
                                     Dia::Graphics::RGBA colour) = 0;

            virtual void RequestDrawRect(const Dia::Maths::Vector2D& min,
                                         const Dia::Maths::Vector2D& max,
                                         Dia::Graphics::RGBA colour) = 0;

            virtual void RequestDrawCircle(const Dia::Maths::Vector2D& centre,
                                           float radius,
                                           Dia::Graphics::RGBA colour) = 0;

            virtual bool IsFull() const = 0;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
