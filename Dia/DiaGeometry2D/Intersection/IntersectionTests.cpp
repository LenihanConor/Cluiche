#include "DiaGeometry2D/Intersection/IntersectionTests.h"

#include "DiaCore/Core/Assert.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/MathsDefines.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Capsule.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Arc.h"
#include "DiaGeometry2D/Shapes/OORect.h"
#include "DiaGeometry2D/Shapes/Triangle.h"

namespace Dia
{
	namespace Geometry2D
	{
		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Dia::Maths::Vector2D& point, const Circle& circle)
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
		IntersectionClassify IntersectionTests::IsIntersecting(const AARect& rect, const Circle& circle)
		{
			const float r  = circle.GetRadius();
			const Dia::Maths::Vector2D& c  = circle.GetCenter();
			const Dia::Maths::Vector2D& bl = rect.GetBottomLeft();
			const Dia::Maths::Vector2D& tr = rect.GetTopRight();

			// A (rect) contains B (circle): circle fits entirely inside the rect
			if (c.x - r >= bl.x && c.x + r <= tr.x &&
			    c.y - r >= bl.y && c.y + r <= tr.y)
			{
				return IntersectionClassify::kAContainsB;
			}

			// B (circle) contains A (rect): all four rect corners inside the circle
			const float sqR = r * r;
			auto sqDist = [&](float px, float py) {
				float dx = px - c.x, dy = py - c.y;
				return dx * dx + dy * dy;
			};
			if (sqDist(bl.x, bl.y) <= sqR &&
			    sqDist(tr.x, bl.y) <= sqR &&
			    sqDist(tr.x, tr.y) <= sqR &&
			    sqDist(bl.x, tr.y) <= sqR)
			{
				return IntersectionClassify::kBContainsA;
			}

			// Penetrating: closest point on rect to circle center is within radius
			Dia::Maths::Vector2D closest;
			rect.ClosestPointOnAARectTo(c, closest);
			float dx = closest.x - c.x, dy = closest.y - c.y;
			if (dx * dx + dy * dy <= sqR)
			{
				return IntersectionClassify::kPenatrating;
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Arc& arc, const Circle& circle)
		{
			// Test for A Containment B
			if (!circle.IsIntersecting(arc.GetFocal()).IsIntersecting())
			{
				Dia::Maths::Vector2D tangent1, tangent2, closePt, farPoint;
				circle.CalculateAxisPointToPoint(arc.GetFocal(), closePt, farPoint);
				circle.CalculateTangents(arc.GetFocal(), tangent1, tangent2);

				if (arc.IsIntersecting(tangent1).IsContainment() && arc.IsIntersecting(tangent2).IsContainment() && arc.IsIntersecting(farPoint).IsContainment())
				{
					return IntersectionClassify::kAContainsB;
				}
			}

			Dia::Maths::Vector2D extent1, extent2;
			extent1 = arc.CalculateExtentPositionClockwise();
			extent2 = arc.CalculateExtentPositionCounterClockwise();

			if (circle.IsIntersecting(arc.GetFocal()).IsContainment() &&
				circle.IsIntersecting(extent1).IsContainment() &&
				circle.IsIntersecting(extent2).IsContainment())
			{
				return IntersectionClassify::kBContainsA;
			}

			float sinOfArc = Dia::Maths::Sin(arc.GetAngle());
			float cosOfArc = Dia::Maths::Cos(arc.GetAngle());

			Dia::Maths::Vector2D U = arc.GetFocal() - (arc.GetAxis() * (circle.GetRadius() * sinOfArc));
			Dia::Maths::Vector2D D = circle.GetCenter() - U;
			float dsqr = D.Dot(D);
			float e = arc.GetAxis().Dot(D);
			if ((e > 0) && ((e * e) >= (dsqr * Dia::Maths::Square(cosOfArc))))
			{
				D = circle.GetCenter() - arc.GetFocal();
				dsqr = D.Dot(D);
				float e2 = -arc.GetAxis().Dot(D);

				if ((e2 > 0) && ((e2 * e2) >= (dsqr * Dia::Maths::Square(sinOfArc))) && (dsqr <= circle.GetSquaredRadius()))
				{
					return IntersectionClassify::kPenatrating;
				}
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Circle& circleA, const Circle& circleB)
		{
			const float rA = circleA.GetRadius();
			const float rB = circleB.GetRadius();
			const float rSum = rA + rB;
			const float sqRSum = rSum * rSum;

			const float sqDistFromCenters = circleA.GetCenter().SquareDistanceTo(circleB.GetCenter());

			// No intersection when centers are farther apart than sum of radii
			if (sqDistFromCenters > sqRSum)
			{
				return IntersectionClassify::kNoIntersection;
			}

			// Check containment: B contains A when dist + rA < rB → (rB - rA)² > dist²
			if (rB > rA)
			{
				const float rDiff = rB - rA;
				if (sqDistFromCenters < rDiff * rDiff)
				{
					return IntersectionClassify::kBContainsA;
				}
			}
			else if (rA > rB)
			{
				const float rDiff = rA - rB;
				if (sqDistFromCenters < rDiff * rDiff)
				{
					return IntersectionClassify::kAContainsB;
				}
			}

			return IntersectionClassify::kPenatrating;
		}

		//----------------------------------------------------------------------------------------------------
		int IntersectionTests::CalculateIntercepts(const Circle& circleA, const Circle& circleB, Dia::Maths::Vector2D& intercept1, Dia::Maths::Vector2D& intercept2)
		{
			IntersectionClassify intersectionClassify = circleA.IsIntersecting(circleB);

			if (intersectionClassify.GetClassification() != IntersectionClassify::kPenatrating)
			{
				return 0;
			}

			const Dia::Maths::Vector2D lineBetweenCentres = circleA.GetCenter() - circleB.GetCenter();
			const float D = lineBetweenCentres.Magnitude();

			const float R1 = circleA.GetRadius();
			const float R2 = circleB.GetRadius();

			const float y1 = circleA.GetCenter().Y();
			const float y2 = circleB.GetCenter().Y();
			const float x1 = circleA.GetCenter().X();
			const float x2 = circleB.GetCenter().X();

			const float A = Dia::Maths::SquareRoot((D + R1 + R2) * (D + R1 - R2) * (D - R1 + R2) * (R1 + R2 - D)) / 4.0f;

			const float DSquared  = Dia::Maths::Square(D);
			const float R1Squared = Dia::Maths::Square(R1);
			const float R2Squared = Dia::Maths::Square(R2);

			const float xBase  = ((x1 + x2) / 2.0f) - ((x1 - x2) * (R1Squared - R2Squared) / (2.0f * DSquared));
			const float xShift = 2 * (y1 - y2) * A / DSquared;

			const float yBase  = ((y1 + y2) / 2.0f) - ((y1 - y2) * (R1Squared - R2Squared) / (2.0f * DSquared));
			const float yShift = 2 * (x1 - x2) * A / DSquared;

			intercept1.X(xBase + xShift);
			intercept1.Y(yBase - yShift);

			intercept2.X(xBase - xShift);
			intercept2.Y(yBase + yShift);

			return 2;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Dia::Maths::Vector2D& point, const AARect& rect)
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
		IntersectionClassify IntersectionTests::IsIntersecting(const Line& line, const Circle& circle)
		{
			Dia::Maths::Vector2D closestPt;

			line.ClosestPointOnLineTo(circle, closestPt);

			IntersectionClassify circleIntersection = IsIntersecting(closestPt, circle);

			if (circleIntersection.IsNotIntersecting())
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
		IntersectionClassify IntersectionTests::IsIntersecting(const Capsule& capsule, const Circle& circle)
		{
			Line axis;

			capsule.Axis(axis);

			Dia::Maths::Vector2D ptOnAxis;
			axis.ClosestPointOnLineTo(circle.GetCenter(), ptOnAxis);

			float sqDistanceToAxis = circle.GetCenter().SquareDistanceTo(ptOnAxis);
			float sqCircleRadius  = circle.GetSquaredRadius();
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
		IntersectionClassify IntersectionTests::IsIntersecting(const OORect& rect, const Circle& circle)
		{
			int numEdgesContained = 0;
			for (int i = 0; i < OORect::kNumEdges; i++)
			{
				Line edge;

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

			if (numEdgesContained == OORect::kNumEdges)
			{
				return IntersectionClassify::kBContainsA;
			}

			DIA_ASSERT(numEdgesContained == 0, "No edges should be contained if we got this far");

			// Not penetrating and not contained - test one point
			return IsIntersecting(rect.GetPt(0), circle);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Triangle& tri, const Circle& circle)
		{
			int numEdgesContained = 0;
			for (int i = 0; i < Triangle::kNumEdges; i++)
			{
				Line edge;

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

			if (numEdgesContained == Triangle::kNumEdges)
			{
				return IntersectionClassify::kBContainsA;
			}

			DIA_ASSERT(numEdgesContained == 0, "No edges should be contained if we got this far");

			// Not penetrating and not contained - test one point
			return IsIntersecting(tri.GetPt(0), circle);
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Dia::Maths::Vector2D& point, const Line& line)
		{
			if (line.GetPt1() == point)
			{
				return IntersectionClassify::kPenatrating;
			}

			Dia::Maths::Vector2D AtoB = (line.GetPt2() - line.GetPt1());
			Dia::Maths::Vector2D AtoPoint = (point - line.GetPt1());

			DIA_ASSERT(line.GetPt2() != line.GetPt1(), "Can not be the same point");

			float sqDist = AtoB.SquareMagnitude();
			float sqDistToPoint = AtoPoint.SquareMagnitude();

			if (sqDistToPoint <= sqDist)
			{
				AtoB.Normalize();
				AtoPoint.Normalize();

				Dia::Maths::Angle betweenAngle;
				AtoB.GetAngleBetween(AtoPoint, betweenAngle);

				if (betweenAngle == Dia::Maths::Angle::Deg0)
				{
					return IntersectionClassify::kPenatrating;
				}
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Dia::Maths::Vector2D& point, const Arc& arc)
		{
			Dia::Maths::Vector2D toPoint = point - arc.GetFocal();
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
		IntersectionClassify IntersectionTests::IsIntersecting(const AARect& rect1, const AARect& rect2)
		{
			const Dia::Maths::Vector2D& min1 = rect1.GetBottomLeft();
			const Dia::Maths::Vector2D& max1 = rect1.GetTopRight();
			const Dia::Maths::Vector2D& min2 = rect2.GetBottomLeft();
			const Dia::Maths::Vector2D& max2 = rect2.GetTopRight();

			bool overlapX = (min1.x <= max2.x) && (max1.x >= min2.x);
			bool overlapY = (min1.y <= max2.y) && (max1.y >= min2.y);

			if (!overlapX || !overlapY)
			{
				return IntersectionClassify::kNoIntersection;
			}

			bool rect1ContainsRect2 = (min1.x <= min2.x && max1.x >= max2.x &&
			                           min1.y <= min2.y && max1.y >= max2.y);

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

			return IntersectionClassify::kPenatrating;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Line& line1, const Line& line2)
		{
			const Dia::Maths::Vector2D& p1 = line1.GetPt1();
			const Dia::Maths::Vector2D& p2 = line1.GetPt2();
			const Dia::Maths::Vector2D& p3 = line2.GetPt1();
			const Dia::Maths::Vector2D& p4 = line2.GetPt2();

			Dia::Maths::Vector2D dir1 = p2 - p1;
			Dia::Maths::Vector2D dir2 = p4 - p3;

			float denominator = dir1.x * dir2.y - dir1.y * dir2.x;

			if (Dia::Maths::Float::FAbs(denominator) < Dia::Maths::FLOAT_EPSILON)
			{
				return IntersectionClassify::kNoIntersection;
			}

			Dia::Maths::Vector2D p1ToP3 = p3 - p1;

			float t1 = (p1ToP3.x * dir2.y - p1ToP3.y * dir2.x) / denominator;
			float t2 = (p1ToP3.x * dir1.y - p1ToP3.y * dir1.x) / denominator;

			if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f)
			{
				return IntersectionClassify::kPenatrating;
			}

			return IntersectionClassify::kNoIntersection;
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify IntersectionTests::IsIntersecting(const Line& line, const AARect& rect)
		{
			const Dia::Maths::Vector2D& p1  = line.GetPt1();
			const Dia::Maths::Vector2D& p2  = line.GetPt2();
			const Dia::Maths::Vector2D& min = rect.GetBottomLeft();
			const Dia::Maths::Vector2D& max = rect.GetTopRight();

			IntersectionClassify p1Inside = IsIntersecting(p1, rect);
			IntersectionClassify p2Inside = IsIntersecting(p2, rect);

			if (p1Inside.IsIntersecting() && p2Inside.IsIntersecting())
			{
				return IntersectionClassify::kBContainsA;
			}

			// Test line against each rectangle edge
			Dia::Maths::Vector2D edgePoints[8] = {
				Dia::Maths::Vector2D(min.x, min.y), Dia::Maths::Vector2D(max.x, min.y), // Bottom
				Dia::Maths::Vector2D(max.x, min.y), Dia::Maths::Vector2D(max.x, max.y), // Right
				Dia::Maths::Vector2D(max.x, max.y), Dia::Maths::Vector2D(min.x, max.y), // Top
				Dia::Maths::Vector2D(min.x, max.y), Dia::Maths::Vector2D(min.x, min.y)  // Left
			};

			for (int i = 0; i < 4; ++i)
			{
				const Dia::Maths::Vector2D& e1 = edgePoints[i * 2];
				const Dia::Maths::Vector2D& e2 = edgePoints[i * 2 + 1];

				Dia::Maths::Vector2D dir1 = p2 - p1;
				Dia::Maths::Vector2D dir2 = e2 - e1;

				float denominator = dir1.x * dir2.y - dir1.y * dir2.x;

				if (Dia::Maths::Float::FAbs(denominator) > Dia::Maths::FLOAT_EPSILON)
				{
					Dia::Maths::Vector2D p1ToE1 = e1 - p1;

					float t1 = (p1ToE1.x * dir2.y - p1ToE1.y * dir2.x) / denominator;
					float t2 = (p1ToE1.x * dir1.y - p1ToE1.y * dir1.x) / denominator;

					if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f)
					{
						return IntersectionClassify::kPenatrating;
					}
				}
			}

			return IntersectionClassify::kNoIntersection;
		}
	}
}
