#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
	}

	namespace Geometry2D
	{
		class Circle;
		class AARect;

		namespace Random
		{
			// Returns random point inside a circle with uniform distribution
			// Parameters:
			//   circle - The circle to pick a point within
			// Note: Uses sqrt for uniform distribution (naive random radius creates center bias)
			// Usage: Spawning particles, random spawn points within an area
			Dia::Maths::Vector2D RandomPointInCircle(const Circle& circle);

			// Returns random point on the edge (circumference) of a circle
			// Parameters:
			//   circle - The circle to pick a point on
			// Usage: Spawning enemies around player, placing objects in a ring
			Dia::Maths::Vector2D RandomPointOnCircle(const Circle& circle);

			// Returns random point inside an axis-aligned rectangle
			// Parameters:
			//   rect - The rectangle to pick a point within
			// Usage: Random spawn positions, scatter placement
			Dia::Maths::Vector2D RandomPointInRect(const AARect& rect);
		}
	}
}
