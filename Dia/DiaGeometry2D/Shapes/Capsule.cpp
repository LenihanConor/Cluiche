#include "DiaGeometry2D/Shapes/Capsule.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Line.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		void Capsule::ClosestPointOnCapsuleTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const
		{
			Line axis;
			Axis(axis);

			Dia::Maths::Vector2D closestPtOnAxis;
			axis.ClosestPointOnLineTo(point, closestPtOnAxis);

			Dia::Maths::Vector2D vec = (point - closestPtOnAxis).Normalize();
			result = closestPtOnAxis + (vec * mRadius);
		}

		//-----------------------------------------------------------------------------
		void Capsule::ClosestPointOnCapsuleTo(const Circle& circle, Dia::Maths::Vector2D& result) const
		{
			ClosestPointOnCapsuleTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		void Capsule::Axis(Line& axis) const
		{
			axis.Create(mPt1, mPt2);
		}

		//-----------------------------------------------------------------------------
		void Capsule::GetPoint1Circle(Circle& circle) const
		{
			circle = Circle::CreateFrom(mRadius, mPt1);
		}

		//-----------------------------------------------------------------------------
		void Capsule::GetPoint2Circle(Circle& circle) const
		{
			circle = Circle::CreateFrom(mRadius, mPt2);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Capsule::IsIntersecting(const Circle& circle) const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
