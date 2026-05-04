////////////////////////////////////////////////////////////////////////////////
// Filename: FixedPrimitiveBuffer.h
// Description: Concrete IFixedPrimitiveBuffer — heap-allocated DebugPrimitive[].
//              Built once at RegisterFixed(); drawn each frame via AcceptVisitor().
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
    namespace Debug
    {
        class FixedPrimitiveBuffer : public IFixedPrimitiveBuffer
        {
        public:
            static constexpr unsigned int kLargeBufferThreshold = 65536u;

            explicit FixedPrimitiveBuffer(unsigned int capacity);
            ~FixedPrimitiveBuffer();

            // IFixedPrimitiveBuffer
            void RequestDraw(const Dia::Maths::Vector2D& from,
                             const Dia::Maths::Vector2D& to,
                             Dia::Graphics::RGBA colour) override;

            void RequestDrawRect(const Dia::Maths::Vector2D& min,
                                 const Dia::Maths::Vector2D& max,
                                 Dia::Graphics::RGBA colour) override;

            void RequestDrawCircle(const Dia::Maths::Vector2D& centre,
                                   float radius,
                                   Dia::Graphics::RGBA colour) override;

            bool IsFull() const override;

            // Buffer management
            void Clear();

            // Replay stored primitives through a visitor
            void AcceptVisitor(const Dia::Graphics::DebugFrameDataVisitor& visitor) const;

            unsigned int GetCount()    const { return mCount; }
            unsigned int GetCapacity() const { return mCapacity; }

        private:
            Dia::Graphics::DebugPrimitive* mBuffer;
            unsigned int                   mCount;
            unsigned int                   mCapacity;

            FixedPrimitiveBuffer(const FixedPrimitiveBuffer&) = delete;
            FixedPrimitiveBuffer& operator=(const FixedPrimitiveBuffer&) = delete;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
