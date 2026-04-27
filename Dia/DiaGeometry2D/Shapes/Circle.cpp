#include "DiaGeometry2D/Shapes/Circle.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Capsule.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Arc.h"
#include "DiaGeometry2D/Shapes/OORect.h"
#include "DiaGeometry2D/Shapes/Triangle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Circle& Circle::UnitCircle()
		{
			static Circle unitCircle(1.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
			return unitCircle;
		}

		//-----------------------------------------------------------------------------
		Circle& Circle::operator =(const Circle& rhs)
		{
			mCenter = rhs.mCenter;
			mRadius = rhs.mRadius;

			return *this;
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(float radius, const Dia::Maths::Vector2D& position)
		{
			return Circle(radius, position);
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const AARect& rect)
		{
			return Circle(rect.CalculateRadius(), rect.CalculateCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const Capsule& capsule)
		{
			return Circle(capsule.GetLength() / 2.0f, capsule.GetCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const Arc& arc)
		{
			return Circle(arc.GetRadius(), arc.GetFocal());
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const Line& line)
		{
			return Circle(line.Length() / 2.0f, line.CalculateCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const OORect& rect)
		{
			return Circle(rect.CalculateRadius(), rect.CalculateCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle Circle::CreateFrom(const Triangle& tri)
		{
			Circle circle;
			tri.CalculateCircumCircle(circle);

			return circle;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Dia::Maths::Vector2D& point)
		{
			Dia::Maths::Vector2D vecDir = (point - mCenter);
			float sqNewDistance = vecDir.SquareMagnitude();

			if (sqNewDistance > GetSquaredRadius())
			{
				Dia::Maths::Vector2D normalizedVecDir = vecDir.AsNormal();
				Dia::Maths::Vector2D farPoint = mCenter + (-normalizedVecDir * mRadius);

				mCenter = (point + farPoint) / 2.0f;
				mRadius = mCenter.DistanceTo(farPoint);
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const AARect& rect)
		{
			ReContructToInclude(rect.GetBottomLeft());
			ReContructToInclude(rect.GetTopRight());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Arc& arc)
		{
			Dia::Maths::Vector2D extent1 = arc.CalculateExtentPositionClockwise();
			Dia::Maths::Vector2D extent2 = arc.CalculateExtentPositionCounterClockwise();

			ReContructToInclude(arc.GetFocal());
			ReContructToInclude(extent1);
			ReContructToInclude(extent2);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Line& line)
		{
			ReContructToInclude(line.GetPt1());
			ReContructToInclude(line.GetPt2());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Capsule& capsule)
		{
			Circle circle1, circle2;

			capsule.GetPoint1Circle(circle1);
			capsule.GetPoint2Circle(circle2);

			ReContructToInclude(circle1);
			ReContructToInclude(circle2);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Circle& circle)
		{
			Dia::Maths::Vector2D extentDir = (circle.GetCenter() - mCenter);

			float distPoint = extentDir.Magnitude() + circle.GetRadius();

			extentDir.Normalize();

			ReContructToInclude(mCenter + (extentDir * distPoint));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const OORect& rect)
		{
			for (int i = 0; i < OORect::kNumPts; i++)
			{
				ReContructToInclude(rect.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ReContructToInclude(const Triangle& tri)
		{
			for (int i = 0; i < Triangle::kNumPts; i++)
			{
				ReContructToInclude(tri.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Dia::Maths::Vector2D& point)
		{
			Dia::Maths::Vector2D vecDir = (point - mCenter);
			float sqNewDistance = vecDir.SquareMagnitude();

			if (sqNewDistance > GetSquaredRadius())
			{
				mRadius = Dia::Maths::SquareRoot(sqNewDistance);
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const AARect& rect)
		{
			ExpandToInclude(rect.GetBottomLeft());
			ExpandToInclude(rect.GetTopRight());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Arc& arc)
		{
			Dia::Maths::Vector2D extent1 = arc.CalculateExtentPositionClockwise();
			Dia::Maths::Vector2D extent2 = arc.CalculateExtentPositionCounterClockwise();

			ExpandToInclude(arc.GetFocal());
			ExpandToInclude(extent1);
			ExpandToInclude(extent2);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Line& line)
		{
			ExpandToInclude(line.GetPt1());
			ExpandToInclude(line.GetPt2());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Capsule& capsule)
		{
			Dia::Maths::Vector2D extentDir1 = (capsule.GetPoint1() - mCenter);
			Dia::Maths::Vector2D extentDir2 = (capsule.GetPoint2() - mCenter);

			float distPoint1 = extentDir1.Magnitude() + capsule.GetRadius();
			float distPoint2 = extentDir2.Magnitude() + capsule.GetRadius();

			extentDir1.Normalize();
			extentDir2.Normalize();

			ExpandToInclude(mCenter + (extentDir1 * distPoint1));
			ExpandToInclude(mCenter + (extentDir2 * distPoint2));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Circle& circle)
		{
			Dia::Maths::Vector2D extentDir = (circle.GetCenter() - mCenter);

			float distPoint = extentDir.Magnitude() + circle.GetRadius();

			extentDir.Normalize();

			ExpandToInclude(mCenter + (extentDir * distPoint));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const OORect& rect)
		{
			for (int i = 0; i < OORect::kNumPts; i++)
			{
				ExpandToInclude(rect.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ExpandToInclude(const Triangle& tri)
		{
			for (int i = 0; i < Triangle::kNumPts; i++)
			{
				ExpandToInclude(tri.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::SetCenter(const Dia::Maths::Vector2D& position)
		{
			mCenter = position;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::SetRadius(float radius)
		{
			mRadius = radius;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::Translate(const Dia::Maths::Vector2D& translation)
		{
			mCenter += translation;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::Scale(float scale)
		{
			mRadius *= scale;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result)const
		{
			DIA_ASSERT(point != mCenter, "Point must not be equal to center");

			result = mCenter + ((point - mCenter).AsNormal() * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Arc& arc, Dia::Maths::Vector2D& result)const
		{
			Dia::Maths::Vector2D closestPtOnArc;

			arc.ClosestPointOnArcTo(*this, closestPtOnArc);

			ClosestPointOnCircleTo(closestPtOnArc, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Circle& circle, Dia::Maths::Vector2D& result)const
		{
			result = mCenter + ((circle.GetCenter() - mCenter).AsNormal() * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Line& line, Dia::Maths::Vector2D& result)const
		{
			Dia::Maths::Vector2D posOnLine;
			line.ClosestPointOnLineTo(*this, posOnLine);

			result = mCenter + ((posOnLine - mCenter).AsNormal() * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Capsule& capsule, Dia::Maths::Vector2D& result)const
		{
			Line axis;

			capsule.Axis(axis);

			Dia::Maths::Vector2D closestPtOnAxis;
			axis.ClosestPointOnLineTo(mCenter, closestPtOnAxis);

			Dia::Maths::Vector2D vec = (closestPtOnAxis - mCenter).AsNormal();

			result = mCenter + (vec * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const AARect& rect, Dia::Maths::Vector2D& result)const
		{
			Dia::Maths::Vector2D closestPtOnAARect;
			rect.ClosestPointOnAARectTo(*this, closestPtOnAARect);

			ClosestPointOnCircleTo(closestPtOnAARect, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const OORect& rect, Dia::Maths::Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(rect).IsNotIntersecting(), "Rectangle intersecting Circle");

			Dia::Maths::Vector2D closetPtOnRectToCircle;

			rect.ClosestPointOnOORectTo(*this, closetPtOnRectToCircle);

			ClosestPointOnCircleTo(closetPtOnRectToCircle, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::ClosestPointOnCircleTo(const Triangle& tri, Dia::Maths::Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(tri).IsNotIntersecting(), "Triangle intersecting Circle");

			Dia::Maths::Vector2D closetPtOnRectToCircle;

			tri.ClosestPointOnOORectTo(*this, closetPtOnRectToCircle);

			ClosestPointOnCircleTo(closetPtOnRectToCircle, result);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Dia::Maths::Vector2D& point)const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Circle& circle)const
		{
			return IntersectionTests::IsIntersecting(circle, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const AARect& rect)const
		{
			return IntersectionTests::IsIntersecting(rect, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Arc& arc)const
		{
			return IntersectionTests::IsIntersecting(arc, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Line& line)const
		{
			return IntersectionTests::IsIntersecting(line, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Capsule& capsule)const
		{
			return IntersectionTests::IsIntersecting(capsule, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const OORect& rect)const
		{
			return IntersectionTests::IsIntersecting(rect, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle::IsIntersecting(const Triangle& rhs)const
		{
			return IntersectionTests::IsIntersecting(rhs, *this);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::CalculateAxisPointToPoint(const Dia::Maths::Vector2D& pointOutsideCircle, Dia::Maths::Vector2D& closestPt, Dia::Maths::Vector2D& furthestPt) const
		{
			DIA_ASSERT(!IsIntersecting(pointOutsideCircle).IsIntersecting(), "Point inside circle");
			DIA_ASSERT(!(pointOutsideCircle == mCenter), "Point equal to circle center");

			Dia::Maths::Vector2D vec = (pointOutsideCircle - mCenter).AsNormal();

			closestPt = mCenter + (vec * mRadius);
			furthestPt = mCenter + (vec.AsInverse() * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle::CalculateTangents(const Dia::Maths::Vector2D& pointOutsideCircle, Dia::Maths::Vector2D& tangent1, Dia::Maths::Vector2D& tangent2) const
		{
			DIA_ASSERT(!IsIntersecting(pointOutsideCircle).IsIntersecting(), "Point inside circle");

			Dia::Maths::Vector2D lineToCentre = GetCenter() - pointOutsideCircle;

			float intersectingCircleRadius = Dia::Maths::SquareRoot(lineToCentre.SquareMagnitude() - GetSquaredRadius());

			DIA_ASSERT_SUPPORT(int nIntersections = )
			CalculateIntercepts(Circle(intersectingCircleRadius, pointOutsideCircle), tangent1, tangent2);
			DIA_ASSERT(nIntersections == 2, "Should return 2 intersection points");
		}

		//----------------------------------------------------------------------------------------------------
		int Circle::CalculateIntercepts(const Circle& circle, Dia::Maths::Vector2D& intercept1, Dia::Maths::Vector2D& intercept2)const
		{
			return IntersectionTests::CalculateIntercepts(*this, circle, intercept1, intercept2);
		}
	}
}
