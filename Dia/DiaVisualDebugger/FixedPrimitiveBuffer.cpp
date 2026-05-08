////////////////////////////////////////////////////////////////////////////////
// Filename: FixedPrimitiveBuffer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaVisualDebugger/FixedPrimitiveBuffer.h"

#ifdef DIA_DEBUG

#include <DiaLogger/DiaLog.h>

namespace Dia
{
    namespace Debug
    {
        FixedPrimitiveBuffer::FixedPrimitiveBuffer(unsigned int capacity)
            : mBuffer(new Dia::Graphics::DebugPrimitive[capacity])
            , mCount(0)
            , mCapacity(capacity)
        {
            if (capacity > kLargeBufferThreshold)
            {
                DIA_LOG_WARNING("visualdebugger",
                    "FixedPrimitiveBuffer: capacity %u exceeds threshold %u — consider reducing.",
                    capacity, kLargeBufferThreshold);
            }
        }

        FixedPrimitiveBuffer::~FixedPrimitiveBuffer()
        {
            delete[] mBuffer;
        }

        void FixedPrimitiveBuffer::RequestDraw(
            const Dia::Maths::Vector2D& from,
            const Dia::Maths::Vector2D& to,
            Dia::Graphics::RGBA colour)
        {
            if (IsFull()) return;
            Dia::Graphics::DebugPrimitive& p = mBuffer[mCount++];
            p.type          = Dia::Graphics::DebugPrimitiveType::Line2D;
            p.entityId      = 0;
            p.line2D.start  = from;
            p.line2D.end    = to;
            p.line2D.colour = colour;
        }

        void FixedPrimitiveBuffer::RequestDrawRect(
            const Dia::Maths::Vector2D& min,
            const Dia::Maths::Vector2D& max,
            Dia::Graphics::RGBA colour)
        {
            if (IsFull()) return;
            Dia::Graphics::DebugPrimitive& p = mBuffer[mCount++];
            p.type                 = Dia::Graphics::DebugPrimitiveType::Rect2D;
            p.entityId             = 0;
            p.rect2D.min           = min;
            p.rect2D.max           = max;
            p.rect2D.outlineColour = colour;
            p.rect2D.fillColour    = Dia::Graphics::RGBA(0, 0, 0, 0);
        }

        void FixedPrimitiveBuffer::RequestDrawCircle(
            const Dia::Maths::Vector2D& centre,
            float radius,
            Dia::Graphics::RGBA colour)
        {
            if (IsFull()) return;
            Dia::Graphics::DebugPrimitive& p = mBuffer[mCount++];
            p.type                   = Dia::Graphics::DebugPrimitiveType::Circle2D;
            p.entityId               = 0;
            p.circle2D.position      = centre;
            p.circle2D.radius        = radius;
            p.circle2D.outlineColour = colour;
            p.circle2D.fillColour    = Dia::Graphics::RGBA(0, 0, 0, 0);
        }

        bool FixedPrimitiveBuffer::IsFull() const
        {
            return mCount >= mCapacity;
        }

        void FixedPrimitiveBuffer::Clear()
        {
            mCount = 0;
        }

        void FixedPrimitiveBuffer::AcceptVisitor(
            const Dia::Graphics::DebugFrameDataVisitor& visitor) const
        {
            for (unsigned int i = 0; i < mCount; ++i)
                visitor.Visit(mBuffer[i]);
        }

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
