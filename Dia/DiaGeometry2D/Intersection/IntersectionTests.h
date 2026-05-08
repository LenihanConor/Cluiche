#pragma once

#ifndef DIA_GEOMETRY2D_INTERSECTIONTESTS_H
#define DIA_GEOMETRY2D_INTERSECTIONTESTS_H

#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
	}

	namespace Geometry2D
	{
		class Circle;
		class AARect;
		class Arc;
		class Capsule;
		class Line;
		class OORect;
		class Triangle;

		//==============================================================================
		// CLASS IntersectionTests
		//==============================================================================
		// Static utility class providing intersection tests between 2D geometry types.
		// All methods return an IntersectionClassify describing the relationship
		// between the two shapes (no intersection, penetrating, A contains B, etc.)
		//==============================================================================
		class IntersectionTests
		{
		public:

			// Circle intersection tests
			static IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point, const Circle& circle);
			static IntersectionClassify IsIntersecting(const AARect& rect, const Circle& circle);
			static IntersectionClassify IsIntersecting(const Arc& arc, const Circle& circle);
			static IntersectionClassify IsIntersecting(const Capsule& capsule, const Circle& circle);
			static IntersectionClassify IsIntersecting(const Circle& circleA, const Circle& circleB);
			static IntersectionClassify IsIntersecting(const Line& line, const Circle& circle);
			static IntersectionClassify IsIntersecting(const OORect& rect, const Circle& circle);
			static IntersectionClassify IsIntersecting(const Triangle& tri, const Circle& circle);

			static int CalculateIntercepts(const Circle& circleA, const Circle& circleB, Dia::Maths::Vector2D& intercept1, Dia::Maths::Vector2D& intercept2);

			// AARect intersection tests
			static IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point, const AARect& rect);
			static IntersectionClassify IsIntersecting(const AARect& rect1, const AARect& rect2);

			// Arc intersection test
			static IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point, const Arc& arc);

			// Line intersection tests
			static IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point, const Line& line);
			static IntersectionClassify IsIntersecting(const Line& line1, const Line& line2);
			static IntersectionClassify IsIntersecting(const Line& line, const AARect& rect);
		};
	}
}

#endif // DIA_GEOMETRY2D_INTERSECTIONTESTS_H
