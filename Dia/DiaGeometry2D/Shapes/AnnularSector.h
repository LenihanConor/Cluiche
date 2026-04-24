#ifndef DIA_GEOMETRY2D_ANNULARSECTOR_H
#define DIA_GEOMETRY2D_ANNULARSECTOR_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia
{
	namespace Geometry2D
	{
		// AnnularSector - a donut-slice shape: sector with inner and outer radii.
		class AnnularSector
		{
		public:
			AnnularSector();
			AnnularSector(const Dia::Maths::Vector2D& center, float innerRadius, float outerRadius, const Dia::Maths::Vector2D& axis, const Dia::Maths::Angle& halfAngle);

			const Dia::Maths::Vector2D& GetCenter() const;
			float GetInnerRadius() const;
			float GetOuterRadius() const;
			const Dia::Maths::Vector2D& GetAxis() const;
			const Dia::Maths::Angle& GetHalfAngle() const;

		private:
			Dia::Maths::Vector2D mCenter;
			float mInnerRadius;
			float mOuterRadius;
			Dia::Maths::Vector2D mAxis;
			Dia::Maths::Angle mHalfAngle;
		};
	}
}

#endif // DIA_GEOMETRY2D_ANNULARSECTOR_H
