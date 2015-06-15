#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		class Line2D;
		class Circle2D;
		class Angle;
		class Vector2D;

		class Arc2D
		{
		public:
			Arc2D();
			Arc2D(float radius, const Angle& angle, const Vector2D& focal, const Vector2D& axis);

			Arc2D(const Arc2D& rhs);

			Arc2D&	operator =	(const Arc2D& rhs);
			
			float				GetRadius() const;
			float				GetSquaredRadius() const;
			const Angle&		GetAngle() const;
			const Vector2D&		GetFocal() const;
			const Vector2D&		GetAxis() const;
			
			bool IsBetweenExtents(const Vector2D& vec)const;
			
			Vector2D CalculateExtentVectorClockwise()const;
			Vector2D CalculateExtentVectorCounterClockwise()const;
			
			void CalculateExtentClockwise(Line2D& extent)const;
			void CalculateExtentCounterClockwise(Line2D& extent)const;
			
			Vector2D CalculateExtentPositionClockwise()const;
			Vector2D CalculateExtentPositionCounterClockwise()const;

			void ClosestPointOnArcTo(const Circle2D& circle, Vector2D& result)const;

			IntersectionClassify IsIntersecting(const Vector2D& point)const;
			//IntersectionClassify IsIntersecting(const AARect2D& rect)const;
			//IntersectionClassify IsIntersecting(const Arc2D& arc)const;
			//IntersectionClassify IsIntersecting(const Capsule2D& capsule)const;
			IntersectionClassify IsIntersecting(const Circle2D& circle)const;
			//IntersectionClassify IsIntersecting(const Line2D& line)const;
			//IntersectionClassify IsIntersecting(const OORect2D& rhs)const;
			//IntersectionClassify IsIntersecting(const Triangle2D& rhs)const;

		private:
			float mRadius;
			Angle mAngle;

			Vector2D mFocal;
			Vector2D mAxis;
		};
	}
}

#include "DiaMaths/Shape/2D/Arc2D.inl"