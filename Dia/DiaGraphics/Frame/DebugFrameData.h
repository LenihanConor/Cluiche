////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameData.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaMaths/Vector/Vector2D.h>

#include "DiaGraphics/Frame/DebugPrimitive.h"

namespace Dia
{
	namespace Graphics
	{
		// Forward declarations
		class DebugFrameDataVisitor;

		///
		/// DebugFrameData - Stores all debug geometry for a single frame.
		/// Add new shape types by adding an enum value + union member to DebugPrimitive — no changes needed here.
		///
		class DebugFrameData
		{
		public:
			DebugFrameData();
			~DebugFrameData();

			void ClearDebugBuffer();
			void CopyDebugBuffer(const DebugFrameData& rhs);

			// Circle2D — with explicit fill colour
			void RequestDraw(const Maths::Vector2D& position, float radius,
				RGBA outlineColour, RGBA fillColour);
			// Circle2D — outline only (fill defaults to transparent)
			void RequestDraw(const Maths::Vector2D& position, float radius,
				RGBA outlineColour)
			{
				RequestDraw(position, radius, outlineColour, RGBA(0, 0, 0, 0));
			}

			// Line2D
			void RequestDraw(const Maths::Vector2D& start, const Maths::Vector2D& end,
				RGBA colour);

			// Point2D
			void RequestDrawPoint(const Maths::Vector2D& position, RGBA colour);

			// Rect2D — with explicit fill colour
			void RequestDrawRect(const Maths::Vector2D& min, const Maths::Vector2D& max,
				RGBA outlineColour, RGBA fillColour);
			// Rect2D — outline only
			void RequestDrawRect(const Maths::Vector2D& min, const Maths::Vector2D& max,
				RGBA outlineColour)
			{
				RequestDrawRect(min, max, outlineColour, RGBA(0, 0, 0, 0));
			}

			// Arc2D (angles in degrees, clockwise from positive X)
			void RequestDrawArc(const Maths::Vector2D& position, float radius,
				float startAngleDeg, float endAngleDeg, RGBA colour);

			// Ray2D — direction must be a unit vector
			void RequestDrawRay(const Maths::Vector2D& origin, const Maths::Vector2D& direction,
				float length, RGBA colour);

			// Triangle2D — with explicit fill colour
			void RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
				const Maths::Vector2D& p3, RGBA outlineColour, RGBA fillColour);
			// Triangle2D — outline only
			void RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
				const Maths::Vector2D& p3, RGBA outlineColour)
			{
				RequestDraw(p1, p2, p3, outlineColour, RGBA(0, 0, 0, 0));
			}

			void AcceptVisitor(const DebugFrameDataVisitor& visitor) const;

		private:
			Core::Containers::DynamicArrayC<DebugPrimitive, kDebugPrimitiveCapacity> mDebugPrimitiveBuffer;
		};
	}
}
