#include "DiaMaths/Shape/Common/IntersectionTests.h"

#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Shape/2D/Circle2D.h"
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
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Vector2D& point, const Circle2D& circle)
		{
			float sqRadius = Dia::Maths::Square(circle.GetRadius());
			float sqDistToPoint = circle.GetCenter().SquareDistanceTo(point);

			if (Dia::Maths::Float::FLessEqual(sqDistToPoint, sqRadius))
			{
				return IntersectionClassify::kBContainsA;
			}

			return IntersectionClassify::kNoIntersection;
		}
		
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const AARect2D& rect, const Circle2D& circle)
		{			
			Vector2D closestPointOnAARectTo;
			rect.ClosestPointOnAARectTo(circle, closestPointOnAARectTo);
			
			IntersectionClassify centerInsideRect = IsIntersecting(circle.GetCenter(), rect);

			if (centerInsideRect.IsIntersecting())
			{
				// Can still be kPenetrating, kBContainedA, kAContainedB
				float rectSqRadius = rect.CalculateSquaredRadius();
				float circleSqRadius = circle.GetSquaredRadius();

				if (rectSqRadius < circleSqRadius)
				{
					// Can still be kPenetrating, kBContainedA
					Circle2D testCircle(Circle2D::CreateFrom(rect));
				
					if (IsIntersecting(circle, testCircle) == IntersectionClassify::kAContainsB)
					{
						return IntersectionClassify::kBContainsA;
					}
				}
				
				// Can still be kPenetrating, kAContainedB
				if (circle.IsIntersecting(closestPointOnAARectTo).IsIntersecting())
				{
					return IntersectionClassify::kPenatrating;
				}

				return IntersectionClassify::kAContainsB;
			}
			
			// Can still be kPenetrating, kNoIntersection
			if (circle.IsIntersecting(closestPointOnAARectTo).IsIntersecting())
			{
				return IntersectionClassify::kPenatrating;
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Arc2D& arc, const Circle2D& circle)
		{
			// Test for A Containment B
			if (!circle.IsIntersecting(arc.GetFocal()).IsIntersecting())
			{
				Vector2D tangent1, tangent2, closePt, farPoint;
				circle.CalculateAxisPointToPoint(arc.GetFocal(), closePt, farPoint);
				circle.CalculateTangents(arc.GetFocal(), tangent1, tangent2);

				if (arc.IsIntersecting(tangent1).IsContainment() && arc.IsIntersecting(tangent2).IsContainment() && arc.IsIntersecting(farPoint).IsContainment())
				{
					return IntersectionClassify::kAContainsB;
				}
			}
			
			Vector2D extent1, extent2;
			extent1 = arc.CalculateExtentPositionClockwise();
			extent2 = arc.CalculateExtentPositionCounterClockwise();

			if (circle.IsIntersecting(arc.GetFocal()).IsContainment() && 
				circle.IsIntersecting(extent1).IsContainment() &&
				circle.IsIntersecting(extent2).IsContainment())
			{
				return IntersectionClassify::kBContainsA;
			}
	
			float sinOfArc = Sin(arc.GetAngle());
			float cosOfArc = Cos(arc.GetAngle());

			Vector2D U = arc.GetFocal() - ( arc.GetAxis() * (circle.GetRadius() * sinOfArc) );
			Vector2D D = circle.GetCenter() - U;
			float dsqr = D.Dot(D);
			float e = arc.GetAxis().Dot(D);
			if ( ( e > 0 ) && ( ( e*e ) >= ( dsqr * Square( cosOfArc ) ) ) )
			{
				D = circle.GetCenter() - arc.GetFocal();
				dsqr = D.Dot(D);
				float e = -arc.GetAxis().Dot(D);

				if ( ( e > 0 ) && ( ( e*e ) >= ( dsqr * Square(sinOfArc) ) ) && ( dsqr <= circle.GetSquaredRadius() ) )
				{
					return IntersectionClassify::kPenatrating;
				}
			}

			return IntersectionClassify::kNoIntersection;
		}
		
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Circle2D& circleA, const Circle2D& circleB)
		{
			float sqRadiusA = Dia::Maths::Square(circleA.GetRadius());
			float sqRadiusB = Dia::Maths::Square(circleB.GetRadius());
			float sqRadii = sqRadiusA + sqRadiusB;

			float sqDistFromCenters = circleA.GetCenter().SquareDistanceTo(circleB.GetCenter());

			if (Dia::Maths::Float::FLessEqual(sqDistFromCenters, sqRadii))
			{
				if (sqRadiusA < sqRadiusB)
				{
					float extent = sqDistFromCenters + sqRadiusA;
					if (Dia::Maths::Float::FLess(extent, sqRadiusB))
					{
						return IntersectionClassify::kBContainsA;
					}
				}
				else if (sqRadiusB < sqRadiusA)
				{
					float extent = sqDistFromCenters + sqRadiusB;
					if (Dia::Maths::Float::FLess(extent, sqRadiusA))
					{
						return IntersectionClassify::kAContainsB;
					}
				}
				
				return IntersectionClassify::kPenatrating;
			}

			return IntersectionClassify::kNoIntersection;		
		}

		//----------------------------------------------------------------------------------------------------
		int IntersectionTests::CalculateIntercepts(const Circle2D& circleA, const Circle2D& circleB, Vector2D& intercept1, Vector2D& intercept2)
		{
			IntersectionClassify intersectionClassify = circleA.IsIntersecting(circleB);

			if (intersectionClassify.GetClassification() != IntersectionClassify::kPenatrating)
			{
				return 0;
			}

			const Vector2D lineBetweenCentres = circleA.GetCenter() - circleB.GetCenter();
			const float D = lineBetweenCentres.Magnitude();

			const float R1 = circleA.GetRadius();
			const float R2 = circleB.GetRadius();

			const float y1 = circleA.GetCenter().Y();
			const float y2 = circleB.GetCenter().Y();
			const float x1 = circleA.GetCenter().X();
			const float x2 = circleB.GetCenter().X();

			const float A = Dia::Maths::SquareRoot( ( D + R1 + R2 ) * ( D + R1 - R2 ) * ( D - R1 + R2 ) * ( R1 + R2 - D ) ) / 4.0f;

			const float DSquared = Dia::Maths::Square( D );
			const float R1Squared = Dia::Maths::Square( R1 );
			const float R2Squared = Dia::Maths::Square( R2 );

			const float xBase = ( ( x1 + x2 ) / 2.0f ) - ( ( x1 - x2 ) * ( R1Squared - R2Squared ) / ( 2.0f * DSquared ) );
			const float xShift = 2 * ( y1 - y2 ) * A / DSquared;

			const float yBase = ( ( y1 + y2 ) / 2.0f ) - ( ( y1 - y2 ) * ( R1Squared - R2Squared ) / ( 2.0f * DSquared ) );
			const float yShift = 2 * ( x1 - x2 ) * A / DSquared;

			intercept1.X( xBase + xShift );
			intercept1.Y( yBase - yShift );

			intercept2.X( xBase - xShift );
			intercept2.Y( yBase + yShift );

			return 2;
		}
		
		//----------------------------------------------------------------------------------------------------
	/*	int IntersectionTests::CalculateIntercepts(const Line2D& line, const Circle2D& circle, Vector2D& intercept1, Vector2D& intercept2)
		{
			DIA_ASSERT( line.SquareMagnitude() > 0.0f );

			IntersectionClassify intersectionClassify = circle.IsIntersecting(line);

			if (intersectionClassify.GetClassification() != IntersectionClassify::kPenatrating)
			{
				return 0;
			}








			const bool lineIsFinite = ( treatLineAsInfiniteEnum == ETreatLineAsInfinite_NO );

			const Vector2 lineEnds[] = { line.ValueA(), line.ValueB() };

			const float sqrRadius = Square( m_Radius );

			const Vector2 toCentreFromEnd[] = { m_CentrePos - lineEnds[0], m_CentrePos - lineEnds[1] };
			const float toCentreFromEndDistance[] = { toCentreFromEnd[0].Magnitude(), toCentreFromEnd[1].Magnitude() };

			int result = 0;

			const Vector2 toBFromA = line.ToBFromA();
			Vector2 orthogonal( toBFromA.Y(), -toBFromA.X() );
			DIA_ASSERT( orthogonal.SquareMagnitude() > 0.0f );
			DIA_ASSERT.Normalize();

			bool lineEnd1IsCentre = ( toCentreFromEndDistance[0] == 0.0f );
			const Vector2& toCentreFromEndNonZero = lineEnd1IsCentre ? toCentreFromEnd[1] : toCentreFromEnd[0];
			DIA_ASSERT( toCentreFromEndNonZero.SquareMagnitude() != 0.0f );

			const float normalToCentreDistance = toCentreFromEndNonZero.Dot( orthogonal );

			// otherwise, there is at least one intercept point
			orthogonal *= normalToCentreDistance;
			const Vector2 orthogonalPoint = m_CentrePos - orthogonal;

			const float toCircleFromOP = SquareRoot( sqrRadius - Square( normalToCentreDistance ) );
			const Vector2 toBFromANormal = toBFromA.AsNormal();

		
			Vector2* pIntercept = pIntercept1;
			Vector2 lineEndFromOP[2];

			for( int i = 0; i < 2; ++i )
			{
				lineEndFromOP[i] = lineEnds[i] - orthogonalPoint;
				if( lineEndFromOP[i].SquareMagnitude() >= Square( toCircleFromOP ) )
				{
					++result;
					*pIntercept = orthogonalPoint + ( lineEndFromOP[i].AsNormal() * toCircleFromOP );
					pIntercept = pIntercept2;
				}
			}

			if( result == 2 )
			{
				if( lineEndFromOP[0].Dot( lineEndFromOP[1] ) > 0.0f )
				{
					// both points are the same side of the normal line. This means there are, in fact, NO intercepts
					result = 0;
				}
				else if( *pIntercept1 == *pIntercept2 )
				{
					// no point returning two identical results; treat it as a single intercept
					result = 1;
				}
			}

			return result;






			bool inCircle = false;
			CVector3 intersectionDirection;

			//get distance from the radius to the point in question
			float distance = math->lenghtLine( point, center );

			//if the distance is less than the radius then it is contact
			if( distance <= radius )
			{
				inCircle = true;
				//create a normalized vector
				intersectionDirection = math->Normalize( math->Vector( center, point ) );
				pointIntersection = center - ( intersectionDirection * radius );
				intersectionNormal = math->Normalize( math->Vector( pointIntersection, center ) );
			}
			return inCircle;
		}*/

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Vector2D& point, const AARect2D& rect)
		{
			if (point.x >= rect.GetBottomLeft().x &&
				point.y >= rect.GetBottomLeft().y && 
				point.x <= rect.GetTopRight().x &&
				point.y <= rect.GetTopRight().y)
			{
				return IntersectionClassify::kBContainsA;
			}
			
			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Line2D& line, const Circle2D& circle)
		{
			Vector2D closestPt;
			
			line.ClosestPointOnLineTo(circle, closestPt);

			IntersectionClassify circleIntersecton = IsIntersecting(closestPt, circle);
			
			if (circleIntersecton.IsNotIntersecting())
			{
				return IntersectionClassify::kNoIntersection;
			}

			bool pt1IsContained = (IsIntersecting(line.GetPt1(), circle) == IntersectionClassify::kBContainsA);
			bool pt2IsContained = (IsIntersecting(line.GetPt2(), circle) == IntersectionClassify::kBContainsA);

			if (pt1IsContained && pt2IsContained)
			{
				return IntersectionClassify::kBContainsA;
			}

			return IntersectionClassify::kPenatrating;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Capsule2D& capsule, const Circle2D& circle)
		{
			Line2D axis;

			capsule.Axis(axis);

			Vector2D ptOnAxis;
			axis.ClosestPointOnLineTo(circle.GetCenter(), ptOnAxis);

			float sqDistanceToAxis = circle.GetCenter().SquareDistanceTo(ptOnAxis);
			float sqCircleRadius = circle.GetSquaredRadius(); 
			float sqCapsuleRadius = capsule.GetSquaredRadius(); 
			float sqRadii = sqCircleRadius + sqCapsuleRadius;

			if (sqDistanceToAxis >= sqRadii)
			{
				return IntersectionClassify::kNoIntersection;
			}

			if ((sqCircleRadius <= sqCapsuleRadius) && 
							((sqDistanceToAxis + sqCircleRadius) < sqCapsuleRadius))
			{
				return IntersectionClassify::kAContainsB;
			}
			
			if ((sqCircleRadius >= sqCapsuleRadius) && 
				(sqDistanceToAxis + sqCapsuleRadius) < sqCircleRadius)
			{
					return IntersectionClassify::kBContainsA;
			}

			return IntersectionClassify::kPenatrating;
		}
		
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const OORect2D& rect, const Circle2D& circle)
		{
			int numEdgesContained = 0;
			for (int i = 0; i < OORect2D::kNumEdges; i++)
			{
				Line2D edge;

				rect.CalculateEdge(i, edge);

				IntersectionClassify classify = IsIntersecting(edge, circle);

				if (classify.GetClassification() == IntersectionClassify::kPenatrating)
				{
					return IntersectionClassify::kPenatrating;
				}
				else if (classify.IsContainment())
				{
					numEdgesContained++;
				}
			}
			
			if (numEdgesContained == OORect2D::kNumEdges)
			{
				return IntersectionClassify::kBContainsA;
			}

			DIA_ASSERT(numEdgesContained == 0, "No edges should be contained if we got this far");

			// We know it is not penetrating and we know it is not contained so we only need to test one point
			return IsIntersecting(rect.GetPt(0), circle);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Triangle2D& tri, const Circle2D& circle)
		{
			int numEdgesContained = 0;
			for (int i = 0; i < Triangle2D::kNumEdges; i++)
			{
				Line2D edge;

				tri.CalculateEdge(i, edge);

				IntersectionClassify classify = IsIntersecting(edge, circle);

				if (classify.GetClassification() == IntersectionClassify::kPenatrating)
				{
					return IntersectionClassify::kPenatrating;
				}
				else if (classify.IsContainment())
				{
					numEdgesContained++;
				}
			}

			if (numEdgesContained == Triangle2D::kNumEdges)
			{
				return IntersectionClassify::kBContainsA;
			}

			DIA_ASSERT(numEdgesContained == 0, "No edges should be contained if we got this far");

			// We know it is not penetrating and we know it is not contained so we only need to test one point
			return IsIntersecting(tri.GetPt(0), circle);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Vector2D& point, const Line2D& line)
		{
			if (line.GetPt1() == point)
			{
				return IntersectionClassify::kPenatrating;
			}

			Vector2D AtoB = (line.GetPt2() - line.GetPt1());
			Vector2D AtoPoint = (point - line.GetPt1());
			
			DIA_ASSERT(line.GetPt2() != line.GetPt1(), "Can not be the same point");

			float sqDist = AtoB.SquareMagnitude();
			float sqDistToPoint = AtoPoint.SquareMagnitude();

			if ( sqDistToPoint <= sqDist )
			{
				AtoB.Normalize();
				AtoPoint.Normalize();

				Angle betweenAngle;
				AtoB.GetAngleBetween(AtoPoint, betweenAngle);

				if (betweenAngle == Angle::Deg0)
				{
					return IntersectionClassify::kPenatrating;
				}
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Vector2D& point, const Arc2D& arc)
		{
			Vector2D toPoint = point - arc.GetFocal();
			float sqMag = toPoint.SquareMagnitude();

			// Not within the sphere radius
			if (sqMag >= (arc.GetRadius() * arc.GetRadius()))
			{
				return IntersectionClassify::kNoIntersection;
			}

			// Test to see if it is within the arc angle
			toPoint.Normalize();
			Dia::Maths::Angle angle;
			arc.GetAxis().GetAngleBetween(toPoint, angle);

			float halfAngle = (arc.GetAngle().AsRadians() / 2.0f);
			if (angle.AsRadians() < halfAngle)
			{
				return IntersectionClassify::kPenatrating;
			}

			return IntersectionClassify::kNoIntersection;
		}
	}
}