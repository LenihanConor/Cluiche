#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class AARect2D;
		class Arc2D;
		class Capsule2D;
		class Line2D;
		class OORect2D;
		class Triangle2D;

		class Circle2D
		{
		public:
			Circle2D();
			Circle2D(float radius, const Vector2D& center);

			Circle2D(const Circle2D& rhs);

			static Circle2D& UnitCircle();

			Circle2D&	operator = (const Circle2D& rhs);
			bool		operator == (const Circle2D& rhs);
			bool		operator != (const Circle2D& rhs);
				
			float				GetRadius()const;
			float				GetSquaredRadius()const;
			const Vector2D&		GetCenter()const;
			
			static Circle2D CreateFrom (float radius, const Vector2D& position);
			static Circle2D CreateFrom (const AARect2D& rect);
			static Circle2D CreateFrom (const Arc2D& arc);
			static Circle2D CreateFrom (const Capsule2D& capsule);
			static Circle2D CreateFrom (const Line2D& line);
			static Circle2D CreateFrom (const OORect2D& rect);
			static Circle2D CreateFrom (const Triangle2D& tri);

			// Center and radius change
			void ReContructToInclude (const Vector2D& point);
			void ReContructToInclude (const AARect2D& rect);
			void ReContructToInclude (const Arc2D& arc);
			void ReContructToInclude (const Capsule2D& capsule);
			void ReContructToInclude (const Circle2D& circle);
			void ReContructToInclude (const Line2D& line);
			void ReContructToInclude (const OORect2D& rect);
			void ReContructToInclude (const Triangle2D& tri);

			// Only radius will change
			void ExpandToInclude (const Vector2D& point);
			void ExpandToInclude (const AARect2D& rect);
			void ExpandToInclude (const Arc2D& arc);
			void ExpandToInclude (const Capsule2D& capsule);
			void ExpandToInclude (const Circle2D& circle);
			void ExpandToInclude (const Line2D& line);
			void ExpandToInclude (const OORect2D& rect);
			void ExpandToInclude (const Triangle2D& tri);

			void SetCenter(const Vector2D& position);
			void SetRadius(float radius);

			void Translate(const Vector2D& translation);
			void Scale(float scale);
			
			void CalculateAxisPointToPoint( const Vector2D& pointOutsideCircle, Vector2D& closestPt, Vector2D& furthestPt ) const;
			void CalculateTangents( const Vector2D& pointOutsideCircle, Vector2D& tangent1, Vector2D& tangent2 ) const;

			void ClosestPointOnCircleTo(const Vector2D& point, Vector2D& result)const;
			void ClosestPointOnCircleTo(const AARect2D& rect, Vector2D& result)const;
			void ClosestPointOnCircleTo(const Arc2D& arc, Vector2D& result)const;
			void ClosestPointOnCircleTo(const Capsule2D& capsule, Vector2D& result)const;
			void ClosestPointOnCircleTo(const Circle2D& circle, Vector2D& result)const;
			void ClosestPointOnCircleTo(const Line2D& line, Vector2D& result)const;
			void ClosestPointOnCircleTo(const OORect2D& rect, Vector2D& result)const;
			void ClosestPointOnCircleTo(const Triangle2D& tri, Vector2D& result)const;

			IntersectionClassify IsIntersecting(const Vector2D& point)const;
			IntersectionClassify IsIntersecting(const AARect2D& rect)const;
			IntersectionClassify IsIntersecting(const Arc2D& arc)const;
			IntersectionClassify IsIntersecting(const Capsule2D& capsule)const;
			IntersectionClassify IsIntersecting(const Circle2D& circle)const;
			IntersectionClassify IsIntersecting(const Line2D& line)const;
			IntersectionClassify IsIntersecting(const OORect2D& rhs)const;
			IntersectionClassify IsIntersecting(const Triangle2D& rhs)const;
			
			//int CalculateIntercepts(const AARect2D& rect, Vector2D& intercept1, Vector2D& intercept1)const;
			//int CalculateIntercepts(const Arc2D& arc, Vector2D& intercept1, Vector2D& intercept1)const;
			//int CalculateIntercepts(const Capsule2D& capsule, Vector2D& intercept1, Vector2D& intercept1)const;
			int CalculateIntercepts(const Circle2D& circle, Vector2D& intercept1, Vector2D& intercept2)const;
//			int CalculateIntercepts(const Line2D& rhs, Vector2D& intercept1, Vector2D& intercept2)const;
			//int CalculateIntercepts(const OORect2D& rhs, Vector2D& intercept1, Vector2D& intercept1)const;
			//int CalculateIntercepts(const Triangle2D& rhs, Vector2D& intercept1, Vector2D& intercept1)const;

		private:
			Vector2D mCenter;
			float mRadius;		
		};
	}
}

#include "DiaMaths/Shape/2D/Circle2D.inl"