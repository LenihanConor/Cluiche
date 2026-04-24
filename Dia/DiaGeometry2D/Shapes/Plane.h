#ifndef DIA_GEOMETRY2D_PLANE_H
#define DIA_GEOMETRY2D_PLANE_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		// Plane - a 2D half-plane defined by a normal and distance from origin.
		// Equation: normal.Dot(point) = distance (normal is unit length)
		class Plane
		{
		public:
			Plane();
			Plane(const Dia::Maths::Vector2D& normal, float distance);

			// Construct from two points on the plane (line defines the plane boundary)
			static Plane FromPoints(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b);

			const Dia::Maths::Vector2D& GetNormal() const;
			float GetDistance() const;
			float SignedDistance(const Dia::Maths::Vector2D& point) const;
			bool IsOnPositiveSide(const Dia::Maths::Vector2D& point) const;

		private:
			Dia::Maths::Vector2D mNormal;
			float mDistance;
		};
	}
}

#endif // DIA_GEOMETRY2D_PLANE_H
