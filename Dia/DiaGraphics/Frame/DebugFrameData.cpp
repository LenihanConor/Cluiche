////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameData.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/DebugFrameData.h"

#include "DiaGraphics/Frame/DebugFrameDataVisitor.h"

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
			mDebugPrimitiveBuffer.RemoveAll();
			mDroppedCount = 0;
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::CopyDebugBuffer(const DebugFrameData& rhs)
		{
			mDebugPrimitiveBuffer = rhs.mDebugPrimitiveBuffer;
			mDroppedCount         = rhs.mDroppedCount;
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
			if (!CanAdd()) return;

			DebugPrimitive p;
			p.type               = DebugPrimitiveType::Text2D;
			p.text2D.position    = position;
			p.text2D.fontSize    = fontSize;
			p.text2D.colour      = colour;

			// Safe truncating copy — null terminator always at [63]
			unsigned int i = 0;
			if (text != nullptr)
			{
				for (; i < 63 && text[i] != '\0'; ++i)
					p.text2D.text[i] = text[i];
			}
			p.text2D.text[i] = '\0';

			mDebugPrimitiveBuffer.Add(p);
		}

		//------------------------------------------------------------------------------
		void DebugFrameData::AcceptVisitor(const DebugFrameDataVisitor& visitor) const
		{
			for (unsigned int i = 0; i < mDebugPrimitiveBuffer.Size(); i++)
			{
				visitor.Visit(mDebugPrimitiveBuffer[i]);
			}
		}
	}
}
