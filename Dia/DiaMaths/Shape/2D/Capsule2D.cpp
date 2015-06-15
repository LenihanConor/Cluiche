#include "DiaMaths/Shape/2D/Capsule2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Circle2D.h"
#include "DiaMaths/Shape/2D/Line2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		void Capsule2D::ClosestPointOnCapsuleTo(const Vector2D& point, Vector2D& result)const
		{
			Line2D axis;
			
			Axis(axis);
			
			Vector2D closestPtOnAxis;
			axis.ClosestPointOnLineTo(point, closestPtOnAxis);

			Vector2D vec = (point - closestPtOnAxis).Normalize();

			result = closestPtOnAxis + (vec * mRadius);
		}

		//-----------------------------------------------------------------------------
		void Capsule2D::ClosestPointOnCapsuleTo(const Circle2D& circle, Vector2D& result)const
		{
			ClosestPointOnCapsuleTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		void Capsule2D::Axis(Line2D& axis)const
		{
			axis.Create(mPt1, mPt2);
		}

		//-----------------------------------------------------------------------------
		void Capsule2D::GetPoint1Circle(Circle2D& circle)const
		{
			circle = Circle2D::CreateFrom(mRadius, mPt1);
		}

		//-----------------------------------------------------------------------------
		void Capsule2D::GetPoint2Circle(Circle2D& circle)const
		{
			circle = Circle2D::CreateFrom(mRadius, mPt2);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Capsule2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}