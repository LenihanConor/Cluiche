////////////////////////////////////////////////////////////////////////////////
// Filename: IDebugDraw.h
// Description: Neutral debug-draw contract. Implemented by FrameData (DiaGraphics)
//              so domain visual debuggers can call draw primitives without
//              depending on the visual domain.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <stdint.h>

namespace Dia
{
    namespace Core
    {
        ////////////////////////////////////////////////////////////////////////
        // IDebugDraw
        //
        // Pure virtual interface for submitting debug draw primitives.
        // Implementors: Dia::Graphics::DebugFrameData.
        // Consumers: IVisualDebugger implementations across all domains.
        ////////////////////////////////////////////////////////////////////////
        class IDebugDraw
        {
        public:
            virtual ~IDebugDraw() = default;

            // Circle2D — with explicit fill colour
            virtual void RequestDraw(const Maths::Vector2D& position, float radius,
                RGBA outlineColour, RGBA fillColour) = 0;
            // Circle2D — outline only
            virtual void RequestDraw(const Maths::Vector2D& position, float radius,
                RGBA outlineColour)
            {
                RequestDraw(position, radius, outlineColour, RGBA(0, 0, 0, 0));
            }

            // Line2D
            virtual void RequestDraw(const Maths::Vector2D& start, const Maths::Vector2D& end,
                RGBA colour) = 0;

            // Point2D
            virtual void RequestDrawPoint(const Maths::Vector2D& position, RGBA colour) = 0;

            // Rect2D — with explicit fill colour
            virtual void RequestDrawRect(const Maths::Vector2D& min, const Maths::Vector2D& max,
                RGBA outlineColour, RGBA fillColour) = 0;
            // Rect2D — outline only
            virtual void RequestDrawRect(const Maths::Vector2D& min, const Maths::Vector2D& max,
                RGBA outlineColour)
            {
                RequestDrawRect(min, max, outlineColour, RGBA(0, 0, 0, 0));
            }

            // Arc2D (angles in degrees, clockwise from positive X)
            virtual void RequestDrawArc(const Maths::Vector2D& position, float radius,
                float startAngleDeg, float endAngleDeg, RGBA colour) = 0;

            // Ray2D — direction must be a unit vector
            virtual void RequestDrawRay(const Maths::Vector2D& origin, const Maths::Vector2D& direction,
                float length, RGBA colour) = 0;

            // Triangle2D — with explicit fill colour
            virtual void RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
                const Maths::Vector2D& p3, RGBA outlineColour, RGBA fillColour) = 0;
            // Triangle2D — outline only
            virtual void RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
                const Maths::Vector2D& p3, RGBA outlineColour)
            {
                RequestDraw(p1, p2, p3, outlineColour, RGBA(0, 0, 0, 0));
            }

            // Text2D — world-space text label.
            virtual void RequestDrawText(const Maths::Vector2D& position,
                const char* text,
                float fontSize,
                RGBA colour) = 0;

            // Line3D
            virtual void RequestDrawLine3D(const Maths::Vector3D& from, const Maths::Vector3D& to, RGBA colour) = 0;

            // Ray3D — direction must be a unit vector
            virtual void RequestDrawRay3D(const Maths::Vector3D& origin, const Maths::Vector3D& direction,
                float length, RGBA colour) = 0;

            // Box3D — world-space AABB
            virtual void RequestDrawBox3D(const Maths::Vector3D& min, const Maths::Vector3D& max, RGBA colour) = 0;

            // Sphere3D
            virtual void RequestDrawSphere3D(const Maths::Vector3D& center, float radius, RGBA colour) = 0;

            // Arrow3D — direction must be a unit vector
            virtual void RequestDrawArrow3D(const Maths::Vector3D& origin, const Maths::Vector3D& direction,
                float length, float headSize, RGBA colour) = 0;

            // Budget monitoring — number of geometry draw calls dropped this frame due to buffer overflow.
            virtual uint32_t DroppedCount() const = 0;

            // Mouse cursor position in screen pixels for the current frame (world-space origin if not set).
            virtual const Maths::Vector2D& GetMousePixel() const = 0;
        };

    } // namespace Core
} // namespace Dia
