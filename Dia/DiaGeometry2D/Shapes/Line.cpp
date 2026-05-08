#include "DiaGeometry2D/Shapes/Line.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		void Line::Create(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2)
		{
			DIA_ASSERT(pt1 != pt2, "Points are equal");
			mPt[0] = pt1;
			mPt[1] = pt2;
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Line::Pt1ToPt2Vector() const
		{
			DIA_ASSERT(mPt[0] != mPt[1], "Points are equal");
			return (mPt[1] - mPt[0]).Normalize();
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Line::Pt2ToPt1Vector() const
		{
			DIA_ASSERT(mPt[0] != mPt[1], "Points are equal");
			return (mPt[0] - mPt[1]).Normalize();
		}

		//-----------------------------------------------------------------------------
		float Line::Length() const
		{
			return (mPt[0] - mPt[1]).Magnitude();
		}

		//-----------------------------------------------------------------------------
		float Line::SquareLength() const
		{
			return (mPt[0] - mPt[1]).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Line::CalculateCenter() const
		{
			return (mPt[0] + mPt[1]) / 2.0f;
		}

		//-----------------------------------------------------------------------------
		void Line::ClosestPointOnLineTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const
		{
			float d = Dia::Maths::SquareRoot(SquareLength());
			Dia::Maths::Vector2D vVector1 = point - mPt[0];
			Dia::Maths::Vector2D vVector2 = (mPt[1] - mPt[0]).Normalize();
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
				result = mPt[0] + (vVector2 * t);
			}
		}

		//-----------------------------------------------------------------------------
		void Line::ClosestPointOnLineTo(const Circle& circle, Dia::Maths::Vector2D& result) const
		{
			ClosestPointOnLineTo(circle.GetCenter(), result);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Line::IsIntersecting(const Dia::Maths::Vector2D& point) const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Line::IsIntersecting(const Circle& circle) const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
