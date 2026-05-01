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

		//----------------------------------------------------------------------------------------------------
		// AARect vs AARect (AABB collision test)
		//
		// Classic Axis-Aligned Bounding Box (AABB) intersection test
		// One of the fastest collision tests in 2D game development!
		//
		// ALGORITHM:
		//   Two AABBs overlap if they overlap on ALL axes
		//   They DON'T overlap if separated on ANY axis (Separating Axis Theorem)
		//
		// TEST:
		//   X-axis: rect1.max.x >= rect2.min.x AND rect1.min.x <= rect2.max.x
		//   Y-axis: rect1.max.y >= rect2.min.y AND rect1.min.y <= rect2.max.y
		//   If both true: rectangles overlap
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const AARect2D& rect1, const AARect2D& rect2)
		{
			const Vector2D& min1 = rect1.GetBottomLeft();
			const Vector2D& max1 = rect1.GetTopRight();
			const Vector2D& min2 = rect2.GetBottomLeft();
			const Vector2D& max2 = rect2.GetTopRight();

			// Check if rectangles overlap on each axis
			// If they DON'T overlap on either axis, they don't intersect at all
			bool overlapX = (min1.x <= max2.x) && (max1.x >= min2.x);
			bool overlapY = (min1.y <= max2.y) && (max1.y >= min2.y);

			if (!overlapX || !overlapY)
			{
				return IntersectionClassify::kNoIntersection;
			}

			// Rectangles overlap! Determine relationship:
			// - One completely contains the other? (full containment)
			// - Or they partially overlap? (penetrating)

			// Check if rect1 completely contains rect2
			// (rect2's bounds are entirely within rect1's bounds)
			bool rect1ContainsRect2 = (min1.x <= min2.x && max1.x >= max2.x &&
			                           min1.y <= min2.y && max1.y >= max2.y);

			// Check if rect2 completely contains rect1
			bool rect2ContainsRect1 = (min2.x <= min1.x && max2.x >= max1.x &&
			                           min2.y <= min1.y && max2.y >= max1.y);

			if (rect1ContainsRect2)
			{
				return IntersectionClassify::kAContainsB;
			}
			else if (rect2ContainsRect1)
			{
				return IntersectionClassify::kBContainsA;
			}

			// Rectangles overlap but neither fully contains the other
			// This means they're partially intersecting (penetrating)
			return IntersectionClassify::kPenatrating;
		}

		//----------------------------------------------------------------------------------------------------
		// Line vs Line segment intersection test
		//
		// ALGORITHM: Parametric line equation solving
		//   Line1: P = p1 + dir1 * t1  (t1 in [0,1] for segment)
		//   Line2: P = p3 + dir2 * t2  (t2 in [0,1] for segment)
		//
		// Solve for t1 and t2 where lines intersect:
		//   If both t1 and t2 are in [0,1], segments intersect
		//   If either is outside [0,1], lines would intersect but segments don't
		//
		// MATH:
		//   p1 + dir1*t1 = p3 + dir2*t2
		//   Rearrange: dir1*t1 - dir2*t2 = p3 - p1
		//   Solve 2x2 system using cross products
		//
		// SPECIAL CASES:
		//   - Parallel lines: denominator = 0 (no intersection)
		//   - Coincident lines: parallel AND overlap (not handled here - returns no intersection)
		//   - Endpoint touching: t=0 or t=1 (still considered intersection)
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Line2D& line1, const Line2D& line2)
		{
			const Vector2D& p1 = line1.GetPt1();
			const Vector2D& p2 = line1.GetPt2();
			const Vector2D& p3 = line2.GetPt1();
			const Vector2D& p4 = line2.GetPt2();

			Vector2D dir1 = p2 - p1;
			Vector2D dir2 = p4 - p3;

			// Calculate denominator (2D cross product of directions)
			// This is zero if lines are parallel
			float denominator = dir1.x * dir2.y - dir1.y * dir2.x;

			// Check if lines are parallel (denominator near zero)
			if (Dia::Maths::Float::FAbs(denominator) < FLOAT_EPSILON)
			{
				// Lines are parallel or coincident
				// Could test for coincident overlap, but that's complex
				// For now, return no intersection (parallel lines don't intersect)
				return IntersectionClassify::kNoIntersection;
			}

			Vector2D p1ToP3 = p3 - p1;

			// Solve for t parameters using Cramer's rule
			// t1 = (p1ToP3 × dir2) / (dir1 × dir2)
			// t2 = (p1ToP3 × dir1) / (dir1 × dir2)
			float t1 = (p1ToP3.x * dir2.y - p1ToP3.y * dir2.x) / denominator;
			float t2 = (p1ToP3.x * dir1.y - p1ToP3.y * dir1.x) / denominator;

			// Check if intersection point is on both line segments
			// t in [0,1] means point is within the segment
			// t < 0 means before start, t > 1 means after end
			if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f)
			{
				// Intersection point is on both segments!
				// The actual point would be: p1 + dir1*t1 (or equivalently p3 + dir2*t2)

				// Note: Could distinguish between endpoint touches vs midpoint crossings
				// by checking if t1 or t2 is exactly 0 or 1, but for now we treat
				// all intersections the same way
				return IntersectionClassify::kPenatrating;
			}

			// Lines would intersect if extended, but segments don't reach intersection point
			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		// Line vs AARect intersection test (optimized to avoid object creation)
		//
		// ALGORITHM:
		//   1. Quick check: If both endpoints inside rect, line is contained
		//   2. Otherwise, test line against all 4 edges of rectangle
		//   3. If line intersects any edge, they're intersecting
		//
		// CASES:
		//   - Both endpoints inside: Rectangle contains line
		//   - Line crosses any edge: Penetrating intersection
		//   - No edge intersections: No intersection
		//
		// OPTIMIZATION: Tests edges directly without creating Line2D objects
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Line2D& line, const AARect2D& rect)
		{
			const Vector2D& p1 = line.GetPt1();
			const Vector2D& p2 = line.GetPt2();
			const Vector2D& min = rect.GetBottomLeft();
			const Vector2D& max = rect.GetTopRight();

			// Quick containment test: Are both endpoints inside rectangle?
			IntersectionClassify p1Inside = IsIntersecting(p1, rect);
			IntersectionClassify p2Inside = IsIntersecting(p2, rect);

			if (p1Inside.IsIntersecting() && p2Inside.IsIntersecting())
			{
				// Both endpoints are inside - entire line is contained by rectangle
				return IntersectionClassify::kBContainsA;
			}

			// Test line segment against each edge of the rectangle
			// We inline the edge tests to avoid creating Line2D objects
			// Rectangle edges: bottom, right, top, left

			// Define edge points (we'll test them directly)
			Vector2D edgePoints[8] = {
				Vector2D(min.x, min.y), Vector2D(max.x, min.y), // Bottom edge
				Vector2D(max.x, min.y), Vector2D(max.x, max.y), // Right edge
				Vector2D(max.x, max.y), Vector2D(min.x, max.y), // Top edge
				Vector2D(min.x, max.y), Vector2D(min.x, min.y)  // Left edge
			};

			// Test against each of the 4 edges
			for (int i = 0; i < 4; ++i)
			{
				const Vector2D& e1 = edgePoints[i * 2];
				const Vector2D& e2 = edgePoints[i * 2 + 1];

				// Inline line-line intersection test to avoid function call overhead
				Vector2D dir1 = p2 - p1;
				Vector2D dir2 = e2 - e1;

				float denominator = dir1.x * dir2.y - dir1.y * dir2.x;

				// Skip if parallel
				if (Dia::Maths::Float::FAbs(denominator) > FLOAT_EPSILON)
				{
					Vector2D p1ToE1 = e1 - p1;

					float t1 = (p1ToE1.x * dir2.y - p1ToE1.y * dir2.x) / denominator;
					float t2 = (p1ToE1.x * dir1.y - p1ToE1.y * dir1.x) / denominator;

					// Check if intersection is on both segments
					if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f)
					{
						// Line crosses this edge - they intersect!
						return IntersectionClassify::kPenatrating;
					}
				}
			}

			// Line doesn't cross any edges and isn't contained
			// This means line is completely outside rectangle
			return IntersectionClassify::kNoIntersection;
		}
	}
}