#ifndef DIA_GEOMETRY2D_ARC_H
#define DIA_GEOMETRY2D_ARC_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Line;
		class Circle;

		class Arc
		{
		public:
			Arc();
			Arc(float radius, const Dia::Maths::Angle& angle, const Dia::Maths::Vector2D& focal, const Dia::Maths::Vector2D& axis);
			Arc(const Arc& rhs);

			Arc& operator = (const Arc& rhs);

			float						GetRadius()			const;
			float						GetSquaredRadius()	const;
			const Dia::Maths::Angle&	GetAngle()			const;
			const Dia::Maths::Vector2D&	GetFocal()			const;
			const Dia::Maths::Vector2D&	GetAxis()			const;

			bool IsBetweenExtents(const Dia::Maths::Vector2D& vec) const;

			Dia::Maths::Vector2D CalculateExtentVectorClockwise()		const;
			Dia::Maths::Vector2D CalculateExtentVectorCounterClockwise()const;

			void CalculateExtentClockwise(Line& extent)			const;
			void CalculateExtentCounterClockwise(Line& extent)	const;

			Dia::Maths::Vector2D CalculateExtentPositionClockwise()			const;
			Dia::Maths::Vector2D CalculateExtentPositionCounterClockwise()	const;

			void ClosestPointOnArcTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point) const;
			IntersectionClassify IsIntersecting(const Circle& circle) const;

		private:
			float				mRadius;
			Dia::Maths::Angle	mAngle;
			Dia::Maths::Vector2D mFocal;
			Dia::Maths::Vector2D mAxis;
		};
	}
}

#include "DiaGeometry2D/Shapes/Arc.inl"

#endif // DIA_GEOMETRY2D_ARC_H
