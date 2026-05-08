#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/MathsDefines.h"

namespace Dia
{
	namespace Geometry2D
	{
		namespace Raycast
		{
			//-----------------------------------------------------------------------------
			// Ray vs Circle Intersection
			//
			// ALGORITHM:
			//   1. Project circle center onto ray
			//   2. Find closest point on ray to circle center
			//   3. Check if that point is within circle radius
			//   4. Use Pythagorean theorem to find actual intersection point
			//-----------------------------------------------------------------------------
			bool CastCircle(const Ray& ray, const Circle& circle, RaycastHit& hitInfo)
			{
				return CastCircle(ray, circle, FLT_MAX, hitInfo);
			}

			bool CastCircle(const Ray& ray, const Circle& circle, float maxDistance, RaycastHit& hitInfo)
			{
				hitInfo.hit = false;

				Dia::Maths::Vector2D toCenter = circle.GetCenter() - ray.GetOrigin();
				float radius = circle.GetRadius();

				// Project circle center onto ray direction
				float centerProjection = toCenter.Dot(ray.GetDirection());

				// If projection is negative, circle is behind ray origin
				if (centerProjection < 0.0f)
				{
					// Special case: ray starts inside circle
					if (toCenter.SquareMagnitude() <= radius * radius)
					{
						hitInfo.hit = true;
						hitInfo.point = ray.GetOrigin();
						hitInfo.normal = -ray.GetDirection();
						hitInfo.distance = 0.0f;
						return true;
					}
					return false;
				}

				// Find closest point on ray to circle center
				Dia::Maths::Vector2D closestPoint = ray.GetOrigin() + ray.GetDirection() * centerProjection;
				Dia::Maths::Vector2D toClosest = closestPoint - circle.GetCenter();
				float distSq = toClosest.SquareMagnitude();

				// Check if ray passes close enough to circle to intersect
				if (distSq > radius * radius)
				{
					return false;
				}

				// Ray intersects - find the exact intersection point using Pythagorean theorem
				float halfChord = Dia::Maths::SquareRoot(radius * radius - distSq);

				float hitDistance = centerProjection - halfChord;

				// Check if hit is beyond maximum ray distance
				if (hitDistance > maxDistance)
				{
					return false;
				}

				// Handle case where ray starts inside circle
				if (hitDistance < 0.0f)
				{
					hitDistance = centerProjection + halfChord;
					if (hitDistance < 0.0f || hitDistance > maxDistance)
					{
						return false;
					}
				}

				// Fill hit information
				hitInfo.hit = true;
				hitInfo.distance = hitDistance;
				hitInfo.point = ray.GetOrigin() + ray.GetDirection() * hitDistance;
				hitInfo.normal = (hitInfo.point - circle.GetCenter()).AsNormal();

				return true;
			}

			//-----------------------------------------------------------------------------
			// Ray vs AARect (Axis-Aligned Bounding Box) - Slab Method
			//-----------------------------------------------------------------------------
			bool CastAARect(const Ray& ray, const AARect& rect, RaycastHit& hitInfo)
			{
				return CastAARect(ray, rect, FLT_MAX, hitInfo);
			}

			bool CastAARect(const Ray& ray, const AARect& rect, float maxDistance, RaycastHit& hitInfo)
			{
				hitInfo.hit = false;

				const Dia::Maths::Vector2D& origin = ray.GetOrigin();
				const Dia::Maths::Vector2D& dir = ray.GetDirection();
				const Dia::Maths::Vector2D& min = rect.GetBottomLeft();
				const Dia::Maths::Vector2D& max = rect.GetTopRight();

				float tMin = 0.0f;
				float tMax = maxDistance;

				Dia::Maths::Vector2D hitNormal = Dia::Maths::Vector2D::Zero();

				// Test X axis slab
				if (Dia::Maths::Float::FAbs(dir.x) > Dia::Maths::FLOAT_EPSILON)
				{
					float t1 = (min.x - origin.x) / dir.x;
					float t2 = (max.x - origin.x) / dir.x;

					if (t1 > t2)
					{
						Dia::Maths::Swap(t1, t2);
					}

					if (t1 > tMin)
					{
						tMin = t1;
						hitNormal = (dir.x < 0.0f) ? Dia::Maths::Vector2D(1.0f, 0.0f) : Dia::Maths::Vector2D(-1.0f, 0.0f);
					}

					tMax = Dia::Maths::Min(tMax, t2);

					if (tMin > tMax)
					{
						return false;
					}
				}
				else
				{
					if (origin.x < min.x || origin.x > max.x)
					{
						return false;
					}
				}

				// Test Y axis slab
				if (Dia::Maths::Float::FAbs(dir.y) > Dia::Maths::FLOAT_EPSILON)
				{
					float t1 = (min.y - origin.y) / dir.y;
					float t2 = (max.y - origin.y) / dir.y;

					if (t1 > t2)
					{
						Dia::Maths::Swap(t1, t2);
					}

					if (t1 > tMin)
					{
						tMin = t1;
						hitNormal = (dir.y < 0.0f) ? Dia::Maths::Vector2D(0.0f, 1.0f) : Dia::Maths::Vector2D(0.0f, -1.0f);
					}

					tMax = Dia::Maths::Min(tMax, t2);

					if (tMin > tMax)
					{
						return false;
					}
				}
				else
				{
					if (origin.y < min.y || origin.y > max.y)
					{
						return false;
					}
				}

				if (tMin >= 0.0f && tMin <= maxDistance)
				{
					hitInfo.hit = true;
					hitInfo.distance = tMin;
					hitInfo.point = origin + dir * tMin;
					hitInfo.normal = hitNormal;
					return true;
				}

				return false;
			}

			//-----------------------------------------------------------------------------
			// Ray vs Line Segment
			//-----------------------------------------------------------------------------
			bool CastLine(const Ray& ray, const Line& line, RaycastHit& hitInfo)
			{
				return CastLine(ray, line, FLT_MAX, hitInfo);
			}

			bool CastLine(const Ray& ray, const Line& line, float maxDistance, RaycastHit& hitInfo)
			{
				hitInfo.hit = false;

				Dia::Maths::Vector2D p1 = line.GetPt1();
				Dia::Maths::Vector2D p2 = line.GetPt2();

				Dia::Maths::Vector2D lineDir = p2 - p1;
				Dia::Maths::Vector2D rayDir = ray.GetDirection();
				Dia::Maths::Vector2D rayOrigin = ray.GetOrigin();

				// Calculate denominator for parametric equations using 2D cross product
				float denominator = lineDir.x * (-rayDir.y) - lineDir.y * (-rayDir.x);

				// Check if ray and line are parallel
				if (Dia::Maths::Float::FAbs(denominator) < Dia::Maths::FLOAT_EPSILON)
				{
					return false;
				}

				Dia::Maths::Vector2D originToLine = p1 - rayOrigin;

				// Calculate t parameter for line segment (0 to 1)
				float tLine = (originToLine.x * (-rayDir.y) - originToLine.y * (-rayDir.x)) / denominator;

				// Check if intersection point is on line segment
				if (tLine < 0.0f || tLine > 1.0f)
				{
					return false;
				}

				// Calculate t parameter for ray (must be >= 0)
				float tRay = (originToLine.x * lineDir.y - originToLine.y * lineDir.x) / denominator;

				// Check if intersection is in front of ray and within max distance
				if (tRay < 0.0f || tRay > maxDistance)
				{
					return false;
				}

				// Hit!
				hitInfo.hit = true;
				hitInfo.distance = tRay;
				hitInfo.point = rayOrigin + rayDir * tRay;

				// Calculate normal (perpendicular to line, facing ray)
				Dia::Maths::Vector2D lineNormal = lineDir.AsRotated90DegreeCounterClockwise().AsNormal();
				if (lineNormal.Dot(rayDir) > 0.0f)
				{
					lineNormal = -lineNormal;
				}
				hitInfo.normal = lineNormal;

				return true;
			}
		}
	}
}
