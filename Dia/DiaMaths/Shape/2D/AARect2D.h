#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class Circle2D;

		class AARect2D
		{
		public:
			AARect2D();

			AARect2D(const AARect2D& rhs);
			AARect2D(const Vector2D& bottonLeft, const Vector2D& topRight);

			AARect2D& operator = (const AARect2D& rhs);
			
			const Vector2D&		GetBottomLeft()		const;
			const Vector2D&		GetTopRight()		const;
			
			float		CalculateRadius()const;
			float		CalculateSquaredRadius()const;
			Vector2D	CalculateCenter()const;

			void ClosestPointOnAARectTo(const Vector2D& point, Vector2D& result)const;
			//void ClosestPointOnCircleTo(const AARect2D& rect, Vector2D& result)const;
			//bool ClosestPointTo(const Arc2D& rhsv)const;
			//bool ClosestPointTo(const Capsule2D& rhs, Vector2D& result)const;
			void ClosestPointOnAARectTo(const Circle2D& circle, Vector2D& result)const;
			//void ClosestPointOnCircleTo(const Line2D& line, Vector2D& result)const;
			//bool ClosestPointTo(const OORect2D& rhs, Vector2D& result)const;
			//bool ClosestPointTo(const Triangle2D& rhs, Vector2D& result)const;

			IntersectionClassify IsIntersecting(const Vector2D& point)const;
			IntersectionClassify IsIntersecting(const Circle2D& circle)const;
		
		private:
			Vector2D mBottonLeft;
			Vector2D mTopRight;
		};
	}
}

#include "DiaMaths/Shape/2D/AARect2D.inl"