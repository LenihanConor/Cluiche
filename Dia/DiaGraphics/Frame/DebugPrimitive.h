////////////////////////////////////////////////////////////////////////////////
// Filename: DebugPrimitive.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia
{
	namespace Graphics
	{
		// Arc angles follow SFML convention: clockwise from positive X axis (degrees)

		enum class DebugPrimitiveType : unsigned char
		{
			Circle2D   = 0,
			Line2D     = 1,
			Point2D    = 2,
			Rect2D     = 3,
			Arc2D      = 4,
			Ray2D      = 5,
			Triangle2D = 6
		};

		struct DebugPrimitiveCircle2D
		{
			Maths::Vector2D position;
			float           radius;
			RGBA            outlineColour;
			RGBA            fillColour;   // alpha == 0 means no fill
		};

		struct DebugPrimitiveLine2D
		{
			Maths::Vector2D start;
			Maths::Vector2D end;
			RGBA            colour;
		};

		struct DebugPrimitivePoint2D
		{
			Maths::Vector2D position;
			RGBA            colour;
		};

		struct DebugPrimitiveRect2D
		{
			Maths::Vector2D min;
			Maths::Vector2D max;
			RGBA            outlineColour;
			RGBA            fillColour;   // alpha == 0 means no fill
		};

		struct DebugPrimitiveArc2D
		{
			Maths::Vector2D position;
			float           radius;
			float           startAngleDeg;
			float           endAngleDeg;
			RGBA            colour;
		};

		struct DebugPrimitiveRay2D
		{
			Maths::Vector2D origin;
			Maths::Vector2D direction;  // caller must supply unit vector
			float           length;
			RGBA            colour;
		};

		struct DebugPrimitiveTriangle2D
		{
			Maths::Vector2D p1;
			Maths::Vector2D p2;
			Maths::Vector2D p3;
			RGBA            outlineColour;
			RGBA            fillColour;   // alpha == 0 means no fill
		};

		struct DebugPrimitive
		{
			DebugPrimitiveType type;

			union
			{
				DebugPrimitiveCircle2D   circle2D;
				DebugPrimitiveLine2D     line2D;
				DebugPrimitivePoint2D    point2D;
				DebugPrimitiveRect2D     rect2D;
				DebugPrimitiveArc2D      arc2D;
				DebugPrimitiveRay2D      ray2D;
				DebugPrimitiveTriangle2D triangle2D;
			};

			// Union members with non-trivial types require explicit constructor/destructor
			DebugPrimitive() : type(DebugPrimitiveType::Circle2D), circle2D() {}
			DebugPrimitive(const DebugPrimitive& rhs) { *this = rhs; }
			DebugPrimitive& operator=(const DebugPrimitive& rhs)
			{
				type = rhs.type;
				switch (type)
				{
					case DebugPrimitiveType::Circle2D:   circle2D   = rhs.circle2D;   break;
					case DebugPrimitiveType::Line2D:     line2D     = rhs.line2D;     break;
					case DebugPrimitiveType::Point2D:    point2D    = rhs.point2D;    break;
					case DebugPrimitiveType::Rect2D:     rect2D     = rhs.rect2D;     break;
					case DebugPrimitiveType::Arc2D:      arc2D      = rhs.arc2D;      break;
					case DebugPrimitiveType::Ray2D:      ray2D      = rhs.ray2D;      break;
					case DebugPrimitiveType::Triangle2D: triangle2D = rhs.triangle2D; break;
				}
				return *this;
			}
		};

		static constexpr unsigned int kDebugPrimitiveCapacity = 1024;

	} // namespace Graphics
} // namespace Dia
