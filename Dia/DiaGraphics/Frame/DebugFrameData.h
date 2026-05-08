////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameData.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaLogger/DiaLog.h>

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
			// Compile-time budget knob — increase here if primitives are dropped (SD-DBG).
			static constexpr uint32_t kCapacity = 1024u;

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

			// Text2D — world-space text label.
			// position: world-space top-left origin.
			// text:     null-terminated ASCII; strings > 63 chars are silently truncated.
			// fontSize: pixel size (SFML setCharacterSize); 0 or negative is a no-op.
			// colour:   RGBA text colour.
			void RequestDrawText(const Maths::Vector2D& position,
				const char* text,
				float fontSize,
				RGBA colour);

			// ----------------------------------------------------------------
			// Budget tracking (debug-budget)
			// ----------------------------------------------------------------

			/// Number of RequestDraw* calls dropped this frame because the buffer was full.
			uint32_t DroppedCount()   const { return mDroppedCount; }
			/// True if any primitives were dropped this frame.
			bool     IsOverCapacity() const { return mDroppedCount > 0; }

			// ----------------------------------------------------------------
			// Test / inspection accessors
			// ----------------------------------------------------------------

			/// Total number of primitives currently stored.
			uint32_t              GetDebugPrimitiveCount()          const { return mDebugPrimitiveBuffer.Size(); }
			/// Access a stored primitive by index (0-based).
			const DebugPrimitive& GetDebugPrimitive(uint32_t index) const { return mDebugPrimitiveBuffer[index]; }

			void AcceptVisitor(const DebugFrameDataVisitor& visitor) const;

		private:
			/// Returns true if space remains; increments mDroppedCount and returns false when full.
			/// Logs a warning once when the budget is first exceeded, and once again when it recovers.
			bool CanAdd()
			{
				if (mDebugPrimitiveBuffer.Size() >= kCapacity)
				{
					if (!mOverCapacityLogged)
					{
						DIA_LOG_WARNING("graphics", "DebugFrameData: primitive budget exceeded (%u). Draw calls will be dropped.", kCapacity);
						mOverCapacityLogged = true;
					}
					++mDroppedCount;
					return false;
				}
				return true;
			}

			Core::Containers::DynamicArrayC<DebugPrimitive, kCapacity> mDebugPrimitiveBuffer;
			uint32_t mDroppedCount         = 0;
			bool     mOverCapacityLogged   = false;
		};
	}
}
