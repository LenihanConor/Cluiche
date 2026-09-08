#include "DiaGeometry2D/Random/Random.h"

#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaMaths/Core/Random.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		namespace Random
		{
			//-----------------------------------------------------------------------------
			// Generate random point inside circle with uniform distribution
			// IMPORTANT: We use sqrt(random) for radius to get uniform distribution!
			//
			// Why sqrt? Area increases with r^2, so:
			//   - Without sqrt: points cluster near center (linear radius sampling)
			//   - With sqrt: uniform distribution across the entire circle area
			//
			// Algorithm:
			//   1. Random angle (0 to 2π)
			//   2. Random radius using sqrt(random) for uniform density
			//   3. Convert polar coordinates (r, θ) to Cartesian (x, y)
			Dia::Maths::Vector2D RandomPointInCircle(const Circle& circle)
			{
				float angle = Dia::Maths::Random::RandomRange(0.0f, Dia::Maths::PI_2);
				float radius = Dia::Maths::SquareRoot(Dia::Maths::Random::RandomUnit()) * circle.GetRadius();

				// Convert polar to Cartesian: x = r*cos(θ), y = r*sin(θ)
				float cosAngle = Dia::Maths::Cos(angle);
				float sinAngle = Dia::Maths::Sin(angle);

				return circle.GetCenter() + Dia::Maths::Vector2D(cosAngle * radius, sinAngle * radius);
			}

			//-----------------------------------------------------------------------------
			Dia::Maths::Vector2D RandomPointOnCircle(const Circle& circle)
			{
				float angle = Dia::Maths::Random::RandomRange(0.0f, Dia::Maths::PI_2);
				float radius = circle.GetRadius();

				float cosAngle = Dia::Maths::Cos(angle);
				float sinAngle = Dia::Maths::Sin(angle);

				return circle.GetCenter() + Dia::Maths::Vector2D(cosAngle * radius, sinAngle * radius);
			}

			//-----------------------------------------------------------------------------
			Dia::Maths::Vector2D RandomPointInRect(const AARect& rect)
			{
				const Dia::Maths::Vector2D& bottomLeft = rect.GetBottomLeft();
				const Dia::Maths::Vector2D& topRight = rect.GetTopRight();

				float x = Dia::Maths::Random::RandomRange(bottomLeft.x, topRight.x);
				float y = Dia::Maths::Random::RandomRange(bottomLeft.y, topRight.y);

				return Dia::Maths::Vector2D(x, y);
			}
		}
	}
}
