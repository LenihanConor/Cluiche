#include "DiaMaths/Shape/2D/AARect2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Circle2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		Vector2D AARect2D::CalculateCenter()const
		{
			return (mTopRight + mBottonLeft) / 2.0f;
		}

		//-----------------------------------------------------------------------------
		void AARect2D::ClosestPointOnAARectTo(const Vector2D& point, Vector2D& result)const
		{
			if (point.x < mBottonLeft.x) 
				result.x = mBottonLeft.x;
			else if (point.x > mTopRight.x) 
				result.x = mTopRight.x;

			if (point.y < mBottonLeft.y) 
				result.y = mBottonLeft.y;
			else if (point.y > mTopRight.y) 
				result.y = mTopRight.y;
		}

		//-----------------------------------------------------------------------------
		void AARect2D::ClosestPointOnAARectTo(const Circle2D& circle, Vector2D& result)const
		{
			ClosestPointOnAARectTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify AARect2D::IsIntersecting(const Vector2D& point)const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify AARect2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}