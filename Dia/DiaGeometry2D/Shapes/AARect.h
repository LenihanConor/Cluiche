#ifndef DIA_GEOMETRY2D_AARECT_H
#define DIA_GEOMETRY2D_AARECT_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;

		class AARect
		{
		public:
			AARect();
			AARect(const AARect& rhs);
			AARect(const Dia::Maths::Vector2D& bottomLeft, const Dia::Maths::Vector2D& topRight);

			AARect& operator = (const AARect& rhs);

			const Dia::Maths::Vector2D&		GetBottomLeft()		const;
			const Dia::Maths::Vector2D&		GetTopRight()		const;

			float		CalculateRadius()		const;
			float		CalculateSquaredRadius()const;
			Dia::Maths::Vector2D	CalculateCenter()	const;

			void ClosestPointOnAARectTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const;
			void ClosestPointOnAARectTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point) const;
			IntersectionClassify IsIntersecting(const Circle& circle) const;

		private:
			Dia::Maths::Vector2D mBottomLeft;
			Dia::Maths::Vector2D mTopRight;
		};
	}
}

#include "DiaGeometry2D/Shapes/AARect.inl"

#endif // DIA_GEOMETRY2D_AARECT_H
