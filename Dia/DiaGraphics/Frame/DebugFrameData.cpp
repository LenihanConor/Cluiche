////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameData.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/DebugFrameData.h"

#include "DiaGraphics/Frame/DebugFrameDataVisitor.h"
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		DebugFrameData::DebugFrameData()
			: mDroppedCount(0)
		{}

		//------------------------------------------------------------------------------
		DebugFrameData::~DebugFrameData()
		{}

		//------------------------------------------------------------------------------
		void DebugFrameData::ClearDebugBuffer()
		{
			if (mOverCapacityLogged)
			{
				DIA_LOG_WARNING("graphics", "DebugFrameData: geometry budget recovered — no drops this frame.");
				mOverCapacityLogged = false;
			}
			if (mTextOverCapacityLogged)
			{
				DIA_LOG_WARNING("graphics", "DebugFrameData: text budget recovered — no drops this frame.");
				mTextOverCapacityLogged = false;
			}
			if (m3DOverCapacityLogged)
			{
				DIA_LOG_WARNING("graphics", "DebugFrameData: 3D geometry budget recovered — no drops this frame.");
				m3DOverCapacityLogged = false;
			}
			mDebugPrimitiveBuffer.RemoveAll();
			mTextBuffer.RemoveAll();
			mDebug3DPrimitiveBuffer.RemoveAll();
			mDroppedCount     = 0;
			mTextDroppedCount = 0;
			mDropped3DCount   = 0;
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::CopyDebugBuffer(const DebugFrameData& rhs)
		{
			mDebugPrimitiveBuffer = rhs.mDebugPrimitiveBuffer;
			mTextBuffer           = rhs.mTextBuffer;
			mDroppedCount         = rhs.mDroppedCount;
			mTextDroppedCount     = rhs.mTextDroppedCount;
			mDebug3DPrimitiveBuffer = rhs.mDebug3DPrimitiveBuffer;
			mDropped3DCount         = rhs.mDropped3DCount;
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDraw(const Maths::Vector2D& position, float radius,
			RGBA outlineColour, RGBA fillColour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type                    = DebugPrimitiveType::Circle2D;
			p.circle2D.position       = position;
			p.circle2D.radius         = radius;
			p.circle2D.outlineColour  = outlineColour;
			p.circle2D.fillColour     = fillColour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDraw(const Maths::Vector2D& start, const Maths::Vector2D& end,
			RGBA colour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type          = DebugPrimitiveType::Line2D;
			p.line2D.start  = start;
			p.line2D.end    = end;
			p.line2D.colour = colour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawPoint(const Maths::Vector2D& position, RGBA colour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type               = DebugPrimitiveType::Point2D;
			p.point2D.position   = position;
			p.point2D.colour     = colour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawRect(const Maths::Vector2D& min, const Maths::Vector2D& max,
			RGBA outlineColour, RGBA fillColour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type                   = DebugPrimitiveType::Rect2D;
			p.rect2D.min             = min;
			p.rect2D.max             = max;
			p.rect2D.outlineColour   = outlineColour;
			p.rect2D.fillColour      = fillColour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawArc(const Maths::Vector2D& position, float radius,
			float startAngleDeg, float endAngleDeg, RGBA colour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type                  = DebugPrimitiveType::Arc2D;
			p.arc2D.position        = position;
			p.arc2D.radius          = radius;
			p.arc2D.startAngleDeg   = startAngleDeg;
			p.arc2D.endAngleDeg     = endAngleDeg;
			p.arc2D.colour          = colour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawRay(const Maths::Vector2D& origin,
			const Maths::Vector2D& direction, float length, RGBA colour)
		{
			DIA_ASSERT(direction.SquareMagnitude() > 0.0f, "Ray2D direction must be a non-zero unit vector");
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type              = DebugPrimitiveType::Ray2D;
			p.ray2D.origin      = origin;
			p.ray2D.direction   = direction;
			p.ray2D.length      = length;
			p.ray2D.colour      = colour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
			const Maths::Vector2D& p3, RGBA outlineColour, RGBA fillColour)
		{
			if (!CanAdd()) return;
			DebugPrimitive p;
			p.type                       = DebugPrimitiveType::Triangle2D;
			p.triangle2D.p1              = p1;
			p.triangle2D.p2              = p2;
			p.triangle2D.p3              = p3;
			p.triangle2D.outlineColour   = outlineColour;
			p.triangle2D.fillColour      = fillColour;
			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawText(const Maths::Vector2D& position,
			const char* text,
			float fontSize,
			RGBA colour)
		{
			if (fontSize <= 0.0f) return;
			if (!CanAddText()) return;

			DebugPrimitiveText2D t;
			t.position = position;
			t.fontSize = fontSize;
			t.colour   = colour;

			unsigned int i = 0;
			if (text != nullptr)
			{
				for (; i < 63 && text[i] != '\0'; ++i)
					t.text[i] = text[i];
			}
			t.text[i] = '\0';

			mTextBuffer.Add(t);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawLine3D(const Maths::Vector3D& from, const Maths::Vector3D& to,
			RGBA colour)
		{
			if (!CanAdd3D()) return;
			DebugPrimitive p;
			p.type          = DebugPrimitiveType::Line3D;
			p.line3D.from   = from;
			p.line3D.to     = to;
			p.line3D.colour = colour;
			mDebug3DPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawRay3D(const Maths::Vector3D& origin,
			const Maths::Vector3D& direction, float length, RGBA colour)
		{
			DIA_ASSERT(direction.SquareMagnitude() > 0.0f, "Ray3D direction must be a non-zero unit vector");
			if (!CanAdd3D()) return;
			DebugPrimitive p;
			p.type              = DebugPrimitiveType::Ray3D;
			p.ray3D.origin      = origin;
			p.ray3D.direction   = direction;
			p.ray3D.length      = length;
			p.ray3D.colour      = colour;
			mDebug3DPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawBox3D(const Maths::Vector3D& min, const Maths::Vector3D& max,
			RGBA colour)
		{
			if (!CanAdd3D()) return;
			DebugPrimitive p;
			p.type          = DebugPrimitiveType::Box3D;
			p.box3D.min     = min;
			p.box3D.max     = max;
			p.box3D.colour  = colour;
			mDebug3DPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawSphere3D(const Maths::Vector3D& center, float radius,
			RGBA colour)
		{
			if (!CanAdd3D()) return;
			DebugPrimitive p;
			p.type               = DebugPrimitiveType::Sphere3D;
			p.sphere3D.center    = center;
			p.sphere3D.radius    = radius;
			p.sphere3D.colour    = colour;
			mDebug3DPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::RequestDrawArrow3D(const Maths::Vector3D& origin,
			const Maths::Vector3D& direction, float length, float headSize, RGBA colour)
		{
			DIA_ASSERT(direction.SquareMagnitude() > 0.0f, "Arrow3D direction must be a non-zero unit vector");
			if (!CanAdd3D()) return;
			DebugPrimitive p;
			p.type               = DebugPrimitiveType::Arrow3D;
			p.arrow3D.origin     = origin;
			p.arrow3D.direction  = direction;
			p.arrow3D.length     = length;
			p.arrow3D.headSize   = headSize;
			p.arrow3D.colour     = colour;
			mDebug3DPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::AcceptVisitor(const DebugFrameDataVisitor& visitor) const
		{
			for (unsigned int i = 0; i < mDebugPrimitiveBuffer.Size(); i++)
				visitor.Visit(mDebugPrimitiveBuffer[i]);

			for (unsigned int i = 0; i < mTextBuffer.Size(); i++)
				visitor.VisitText(mTextBuffer[i]);
		}
	}
}
