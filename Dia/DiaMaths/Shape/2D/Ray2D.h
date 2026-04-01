#pragma once

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Maths
	{
		class Circle2D;
		class AARect2D;
		class Line2D;

		//==============================================================================
		// CLASS Ray2D
		//==============================================================================
		// Represents a ray in 2D space: a half-infinite line starting at origin
		// and extending infinitely in direction.
		//
		// DEFINITION:
		//   Ray(t) = origin + direction * t, where t >= 0
		//   - t = 0: at origin
		//   - t > 0: along direction
		//   - t < 0: NOT on ray (that would be the opposite direction)
		//
		// COMMON USES:
		//   - Mouse picking: Cast ray from mouse through world, find what it hits
		//   - Line-of-sight: Can enemy see player? Cast ray, check for obstacles
		//   - Projectiles: Does bullet hit target? Cast ray along path
		//   - Collision detection: Fast broad-phase checks
		//
		// NOTE: Direction is automatically normalized on construction/setting
		//==============================================================================
		class Ray2D
		{
		public:
			// Default constructor - ray at origin pointing along +X axis
			Ray2D();

			// Construct ray with origin and direction (direction will be normalized)
			Ray2D(const Vector2D& origin, const Vector2D& direction);

			// Copy constructor
			Ray2D(const Ray2D& other);

			// Assignment operator
			Ray2D& operator=(const Ray2D& other);

			// Get ray origin (starting point)
			const Vector2D&		GetOrigin() const;

			// Get ray direction (always normalized)
			const Vector2D&		GetDirection() const;

			// Set ray origin
			void				SetOrigin(const Vector2D& origin);

			// Set ray direction (will be normalized automatically)
			void				SetDirection(const Vector2D& direction);

			// Get point along ray at given distance from origin
			// Returns: origin + direction * distance
			// Use for: Visualizing ray, stepping along ray
			Vector2D			GetPoint(float distance) const;

			// Get closest point on ray to given point
			// Returns point on ray (t >= 0) nearest to input point
			// Use for: Distance queries, proximity checks
			Vector2D			ClosestPoint(const Vector2D& point) const;

			// Get perpendicular distance from ray to point
			// Returns: Shortest distance from point to ray line
			// Use for: Quick "how close does ray pass to this point?" checks
			float				DistanceToPoint(const Vector2D& point) const;

		private:
			Vector2D			mOrigin;
			Vector2D			mDirection;		// Must be normalized
		};

		//==============================================================================
		// STRUCT RaycastHit2D
		//==============================================================================
		// Contains information about a raycast hit
		//
		// FIELDS:
		//   hit      - True if ray hit the shape, false otherwise
		//   point    - Exact point where ray intersected surface (world coordinates)
		//   normal   - Surface normal at hit point (points away from surface)
		//   distance - Distance from ray origin to hit point
		//
		// USAGE:
		//   RaycastHit2D hit;
		//   if (Raycast::CastCircle(ray, circle, hit))
		//   {
		//       // hit.point is where ray hit circle
		//       // hit.normal is perpendicular to circle at that point
		//       // hit.distance is how far along ray the hit occurred
		//   }
		//==============================================================================
		struct RaycastHit2D
		{
			bool				hit;			// True if ray hit something
			Vector2D			point;			// Hit point in world space
			Vector2D			normal;			// Surface normal at hit point (normalized)
			float				distance;		// Distance from ray origin to hit point

			RaycastHit2D()
				: hit(false)
				, point(Vector2D::Zero())
				, normal(Vector2D::Zero())
				, distance(0.0f)
			{
			}
		};

		//==============================================================================
		// Raycast Functions
		//
		// Cast rays against shapes to detect intersections.
		// All functions return true if ray hits shape, false otherwise.
		// If hit, RaycastHit2D is filled with intersection details.
		//
		// VARIANTS:
		//   - Without maxDistance: Ray extends infinitely
		//   - With maxDistance: Ray stops after traveling maxDistance units
		//
		// PERFORMANCE TIP:
		//   Use maxDistance to avoid detecting distant objects you don't care about.
		//   Example: Visibility checks might only need 10-20 units range.
		//==============================================================================

		namespace Raycast
		{
			// Cast ray against circle (infinite distance)
			// Returns: true if ray intersects circle, false otherwise
			// Fills: hitInfo with intersection point, normal, and distance
			bool CastCircle(const Ray2D& ray, const Circle2D& circle, RaycastHit2D& hitInfo);

			// Cast ray against axis-aligned rectangle (infinite distance)
			// Returns: true if ray intersects rectangle, false otherwise
			// Fills: hitInfo with intersection point, normal, and distance
			bool CastAARect(const Ray2D& ray, const AARect2D& rect, RaycastHit2D& hitInfo);

			// Cast ray against line segment (infinite distance)
			// Returns: true if ray intersects line segment, false otherwise
			// Fills: hitInfo with intersection point, normal, and distance
			bool CastLine(const Ray2D& ray, const Line2D& line, RaycastHit2D& hitInfo);

			// Cast ray against circle with maximum distance
			// Parameters:
			//   maxDistance - Ray stops after this distance (useful for optimization)
			bool CastCircle(const Ray2D& ray, const Circle2D& circle, float maxDistance, RaycastHit2D& hitInfo);

			// Cast ray against rectangle with maximum distance
			bool CastAARect(const Ray2D& ray, const AARect2D& rect, float maxDistance, RaycastHit2D& hitInfo);

			// Cast ray against line segment with maximum distance
			bool CastLine(const Ray2D& ray, const Line2D& line, float maxDistance, RaycastHit2D& hitInfo);
		}
	}
}

#include "DiaMaths/Shape/2D/Ray2D.inl"
