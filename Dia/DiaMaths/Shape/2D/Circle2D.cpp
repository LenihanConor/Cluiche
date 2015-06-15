#include "DiaMaths/Shape/2D/Circle2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Capsule2D.h"
#include "DiaMaths/Shape/2D/Line2D.h"
#include "DiaMaths/Shape/2D/AARect2D.h"
#include "DiaMaths/Shape/2D/Arc2D.h"
#include "DiaMaths/Shape/2D/OORect2D.h"
#include "DiaMaths/Shape/2D/Triangle2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		Circle2D& Circle2D::UnitCircle()
		{
			static Circle2D unitCircle(1.0f, Vector2D(0.0f, 0.0f)); 
			return unitCircle;
		}

		//-----------------------------------------------------------------------------
		Circle2D& Circle2D::operator =(const Circle2D& rhs)
		{
			mCenter = rhs.mCenter;
			mRadius = rhs.mRadius;

			return *this;
		}

		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom(float radius, const Vector2D& position)
		{
			return Circle2D(radius, position);
		}
		
		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const AARect2D& rect)
		{
			return Circle2D(rect.CalculateRadius(), rect.CalculateCenter());
		}
	
		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const Capsule2D& capsule)
		{
			return Circle2D( capsule.GetLength() / 2.0f, capsule.GetCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const Arc2D& arc)
		{
			return Circle2D(arc.GetRadius(), arc.GetFocal());
		}

		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const Line2D& line)
		{
			return Circle2D(line.Length() / 2.0f, line.CalculateCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const OORect2D& rect)
		{
			return Circle2D(rect.CalculateRadius(), rect.CalculateCenter());
		}

		//----------------------------------------------------------------------------------------------------
		Circle2D Circle2D::CreateFrom (const Triangle2D& tri)
		{
			Circle2D circle;
			tri.CalculateCircumCircle(circle);

			return circle;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Vector2D& point)
		{
			Vector2D vecDir = (point - mCenter);
			float sqNewDistance = vecDir.SquareMagnitude();

			if (sqNewDistance > GetSquaredRadius())
			{
				Vector2D normalizedVecDir = vecDir.AsNormal();
				Vector2D farPoint = mCenter + (-normalizedVecDir * mRadius);
				
				mCenter = (point + farPoint) / 2.0f;
				mRadius = mCenter.DistanceTo(farPoint);
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const AARect2D& rect)
		{
			ReContructToInclude(rect.GetBottomLeft());
			ReContructToInclude(rect.GetTopRight());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Arc2D& arc)
		{
			Vector2D extent1 = arc.CalculateExtentPositionClockwise();
			Vector2D extent2 = arc.CalculateExtentPositionCounterClockwise();

			ReContructToInclude(arc.GetFocal());
			ReContructToInclude(extent1);
			ReContructToInclude(extent2);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Line2D& line)
		{
			ReContructToInclude(line.GetPt1());
			ReContructToInclude(line.GetPt2());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Capsule2D& capsule)
		{
			Circle2D circle1, circle2;
			
			capsule.GetPoint1Circle(circle1);
			capsule.GetPoint2Circle(circle2);

			ReContructToInclude(circle1);
			ReContructToInclude(circle2);
		}
			
		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Circle2D& circle)
		{
			Vector2D extentDir = (circle.GetCenter() - mCenter);

			float distPoint = extentDir.Magnitude() + circle.GetRadius();

			extentDir.Normalize();

			ReContructToInclude(mCenter + (extentDir * distPoint));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const OORect2D& rect)
		{
			for (int i = 0; i < OORect2D::kNumPts; i++)
			{
				ReContructToInclude(rect.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ReContructToInclude (const Triangle2D& tri)
		{
			for (int i = 0; i < Triangle2D::kNumPts; i++)
			{
				ReContructToInclude(tri.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Vector2D& point)
		{
			Vector2D vecDir = (point - mCenter);
			float sqNewDistance = vecDir.SquareMagnitude();

			if (sqNewDistance > GetSquaredRadius())
			{
				mRadius = Dia::Maths::SquareRoot(sqNewDistance);
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const AARect2D& rect)
		{
			ExpandToInclude(rect.GetBottomLeft());
			ExpandToInclude(rect.GetTopRight());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Arc2D& arc)
		{
			Vector2D extent1 = arc.CalculateExtentPositionClockwise();
			Vector2D extent2 = arc.CalculateExtentPositionCounterClockwise();

			ExpandToInclude(arc.GetFocal());
			ExpandToInclude(extent1);
			ExpandToInclude(extent2);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Line2D& line)
		{
			ExpandToInclude(line.GetPt1());
			ExpandToInclude(line.GetPt2());
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Capsule2D& capsule)
		{
			Vector2D extentDir1 = (capsule.GetPoint1() - mCenter);
			Vector2D extentDir2 = (capsule.GetPoint2() - mCenter);

			float distPoint1 = extentDir1.Magnitude() + capsule.GetRadius();
			float distPoint2 = extentDir2.Magnitude() + capsule.GetRadius();

			extentDir1.Normalize();
			extentDir2.Normalize();

			ExpandToInclude(mCenter + (extentDir1 * distPoint1));
			ExpandToInclude(mCenter + (extentDir2 * distPoint2));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Circle2D& circle)
		{
			Vector2D extentDir = (circle.GetCenter() - mCenter);

			float distPoint = extentDir.Magnitude() + circle.GetRadius();

			extentDir.Normalize();

			ExpandToInclude(mCenter + (extentDir * distPoint));
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const OORect2D& rect)
		{
			for (int i = 0; i < OORect2D::kNumPts; i++)
			{
				ExpandToInclude(rect.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ExpandToInclude (const Triangle2D& tri)
		{
			for (int i = 0; i < Triangle2D::kNumPts; i++)
			{
				ExpandToInclude(tri.GetPt(i));
			}
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::SetCenter(const Vector2D& position)
		{
			mCenter = position;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::SetRadius(float radius)
		{
			mRadius = radius;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::Translate(const Vector2D& translation)
		{
			mCenter += translation;
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::Scale(float scale)
		{
			mRadius *= scale;
		}
		
		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const Vector2D& point, Vector2D& result)const
		{
			DIA_ASSERT( point == mCenter, "Point == on center" );

			result = ((point - mCenter).AsNormal() * mRadius);
		}
		
		void Circle2D::ClosestPointOnCircleTo(const Arc2D& arc, Vector2D& result)const
		{
			Vector2D closestPtOnArc;
			
			arc.ClosestPointOnArcTo(*this, closestPtOnArc);
			
			ClosestPointOnCircleTo(closestPtOnArc, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const Circle2D& circle, Vector2D& result)const
		{
			result = ((circle.GetCenter() - mCenter).AsNormal() * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const Line2D& line, Vector2D& result)const
		{
			Vector2D posOnLine;
			line.ClosestPointOnLineTo(*this, posOnLine);

			result = ((posOnLine - mCenter).AsNormal() * mRadius);
		}
		
		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const Capsule2D& caspule, Vector2D& result)const
		{
			Line2D axis;

			caspule.Axis(axis);

			Vector2D closestPtOnAxis;
			axis.ClosestPointOnLineTo(mCenter, closestPtOnAxis);

			Vector2D vec = (closestPtOnAxis - mCenter).AsNormal();

			result = mCenter + (vec * mRadius);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const AARect2D& rect, Vector2D& result)const
		{
			Vector2D closestPtOnAARect;
			rect.ClosestPointOnAARectTo(*this, closestPtOnAARect);

			ClosestPointOnCircleTo(closestPtOnAARect, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const OORect2D& rect, Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(rect).IsNotIntersecting(), "Rectangle intersecting Circle");

			Vector2D closetPtOnRectToCircle;

			rect.ClosestPointOnOORectTo(*this, closetPtOnRectToCircle);
				
			ClosestPointOnCircleTo(closetPtOnRectToCircle, result);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::ClosestPointOnCircleTo(const Triangle2D& tri, Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(tri).IsNotIntersecting(), "Triangle intersecting Circle");

			Vector2D closetPtOnRectToCircle;

			tri.ClosestPointOnOORectTo(*this, closetPtOnRectToCircle);

			ClosestPointOnCircleTo(closetPtOnRectToCircle, result);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Vector2D& point)const
		{
			return IntersectionTests::IsIntersecting(point, *this);
		}
		
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(circle, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const AARect2D& rect)const
		{
			return IntersectionTests::IsIntersecting(rect, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Arc2D& arc)const
		{
			return IntersectionTests::IsIntersecting(arc, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Line2D& line)const
		{
			return IntersectionTests::IsIntersecting(line, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Capsule2D& capsule)const
		{
			return IntersectionTests::IsIntersecting(capsule, *this);
		}
		
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const OORect2D& rect)const
		{
			return IntersectionTests::IsIntersecting(rect, *this);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Circle2D::IsIntersecting(const Triangle2D& rhs)const
		{
			return IntersectionTests::IsIntersecting(rhs, *this);
		}

		//----------------------------------------------------------------------------------------------------
		void Circle2D::CalculateAxisPointToPoint( const Vector2D& pointOutsideCircle, Vector2D& closestPt, Vector2D& furthestPt ) const
		{
			DIA_ASSERT( !IsIntersecting( pointOutsideCircle ).IsIntersecting(), "Point inside circle" );
			DIA_ASSERT( pointOutsideCircle == mCenter, "Point equal to circle" );

			Vector2D vec = (pointOutsideCircle - mCenter).AsNormal();

			closestPt = (vec * mRadius);
			furthestPt = (vec.AsInverse() * mRadius);
		}

		void Circle2D::CalculateTangents( const Vector2D& pointOutsideCircle, Vector2D& tangent1, Vector2D& tangent2 ) const
		{
			DIA_ASSERT( !IsIntersecting( pointOutsideCircle ).IsIntersecting(), "Point inside circle" );

			Vector2D lineToCentre = GetCenter() - pointOutsideCircle;

			float intersectingCircleRadius = Dia::Maths::SquareRoot( lineToCentre.SquareMagnitude() - Square( GetRadius() ) );

			DIA_ASSERT_SUPPORT(int nIntersections = )
			CalculateIntercepts( Circle2D( intersectingCircleRadius, pointOutsideCircle ), tangent1, tangent2 );
			DIA_ASSERT( nIntersections == 2, "Should return 2 intersection points" );
		}

		int Circle2D::CalculateIntercepts(const Circle2D& circle, Vector2D& intercept1, Vector2D& intercept2)const
		{
			return IntersectionTests::CalculateIntercepts(*this, circle, intercept1, intercept2);
		}

	/*	int CalculateIntercepts(const Line2D& rhs, Vector2D& intercept1, Vector2D& intercept1)const
		{

		}*/
	}
}