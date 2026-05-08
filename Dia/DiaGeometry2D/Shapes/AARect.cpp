#include "DiaGeometry2D/Shapes/AARect.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Circle.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D AARect::CalculateCenter() const
		{
			return (mTopRight + mBottomLeft) / 2.0f;
		}

		//-----------------------------------------------------------------------------
		void AARect::ClosestPointOnAARectTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const
		{
			result = point;

			if (point.x < mBottomLeft.x)
				result.x = mBottomLeft.x;
			else if (point.x > mTopRight.x)
				result.x = mTopRight.x;

			if (point.y < mBottomLeft.y)
				result.y = mBottomLeft.y;
			else if (point.y > mTopRight.y)
				result.y = mTopRight.y;
		}

		//-----------------------------------------------------------------------------
		void AARect::ClosestPointOnAARectTo(const Circle& circle, Dia::Maths::Vector2D& result) const
		{
			ClosestPointOnAARectTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify AARect::IsIntersecting(const Dia::Maths::Vector2D& point) const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify AARect::IsIntersecting(const Circle& circle) const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
