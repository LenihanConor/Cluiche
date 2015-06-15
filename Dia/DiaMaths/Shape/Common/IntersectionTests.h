#pragma once

#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
		class Circle2D;
		class AARect2D;
		class Arc2D;
		class Capsule2D;
		class Line2D;
		class OORect2D;
		class Triangle2D;

		//==============================================================================
		// CLASS PrimitiveIntersectionTests2D
		//==============================================================================
		class IntersectionTests
		{
		public:

			//Circle Intersection Test
static IntersectionClassify IsIntersecting(const Vector2D& point, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const AARect2D& rect, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const Arc2D& arc, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const Capsule2D& capsule, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const Circle2D& circleA, const Circle2D& circleB);
static IntersectionClassify IsIntersecting(const Line2D& line, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const OORect2D& rect, const Circle2D& circle);
static IntersectionClassify IsIntersecting(const Triangle2D& tri, const Circle2D& circle);

static int CalculateIntercepts(const Circle2D& circleA, const Circle2D& circleB, Vector2D& intercept1, Vector2D& intercept2);
//			static int CalculateIntercepts(const Line2D& line, const Circle2D& circle, Vector2D& intercept1, Vector2D& intercept2);

			// AARect Intersection Test
static IntersectionClassify IsIntersecting(const Vector2D& point, const AARect2D& rect);
		/*	static IntersectionClassify2D IsIntersecting(const AARect2D& rect1, const AARect2D& rec2t);
			static IntersectionClassify2D IsIntersecting(const Arc2D& arc, const AARect2D& rect);
			static IntersectionClassify2D IsIntersecting(const Capsule2D& capsule, const AARect2D& rect);
			static IntersectionClassify2D IsIntersecting(const Circle2D& circle, const AARect2D& rect);
			static IntersectionClassify2D IsIntersecting(const Line2D& line, const AARect2D& rect);
			//	static IntersectionClassify2D IsIntersecting(const OORect2D& point, const AARect2D& rect);
			//	static IntersectionClassify2D IsIntersecting(const Triangle2D& point, const AARect2D& rect);


			// Arc Intersection Test
*/
static IntersectionClassify IsIntersecting(const Vector2D& point, const Arc2D& arc);
			/*static IntersectionClassify2D IsIntersecting(const AARect2D& rect, const Arc2D& arc);
			static IntersectionClassify2D IsIntersecting(const Arc2D& arc, const Arc2D& arc);
			static IntersectionClassify2D IsIntersecting(const Capsule2D& capsule, const LinArc2De2D& arc);
			static IntersectionClassify2D IsIntersecting(const Line2D& line1, const Arc2D& arc);
			//	static IntersectionClassify2D IsIntersecting(const OORect2D& point, const Arc2D& arc);
			//	static IntersectionClassify2D IsIntersecting(const Triangle2D& point, const Arc2D& arc);


			// Capsule Intersection Test
			static IntersectionClassify2D IsIntersecting(const Vector2D& point, const Capsule2D& capsule);
			static IntersectionClassify2D IsIntersecting(const AARect2D& rect, const Capsule2D& capsule);
			static IntersectionClassify2D IsIntersecting(const Arc2D& arc, const Capsule2D& capsule);
			static IntersectionClassify2D IsIntersecting(const Capsule2D& capsule, const Capsule2D& capsule);*/
			/*static IntersectionClassify2D IsIntersecting(const Line2D& line1, const Capsule2D& capsule);
			//	static IntersectionClassify2D IsIntersecting(const OORect2D& point, const Capsule2D& capsule);
			//	static IntersectionClassify2D IsIntersecting(const Triangle2D& point, const Capsule2D& capsule);
			*/
			// Line Intersection Test
static IntersectionClassify IsIntersecting(const Vector2D& point, const Line2D& line);
			//static IntersectionClassify2D IsIntersecting(const AARect2D& rect, const Line2D& line);
			//static IntersectionClassify2D IsIntersecting(const Arc2D& arc, const Line2D& line);
			//static IntersectionClassify2D IsIntersecting(const Capsule2D& capsule, const Line2D& line);
			//static IntersectionClassify2D IsIntersecting(const Line2D& line1, const Line2D& line2);
			//	static IntersectionClassify2D IsIntersecting(const OORect2D& point, const Line2D& line);
			//	static IntersectionClassify2D IsIntersecting(const Triangle2D& point, const Line2D& line);
			
		};
	}
}