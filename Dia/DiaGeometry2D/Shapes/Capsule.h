#ifndef DIA_GEOMETRY2D_CAPSULE_H
#define DIA_GEOMETRY2D_CAPSULE_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;
		class Line;

		class Capsule
		{
		public:
			Capsule();
			Capsule(float radius, const Dia::Maths::Vector2D& axisPt1, const Dia::Maths::Vector2D& axisPt2);
			Capsule(const Capsule& rhs);

			Capsule& operator = (const Capsule& rhs);

			void Axis(Line& axis) const;

			float	GetRadius()				const;
			float	GetSquaredRadius()		const;
			float	GetLength()				const;
			float	GetSquaredLength()		const;
			float	GetAxisLength()			const;
			float	GetAxisSquaredLength()	const;

			const Dia::Maths::Vector2D&	GetPoint1() const;
			const Dia::Maths::Vector2D&	GetPoint2() const;
			Dia::Maths::Vector2D		GetCenter() const;

			void GetPoint1Circle(Circle& circle) const;
			void GetPoint2Circle(Circle& circle) const;

			void ClosestPointOnCapsuleTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const;
			void ClosestPointOnCapsuleTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Circle& circle) const;

		private:
			Dia::Maths::Vector2D mPt1;
			Dia::Maths::Vector2D mPt2;
			float mRadius;
		};
	}
}

#include "DiaGeometry2D/Shapes/Capsule.inl"

#endif // DIA_GEOMETRY2D_CAPSULE_H
