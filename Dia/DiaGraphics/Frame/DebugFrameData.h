////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameData.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaObservation/Log/DiaLog.h>

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
			// Compile-time budget knobs — increase if primitives are dropped (SD-DBG).
			static constexpr uint32_t kGeometryCapacity = 2048u;
			static constexpr uint32_t kTextCapacity     = 256u;

			// Legacy alias so call-sites that reference kCapacity still compile.
			static constexpr uint32_t kCapacity = kGeometryCapacity;
			static constexpr uint32_t kDebug3DCapacity = 2048u;

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
			// 3D debug primitives
			// ----------------------------------------------------------------

			// Line3D — world-space line segment
			void RequestDrawLine3D(const Maths::Vector3D& from, const Maths::Vector3D& to, RGBA colour);

			// Ray3D — world-space ray; direction must be a unit vector
			void RequestDrawRay3D(const Maths::Vector3D& origin, const Maths::Vector3D& direction,
				float length, RGBA colour);

			// Box3D — world-space AABB; renderer expands to 12 wireframe edges
			void RequestDrawBox3D(const Maths::Vector3D& min, const Maths::Vector3D& max, RGBA colour);

			// Sphere3D — world-space sphere; renderer emits 3 great circles × 24 segments
			void RequestDrawSphere3D(const Maths::Vector3D& center, float radius, RGBA colour);

			// Arrow3D — world-space arrow; direction must be a unit vector
			void RequestDrawArrow3D(const Maths::Vector3D& origin, const Maths::Vector3D& direction,
				float length, float headSize, RGBA colour);

			// ----------------------------------------------------------------
			// Budget tracking (debug-budget)
			// ----------------------------------------------------------------

			/// Number of geometry RequestDraw* calls dropped this frame because the buffer was full.
			uint32_t DroppedCount()       const { return mDroppedCount; }
			/// True if any geometry primitives were dropped this frame.
			bool     IsOverCapacity()     const { return mDroppedCount > 0; }

			/// Number of RequestDrawText calls dropped this frame because the text buffer was full.
			uint32_t DroppedTextCount()   const { return mTextDroppedCount; }
			/// True if any text primitives were dropped this frame.
			bool     IsTextOverCapacity() const { return mTextDroppedCount > 0; }

			/// Number of 3D geometry RequestDraw*3D calls dropped this frame because the 3D buffer was full.
			uint32_t DroppedDebug3DCount()      const { return mDropped3DCount; }
			/// True if any 3D primitives were dropped this frame.
			bool     Is3DOverCapacity()         const { return mDropped3DCount > 0; }

			// ----------------------------------------------------------------
			// Test / inspection accessors
			// ----------------------------------------------------------------

			/// Total number of geometry primitives currently stored.
			uint32_t              GetDebugPrimitiveCount()          const { return mDebugPrimitiveBuffer.Size(); }
			/// Access a stored geometry primitive by index (0-based).
			const DebugPrimitive& GetDebugPrimitive(uint32_t index) const { return mDebugPrimitiveBuffer[index]; }

			/// Total number of text primitives currently stored.
			uint32_t                    GetTextPrimitiveCount()          const { return mTextBuffer.Size(); }
			/// Access a stored text primitive by index (0-based).
			const DebugPrimitiveText2D& GetTextPrimitive(uint32_t index) const { return mTextBuffer[index]; }

			/// Total number of 3D geometry primitives currently stored.
			uint32_t              GetDebug3DPrimitiveCount()          const { return mDebug3DPrimitiveBuffer.Size(); }
			/// Access a stored 3D geometry primitive by index (0-based).
			const DebugPrimitive& GetDebug3DPrimitive(uint32_t index) const { return mDebug3DPrimitiveBuffer[index]; }

			void AcceptVisitor(const DebugFrameDataVisitor& visitor) const;

		private:
			bool CanAdd()
			{
				if (mDebugPrimitiveBuffer.Size() >= kGeometryCapacity)
				{
					if (!mOverCapacityLogged)
					{
						DIA_LOG_WARNING("graphics", "DebugFrameData: geometry budget exceeded (%u). Draw calls will be dropped.", kGeometryCapacity);
						mOverCapacityLogged = true;
					}
					++mDroppedCount;
					return false;
				}
				return true;
			}

			bool CanAddText()
			{
				if (mTextBuffer.Size() >= kTextCapacity)
				{
					if (!mTextOverCapacityLogged)
					{
						DIA_LOG_WARNING("graphics", "DebugFrameData: text budget exceeded (%u). Text draw calls will be dropped.", kTextCapacity);
						mTextOverCapacityLogged = true;
					}
					++mTextDroppedCount;
					return false;
				}
				return true;
			}

			bool CanAdd3D()
			{
				if (mDebug3DPrimitiveBuffer.Size() >= kDebug3DCapacity)
				{
					if (!m3DOverCapacityLogged)
					{
						DIA_LOG_WARNING("graphics", "DebugFrameData: 3D geometry budget exceeded (%u). Draw calls will be dropped.", kDebug3DCapacity);
						m3DOverCapacityLogged = true;
					}
					++mDropped3DCount;
					return false;
				}
				return true;
			}

			Core::Containers::DynamicArrayC<DebugPrimitive,     kGeometryCapacity> mDebugPrimitiveBuffer;
			Core::Containers::DynamicArrayC<DebugPrimitiveText2D, kTextCapacity>   mTextBuffer;
			uint32_t mDroppedCount             = 0;
			uint32_t mTextDroppedCount         = 0;
			bool     mOverCapacityLogged       = false;
			bool     mTextOverCapacityLogged   = false;
			Core::Containers::DynamicArrayC<DebugPrimitive, kDebug3DCapacity> mDebug3DPrimitiveBuffer;
			uint32_t mDropped3DCount         = 0;
			bool     m3DOverCapacityLogged   = false;
		};
	}
}
