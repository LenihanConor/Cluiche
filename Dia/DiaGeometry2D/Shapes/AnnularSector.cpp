#include "DiaGeometry2D/Shapes/AnnularSector.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		AnnularSector::AnnularSector()
			: mCenter(0.0f, 0.0f)
			, mInnerRadius(0.0f)
			, mOuterRadius(0.0f)
			, mAxis(1.0f, 0.0f)
			, mHalfAngle(0.0f)
		{}

		//-----------------------------------------------------------------------------
		AnnularSector::AnnularSector(const Dia::Maths::Vector2D& center, float innerRadius, float outerRadius, const Dia::Maths::Vector2D& axis, const Dia::Maths::Angle& halfAngle)
			: mCenter(center)
			, mInnerRadius(innerRadius)
			, mOuterRadius(outerRadius)
			, mAxis(axis)
			, mHalfAngle(halfAngle)
		{}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& AnnularSector::GetCenter() const
		{
			return mCenter;
		}

		//-----------------------------------------------------------------------------
		float AnnularSector::GetInnerRadius() const
		{
			return mInnerRadius;
		}

		//-----------------------------------------------------------------------------
		float AnnularSector::GetOuterRadius() const
		{
			return mOuterRadius;
		}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& AnnularSector::GetAxis() const
		{
			return mAxis;
		}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Angle& AnnularSector::GetHalfAngle() const
		{
			return mHalfAngle;
		}
	}
}
