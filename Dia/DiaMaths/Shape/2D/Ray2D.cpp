#include "DiaMaths/Shape/2D/Ray2D.h"
#include "DiaMaths/Shape/2D/Circle2D.h"
#include "DiaMaths/Shape/2D/AARect2D.h"
#include "DiaMaths/Shape/2D/Line2D.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"

namespace Dia
{
	namespace Maths
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
			//
			// GEOMETRY:
			//   Ray: P(t) = origin + direction * t
			//   Circle: |P - center| = radius
			//   Solve for t where ray intersects circle surface
			//-----------------------------------------------------------------------------
			bool CastCircle(const Ray2D& ray, const Circle2D& circle, RaycastHit2D& hitInfo)
			{
				return CastCircle(ray, circle, FLT_MAX, hitInfo);
			}

			bool CastCircle(const Ray2D& ray, const Circle2D& circle, float maxDistance, RaycastHit2D& hitInfo)
			{
				hitInfo.hit = false;

				Vector2D toCenter = circle.GetCenter() - ray.GetOrigin();
				float radius = circle.GetRadius();

				// Project circle center onto ray direction
				// This gives us the distance along ray to point closest to circle center
				float centerProjection = toCenter.Dot(ray.GetDirection());

				// If projection is negative, circle is behind ray origin
				if (centerProjection < 0.0f)
				{
					// Special case: ray starts inside circle
					// Check if origin is within circle radius
					if (toCenter.SquareMagnitude() <= radius * radius)
					{
						hitInfo.hit = true;
						hitInfo.point = ray.GetOrigin();
						hitInfo.normal = -ray.GetDirection(); // Normal points back along ray
						hitInfo.distance = 0.0f;
						return true;
					}
					return false; // Circle is behind ray and we're not inside it
				}

				// Find closest point on ray to circle center
				Vector2D closestPoint = ray.GetOrigin() + ray.GetDirection() * centerProjection;
				Vector2D toClosest = closestPoint - circle.GetCenter();
				float distSq = toClosest.SquareMagnitude();

				// Check if ray passes close enough to circle to intersect
				// If perpendicular distance > radius, ray misses circle entirely
				if (distSq > radius * radius)
				{
					return false;
				}

				// Ray intersects! Now find the exact intersection point.
				// Using Pythagorean theorem:
				//   radius² = distSq + halfChord²
				//   halfChord = √(radius² - distSq)
				//
				// Visual: Ray passes through circle, creating a "chord"
				//         halfChord is half that chord's length
				float halfChord = Dia::Maths::SquareRoot(radius * radius - distSq);

				// First intersection is at: centerProjection - halfChord
				// (Moving back from closest point by halfChord distance)
				float hitDistance = centerProjection - halfChord;

				// Check if hit is beyond maximum ray distance
				if (hitDistance > maxDistance)
				{
					return false;
				}

				// Handle case where ray starts inside circle
				if (hitDistance < 0.0f)
				{
					// Use far intersection instead (exiting circle)
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

				// Normal at circle surface points away from center
				hitInfo.normal = (hitInfo.point - circle.GetCenter()).AsNormal();

				return true;
			}

			//-----------------------------------------------------------------------------
			// Ray vs AARect (Axis-Aligned Bounding Box)
			//
			// ALGORITHM: "Slab Method"
			//   An AABB is the intersection of axis-aligned slabs.
			//   In 2D: intersection of vertical slab [xMin, xMax] and horizontal slab [yMin, yMax]
			//
			// PROCESS:
			//   1. Calculate t_enter and t_exit for each axis
			//   2. Final t_enter = max(t_enter_x, t_enter_y)
			//   3. Final t_exit = min(t_exit_x, t_exit_y)
			//   4. If t_enter > t_exit, ray misses box
			//   5. If t_enter < 0, ray starts inside box (use t=0)
			//
			// This is very efficient and commonly used in games/graphics!
			//-----------------------------------------------------------------------------
			bool CastAARect(const Ray2D& ray, const AARect2D& rect, RaycastHit2D& hitInfo)
			{
				return CastAARect(ray, rect, FLT_MAX, hitInfo);
			}

			bool CastAARect(const Ray2D& ray, const AARect2D& rect, float maxDistance, RaycastHit2D& hitInfo)
			{
				hitInfo.hit = false;

				const Vector2D& origin = ray.GetOrigin();
				const Vector2D& dir = ray.GetDirection();
				const Vector2D& min = rect.GetBottomLeft();
				const Vector2D& max = rect.GetTopRight();

				// Initialize with ray's valid range
				float tMin = 0.0f;		// Can't hit before ray starts
				float tMax = maxDistance; // Can't hit beyond max distance

				Vector2D hitNormal = Vector2D::Zero();

				// Test X axis slab
				// Calculate where ray enters and exits the vertical slab [min.x, max.x]
				if (Dia::Maths::Float::FAbs(dir.x) > FLOAT_EPSILON)
				{
					// Ray not parallel to X axis - calculate intersections
					// t = (x - origin.x) / dir.x tells us when ray crosses x-plane
					float t1 = (min.x - origin.x) / dir.x; // Time to hit left edge
					float t2 = (max.x - origin.x) / dir.x; // Time to hit right edge

					// If ray points left, t1 > t2, so swap them
					if (t1 > t2)
					{
						Swap(t1, t2);
					}

					// Update overall entry time and remember which face we hit
					if (t1 > tMin)
					{
						tMin = t1;
						// Normal points opposite to ray direction on this axis
						hitNormal = (dir.x < 0.0f) ? Vector2D(1.0f, 0.0f) : Vector2D(-1.0f, 0.0f);
					}

					// Update overall exit time
					tMax = Min(tMax, t2);

					// If we enter after we exit, ray misses box
					if (tMin > tMax)
					{
						return false;
					}
				}
				else
				{
					// Ray parallel to X axis - must be within slab or miss entirely
					if (origin.x < min.x || origin.x > max.x)
					{
						return false; // Parallel and outside - can never hit
					}
				}

				// Check Y axis
				if (Dia::Maths::Float::FAbs(dir.y) > FLOAT_EPSILON)
				{
					float t1 = (min.y - origin.y) / dir.y;
					float t2 = (max.y - origin.y) / dir.y;

					if (t1 > t2)
					{
						Swap(t1, t2);
					}

					if (t1 > tMin)
					{
						tMin = t1;
						hitNormal = (dir.y < 0.0f) ? Vector2D(0.0f, 1.0f) : Vector2D(0.0f, -1.0f);
					}

					tMax = Min(tMax, t2);

					if (tMin > tMax)
					{
						return false;
					}
				}
				else
				{
					// Ray parallel to Y axis
					if (origin.y < min.y || origin.y > max.y)
					{
						return false;
					}
				}

				// After testing both axes, tMin is where ray enters box
				// Check if entry point is valid (in front of ray and within max distance)
				if (tMin >= 0.0f && tMin <= maxDistance)
				{
					hitInfo.hit = true;
					hitInfo.distance = tMin;
					hitInfo.point = origin + dir * tMin;
					hitInfo.normal = hitNormal; // Normal of face we entered through
					return true;
				}

				return false;
			}

			//-----------------------------------------------------------------------------
			// Ray vs Line Segment
			//-----------------------------------------------------------------------------
			bool CastLine(const Ray2D& ray, const Line2D& line, RaycastHit2D& hitInfo)
			{
				return CastLine(ray, line, FLT_MAX, hitInfo);
			}

			bool CastLine(const Ray2D& ray, const Line2D& line, float maxDistance, RaycastHit2D& hitInfo)
			{
				hitInfo.hit = false;

				// Get line segment endpoints
				Vector2D p1 = line.GetPt1();
				Vector2D p2 = line.GetPt2();

				Vector2D lineDir = p2 - p1;
				Vector2D rayDir = ray.GetDirection();
				Vector2D rayOrigin = ray.GetOrigin();

				// Calculate denominator for parametric equations using 2D cross product
				// The formula uses cross products to solve the system:
				//   rayOrigin + rayDir*t = p1 + lineDir*s
				// Note: The double negation below simplifies to lineDir × rayDir cross product
				float denominator = lineDir.x * (-rayDir.y) - lineDir.y * (-rayDir.x);

				// Check if ray and line are parallel
				if (Dia::Maths::Float::FAbs(denominator) < FLOAT_EPSILON)
				{
					return false;
				}

				Vector2D originToLine = p1 - rayOrigin;

				// Calculate t parameter for line segment (0 to 1)
				// Uses cross product: originToLine × (-rayDir)
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
				Vector2D lineNormal = lineDir.AsRotated90DegreeCounterClockwise().AsNormal();
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
