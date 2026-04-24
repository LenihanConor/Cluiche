#include "DiaGeometry2D/Shapes/Sector.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Sector::Sector()
			: mCenter(0.0f, 0.0f)
			, mRadius(0.0f)
			, mAxis(1.0f, 0.0f)
			, mHalfAngle(0.0f)
		{}

		//-----------------------------------------------------------------------------
		Sector::Sector(const Dia::Maths::Vector2D& center, float radius, const Dia::Maths::Vector2D& axis, const Dia::Maths::Angle& halfAngle)
			: mCenter(center)
			, mRadius(radius)
			, mAxis(axis)
			, mHalfAngle(halfAngle)
		{}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& Sector::GetCenter() const
		{
			return mCenter;
		}

		//-----------------------------------------------------------------------------
		float Sector::GetRadius() const
		{
			return mRadius;
		}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& Sector::GetAxis() const
		{
			return mAxis;
		}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Angle& Sector::GetHalfAngle() const
		{
			return mHalfAngle;
		}
	}
}
