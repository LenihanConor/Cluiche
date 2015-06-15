#include "DiaMaths/Shape/2D/Line2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Circle2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		void Line2D::Create(const Vector2D& pt1, const Vector2D& pt2)
		{
			DIA_ASSERT(pt1 != pt2, "Points are equal");

			mPt[0] = pt1;
			mPt[1] = pt2;
		}

		//-----------------------------------------------------------------------------
		Vector2D Line2D::Pt1ToPt2Vector()const
		{
			DIA_ASSERT(mPt[0] != mPt[1], "Points are equal");

			return (mPt[1] - mPt[0]).Normalize();
		}

		//-----------------------------------------------------------------------------
		Vector2D Line2D::Pt2ToPt1Vector()const
		{
			DIA_ASSERT(mPt[0] != mPt[1], "Points are equal");

			return (mPt[0] - mPt[1]).Normalize();
		}
		
		//-----------------------------------------------------------------------------
		float Line2D::Length() const
		{
			return (mPt[0] - mPt[1]).Magnitude();
		}

		//-----------------------------------------------------------------------------
		float Line2D::SquareLength() const
		{
			return (mPt[0] - mPt[1]).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		Vector2D Line2D::CalculateCenter()const
		{
			return (mPt[0] + mPt[1]) / 2.0f;
		}

		//-----------------------------------------------------------------------------
		void Line2D::ClosestPointOnLineTo(const Vector2D& point, Vector2D& result)const
		{
			float d = Dia::Maths::SquareRoot(SquareLength());
			Vector2D vVector1 = point - mPt[0];
			Vector2D vVector2 = ( mPt[1] - mPt[0] ).Normalize();
			float t = vVector2.Dot(vVector1);

			if (t >= d)
			{
				result = mPt[1];
			}
			else if (t <= 0)
			{
				result = mPt[0];
			}
			else
			{
				result = mPt[0] + ( vVector2 * t );
			}
		}

		//-----------------------------------------------------------------------------
		void Line2D::ClosestPointOnLineTo(const Circle2D& circle, Vector2D& result)const
		{
			ClosestPointOnLineTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Line2D::IsIntersecting(const Vector2D& point)const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Line2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}