#include "DiaMaths/Shape/2D/Arc2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Circle2D.h"
#include "DiaMaths/Shape/2D/Line2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		Arc2D::Arc2D(float radius, const Angle& angle, const Vector2D& focal, const Vector2D& axis)
			: mFocal(focal)
			, mAxis(axis)
			, mRadius(radius)
			, mAngle(angle)
		{
			mAxis.Normalize();
		}

		//-----------------------------------------------------------------------------
		bool Arc2D::IsBetweenExtents(const Vector2D& vec)const
		{
			DIA_ASSERT(vec.IsNormal(), "Must be normal");

			Angle angle;
			mAxis.GetAngleBetween(vec, angle);
			angle.AsPositiveDegrees();

			return (angle >= (mAngle / 2.0f ) );
		}

		//-----------------------------------------------------------------------------
		Vector2D Arc2D::CalculateExtentVectorClockwise()const
		{
			DIA_ASSERT (mAxis.IsNormal(), "Must be normal");

			return mAxis.AsRotateClockwiseBy(mAngle / 2.0f);
		}

		//-----------------------------------------------------------------------------
		Vector2D Arc2D::CalculateExtentVectorCounterClockwise()const
		{
			DIA_ASSERT (mAxis.IsNormal(), "Must be normal");
			
			return mAxis.AsRotateCounterClockwiseBy(mAngle / 2.0f);
		}

		//-----------------------------------------------------------------------------
		void Arc2D::CalculateExtentClockwise(Line2D& extent)const
		{
			Vector2D vec = CalculateExtentVectorClockwise();

			extent.Create(mFocal, mFocal + (vec * mRadius));
		}

		//-----------------------------------------------------------------------------
		void Arc2D::CalculateExtentCounterClockwise(Line2D& extent)const
		{
			Vector2D vec = CalculateExtentVectorCounterClockwise();

			extent.Create(mFocal, mFocal + (vec * mRadius));
		}

		//-----------------------------------------------------------------------------
		Vector2D Arc2D::CalculateExtentPositionClockwise()const
		{
			Vector2D vec = CalculateExtentVectorClockwise();

			Vector2D extent = mFocal + (vec * mRadius);

			return extent;
		}

		//-----------------------------------------------------------------------------
		Vector2D Arc2D::CalculateExtentPositionCounterClockwise()const
		{
			Vector2D vec = CalculateExtentVectorCounterClockwise();

			Vector2D extent = mFocal + (vec * mRadius);

			return extent;
		}

		//-----------------------------------------------------------------------------
		void Arc2D::ClosestPointOnArcTo(const Circle2D& circle, Vector2D& result)const
		{
			Vector2D lineBetweenCenters = (circle.GetCenter() - mFocal);
			Vector2D vecBetweenCenters = lineBetweenCenters.AsNormal();

			if (IsBetweenExtents(vecBetweenCenters))
			{
				result = mFocal + (vecBetweenCenters * mRadius);
			}
			else
			{
				Line2D extent1, extent2;
			
				CalculateExtentClockwise(extent1);
				CalculateExtentCounterClockwise(extent2);
				
				Vector2D closestPt1, closestPt2;
				
				extent1.ClosestPointOnLineTo(circle, closestPt1);
				extent2.ClosestPointOnLineTo(circle, closestPt2);

				float manhattanDistToPt1 = closestPt1.ManhattanDistanceTo(circle.GetCenter());
				float manhattanDistToPt2 = closestPt2.ManhattanDistanceTo(circle.GetCenter());

				if (manhattanDistToPt1 < manhattanDistToPt2)
				{
					result = closestPt1;
				}
				else
				{
					result = closestPt2;
				}
			}
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Arc2D::IsIntersecting(const Vector2D& point)const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Arc2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}