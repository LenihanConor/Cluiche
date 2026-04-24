#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		// Ray Implementation
		//-----------------------------------------------------------------------------

		inline Ray::Ray()
			: mOrigin(Dia::Maths::Vector2D::Zero())
			, mDirection(Dia::Maths::Vector2D::XAxis())
		{
		}

		inline Ray::Ray(const Dia::Maths::Vector2D& origin, const Dia::Maths::Vector2D& direction)
			: mOrigin(origin)
			, mDirection(direction)
		{
			// Ensure direction is normalized
			if (!mDirection.IsNormal())
			{
				mDirection.Normalize();
			}
		}

		inline Ray::Ray(const Ray& other)
			: mOrigin(other.mOrigin)
			, mDirection(other.mDirection)
		{
		}

		inline Ray& Ray::operator=(const Ray& other)
		{
			if (this != &other)
			{
				mOrigin = other.mOrigin;
				mDirection = other.mDirection;
			}
			return *this;
		}

		inline const Dia::Maths::Vector2D& Ray::GetOrigin() const
		{
			return mOrigin;
		}

		inline const Dia::Maths::Vector2D& Ray::GetDirection() const
		{
			return mDirection;
		}

		inline void Ray::SetOrigin(const Dia::Maths::Vector2D& origin)
		{
			mOrigin = origin;
		}

		inline void Ray::SetDirection(const Dia::Maths::Vector2D& direction)
		{
			mDirection = direction;
			if (!mDirection.IsNormal())
			{
				mDirection.Normalize();
			}
		}

		inline Dia::Maths::Vector2D Ray::GetPoint(float distance) const
		{
			return mOrigin + mDirection * distance;
		}

		inline Dia::Maths::Vector2D Ray::ClosestPoint(const Dia::Maths::Vector2D& point) const
		{
			Dia::Maths::Vector2D toPoint = point - mOrigin;
			float distance = toPoint.Dot(mDirection);

			// Clamp to ray (not line)
			if (distance < 0.0f)
			{
				return mOrigin;
			}

			return mOrigin + mDirection * distance;
		}

		inline float Ray::DistanceToPoint(const Dia::Maths::Vector2D& point) const
		{
			Dia::Maths::Vector2D closest = ClosestPoint(point);
			return point.DistanceTo(closest);
		}
	}
}
