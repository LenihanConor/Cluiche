#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Ray2D Implementation
		//-----------------------------------------------------------------------------

		inline Ray2D::Ray2D()
			: mOrigin(Vector2D::Zero())
			, mDirection(Vector2D::XAxis())
		{
		}

		inline Ray2D::Ray2D(const Vector2D& origin, const Vector2D& direction)
			: mOrigin(origin)
			, mDirection(direction)
		{
			// Ensure direction is normalized
			if (!mDirection.IsNormal())
			{
				mDirection.Normalize();
			}
		}

		inline Ray2D::Ray2D(const Ray2D& other)
			: mOrigin(other.mOrigin)
			, mDirection(other.mDirection)
		{
		}

		inline Ray2D& Ray2D::operator=(const Ray2D& other)
		{
			if (this != &other)
			{
				mOrigin = other.mOrigin;
				mDirection = other.mDirection;
			}
			return *this;
		}

		inline const Vector2D& Ray2D::GetOrigin() const
		{
			return mOrigin;
		}

		inline const Vector2D& Ray2D::GetDirection() const
		{
			return mDirection;
		}

		inline void Ray2D::SetOrigin(const Vector2D& origin)
		{
			mOrigin = origin;
		}

		inline void Ray2D::SetDirection(const Vector2D& direction)
		{
			mDirection = direction;
			// Always normalize direction to ensure ray math works correctly
			// Check avoids redundant normalization if already normalized
			if (!mDirection.IsNormal())
			{
				mDirection.Normalize();
			}
		}

		inline Vector2D Ray2D::GetPoint(float distance) const
		{
			return mOrigin + mDirection * distance;
		}

		inline Vector2D Ray2D::ClosestPoint(const Vector2D& point) const
		{
			Vector2D toPoint = point - mOrigin;
			float distance = toPoint.Dot(mDirection);

			// Clamp to ray (not line)
			if (distance < 0.0f)
			{
				return mOrigin;
			}

			return mOrigin + mDirection * distance;
		}

		inline float Ray2D::DistanceToPoint(const Vector2D& point) const
		{
			Vector2D closest = ClosestPoint(point);
			return point.DistanceTo(closest);
		}
	}
}
