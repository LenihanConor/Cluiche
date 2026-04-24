#include "DiaGeometry2D/Shapes/Plane.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Plane::Plane()
			: mNormal(0.0f, 1.0f)
			, mDistance(0.0f)
		{}

		//-----------------------------------------------------------------------------
		Plane::Plane(const Dia::Maths::Vector2D& normal, float distance)
			: mNormal(normal)
			, mDistance(distance)
		{}

		//-----------------------------------------------------------------------------
		Plane Plane::FromPoints(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b)
		{
			// The plane normal is perpendicular to the line direction (rotated 90 degrees CCW)
			Dia::Maths::Vector2D lineDir = (b - a).AsNormal();
			Dia::Maths::Vector2D normal = lineDir.AsRotated90DegreeCounterClockwise();

			// Distance is the dot product of the normal with any point on the plane
			float distance = normal.Dot(a);

			return Plane(normal, distance);
		}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& Plane::GetNormal() const
		{
			return mNormal;
		}

		//-----------------------------------------------------------------------------
		float Plane::GetDistance() const
		{
			return mDistance;
		}

		//-----------------------------------------------------------------------------
		float Plane::SignedDistance(const Dia::Maths::Vector2D& point) const
		{
			return mNormal.Dot(point) - mDistance;
		}

		//-----------------------------------------------------------------------------
		bool Plane::IsOnPositiveSide(const Dia::Maths::Vector2D& point) const
		{
			return SignedDistance(point) >= 0.0f;
		}
	}
}
