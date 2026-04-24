#ifndef DIA_GEOMETRY2D_SECTOR_H
#define DIA_GEOMETRY2D_SECTOR_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia
{
	namespace Geometry2D
	{
		// Sector - a pie-slice shape: circle sector defined by center, radius, axis direction, and half-angle.
		class Sector
		{
		public:
			Sector();
			Sector(const Dia::Maths::Vector2D& center, float radius, const Dia::Maths::Vector2D& axis, const Dia::Maths::Angle& halfAngle);

			const Dia::Maths::Vector2D& GetCenter() const;
			float GetRadius() const;
			const Dia::Maths::Vector2D& GetAxis() const;
			const Dia::Maths::Angle& GetHalfAngle() const;

		private:
			Dia::Maths::Vector2D mCenter;
			float mRadius;
			Dia::Maths::Vector2D mAxis;
			Dia::Maths::Angle mHalfAngle;
		};
	}
}

#endif // DIA_GEOMETRY2D_SECTOR_H
