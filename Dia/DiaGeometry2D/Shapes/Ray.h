#ifndef DIA_GEOMETRY2D_RAY_H
#define DIA_GEOMETRY2D_RAY_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;
		class AARect;
		class Line;

		//==============================================================================
		// CLASS Ray
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
		// NOTE: Direction is automatically normalized on construction/setting
		//==============================================================================
		class Ray
		{
		public:
			// Default constructor - ray at origin pointing along +X axis
			Ray();

			// Construct ray with origin and direction (direction will be normalized)
			Ray(const Dia::Maths::Vector2D& origin, const Dia::Maths::Vector2D& direction);

			// Copy constructor
			Ray(const Ray& other);

			// Assignment operator
			Ray& operator=(const Ray& other);

			// Get ray origin (starting point)
			const Dia::Maths::Vector2D& GetOrigin() const;

			// Get ray direction (always normalized)
			const Dia::Maths::Vector2D& GetDirection() const;

			// Set ray origin
			void SetOrigin(const Dia::Maths::Vector2D& origin);

			// Set ray direction (will be normalized automatically)
			void SetDirection(const Dia::Maths::Vector2D& direction);

			// Get point along ray at given distance from origin
			// Returns: origin + direction * distance
			Dia::Maths::Vector2D GetPoint(float distance) const;

			// Get closest point on ray to given point
			// Returns point on ray (t >= 0) nearest to input point
			Dia::Maths::Vector2D ClosestPoint(const Dia::Maths::Vector2D& point) const;

			// Get perpendicular distance from ray to point
			float DistanceToPoint(const Dia::Maths::Vector2D& point) const;

		private:
			Dia::Maths::Vector2D mOrigin;
			Dia::Maths::Vector2D mDirection;	// Must be normalized
		};

		//==============================================================================
		// STRUCT RaycastHit
		//==============================================================================
		// Contains information about a raycast hit
		//==============================================================================
		struct RaycastHit
		{
			bool				hit;		// True if ray hit something
			Dia::Maths::Vector2D point;		// Hit point in world space
			Dia::Maths::Vector2D normal;	// Surface normal at hit point (normalized)
			float				distance;	// Distance from ray origin to hit point

			RaycastHit()
				: hit(false)
				, point(Dia::Maths::Vector2D::Zero())
				, normal(Dia::Maths::Vector2D::Zero())
				, distance(0.0f)
			{
			}
		};

		//==============================================================================
		// Raycast Functions
		//
		// Cast rays against shapes to detect intersections.
		// All functions return true if ray hits shape, false otherwise.
		// If hit, RaycastHit is filled with intersection details.
		//
		// VARIANTS:
		//   - Without maxDistance: Ray extends infinitely
		//   - With maxDistance: Ray stops after traveling maxDistance units
		//==============================================================================
		namespace Raycast
		{
			// Cast ray against circle (infinite distance)
			bool CastCircle(const Ray& ray, const Circle& circle, RaycastHit& hitInfo);

			// Cast ray against axis-aligned rectangle (infinite distance)
			bool CastAARect(const Ray& ray, const AARect& rect, RaycastHit& hitInfo);

			// Cast ray against line segment (infinite distance)
			bool CastLine(const Ray& ray, const Line& line, RaycastHit& hitInfo);

			// Cast ray against circle with maximum distance
			bool CastCircle(const Ray& ray, const Circle& circle, float maxDistance, RaycastHit& hitInfo);

			// Cast ray against rectangle with maximum distance
			bool CastAARect(const Ray& ray, const AARect& rect, float maxDistance, RaycastHit& hitInfo);

			// Cast ray against line segment with maximum distance
			bool CastLine(const Ray& ray, const Line& line, float maxDistance, RaycastHit& hitInfo);
		}
	}
}

#include "DiaGeometry2D/Shapes/Ray.inl"

#endif // DIA_GEOMETRY2D_RAY_H
