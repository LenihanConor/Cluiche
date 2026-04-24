#include "DiaGeometry2D/Shapes/Arc.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Arc::Arc(float radius, const Dia::Maths::Angle& angle, const Dia::Maths::Vector2D& focal, const Dia::Maths::Vector2D& axis)
			: mFocal(focal)
			, mAxis(axis)
			, mRadius(radius)
			, mAngle(angle)
		{
			mAxis.Normalize();
		}

		//-----------------------------------------------------------------------------
		bool Arc::IsBetweenExtents(const Dia::Maths::Vector2D& vec) const
		{
			DIA_ASSERT(vec.IsNormal(), "Must be normal");

			Dia::Maths::Angle angle;
			mAxis.GetAngleBetween(vec, angle);
			angle.AsPositiveDegrees();

			return (angle >= (mAngle / 2.0f));
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Arc::CalculateExtentVectorClockwise() const
		{
			DIA_ASSERT(mAxis.IsNormal(), "Must be normal");
			return mAxis.AsRotateClockwiseBy(mAngle / 2.0f);
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Arc::CalculateExtentVectorCounterClockwise() const
		{
			DIA_ASSERT(mAxis.IsNormal(), "Must be normal");
			return mAxis.AsRotateCounterClockwiseBy(mAngle / 2.0f);
		}

		//-----------------------------------------------------------------------------
		void Arc::CalculateExtentClockwise(Line& extent) const
		{
			Dia::Maths::Vector2D vec = CalculateExtentVectorClockwise();
			extent.Create(mFocal, mFocal + (vec * mRadius));
		}

		//-----------------------------------------------------------------------------
		void Arc::CalculateExtentCounterClockwise(Line& extent) const
		{
			Dia::Maths::Vector2D vec = CalculateExtentVectorCounterClockwise();
			extent.Create(mFocal, mFocal + (vec * mRadius));
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Arc::CalculateExtentPositionClockwise() const
		{
			Dia::Maths::Vector2D vec = CalculateExtentVectorClockwise();
			return mFocal + (vec * mRadius);
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D Arc::CalculateExtentPositionCounterClockwise() const
		{
			Dia::Maths::Vector2D vec = CalculateExtentVectorCounterClockwise();
			return mFocal + (vec * mRadius);
		}

		//-----------------------------------------------------------------------------
		void Arc::ClosestPointOnArcTo(const Circle& circle, Dia::Maths::Vector2D& result) const
		{
			Dia::Maths::Vector2D lineBetweenCenters = (circle.GetCenter() - mFocal);
			Dia::Maths::Vector2D vecBetweenCenters = lineBetweenCenters.AsNormal();

			if (IsBetweenExtents(vecBetweenCenters))
			{
				result = mFocal + (vecBetweenCenters * mRadius);
			}
			else
			{
				Line extent1, extent2;

				CalculateExtentClockwise(extent1);
				CalculateExtentCounterClockwise(extent2);

				Dia::Maths::Vector2D closestPt1, closestPt2;

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
		IntersectionClassify Arc::IsIntersecting(const Dia::Maths::Vector2D& point) const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//-----------------------------------------------------------------------------
		IntersectionClassify Arc::IsIntersecting(const Circle& circle) const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
