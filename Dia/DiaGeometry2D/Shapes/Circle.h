#ifndef DIA_GEOMETRY2D_CIRCLE_H
#define DIA_GEOMETRY2D_CIRCLE_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class AARect;
		class Arc;
		class Capsule;
		class Line;
		class OORect;
		class Triangle;

		class Circle
		{
		public:
			Circle();
			Circle(float radius, const Dia::Maths::Vector2D& center);
			Circle(const Circle& rhs);

			static Circle& UnitCircle();

			Circle&		operator = (const Circle& rhs);
			bool		operator == (const Circle& rhs);
			bool		operator != (const Circle& rhs);

			float						GetRadius()const;
			float						GetSquaredRadius()const;
			const Dia::Maths::Vector2D&	GetCenter()const;

			static Circle CreateFrom(float radius, const Dia::Maths::Vector2D& position);
			static Circle CreateFrom(const AARect& rect);
			static Circle CreateFrom(const Arc& arc);
			static Circle CreateFrom(const Capsule& capsule);
			static Circle CreateFrom(const Line& line);
			static Circle CreateFrom(const OORect& rect);
			static Circle CreateFrom(const Triangle& tri);

			// Center and radius change
			void ReContructToInclude(const Dia::Maths::Vector2D& point);
			void ReContructToInclude(const AARect& rect);
			void ReContructToInclude(const Arc& arc);
			void ReContructToInclude(const Capsule& capsule);
			void ReContructToInclude(const Circle& circle);
			void ReContructToInclude(const Line& line);
			void ReContructToInclude(const OORect& rect);
			void ReContructToInclude(const Triangle& tri);

			// Only radius will change
			void ExpandToInclude(const Dia::Maths::Vector2D& point);
			void ExpandToInclude(const AARect& rect);
			void ExpandToInclude(const Arc& arc);
			void ExpandToInclude(const Capsule& capsule);
			void ExpandToInclude(const Circle& circle);
			void ExpandToInclude(const Line& line);
			void ExpandToInclude(const OORect& rect);
			void ExpandToInclude(const Triangle& tri);

			void SetCenter(const Dia::Maths::Vector2D& position);
			void SetRadius(float radius);

			void Translate(const Dia::Maths::Vector2D& translation);
			void Scale(float scale);

			void CalculateAxisPointToPoint(const Dia::Maths::Vector2D& pointOutsideCircle, Dia::Maths::Vector2D& closestPt, Dia::Maths::Vector2D& furthestPt) const;
			void CalculateTangents(const Dia::Maths::Vector2D& pointOutsideCircle, Dia::Maths::Vector2D& tangent1, Dia::Maths::Vector2D& tangent2) const;

			void ClosestPointOnCircleTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const AARect& rect, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const Arc& arc, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const Capsule& capsule, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const Circle& circle, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const Line& line, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const OORect& rect, Dia::Maths::Vector2D& result)const;
			void ClosestPointOnCircleTo(const Triangle& tri, Dia::Maths::Vector2D& result)const;

			IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point)const;
			IntersectionClassify IsIntersecting(const AARect& rect)const;
			IntersectionClassify IsIntersecting(const Arc& arc)const;
			IntersectionClassify IsIntersecting(const Capsule& capsule)const;
			IntersectionClassify IsIntersecting(const Circle& circle)const;
			IntersectionClassify IsIntersecting(const Line& line)const;
			IntersectionClassify IsIntersecting(const OORect& rhs)const;
			IntersectionClassify IsIntersecting(const Triangle& rhs)const;

			int CalculateIntercepts(const Circle& circle, Dia::Maths::Vector2D& intercept1, Dia::Maths::Vector2D& intercept2)const;

		private:
			Dia::Maths::Vector2D mCenter;
			float mRadius;
		};
	}
}

#include "DiaGeometry2D/Shapes/Circle.inl"

#endif // DIA_GEOMETRY2D_CIRCLE_H
