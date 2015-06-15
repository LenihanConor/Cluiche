#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class Circle2D;
		class Line2D;

		class Capsule2D
		{
		public:
			Capsule2D();
			Capsule2D(float radius, const Vector2D& axisPt1, const Vector2D& axisPt2);
			Capsule2D(const Capsule2D& rhs);

			Capsule2D&		operator =			(const Capsule2D& rhs);

			void			Axis(Line2D& axis)const;
			
			float			GetRadius()	const;
			float			GetSquaredRadius()const;
			float			GetLength()const;
			float			GetSquaredLength()const;
			float			GetAxisLength()const;
			float			GetAxisSquaredLength()const;
			
			const Vector2D& GetPoint1()	const;
			const Vector2D& GetPoint2()	const;
			Vector2D		GetCenter()const;
			
			void			GetPoint1Circle(Circle2D& circle)const;
			void			GetPoint2Circle(Circle2D& circle)const;

			void ClosestPointOnCapsuleTo(const Vector2D& point, Vector2D& result)const;
			//void ClosestPointOnCircleTo(const AARect2D& rect, Vector2D& result)const;
			//bool ClosestPointTo(const Arc2D& rhsv)const;
			//bool ClosestPointTo(const Capsule2D& rhs, Vector2D& result)const;
			void ClosestPointOnCapsuleTo(const Circle2D& circle, Vector2D& result)const;
			//void ClosestPointOnCircleTo(const Line2D& line, Vector2D& result)const;
			//bool ClosestPointTo(const OORect2D& rhs, Vector2D& result)const;
			//bool ClosestPointTo(const Triangle2D& rhs, Vector2D& result)const;
			
			IntersectionClassify IsIntersecting(const Circle2D& circle)const;
		private:
			Vector2D mPt1;
			Vector2D mPt2;

			float mRadius;
		};
	}
}

#include "DiaMaths/Shape/2D/Capsule2D.inl"